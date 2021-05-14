// -*- mode: c -*-

#include <stdio.h>

#include "spi/spi-dispatch.h"

void setup(void) {
    for(int i=10; i; i--) {
        printf("Counting: %d\n", i);
        delay(1000);
    }

    spi_dispatch_setup();
}


void loop(void) {
    while(1) {
        spi_dispatch_run();
    }
}
