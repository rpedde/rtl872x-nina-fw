#ifndef __WIFI_API_H__
#define __WIFI_API_H__

/* wifi status */
#define WL_IDLE_STATUS     0
#define WL_NO_SSID_AVAIL   1
#define WL_SCAN_COMPLETED  2
#define WL_CONNECTED       3
#define WL_CONNECT_FAILED  4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED    6
#define WL_AP_LISTENING    7
#define WL_AP_CONNECTD     8
#define WL_AP_FAILED       9


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
int wifi_api_get_socket(uint8_t *socket);
int wifi_api_stop_socket(uint8_t socket);

#endif /* __WIFI_API_H__ */
