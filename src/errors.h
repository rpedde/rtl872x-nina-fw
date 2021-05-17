#ifndef __ERRORS_H__
#define __ERROR_H__

#define E_SSID_TOO_LONG      0x01 // max len 32
#define E_PASSWORD_TOO_LONG  0x02 // max len 64
#define E_NOT_CONNECTED      0x03 // trying to do an operation that requires connected
#define E_SCAN_ERROR         0x04 // some internal error?
#define E_SSID_NOT_FOUND     0x05 // scan didn't find the requested ap to join
#define E_CONNECT_ERROR      0x06 // maybe bad password?
#define E_DHCP_ERROR         0x07 // couldn't get a dhcp ip
#define E_WIFI_INIT_ERROR    0x08 // couldn't initialize a network if
#define E_WIFI_STARTUP_ERROR 0x09 // couldn't start the if
#define E_WIFI_TIMEOUT       0x0a // timeout waiting for wlan startup



#endif /* __ERRORS_H__ */
