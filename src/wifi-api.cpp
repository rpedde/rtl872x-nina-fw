#include "wifi_conf.h"
#include "wifi-api.h"

#define GET_CLIENT_MAC  1
#define GET_AP_MAC      2

typedef struct {
    int status;
} WIFI;

static WIFI wifi;

int wifi_api_setup(void) {
    wifi.status = 0;

    if(RTW_SUCCESS != wifi_manager_init()) {
        return 1;
    }
    return 0;
}

int wifi_api_conn_status(uint8_t *status) {
    *status = wifi.status;
    return 0;
}

int wifi_api_mac_addr(uint8_t *mac) {
    return wifi_get_mac_address((char*)mac) != RTW_SUCCESS;
}
