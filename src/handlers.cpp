#include <osdep_service.h>
#include <string.h>
#include <stdio.h>

#include "handlers.h"
#include "wifi-api.h"
#include "errors.h"


const char* FIRMWARE_VERSION = "0.0.1-alpha";
uint32_t resolved_hostname;


command_handler command_handlers[] = {
    // 0x00 - 0x0f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    // 0x10 - 0x1f
    h_set_net, h_set_passphrase, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    // 0x20 - 0x2f
    h_get_conn_status, h_get_ip_addr, h_get_mac_addr, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    // 0x30 - 0x3f
    NULL, NULL, NULL, NULL,
    h_req_host_by_name, h_get_host_by_name, NULL, h_get_firmware_version,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    // 0x40 - 0x4f
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    // 0x50 - 0x5f
    NULL, NULL, NULL, NULL,
    NULL
};

#define MAX_HANDLERS (sizeof(command_handlers) / sizeof(command_handlers[0]))

command_handler handler_for(uint8_t command) {
    if(command > MAX_HANDLERS)
        return NULL;
    return command_handlers[command];
}

/* h_set_net: 0x20
 *
 * in param 1: ssid of open network to connect to
 * out param 1: 1 for success
 */
int h_set_net(const uint8_t *command, uint8_t *response) {
    char ssid[32 + 1];

    if(command[3] > 32) {
        wifi_set_last_error(E_SSID_TOO_LONG);
        return 0;
    }

    memset(ssid, 0x0, sizeof(ssid));
    memcpy(ssid, &command[4], command[3]);

    int res = wifi_api_connect(ssid, NULL);

    if(res)
        return 0;

    response[2] = 1; // number of parameters
    response[3] = 1; // length of parameter
    response[4] = 1; // success!

    return 6;
}

/* h_set_passphrase: 0x21
 *
 * in param 1: ssid of protected network to connect to
 * in param 2: password/passphrase
 */
int h_set_passphrase(const uint8_t *command, uint8_t *response) {
    char ssid[32 + 1];
    char password[64 + 1];

    uint8_t ssid_len = command[3];
    uint8_t pass_len = command[4 + ssid_len];

    memset(ssid, 0x0, sizeof(ssid));
    memset(password, 0x0, sizeof(password));

    if(ssid_len > 32) {
        wifi_set_last_error(E_SSID_TOO_LONG);
        return 0;
    }

    if(pass_len > 64) {
        wifi_set_last_error(E_PASSWORD_TOO_LONG);
        return 0;
    }

    memcpy(ssid, &command[4], ssid_len);
    memcpy(password, &command[5 + ssid_len], pass_len);

    int res = wifi_api_connect(ssid, password);

    if(res)
        return 0;

    response[2] = 1; // number of parameters
    response[3] = 1; // length of parameter
    response[4] = 1; // success!

    return 6;
}


/* h_get_conn_status: 0x20
 *
 * out param 1: wifi conn status (see wifi-api.h)
 */
int h_get_conn_status(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;  // parameters
    response[3] = 1;  // len

    if(wifi_api_conn_status(&response[4]))
        return 0;  // error

    return 6;
}

/* h_get_ip_addr: 0x21
 *
 * out param 1: ip (uint32_t)
 * out param 2: mask (uint32_t)
 * out param 3: gw (uint32_t)
 */
int h_get_ip_addr(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 3;  // parameters
    response[3] = 4;  // len
    wifi_api_get_ip(&response[4]);
    response[8] = 4;
    wifi_api_get_netmask(&response[9]);
    response[13] = 4;
    wifi_api_get_gw(&response[14]);

    return 19;
}

/* h_get_mac_addr: 0x22
 *
 * out param 1: mac of interface (only when up)
 */
int h_get_mac_addr(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;  // parameters
    response[3] = 6;  // len
    wifi_api_mac_addr(&response[4]);

    return 11;
}


/* h_req_host_by_name: 0x35
 *
 * out param 1: success (1) or failure (0)
 */
int h_req_host_by_name(const uint8_t *command, uint8_t *response) {
    char hostname[255 + 1];

    memset(hostname, 0x00, sizeof(hostname));
    memcpy(hostname, &command[4], command[3]);

    response[2] = 1;  // parameters
    response[3] = 1;  // len

    if(wifi_api_get_host_by_name(hostname, &resolved_hostname) == 0) {
        response[4] = 1;
    } else {
        response[4] = 0;
    }

    return 6;
}

/* h_get_host_by_name: 0x35
 *
 * out param 1: ip (uint32_t)
 */
int h_get_host_by_name(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;  // parameters
    response[3] = 4;  // len
    memcpy(&response[4], &resolved_hostname, 4);

    return 9;
}


/* h_get_firmware_version: 0x37
 *
 * out param 1: firmware version string
 */
int h_get_firmware_version(const uint8_t *command, uint8_t *response) {
    UNUSED(command);

    response[2] = 1;
    response[3] = strlen(FIRMWARE_VERSION);
    memcpy(&response[4], FIRMWARE_VERSION, response[3]);

    return 5 + response[3];
}
