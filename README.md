AVR-C people counter
====================

This repository contains a C program for an AVR microcontroller that 
implements a people counter using an external circuit. A schematic of 
this circuit has been included in this repository. Two sense wires are
connected to the circuit and are placed such that a person walking by
them will be close to one wire and then the other, or vice versa if
they are walking in the opposite direction. I have only tested this in
a hallway with both sense wires mounted to the wall. I do not know if
the sensors would work if they were embedded in the floor or in a
different location.

Additionally, the number of people estimated to be in the room is
displayed on a single digit 7-segment display.

The microcontroller I used was a Arduino Mega, so the program has been
adjusted to fit the pins available on the Arduino. It will be
necessary to adapt the code to work on smaller AVR microcontrollers
such as an ATMega328 (Arduino Uno).

## Hardware setup
The schematic of the circuit is shown in both gschem format (`.sch`)
and as a PNG image. The circuit may be assembled on a PCB or
prototyping board, but I have only tried it with a breadboard. The
ground of the oscillator circuit and Arduino should be connected to 
earth ground for greater reliability. It may be possible to isolate
the microcontroller circuit from the oscillator circuit, but I have
not tried this.

As stated above, this setup works best in narrow hallways. The sense
wires should be close together so that there is at most one person
between the sense wires at any time.

## Software setup
The program `people_counter.c` must be uploaded onto the Arduino Mega
or a different AVR microcontroller. Modifications are needed to make
the program work on non-Mega microcontrollers.

## Operation
Each sense wire acts as one plate of a capacitor. When no one is near
the sense wire, there will be a very small amount of capacitance
between the sense wire and earth. If a person is near the sense wire, 
this capacitance increases, and the frequency of the corresponding
oscillator decreases. Each oscillator is connected to one of the
microcontroller's timer's clock inputs, specifically the clock inputs
for timers 0 and 5. These timers are used to measure the frequency
of each oscillator. When the frequency goes below a certain
threshold, the program determines that someone is near that
sensor. If a person walks past the sensors, they would be near one
first, and then near the other, so if the person triggers both sensors
within two seconds, then the Arduino assumes the person has entered or
left the room and act accordingly. In this case, a 7-segment display
is updated with the appropriate value.

