#ifndef __HANDLERS_H__
#define __HANDLERS_H__

#include "spi-dispatch.h"

#define UNUSED(x) ((void)(x))

extern int h_set_net(const uint8_t *, uint8_t *); // 0x10
extern int h_set_passphrase(const uint8_t *command, uint8_t *response); // 0x11
extern int h_get_conn_status(const uint8_t *, uint8_t *); // 0x20
extern int h_get_ip_addr(const uint8_t *, uint8_t *); // 0x21
extern int h_get_mac_addr(const uint8_t *, uint8_t *); // 0x22
extern int h_req_host_by_name(const uint8_t *, uint8_t *); //0x34
extern int h_get_host_by_name(const uint8_t *, uint8_t *); // 0x35
extern int h_get_firmware_version(const uint8_t *, uint8_t *); // 0x37


#endif /*__HANDLERS_H__ */
