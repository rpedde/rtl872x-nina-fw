#include <osdep_service.h>
#include <device.h>
#include <stdio.h>

#include "rtl8721d_usi_ssi.h"

#include "spi-dispatch.h"
#include "handlers.h"

#define min(a,b) ((a)<(b) ? (a):(b))

typedef struct SSI_OBJ {
    USI_TypeDef *usi_dev;
    uint32_t rx_len;
    uint32_t tx_len;
    uint8_t *rx_buf;
    uint8_t *tx_buf;
    uint32_t role;
} SSI_OBJ;

static SSI_OBJ ssi_obj;

static _sema sem_tx_done;
static _sema sem_rx_done;

uint8_t tx_buffer[SPI_TX_BUFFER_SIZE];
uint8_t rx_buffer[SPI_RX_BUFFER_SIZE];

// forwards
static void spi_dispatch_setup_write(SSI_OBJ *, uint8_t *, uint32_t);
static void spi_dispatch_setup_read(SSI_OBJ *, uint8_t *, uint32_t);
static uint32_t spi_dispatch_irq_handler(void *);
static void spi_dispatch_set_ready_pin(int value);

int spi_dispatch_setup(void) {
    // according to schematic

    // MOSI - pin 10 (PA25,RXD/USB_DM/I2C_SCL)
    // MISO - pin 9 (PA26,TXD/USB_DP)
    // CS - pin 8 (PA28/CTS/PWM6/RREF)
    // CLK - pin 7 (PA30/PWM7/SWD_CLK)
    // SYNC - pin 3 (PA13/RXD/MISO/PWM1/I2S_TX1)
    // IRQ0 (data ready) - pin 2 (PA12/TXD/MOSI/PWM0/I2S_MCLK)

    // set up semaphores
    rtw_init_sema(&sem_rx_done, 1);
    rtw_init_sema(&sem_tx_done, 1);

    rtw_down_sema(&sem_rx_done);
    rtw_down_sema(&sem_tx_done);

    // set up gpios
    GPIO_InitTypeDef gpioDef;

    memset(&gpioDef, 0, sizeof(gpioDef));
    gpioDef.GPIO_Pin = USI_SPI_READY;
    gpioDef.GPIO_Mode = GPIO_Mode_OUT;
    gpioDef.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(&gpioDef);

    spi_dispatch_set_ready_pin(0);

    /* set up spi device */
    RCC_PeriphClockCmd(APBPeriph_USI_REG, APBPeriph_USI_CLOCK, ENABLE);
    Pinmux_Config(USI_SPI_MOSI, PINMUX_FUNCTION_SPIS);
    Pinmux_Config(USI_SPI_MISO, PINMUX_FUNCTION_SPIS);
    Pinmux_Config(USI_SPI_CS  , PINMUX_FUNCTION_SPIS);
    Pinmux_Config(USI_SPI_SCLK, PINMUX_FUNCTION_SPIS);

    PAD_PullCtrl(USI_SPI_MOSI, GPIO_PuPd_NOPULL);
    PAD_PullCtrl(USI_SPI_MISO, GPIO_PuPd_NOPULL);
    PAD_PullCtrl(USI_SPI_CS  , GPIO_PuPd_UP);
    PAD_PullCtrl(USI_SPI_SCLK, GPIO_PuPd_NOPULL);

    USI_SSI_InitTypeDef USI_SSI_InitStruct;
    USI_SSI_StructInit(&USI_SSI_InitStruct);
    USI_SSI_InitStruct.USI_SPI_Role = USI_SPI_SLAVE;
    USI_SSI_InitStruct.USI_SPI_SclkPhase = USI_SPI_SCPH_TOGGLES_IN_MIDDLE;
    USI_SSI_InitStruct.USI_SPI_SclkPolarity = USI_SPI_SCPOL_INACTIVE_IS_LOW;
    USI_SSI_InitStruct.USI_SPI_DataFrameSize = 8 - 1;
    USI_SSI_Init(USI0_DEV, &USI_SSI_InitStruct);

    USI_SSI_SetTxFifoLevel(USI0_DEV, USI_SPI_TX_FIFO_DEPTH / 2);

    ssi_obj.usi_dev = USI0_DEV;

    InterruptRegister((IRQ_FUN)spi_dispatch_irq_handler, USI_IRQ, (uint32_t)&ssi_obj, 10);
    InterruptEn(USI_IRQ, 10);
    return 0;
}

