#ifndef __ERRORS_H__
#define __ERROR_H__

#define E_SUCCESS            0x00
#define E_INTERNAL           0x01
#define E_INVALID_PARAMETER  0x02 // out of range, too long, etc.
#define E_SSID_TOO_LONG      0x03 // max len 32
#define E_PASSWORD_TOO_LONG  0x04 // max len 64
#define E_NOT_CONNECTED      0x05 // trying to do an operation that requires connected
#define E_SCAN_ERROR         0x06 // some internal error?
#define E_SSID_NOT_FOUND     0x07 // scan didn't find the requested ap to join
#define E_CONNECT_ERROR      0x08 // maybe bad password?
#define E_DHCP_ERROR         0x09 // couldn't get a dhcp ip
#define E_WIFI_INIT_ERROR    0x0a // couldn't initialize a network if
#define E_WIFI_STARTUP_ERROR 0x0b // couldn't start the if
#define E_WIFI_TIMEOUT       0x0c // timeout waiting for wlan startup
#define E_HOSTNAME_TOO_LONG  0x0d // gethostbyname hostname too long
#define E_HOST_NOT_FOUND     0x0e // gethostbyname lookup failed
#define E_SOCKETS_EXHAUSTED  0x0f // tried to allocate more sockets than available
#define E_NOMEM              0x10 // out of memory

#endif /* __ERRORS_H__ */
