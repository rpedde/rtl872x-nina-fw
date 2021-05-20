#ifndef __WIFI_API_H__
#define __WIFI_API_H__

#define UNUSED(x) ((void)(x))

/* wifi status */
#define WL_IDLE_STATUS     0x00
#define WL_NO_SSID_AVAIL   0x01
#define WL_SCAN_COMPLETED  0x02
#define WL_CONNECTED       0x03
#define WL_CONNECT_FAILED  0x04
#define WL_CONNECTION_LOST 0x05
#define WL_DISCONNECTED    0x06
#define WL_AP_LISTENING    0x07
#define WL_AP_CONNECTD     0x08
#define WL_AP_FAILED       0x09

#define SOCK_MODE_TCP         0x00
#define SOCK_MODE_UDP         0x01
#define SOCK_MODE_TLS         0x02
#define SOCK_MODE_IDLE        0xfe
#define SOCK_MODE_UNALLOCATED 0xff

/* we only return a subset of these */
#define SOCK_STATE_CLOSED      0x00
#define SOCK_STATE_LISTEN      0x01
#define SOCK_STATE_SYN_SENT    0x02
#define SOCK_STATE_SYN_RECVD   0x03
#define SOCK_STATE_ESTABLISHED 0x04
#define SOCK_STATE_FIN_WAIT_1  0x05
#define SOCK_STATE_FIN_WAIT_2  0x06
#define SOCK_STATE_CLOSE_WAIT  0x07
#define SOCK_STATE_CLOSING     0x08
#define SOCK_STATE_LAST_ACK    0x09
#define SOCK_TIME_WAIT         0x0a

/* utils */
extern int wifi_api_setup(void);
extern void wifi_set_last_error(uint8_t);

/* general methods */
extern int wifi_api_conn_status(uint8_t *);

extern void wifi_api_mac_addr(uint8_t *);
extern void wifi_api_get_ip(uint8_t *);
extern void wifi_api_get_netmask(uint8_t *);
extern void wifi_api_get_gw(uint8_t *);

extern int wifi_api_get_host_by_name(char *, uint32_t *);

/* wifi methods */
extern int wifi_api_connect(char *ssid, char *passphrase);

/* socket methods */
extern int wifi_api_get_socket(uint8_t *socket);
extern int wifi_api_stop_socket(uint8_t socket);
extern int wifi_api_socket_connect(char *hostname,
                                   uint32_t ipaddr,
                                   uint16_t port,
                                   uint8_t socket,
                                   uint8_t mode);
int wifi_api_get_socket_state(uint8_t socket, uint8_t *state);
int wifi_api_avail_data(uint8_t socket, uint16_t *avail);
int wifi_api_send_data(uint8_t socket,
                       uint8_t *data,
                       uint16_t bytes_to_write,
                       uint16_t *bytes_written);
int wifi_api_write_flush(uint8_t socket);
#endif /* __WIFI_API_H__ */
