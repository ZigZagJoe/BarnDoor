This is the remote/intervalometer code.

It should work with anything that compatible with the Canon RC-* series remote

Pressing the momentary tactile switch causes just the initial shot pulse to be sent as the user presumably releases the switch.
If the SPST switch is turned on, then ~60 seconds later a pulse is sent to terminate the exposure, and then 2 seconds later the program loops and sends a new shot signal. Repeats forever.

>> Circuit <<
             ___ ___
 n/c	B5  -|  o  |- VCC
 SW2	B3  -|atiny|- B2  SW4
 SW1	B4  -|45/85|- B1  SW3
        GND -|_____|- B0  LED_OUT
 
 B4 could be left unconnected.
 SW_COMMON is connected to ground

Two leds in parallel, their anode is connected to pin 8 of the uC (power),
and their cathode is connected to the collector input of the NPN transistor.

The base of the transistor is connected to the uC pin 5 (port b0) via a 4.7k
resistor, and the emitter is connected to ground.

A momentary tactile switch is connected to the batteries, and the power pin
of the uC. In parallel with the tactile switch is a normal SPST switch.

A green LED is connected to the uC's power pin through a resistor, so
that when the uC is powered on, the LED lights up.

This circuit can be simplified to a microswitch connected to power, a LED
connected to B0 and ground (no resistor), and two button cells for power.
Remove all the switch stuff in the code, of course.
