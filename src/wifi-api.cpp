#include <wifi_conf.h>
#include <wifi_structures.h>
#include <lwip_netconf.h>
#include <wifi_constants.h>
#include <osdep_service.h>
#include <lwip/netdb.h>
#include <lwip/tcp.h>
#include <lwip/api.h>
#include <errno.h>

#include "wifi-api.h"
#include "errors.h"


#define GET_CLIENT_MAC   1
#define GET_AP_MAC       2

#define min(a,b) ((a)<(b) ? (a):(b))

#define MAX_SOCKETS     10

typedef struct {
    uint8_t type;
    uint8_t state;
    uint8_t nagle_enabled;

    uint8_t *current_data;
    struct netconn *pnc;
    struct netbuf *pbuf;
    uint16_t offset;
    uint16_t dlen;
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

api_wifi_config_t wifi;
static uint8_t wifi_api_last_error;
extern struct netif xnetif[NET_IF_NUM];

/* forwards */
static int wifi_api_error(uint8_t error);


int wifi_api_teardown_socket(socket_info_t *psi) {
    err_t err;

    switch(psi->type) {
    case SOCK_TYPE_UNALLOCATED:
    case SOCK_TYPE_IDLE:
        break;
    case SOCK_TYPE_TCP:
        if(psi->state != SOCK_STATE_CLOSED) {
            if(psi->pbuf) {
                netbuf_free(psi->pbuf);
                psi->pbuf = NULL;
            }

            if(psi->pnc) {
                printf("closing netconn\n");
                if((err = netconn_close(psi->pnc)) != ERR_OK)
                    printf("error closing netconn: %d\n", err);

                printf("deleting netconn\n");
                if((err = netconn_delete(psi->pnc)) != ERR_OK)
                    printf("error deleting netconn: %d\n", err);
            psi->pnc = NULL;
            psi->offset = 0;
            psi->dlen = 0;
            }
        }
        break;
    default:
        printf("Unhandled socket type %d\n", psi->type);
        return E_INTERNAL;
    }

    psi->type = SOCK_TYPE_UNALLOCATED;
    return E_SUCCESS;
}

int wifi_reset_state(bool initial) {
    if(initial) {
        memset(&wifi, 0x00, sizeof(wifi));
        for(int idx=0; idx<MAX_SOCKETS; idx++) {
            wifi.sockets[idx].type = SOCK_TYPE_UNALLOCATED;
        }
    } else {
        wifi.status = WL_IDLE_STATUS;
        wifi.mode = RTW_MODE_STA;
        memset(wifi.ssid, 0x00, sizeof(wifi.ssid));
        memset(wifi.ap_mac, 0x00, sizeof(wifi.ap_mac));
        memset(wifi.password, 0x00, sizeof(wifi.password));
        wifi.rssi = 0;
        wifi.channel = 0;
        wifi.security_type = 0;

        if(!initial) {
            for(int idx=0; idx<MAX_SOCKETS; idx++) {
                wifi_api_teardown_socket(&wifi.sockets[idx]);
            }
        }
    }
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
        if(wifi.sockets[i].type == SOCK_TYPE_UNALLOCATED) {
            wifi.sockets[i].type = SOCK_TYPE_IDLE;
            *socket = i;
            return E_SUCCESS;
        }
    }
    return E_SOCKETS_EXHAUSTED;
}

int wifi_api_stop_socket(uint8_t socket) {
    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    return wifi_api_teardown_socket(&wifi.sockets[socket]);
}

