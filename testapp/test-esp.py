import serial
import time

import adafruit_requests as requests
import adafruit_esp32spi.adafruit_esp32spi_socket as socket
from adafruit_esp32spi import adafruit_esp32spi

from secrets import secrets

# rtl_cs = digitalio.DigitalInOut(board.RTL_CS)
# rtl_ready = digitalio.DigitalInOut(board.RTL_READY)
# rtl_reset = digitalio.DigitalInOut(board.RTL_PWR)

# rtl_spi = busio.SPI(board.RTL_CLK, MOSI=board.RTL_MOSI, MISO=board.RTL_MISO)
# rtl = adafruit_esp32spi.ESP_SPIcontrol(rtl_spi, rtl_cs, rtl_ready, rtl_reset)

import serialrtl

uart = serial.Serial('/dev/ttyACM0', 115200)
time.sleep(1)

while uart.in_waiting:
    print(uart.read().decode(), end='')

print()

rtl = serialrtl.SerialRTL(uart, debug=3)

requests.set_socket(socket, rtl)

print('Firmware version: {}'.format(rtl.firmware_version.decode()))
print('mac addr: ', ':'.join(hex(i) for i in rtl.MAC_address))

if rtl.status == adafruit_esp32spi.WL_IDLE_STATUS:
    rtl.connect(secrets)

print('connected')

for k, v in rtl.network_data.items():
    print(f"{k:10}: {'.'.join(str(x) for x in v)}")

print('.'.join(str(x) for x in rtl.get_host_by_name('www.google.com')))

rv = requests.get('http://hafnium:8088')
print('Got %d', rv.status_code)

# s = socket.socket()
# s.connect(('172.31.1.53', 8088), rtl.TCP_MODE)
# s.send(b'PUT /notify HTTP/1.0\r\ncontent-type: application/json\r\ncontent-length: 35\r\n\r\n{"summary": "test", "body": "test"}')
# time.sleep(2)
# s.close()
