#include <avr/io.h>
#include <util/delay.h>

#define bset(X,BIT) (X |= _BV(BIT))
#define bclr(X,BIT) (X &= ~_BV(BIT))
#define bisset(X,BIT) (X & _BV(BIT))

// This will act as an intervalometer and/or a simple remote.
// It should work with anything that is compatible with the Canon RC-* series remote

// Pressing the momentary tactile switch causes just the initial shot pulse to be
// sent as the user presumably releases the switch, removing power. If the other 
// non-momentary switch is turned on, then ~60-300 seconds later a pulse is sent 
// to terminate the exposure, and then 2 seconds later the program repeats itself.

/* Circuit
             ___ ___
 RESET (B5) -|  o  |- VCC
 SW2    B3  -|atiny|- B2  SW4
        B4  -|45/85|- B1  SW3
        GND -|_____|- B0  LED_OUT
 
 B4 could be left unconnected.
 SW_COMMON is connected to ground
 */

// Two leds in parallel, their anode is connected to pin 8 of the uC (power),
// and their cathode is connected to the collector input of the NPN transistor.
// The base of the transistor is connected to the uC pin 5 (port b0) via a 4.7k
// resistor, and the emitter is connected to ground.
// A momentary tactile switch is connected to the batteries, and the power pin
// of the uC. In parallel with the tactile switch is a normal SPST switch.
// A green LED is connected to the uC's power pin through a resistor, so
// that when the uC is powered on, the LED lights up.

// This circuit can be simplified to a microswitch connected to power, a LED
// connected to B0 and ground (no resistor), and a button cell for power.
// Remove all the switch stuff below, of course.

void send_signal() {
    // 32.75khz is the target frequency
    // must pulse at minimum twice
    // too many pulses and the camera will
    // interpert them as seperate signals
    // 9 pulses is too many, 8 safe, 7 more safe
    for (uint8_t repeat = 0; repeat <= 7; repeat++) {
        for (uint8_t i = 0; i < 16; i++ ) {
#if F_CPU == 1000000
            bset(PORTB, 0);                 //  2 cycles     sbi
            __builtin_avr_delay_cycles(12); // 12 cycles
            bclr(PORTB, 0);                 //  2 cycles     cbi
            __builtin_avr_delay_cycles(12); // 12 cycles
            // addi/subi                    //  1 cycle      i++
            // brne                         //  2 cycles     loop check
            // total = 31 cycles
            // 1 clock, 1 microsecond at 1mhz
            // 1 second / 31 microseconds = 32.25 khz
#else
#warning untested send_signal configuration
            bset(PORTB,0);
            _delay_us(15);
            bclr(PORTB,0);
            _delay_us(16);
            // this is probably accurate enough
            // no guarantees, though!
#endif
        }
        _delay_us(7300); // instant shot
        // 5300 for 2sec delay before shot
    }
}

int main (void) {
    // enable pull up resistors
    bset(PORTB, 1);
    bset(PORTB, 2);
    bset(PORTB, 3);

    // burn some time to let inputs settle
    __builtin_avr_delay_cycles(20);
    
    // this is a really inefficent way to do it, but i had the I/Os
    // and a free 4 position switch, so why not. simpler than binary input.
    
    // to read, all pull ups are enabled, and each pin of the switch
    // is connected to a seperate IO. the IO that is low is switch pos
    
    uint16_t delay = 60; // assume position 1 by default
    // could use uint8_t and save some code space
    // divide each delay by 4, then then delay for 4000ms in the loop

    if (!bisset(PINB,3)) // position 2
        delay = 120;
    
    if (!bisset(PINB,1)) // position 3
        delay = 180;
    
    if (!bisset(PINB,2)) // position 4
        delay = 300;
    
    PORTB = 0; // disable pull ups
    
    // enable output
    bset(DDRB, 0);
    
    while(true) {
        send_signal(); // begin bulb exposure
        
        // this is wasteful of power, but i tried using the wdt
        // to let me sleep 1s in power down state, and it turns
        // out that the wdt oscilator is way off. better to busy
        // wait and be about correct than way out in left field
        for(uint16_t i = 0; i <= delay; i++)
            _delay_ms(1000);
        
        send_signal(); // terminate bulb exposure
        _delay_ms(10000); // wait for camera to recover its wits
    }
}