int wifi_api_tcp_socket_connect(ip_addr_t *ip, uint16_t port, uint8_t socket) {
    err_t err;

    wifi.sockets[socket].pnc = netconn_new(NETCONN_TCP);
    if (!wifi.sockets[socket].pnc) {
        return E_NOMEM;
    }

    printf("connecting");
    if((err = netconn_connect(wifi.sockets[socket].pnc, ip, port)) != ERR_OK) {
        printf("error calling connect: %d\n", err);
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

    if(wifi.sockets[socket].type != SOCK_TYPE_IDLE) {
        printf("attempt to connect a socket in state %d",
               wifi.sockets[socket].type);
        return E_INVALID_PARAMETER;
    }

    ip = IPADDR4_INIT(ipaddr);
    if(hostname[0]) {
        if (inet_aton(hostname, &resolved_ip) == 0) {
            host = gethostbyname(hostname);
            if(!host)
                return wifi_api_error(E_HOST_NOT_FOUND);
            memcpy(&resolved_ip, host->h_addr, sizeof(host->h_addr));
        }
        ip = IPADDR4_INIT(resolved_ip);
    }

    printf("setting socket %d type to %d\n", socket, mode);
    wifi.sockets[socket].type = mode;

    switch(mode) {
    case SOCK_TYPE_TCP:
        return wifi_api_tcp_socket_connect(&ip, port, socket);
        break;
    default:
        printf("unhandled socket connect mode: %d\n", mode);
        return E_INTERNAL;
    }

    return E_SUCCESS;
}

int wifi_api_avail_data(uint8_t socket, uint16_t *avail) {
    err_t err;

    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    socket_info_t *psi = &wifi.sockets[socket];

    switch(psi->type) {
    case SOCK_TYPE_TCP:
        *avail = 0;

        if(!psi->pbuf) {
            // force a recv if possible
            psi->pnc->recv_timeout = 1;

            err = netconn_recv(psi->pnc, &psi->pbuf);
            if(err == ERR_TIMEOUT) {
                printf("still no data\n");
            } else if(err != ERR_OK) {
                printf("Error %d on recv\n", err);
            } else {
                printf("Got data\n");
                psi->dlen = netbuf_len(psi->pbuf);
            }
            psi->pnc->recv_timeout = 0;
        }

        if(psi->pbuf) {
            *avail = psi->dlen - psi->offset;
            printf("len: %d\n", *avail);
        }
        printf("return %d bytes available\n", *avail);
        break;
    default:
        printf("avail data for unsupported sock type %d",
               wifi.sockets[socket].type);

        return E_UNSUPPORTED;
    }
    return E_SUCCESS;
}

int wifi_api_send_data_tcp(uint8_t socket, uint8_t *data,
                           uint16_t bytes_to_write, uint16_t *bytes_written) {
    err_t err;

    if(!wifi.sockets[socket].nagle_enabled) {
        tcp_nagle_enable(wifi.sockets[socket].pnc->pcb.tcp);
        wifi.sockets[socket].nagle_enabled = 1;
    }

    printf("Writing %d bytes to socket %d\n", bytes_to_write, socket);
    err = netconn_write(wifi.sockets[socket].pnc, data, bytes_to_write, NETCONN_COPY);
    if(err != ERR_OK) {
        printf("Error writing: %d", err);
        return E_INTERNAL;
    }

    *bytes_written = bytes_to_write;
    printf("success!\n");
    return E_SUCCESS;
}

int wifi_api_send_data(uint8_t socket, uint8_t *data,
                       uint16_t bytes_to_write, uint16_t *bytes_written) {
    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    switch(wifi.sockets[socket].type) {
    case SOCK_TYPE_TCP:
        return wifi_api_send_data_tcp(socket, data, bytes_to_write, bytes_written);
    default:
        printf("unhandled send data for socket type %d", wifi.sockets[socket].type);
        return E_INTERNAL;
    }

    return E_SUCCESS;
}

int wifi_api_write_flush(uint8_t socket) {
    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    switch(wifi.sockets[socket].type) {
    case SOCK_TYPE_TCP:
        if(wifi.sockets[socket].nagle_enabled) {
            tcp_nagle_disable(wifi.sockets[socket].pnc->pcb.tcp);
            wifi.sockets[socket].nagle_enabled = 0;
        }
    default:
        printf("unhandled send data for socket type %d", wifi.sockets[socket].type);
        return E_INTERNAL;
    }

    return E_SUCCESS;
}



int wifi_api_read_data_tcp(uint8_t socket, uint8_t *data,
                           uint16_t bytes_to_read, uint16_t *bytes_read) {
    err_t err;
    uint8_t *outbuf = data;
    socket_info_t *psi = &wifi.sockets[socket];

    *bytes_read = 0;

    while(bytes_to_read) {
        printf("Reading %d bytes from socket %d\n", bytes_to_read, socket);
        if(!psi->pbuf) {
            printf("forcing read\n");
            err = netconn_recv(psi->pnc, &psi->pbuf);
            if(err != ERR_OK) {
                printf("Error %d reading\n", err);
                return E_INTERNAL;
            }
            psi->dlen = netbuf_len(psi->pbuf);
            psi->offset = 0;

        }

        uint16_t to_copy = min(bytes_to_read, psi->dlen - psi->offset);
        netbuf_copy_partial(psi->pbuf, outbuf, to_copy, psi->offset);

        psi->offset += to_copy;
        bytes_to_read -= to_copy;
        *bytes_read += to_copy;
        outbuf += to_copy;

        if(psi->offset >= psi->dlen) {
            // we have exhausted the buffer.
            psi->offset = 0;
            psi->dlen = 0;

            netbuf_free(psi->pbuf);
            psi->pbuf = NULL;

            if(*bytes_read) {
                printf("doing short read of %d", *bytes_read);
                return E_SUCCESS;
            }
        }
    }

    printf("success!\n");
    return E_SUCCESS;
}

int wifi_api_read_data(uint8_t socket, uint8_t *data,
                       uint16_t bytes_to_read,  uint16_t *bytes_read) {
    if(socket > MAX_SOCKETS)
        return E_INVALID_PARAMETER;

    switch(wifi.sockets[socket].type) {
    case SOCK_TYPE_TCP:
        return wifi_api_read_data_tcp(socket, data, bytes_to_read, bytes_read);
    default:
        printf("unhandled send data for socket type %d", wifi.sockets[socket].type);
        return E_INTERNAL;
    }

    return E_SUCCESS;
}
