#include <wifi_conf.h>
#include <wifi_structures.h>
#include <lwip_netconf.h>
#include <wifi_constants.h>
#include <osdep_service.h>

#include "wifi-api.h"
#include "errors.h"


#define GET_CLIENT_MAC  1
#define GET_AP_MAC      2

typedef struct {
    uint8_t status;
    rtw_mode_t mode;

    /* station mode settings */
    char ssid[32 + 1];
    uint8_t ap_mac[6];
    uint8_t rssi;
    uint8_t channel;
    rtw_security_t security_type;
    char password[64 + 1];

    /* ... */
} api_wifi_config_t;

static api_wifi_config_t wifi;
static uint8_t wifi_api_last_error;
extern struct netif xnetif[NET_IF_NUM];

/* forwards */
static int wifi_api_error(uint8_t error);


int wifi_api_setup(void) {
    memset(&wifi, 0x0, sizeof(wifi));

    if(wifi_manager_init() != RTW_SUCCESS)
        return 1;

    LwIP_Init();

    if(wifi_on(RTW_MODE_STA) < 0)
        return 1;

    return 0;
}

void wifi_set_last_error(uint8_t error) {
    wifi_api_last_error = error;
}


int wifi_api_error(uint8_t error) {
    wifi_api_last_error = error;
    return error;
}


int wifi_api_conn_status(uint8_t *status) {
    *status = wifi.status;
    return 0;
}

/* [TODO] should these return values if STA is not up?
 * [TODO] fix when we have other modes
 */
void wifi_api_mac_addr(uint8_t *mac) {
    memcpy(mac, LwIP_GetMAC(&xnetif[0]), 6);
}

void wifi_api_get_ip(uint8_t *ip) {
    memcpy(ip, LwIP_GetIP(&xnetif[0]), 4);
}

void wifi_api_get_netmask(uint8_t *mask) {
    memcpy(mask, LwIP_GetMASK(&xnetif[0]), 4);
}

void wifi_api_get_gw(uint8_t *gw) {
    memcpy(gw, LwIP_GetGW(&xnetif[0]), 4);
}

static int _find_ap(char *buf, int buflen, char *target_ssid, void *user_data) {
    /* [TODO] connect to ap with highest signal strength */

    api_wifi_config_t *pwifi = (api_wifi_config_t *)user_data;

    int plen = 0;
    while(plen < buflen) {
        uint8_t len, ssid_len;
        char *ssid;

        // len u8 @ 0
        // mac 6 x u8 @ 1
        // rssi u32 @ 7
        // security type u8 @ 11
        // wps_password_id u8 @ 12
        // channel u8 @ 13
        // ssid varchar @ 14
        len = (int) *(buf + plen);
        if(!len) // at end
            break;

        ssid_len = len - 14;
        ssid = buf + plen + 14;

        if((ssid_len == strlen(target_ssid)) &&
           (!memcmp(ssid, target_ssid, ssid_len))) {
            // we have a match!
            strcpy(pwifi->ssid, target_ssid);
            memcpy(pwifi->ap_mac, buf + plen + 1, sizeof(pwifi->ap_mac));
            pwifi->rssi = *(buf + plen + 7);
            pwifi->security_type = *(buf + plen + 11);
            pwifi->channel = *(buf + plen + 13);

            if(pwifi->security_type == 0)
                pwifi->security_type = RTW_SECURITY_OPEN;
            else if(pwifi->security_type == 1)
                pwifi->security_type = RTW_SECURITY_WEP_PSK;
            else if(pwifi->security_type == 2)
                pwifi->security_type = RTW_SECURITY_WPA2_TKIP_PSK;
            else if(pwifi->security_type == 3)
                pwifi->security_type = RTW_SECURITY_WPA2_AES_PSK;

            printf("SCAN: ch %d, rssi %d\n", pwifi->channel, pwifi->rssi);
        }
        plen += len;
    }
    return 0;
}

int wifi_connect(char *ssid, char *password) {
    int res;

    /*
     * [TODO] disconnect if already connected?
     * [TODO] shut down ap if in ap mode
     */

    if(strlen(ssid) > 32)
        return wifi_api_error(E_SSID_TOO_LONG);

    if(strlen(password) > 64)
        return wifi_api_error(E_PASSWORD_TOO_LONG);

    // back to idle with no connection info
    memset(&wifi, 0x0, sizeof(wifi));
    res = wifi_scan_networks_with_ssid(_find_ap, (void *)&wifi, 1000, ssid, strlen(ssid));
    if(RTW_SUCCESS != res)
        return wifi_api_error(E_SCAN_ERROR);

    if(strcmp(wifi.ssid, ssid) != 0) {
        wifi.status = WL_NO_SSID_AVAIL;
        return wifi_api_error(E_SSID_NOT_FOUND);
    }


    res = wifi_connect(wifi.ssid, wifi.security_type,
                       password, strlen(wifi.ssid),
                       strlen(password), -1, NULL);

    if(RTW_SUCCESS != res) {
        printf("wifi connect fail: error %d", res);
        wifi.status = WL_CONNECT_FAILED;
        return wifi_api_error(E_CONNECT_ERROR);
    }

    printf("wifi connected\n");

    // [TODO] allow for static ip?
    res = LwIP_DHCP(0, DHCP_START);
    if(DHCP_ADDRESS_ASSIGNED != res)
        return wifi_api_error(E_DHCP_ERROR);

    printf("Got dhcp address\n");

    strcpy(wifi.password, password);

    wifi.status = WL_CONNECTED;

    // [TODO] callback handler for disconnect event to close sockets and set status
    return 0;
}
