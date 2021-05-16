#include <string.h>

#include "handlers.h"
#include "wifi-api.h"

const char* FIRMWARE_VERSION = "0.0.1-alpha";

int get_conn_status(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;  // parameters
    response[3] = 1;  // len

    if(wifi_api_conn_status(&response[4]))
        return 0;  // error

    return 6;
}

int get_mac_addr(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;  // parameters
    response[3] = 6;  // len
    if(wifi_api_mac_addr(&response[4])) // error
        return 0;

    return 11;
}

int get_firmware_version(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;
    response[3] = strlen(FIRMWARE_VERSION);
    memcpy(&response[4], FIRMWARE_VERSION, response[3]);

    return 5 + response[3];
}
