from unittest import mock
from adafruit_esp32spi import adafruit_esp32spi


class ToggleyPin:
    def __init__(self):
        self._value = False

    @property
    def value(self):
        current = self._value
        self._value = not current
        return current

    @value.setter
    def value(self, new_value):
        self._value = new_value


class FakeSerialSPI:
    def __init__(self, serial, debug=False):
        self._serial = serial
        self._debug = debug

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        pass

    def write(self, buffer, start, end):
        assert start == 0
        if end is None:
            end = len(buffer)

        # we assume this is the whole data packet
        if self._debug >= 3:
            hexarray = map(hex, buffer[start:end])
            print('--> {}'.format(':'.join(hexarray)))

        header = bytearray([ord('@'), (end >> 8) & 0xff, end & 0xff])
        self._serial.write(header)
        self._serial.write(buffer[start:end])

        # now read the response
        byte = self._serial.read(1)
        while(byte != b'@'):
            if self._debug:
                print(byte.decode(), end='')
            byte = self._serial.read(1)

        if self._debug:
            print('-- resp --')

        resp = (ord(self._serial.read(1)) << 8) | ord(self._serial.read(1))

        if self._debug >= 3:
            print('reading a response of size {}'.format(resp))

        self._resp_packet = self._serial.read(resp)
        self._resp_idx = 0

        if self._debug >= 3:
            hexarray = map(hex, self._resp_packet)
            print('--> {}'.format(':'.join(hexarray)))

    def readinto(self, buf, start=0, end=None):
        assert self._resp_packet

        if end is None:
            end = len(buf)

        remaining = len(self._resp_packet) - self._resp_idx
        to_copy = min(remaining, end - start)

        for idx in range(to_copy):
            buf[start + idx] = self._resp_packet[self._resp_idx + idx]

        self._resp_idx += to_copy
        if self._resp_idx == len(self._resp_packet):
            self._resp_packet = None
            self._resp_idx = 0


class SerialRTL(adafruit_esp32spi.ESP_SPIcontrol):
    def __init__(self, serial, debug=False):
        # spi, cs, ready, reset
        super().__init__(None,         # spi
                         mock.Mock(),  # cs_pin
                         ToggleyPin(), # ready_pin
                         mock.Mock(),  # reset_pin
                         debug=debug)

        # and replace the spi device
        self._spi_device = FakeSerialSPI(serial, debug=self._debug)
