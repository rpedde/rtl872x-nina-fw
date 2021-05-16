#ifndef __HANDLERS_H__
#define __HANDLERS_H__

#include "spi-dispatch.h"

#define UNUSED(x) ((void)(x))

extern int get_conn_status(const uint8_t *, uint8_t *);
extern int get_mac_addr(const uint8_t *, uint8_t *);
extern int get_firmware_version(const uint8_t *, uint8_t *);

#endif /*__HANDLERS_H__ */
