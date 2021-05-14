#include <osdep_service.h>
#include <device.h>
#include <stdio.h>

#include "rtl8721d_usi_ssi.h"

#include "spi-dispatch.h"

typedef int (*command_handler)(uint8_t **, uint8_t **);


command_handler command_handlers[] = {
    NULL, NULL, NULL, NULL,  // 0x00 - 0x0f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    NULL, NULL, NULL, NULL,  // 0x10 - 0x1f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    NULL, NULL, NULL, NULL,  // 0x20 - 0x2f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    NULL, NULL, NULL, NULL,  // 0x30 - 0x3f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    NULL, NULL, NULL, NULL,  // 0x40 - 0x4f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    NULL, NULL, NULL, NULL,  // 0x50 - 0x5f
    NULL
};

#define MAX_HANDLERS (sizeof(command_handlers) / sizeof(command_handlers[0]))
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
    gpioDef.GPIO_Pin = USI_SPI_DIR;
    gpioDef.GPIO_Mode = GPIO_Mode_OUT;
    gpioDef.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(&gpioDef);

    memset(&gpioDef, 0, sizeof(gpioDef));
    gpioDef.GPIO_Pin = USI_SPI_READY;
    gpioDef.GPIO_Mode = GPIO_Mode_OUT;
    gpioDef.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(&gpioDef);

    spi_dispatch_set_direction(SPI_DIRECTION_RX);
    spi_dispatch_set_data_ready(0);

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

void spi_dispatch_set_direction(int which) {
    GPIO_WriteBit(USI_SPI_DIR, which == SPI_DIRECTION_RX ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

void spi_dispatch_set_data_ready(int ready) {
    GPIO_WriteBit(USI_SPI_READY, ready ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
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

    spi_dispatch_set_data_ready(1);
    rtw_down_sema(&sem_tx_done);
    rtw_down_sema(&sem_rx_done);

    spi_dispatch_set_data_ready(0);
    return len;
}

int spi_dispatch_run(void) {
    uint8_t rx_dat[3];
    uint8_t tx_dat[3];

    while(1) {
        // assume rx sync
        if(ssi_obj.tx_len != 0)
            printf("tx not 0\n");

        if(ssi_obj.rx_len != 0)
            printf("rx not 0\n");

        spi_dispatch_read(&rx_dat[0], 3);
        printf("->%02x:%02x:%02x\n", rx_dat[0], rx_dat[1], rx_dat[2]);
        memcpy(tx_dat, rx_dat, 3);
        tx_dat[0] += 1;
        tx_dat[1] += 1;
        tx_dat[2] += 1;
        spi_dispatch_write(&tx_dat[0], 3);
        printf("<-%02x:%02x:%02x\n", tx_dat[0], tx_dat[1], tx_dat[2]);
    }
}
