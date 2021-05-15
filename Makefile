SOURCES = rtl872x-nina-fw.ino $(wildcard src/*.c) $(wildcard src/*.h)
BOARD = realtek:AmebaD:ameba_rtl8721d
CFLAGS = -Isrc

SERIAL := /dev/ttyACM0

firmware: $(SOURCES)
	arduino-cli compile -b $(BOARD) --build-property build.extra_flags=$(CFLAGS) --clean

flash:
	arduino-cli upload -b $(BOARD) -p $(SERIAL)

.PHONY: firmware

.PHONY: flash
