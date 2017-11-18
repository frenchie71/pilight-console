# pilight-console
an LCD display interface to pilight using JSON api and arduino, lcd display and matrix keypad

pilight-console consists of the following parts:

a raspberry pi, orange pi or similar that runs the pilight-console daemon
an arduino, preferrably arduino nano
an LCD display, I used 4x20 with i2c but any other should do.
a matrix keypad (4x4)
the software for the arduino

the idea is to have the display and the keyboard hooked up to the arduino, while the arduino takes commands over the serial port in order to transmit pilight device states over the lcd display and maybe toggle switches or display temperature etc. or disarm an alarm with a pincode etc.

In order to build this you need

1. On the raspi side

C-Compiler (gcc) and libjansson-dev (for JSON handling)
the source code pilight-console.c

in order to compile just type 

 gcc -o pilight-console pilight-console.c  -ljansson
 
 2. on the arduino side
 
 the arduino ide and possibly avrdude (if you want to program from the raspi)
 Liquidcrystal-I2C library
 Keypad Library (Google is your friend)
  
 just load the .ino sketch into the arduino ide and program the arduino nano. If you want to program from the pi, you may also save the sketch as compiled hex file, transfer it to your pi and run 
 
 avrdude -v -b 57600 -p atmega328p -c arduino -P /dev/ttyUSB0 -D -Uflash:w:Display_Keyboard.ino.hex
 
 Hope you like it, if you want to see examples please check out my posts at curlymo's pilight forum at http://forum.pilight.org
 