static void spi_dispatch_set_ready_pin(int value) {
    GPIO_WriteBit(USI_SPI_READY, value == 1 ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

static uint32_t spi_dispatch_irq_handler(void *arg) {
    SSI_OBJ *pssi = (SSI_OBJ*)arg;
    uint32_t irq_status = USI_SSI_GetIsr(pssi->usi_dev);

    USI_SSI_SetIsrClean(pssi->usi_dev, irq_status);

    if(irq_status & USI_RXFIFO_ALMOST_FULL_INTS) {
        uint32_t xlen = USI_SSI_ReceiveData(pssi->usi_dev, pssi->rx_buf, pssi->rx_len);
        pssi->rx_len -= xlen;
        if(pssi->rx_buf)
            pssi->rx_buf += xlen;

        if(pssi->rx_len) {
            USI_SSI_SetRxFifoLevel(USI0_DEV, min(pssi->rx_len - 1, USI_SPI_RX_FIFO_DEPTH / 2));
        } else {
            USI_SSI_INTConfig(pssi->usi_dev,
                              USI_RXFIFO_ALMOST_FULL_INTR_EN |
                              USI_RXFIFO_OVERFLOW_INTR_EN |
                              USI_RXFIFO_UNDERFLOW_INTR_EN,
                              DISABLE);
            rtw_up_sema(&sem_rx_done);
        }
    }

    if(irq_status & USI_TXFIFO_ALMOST_EMTY_INTS) {
        uint32_t xlen = USI_SSI_SendData(pssi->usi_dev, pssi->tx_buf, pssi->tx_len, pssi->role);
        pssi->tx_len -= xlen;
        if(pssi->tx_buf)
            pssi->tx_buf += xlen;

        if(!pssi->tx_len) {
            USI_SSI_INTConfig(pssi->usi_dev,
                              USI_TXFIFO_OVERFLOW_INTR_EN |
                              USI_TXFIFO_ALMOST_EMTY_INTR_EN,
                              DISABLE);
            rtw_up_sema(&sem_tx_done);
        }
    }

    return 0;
}


static void spi_dispatch_setup_read(SSI_OBJ *pssi, uint8_t *buffer, uint32_t len) {
    pssi->rx_len = len;
    pssi->rx_buf = buffer;

    uint32_t xlen = USI_SSI_ReceiveData(pssi->usi_dev, pssi->rx_buf, pssi->rx_len);

    pssi->rx_len -= xlen;
    if(pssi->rx_buf)
        pssi->rx_buf += xlen;

    if(pssi->rx_len) {
        USI_SSI_SetRxFifoLevel(USI0_DEV, min(pssi->rx_len - 1, USI_SPI_RX_FIFO_DEPTH / 2));
        USI_SSI_INTConfig(
            pssi->usi_dev,
            USI_RXFIFO_ALMOST_FULL_INTR_EN |
            USI_RXFIFO_OVERFLOW_INTR_EN |
            USI_RXFIFO_UNDERFLOW_INTR_EN,
            ENABLE);
    } else {
        rtw_up_sema(&sem_rx_done);
    }
}

/* the version of USI_SSI_SendData in the arduino sdk I have (at least)
 * is fubar, and was apparently always sending a full fifo worth of data,
 * even when you asked to write less.
 *
 * fix it here locally, I guess.  Thx rtl.
 */
uint32_t X_USI_SSI_SendData(USI_TypeDef *usi_dev, uint8_t *buffer, uint32_t len, uint32_t role) {
    uint32_t writable = USI_SSI_Writeable(usi_dev);
    uint32_t tx_write_max = USI_SPI_TX_FIFO_DEPTH - USI_SSI_GetTxCount(usi_dev);
    uint32_t tx_len = len;

    if(writable) {
        while(tx_write_max--) {
            // this assumes dataframesize <= 8 bits.
            if(buffer != NULL) {
                USI_SSI_WriteData(usi_dev, *buffer);
                buffer += 1;
            } else {
                if(role == USI_SPI_MASTER) {
                    // empty write to (for example) force a read
                    USI_SSI_WriteData(usi_dev, 0);
                }
            }

            tx_len -= 1;
            if(!tx_len)
                break;
        }
    }
    return (len - tx_len);
}

static void spi_dispatch_setup_write(SSI_OBJ *pssi, uint8_t *buffer, uint32_t len) {
    while(USI_SSI_Busy(pssi->usi_dev));

    pssi->tx_len = len;
    pssi->tx_buf = buffer;

    uint32_t xlen = X_USI_SSI_SendData(pssi->usi_dev, pssi->tx_buf, pssi->tx_len, pssi->role);

    pssi->tx_len -= xlen;
    if(pssi->tx_buf)
        pssi->tx_buf += xlen;

    if(pssi->tx_len) {
        USI_SSI_INTConfig(
            pssi->usi_dev,
            USI_TXFIFO_OVERFLOW_INTR_EN | USI_TXFIFO_ALMOST_EMTY_INTR_EN,
            ENABLE);
    } else {
        rtw_up_sema(&sem_tx_done);
    }
}

int spi_dispatch_read(uint8_t *buffer, uint16_t len) {
    spi_dispatch_setup_read(&ssi_obj, buffer, len);
    rtw_down_sema(&sem_rx_done);
    return len;
}

int spi_dispatch_write(uint8_t *buffer, uint16_t len) {
    spi_dispatch_setup_write(&ssi_obj, buffer, len);
    spi_dispatch_setup_read(&ssi_obj, NULL, len);
    spi_dispatch_set_ready_pin(0);

    while((USI_SSI_GetTransStatus(ssi_obj.usi_dev) & 1) == 0) {
        rtw_msleep_os(100);
    };
    spi_dispatch_set_ready_pin(1);

    rtw_down_sema(&sem_tx_done);
    rtw_down_sema(&sem_rx_done);

    return len;
}

int spi_dispatch_command(const uint8_t *in, uint8_t *out, uint16_t out_len) {
    memset(out, 0x00, out_len);

    int response_len = 0;
    command_handler handler = handler_for(in[1]);
    if(handler) {
        response_len = handler(in, out);
    } else {
        printf("bad cmd: %02x\n", in[1]);
    }

    if(response_len == 0) {
        out[0] = 0xef;
        out[1] = 0x00;
        out[2] = 0xee;
        response_len = 3;
    } else {
        out[0] = 0xe0;
        out[1] = (0x80 | in[1]);
        out[response_len - 1] = 0xee;
    }

    return response_len;
}


int spi_dispatch_run(void) {
    uint8_t *ptr;
    int len;
    bool len_16;
    int response_len;

    while(1) {
        len_16 = false;
        spi_dispatch_set_ready_pin(0);
        memset(rx_buffer, 0x0, sizeof(rx_buffer));
        ptr = rx_buffer;

        while((USI_SSI_GetTransStatus(ssi_obj.usi_dev) & 1) == 0) {};
        spi_dispatch_set_ready_pin(1);
        spi_dispatch_read(ptr, 3);  // start, command, param_count

        if(rx_buffer[1] == 0x44)  // send tcp data
            len_16 = true;

        ptr = rx_buffer + 3;

        for(uint16_t idx=0; idx < rx_buffer[3]; idx++) {
            // read in the parameters
            if(len_16) {
                spi_dispatch_read(ptr, 2);
                len = (*ptr << 8 | *(ptr+1));
                ptr += 2;
            } else {
                spi_dispatch_read(ptr, 1);
                len = *ptr++;
            }
            spi_dispatch_read(ptr, len);
            ptr += len;
        }
        spi_dispatch_read(ptr++, 1);
        len = ptr - rx_buffer;

        // printf("-> %d byte cmd:  %02x %02x %02x %02x\n",
        //        len, rx_buffer[0],
        //        rx_buffer[1], rx_buffer[2],
        //        *(ptr - 1));

        response_len = spi_dispatch_command(rx_buffer, tx_buffer, sizeof(tx_buffer));

        spi_dispatch_write(tx_buffer, response_len);
        while((USI_SSI_GetTransStatus(ssi_obj.usi_dev) & 1) == 1) {};
        spi_dispatch_set_ready_pin(0);
    }
}
