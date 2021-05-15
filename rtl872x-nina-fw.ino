// -*- mode: c -*-

#include <stdio.h>

#include "spi/spi-dispatch.h"

void setup(void) {
    spi_dispatch_setup();
}


void loop(void) {
    printf("boot\n");
    while(1) {
        spi_dispatch_run();
    }
}
