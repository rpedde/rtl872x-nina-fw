# rtl872x-nina-fw #

This is probaby an ill-fated attempt to make a minimal workalike
of the arduino esp32 nina firmware for the rtl8720dn.

My proximate goal is to get reasonable networking on circuitpython
running on the wio terminal.

Stuff I'm mostly interested in having work:

- [x] wifi in STA mode
- [ ] wifi in AP mode (do I really need this?)
- [ ] wifi in AP/STA mode (again, do I need this?)
- [x] http connections
- [ ] https connections
- [ ] udp
- [ ] tcp server
- [ ] ssl tcp server
- [ ] udp server

So only http works right now, and that with some memory leaks.  Still,
it's a start.

## Building ##

You can follow along if you like.  I'm using 1.18.3 of the arduino
thingie, and 0.18.1 of arduino-cli.  Get something substationally
close to that.  Basically download them both, then drop an empty
config for arduino-cli with `arduino-cli config init`.

Realtek has an arduino sdk available at their [developer site]
(https://www.amebaiot.com.cn/en/amebad-arduino-getting-started/).

Follow that guide, but for arduino-cli... someting like this:

```
arduino-cli config add board_manager.additional_urls https://github.com/ambiot/ambd_arduino/raw/master/Arduino_package/package_realtek.com_amebad_index.json
arduino-cli core update-index
arduino-cli core install realtek:AmebaD
```

Then you can build with `make firmare`, which runs:

```
arduino-cli compile -b realtek:AmebaD:ameba_rtl8721d --build-property build.extra_flags="-Isrc"
```

I feel a little guilty about this makefile, as due to arduino shenanians
it's mostly just a script file and not a real dependency based build
manager.  Note that due to arduino not tracking cache well enough, you
have to do clean builds every time, which kind of defeats the purpose of
dependency based builds.  Thanks Arduino.  :(

Anyway, once firmware is built, get a serial bootload app running on
the wio terminal, like the `rtl8720-update-v2.uf2` firmware desribed
[here](https://create.arduino.cc/projecthub/Salmanfarisvp/the-new-wio-terminal-erpc-firmware-bfd8bd).

I believe that is just [this
sketch](https://github.com/Seeed-Studio/Seeed_Arduino_Sketchbook/tree/master/examples/WioTerminal_USB2Serial_Burn8720),
but compiled for samd51 (the wio itself, not the rtl8720)

Anyway, get the firmware burning helper installed and then `make
flash`, which is:

```
arduino-cli upload -b realtek:AmebaD:ameba_rtl8721d -p /dev/ttyACM0
```

might need to override SERIAL if you have a different port or
something, but this should net you a wio with networking that doesn't
work, but hey.. I already said it didn't work.  That's on you.

## testing ##

It's kind of a pain to have to flash firmare burning stuff, then burn
circuitpython, then test and keep round-tripping like that.  Instead,
I've made a dumb class inherited from the adafruit esp32spi module
that talks serial rather than spi.  That way, I can use a serial
bridge from wio->rtl8720 (the firmware burning software above can also
do usb->serial in addition to firmware burns -- just push the middle
button).

So my workflow for messing with this is write some c code, `make firmware`,
reset the wio, `make flash`, reset the wio and hit the center button,
then run testapp/test-esp.py and poke around at the wifi module.

There are some adafruit python thing you'd need installed to run this on
pc, not sure what they are, I'll perhaps get them all worked into a
requirements file at some point, but you can figure them out by seeing
what import exceptions you get.  lol.

This is kind of hacky and one-offy, obviously, but if you seriously
want to mess with this, that's one way you could do it.

## license ##

This doesn't work.  Honestly, what good is a license?

But seriously, it makes sense to license this as LGPL 2.1+ the same as
the nina firmware is.  I'll header it al up at some point.  With the
exception of the bundled adafruit code in testapp that is clearly
marked with the adafruit licenses, which... oh yeah:

some stuff here is copyright by various parties for adafruit industries

I hope that suffices for MIT license notice.
