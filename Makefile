SOURCES = rtl872x-nina-fw.ino $(wildcard src/*.c) $(wildcard src/*.h)
BOARD = realtek:AmebaD:ameba_rtl8721d
CFLAGS = "-Isrc -DLWIP_SO_RCVBUF=1 -DCONFIG_HIGH_TP_TEST=1"
SERIAL := /dev/ttyACM0

firmware: $(SOURCES)
	arduino-cli compile -b $(BOARD) --build-property build.extra_flags=$(CFLAGS) --clean

flash:
	arduino-cli upload -b $(BOARD) -p $(SERIAL)

.PHONY: firmware

.PHONY: flash
