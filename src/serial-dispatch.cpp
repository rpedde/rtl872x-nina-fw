/*
 * This is just a dumb serial dispatcher that will do the same SPI
 * requests over serial.  So it bundles up a serial requests, hands it
 * off to a handler just like the SPI disaptcher does, and then returns
 * the result over serial.  This makes it easier to poke at from a low
 * level when the the rtl is embedded as a coprocessor in some other
 * device (like the wio terminal)
 */

#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "serial-dispatch.h"
#include "spi-dispatch.h"

int serial_dispatch_setup(void) {
    Serial.begin(115200);
    Serial.setTimeout(0);

    return 0;
}

int serial_dispatch_run(void) {
    uint8_t bytes[3];
    uint16_t len;

    while(1) {
        // first char is @
        while(!Serial.available());
        Serial.readBytes(bytes, 1);
        printf("> 0x%02X\n", bytes[0]);
        if(bytes[0] != '@')
            continue;

        // then 16 bit length
        Serial.readBytes(bytes, 2);
        len = (bytes[0] << 8) | bytes[1];

        if(len > SPI_RX_BUFFER_SIZE) {
            printf("buffer of size %d to large\n", len);
            continue;
        }

        printf("L: %d\n", len);

        int remaining = len;
        uint8_t *pos = rx_buffer;

        while(remaining) {
            int read = Serial.readBytes(pos, remaining);
            remaining -= read;
            if(!remaining)
                break;

            pos += read;
        }

        for(int x=0; x < len; x++)
            printf("%02X:", rx_buffer[x]);
        printf("\n");

        uint16_t response_len = spi_dispatch_command(rx_buffer, tx_buffer);

        printf("R: %d\n", response_len);

        bytes[0] = '@';
        bytes[1] = (response_len >> 8) & 0xff;
        bytes[2] = response_len & 0xff;

        Serial.write(bytes, 3);
        Serial.write(tx_buffer, response_len);
    }
}
