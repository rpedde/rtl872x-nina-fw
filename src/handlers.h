#ifndef __HANDLERS_H__
#define __HANDLERS_H__

#include "spi-dispatch.h"

/* get_conn_status */
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

#define UNUSED(x) ((void)(x))

extern int get_conn_status(const uint8_t *, uint8_t *);
extern int get_mac_addr(const uint8_t *, uint8_t *);
extern int get_firmware_version(const uint8_t *, uint8_t *);

#endif /*__HANDLERS_H__ */
