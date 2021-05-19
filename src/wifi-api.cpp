#include <wifi_conf.h>
#include <wifi_structures.h>
#include <lwip_netconf.h>
#include <wifi_constants.h>
#include <osdep_service.h>
#include <lwip/netdb.h>
//#include <lwip/tcp.h>
#include <lwip/api.h>
#include <errno.h>

#include "wifi-api.h"
#include "errors.h"


#define GET_CLIENT_MAC   1
#define GET_AP_MAC       2

#define MAX_SOCKETS     10

typedef struct {
    uint8_t type;
    uint8_t state;
    struct netconn *pnc;
} socket_info_t;

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
    socket_info_t sockets[MAX_SOCKETS];
} api_wifi_config_t;

static api_wifi_config_t wifi;
static uint8_t wifi_api_last_error;
extern struct netif xnetif[NET_IF_NUM];

/* forwards */
static int wifi_api_error(uint8_t error);


int wifi_reset_state(bool initial) {
    if(!initial) {
        // need to reset all the sockets and
        // servers.
    }
    memset(&wifi, 0x0, sizeof(wifi));


    for(int i=0; i<MAX_SOCKETS; i++)
        wifi.sockets[i].type = SOCK_MODE_UNALLOCATED;

    return E_SUCCESS;
}

int wifi_api_setup(void) {
    wifi_reset_state(true);

    if(wifi_manager_init() != RTW_SUCCESS)
        return 1;

    LwIP_Init();

    if(wifi_on(RTW_MODE_STA) < 0)
        return 1;

    return E_SUCCESS;
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
    return E_SUCCESS;
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
    return E_SUCCESS;
}

int wifi_api_connect(char *ssid, char *password) {
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
    wifi_reset_state(false);

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
    return E_SUCCESS;
}

int wifi_api_get_host_by_name(char *hostname, uint32_t *addr) {
    struct hostent *host;

    if (inet_aton(hostname, &addr) == 0) {
        host = gethostbyname(hostname);
        if(!host)
            return wifi_api_error(E_HOST_NOT_FOUND);
        memcpy(addr, host->h_addr, sizeof(host->h_addr));
    }
    return E_SUCCESS;
}

int wifi_api_get_socket(uint8_t *socket) {
    for(int i=0; i<MAX_SOCKETS; i++) {
        if(wifi.sockets[i].type == SOCK_MODE_UNALLOCATED) {
            wifi.sockets[i].type = SOCK_MODE_IDLE;
            *socket = i;
            return E_SUCCESS;
        }
    }
    return E_SOCKETS_EXHAUSTED;
}

int wifi_api_stop_socket(uint8_t socket) {
    err_t err;

    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    switch(wifi.sockets[socket].type) {
    case SOCK_MODE_IDLE:
        wifi.sockets[socket].type = SOCK_MODE_UNALLOCATED;
        break;
    case SOCK_MODE_TCP:
        if(wifi.sockets[socket].state != SOCK_STATE_CLOSED) {
            if(wifi.sockets[socket].pnc) {
                // not sure either of these cases is recoverable.
                if((err = netconn_close(wifi.sockets[socket].pnc)) != ERR_OK)
                    printf("error closing netconn: %d\n", err);

                if((err = netconn_delete(wifi.sockets[socket].pnc)) != ERR_OK)
                    printf("error deleting netconn: %d\n", err);
            wifi.sockets[socket].pnc = NULL;
            }
        }
        wifi.sockets[socket].type = SOCK_MODE_UNALLOCATED;
        break;
    default:
        // unhandled socket type
        printf("unhandled socket type: %d\n", wifi.sockets[socket].type);
        return E_INTERNAL;
    }
    return E_SUCCESS;
}

int wifi_api_tcp_socket_connect(ip_addr_t *ip, uint16_t port, uint8_t socket) {
    wifi.sockets[socket].pnc = netconn_new(NETCONN_TCP);
    if (!wifi.sockets[socket].pnc) {
        return E_NOMEM;
    }

    printf("connecting\n");
    if(netconn_connect(wifi.sockets[socket].pnc, ip, port) != ERR_OK) {
        printf("error calling connect");
        wifi.sockets[socket].state = SOCK_STATE_CLOSED;
        return E_SUCCESS;
    }

    wifi.sockets[socket].state = SOCK_STATE_ESTABLISHED;
    return E_SUCCESS;
}

int wifi_api_get_socket_state(uint8_t socket, uint8_t *state) {
    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    *state = wifi.sockets[socket].state;
    return E_SUCCESS;
}


int wifi_api_socket_connect(char *hostname, uint32_t ipaddr,
                            uint16_t port, uint8_t socket, uint8_t mode) {
    ip_addr_t ip;
    uint32_t resolved_ip;
    struct hostent *host;

    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    if(wifi.sockets[socket].type != SOCK_MODE_IDLE) {
        printf("attempt to connect a socket in state %d",
               wifi.sockets[socket].type);
        return E_INVALID_PARAMETER;
    }

    memcpy(&ip, &ipaddr, sizeof(ipaddr));

    if(hostname[0]) {
        if (inet_aton(hostname, &resolved_ip) == 0) {
            host = gethostbyname(hostname);
            if(!host)
                return wifi_api_error(E_HOST_NOT_FOUND);
            memcpy(&resolved_ip, host->h_addr, sizeof(host->h_addr));
        }
    }

    ip = IPADDR4_INIT(resolved_ip);
    printf("setting socket %d type to %d\n", socket, mode);
    wifi.sockets[socket].type = mode;

    switch(mode) {
    case SOCK_MODE_TCP:
        return wifi_api_tcp_socket_connect(&ip, port, socket);
        break;
    default:
        printf("unhandled socket connect mode: %d\n", mode);
        return E_INTERNAL;
    }

    return E_SUCCESS;
}
