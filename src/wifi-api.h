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


extern int wifi_api_setup(void);
extern void wifi_set_last_error(uint8_t);
extern int wifi_api_conn_status(uint8_t *);

extern void wifi_api_mac_addr(uint8_t *);
extern void wifi_api_get_ip(uint8_t *);
extern void wifi_api_get_netmask(uint8_t *);
extern void wifi_api_get_gw(uint8_t *);

extern int wifi_connect(char *ssid, char *passphrase);

#endif /* __WIFI_API_H__ */
