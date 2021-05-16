import serial
import time

import adafruit_requests as requests
import adafruit_esp32spi.adafruit_esp32spi_socket as socket
from adafruit_esp32spi import adafruit_esp32spi

# from secrets import secrets

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

if rtl.status == adafruit_esp32spi.WL_IDLE_STATUS:
    print('rtl found an in idle mode')
    print('firmware version: ', rtl.firmware_version)
    # print('mac addr: ', ':'.join(hex(i) for i in rtl.MAC_address))
