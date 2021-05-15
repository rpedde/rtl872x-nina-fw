#include <string.h>

#include "handlers.h"

const char* FIRMWARE_VERSION = "0.0.1-alpha";

int get_conn_status(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;  // parameters
    response[3] = 1;  // len
    response[4] = WL_IDLE_STATUS;

    return 6;
}

int get_mac_addr(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    uint8_t mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    response[2] = 1;  // parameters
    response[3] = 6;  // len
    memcpy(&response[4], mac, 6);

    return 11;
}

int get_firmware_version(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;
    response[3] = strlen(FIRMWARE_VERSION);
    memcpy(&response[4], FIRMWARE_VERSION, response[3]);

    return 5 + response[3];
}
