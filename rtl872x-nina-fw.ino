// -*- mode: c -*-

#include <stdio.h>

#include "spi-dispatch.h"
#include "serial-dispatch.h"
#include "wifi-api.h"

int serial_mode = 1;

void setup(void) {
    wifi_api_setup();

    if(serial_mode) {
        serial_dispatch_setup();
    } else {
        spi_dispatch_setup();
    }
}


void loop(void) {
    if(serial_mode) {
        printf("SERIAL_MODE\n");
        while(1) {
            serial_dispatch_run();
        }
    } else {
        printf("SPI_MODE\n");
        while(1) {
            spi_dispatch_run();
        }
    }
}
