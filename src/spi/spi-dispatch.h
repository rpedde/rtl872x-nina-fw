#ifndef _SPI_DISPATCH_H_
#define _SPI_DISPATCH_H_

#include <stdint.h>

#define USI_SPI_DIR   (_PA_13)
#define USI_SPI_READY (_PA_12)
#define USI_SPI_MISO  (PA_25)
#define USI_SPI_MOSI  (PA_26)
#define USI_SPI_SCLK  (PA_30)
#define USI_SPI_CS    (PA_28)

#define SPI_DIRECTION_RX 1
#define SPI_DIRECTION_TX 0

#define SPI_TX_BUFFER_SIZE 256
#define SPI_RX_BUFFER_SIZE 256


extern int spi_dispatch_setup(void);
extern int spi_dispatch_run(void);

extern int spi_dispatch_read(uint8_t *, uint16_t);
extern int spi_dispatch_write(uint8_t *, uint16_t);

typedef int (*command_handler)(const uint8_t *, uint8_t *);

#endif /* _SPI_DISPATCH_H_ */
