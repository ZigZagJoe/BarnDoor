#include <stdlib.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

/*
 
 BARN DOOR TRACKER, ISOLECES TYPE, AVR CODE
 2014 ZZJ
 
 
 *** AS USED IN THIS APPLICATION ***
        ___ ___
 RESET -|  o  |- VCC
 OSC1  -|atiny|- RESET
 OSC2  -|45/85|- DIRECTION
 GND   -|_____|- STEP
 
 RESET: Connected to SLEEP, RESET lines of A4988 stepper driver
 Connected to limit switch via 1.5k resistor (switch connected to ground)
 Pulled up via 10k resistor.
 
 DIRECTION: Connected to DIRECTION line of A4988 stepper driver
 STEP: Connected to STEP line of A4988 stepper driver
 
 OSC1: Connected to 20mhz crystal and ground via 18pf ceramic capacitor
 OSC2: Connected to 20mhz crystal and ground via 18pf ceramic capacitor
 
 RESET: Connected to momentary SPST switch (switch connected to ground)
 
 Fuse bits: 0xFF, 0xDE, 0xFF (RESET enabled, external crystal, long power up)

 ##### USER INTERFACE ####
 
 Control is simple: the reset pin (1).
 
 On power applied, state var will not be REWIND_NOT_REQUIRED_MAGIC, so a rewind will be executed.
 
 When rewind is completed, state will be set to REWIND_NOT_REQUIRED_MAGIC.
 
   If the uC is reset without losing power after a rewind, the value in state will
 be not have been cleared, and it will proceed to run normally after clearing state.
 A reset while running would cause it to begin rewinding.
 
   In short, when power is hooked up, the tracker will rewind until it hits the limit
 switch, it will advance off the limit switch, and then go to sleep. At some point 
 the user hits the reset button, and it will run forwards at the sidereal rate. A 
 reset while running or power loss would cause another rewind, and so on.
 
   You really want a crystal for this application, as the RC oscillator is A. wildly
 inaccurate and B. highly temperature sensitive. The value doesn't matter, just as 
 long as you have a calibrated delay loop. It's somewhat less important if you are
 using longer step delays; I need to step over 10k times a second, so inaccuracy adds up.
 */

#define bv(BIT) (1 << BIT)
#define bset(X,BIT) (X |= bv(BIT))
#define bclr(X,BIT) (X &= ~bv(BIT))
#define bisset(X,BIT) (X & bv(BIT))

#define delay _delay_ms

typedef uint8_t byte;

////////////////////////////////////////////

/*
             ___ ___
 RESET	B5  -|  o  |- VCC
 ADC3	B3  -|atiny|- B2   SCK   ADC1
 ADC2	B4  -|45/85|- B1   MISO
        GND -|_____|- B0   MOSI
 
*/

// lazy arduino-like reads, writes, pinmode
// pins are indexed by port bit number, see pinout above
// forced inline as that's more efficient but GCC isn't bright enough to do it under -Os constraint

#define OUTPUT 1
#define INPUT 0

__attribute__((always_inline)) inline bool digitalRead(byte pin) {
    return bisset(PINB, pin);
}

__attribute__((always_inline)) inline void digitalWrite(const byte pin, const byte val) {
    val ? bset(PORTB, pin) : bclr(PORTB, pin);
}

__attribute__((always_inline)) inline void pinMode(const byte pin, const byte mode) {
    mode ? bset(DDRB, pin) : bclr(DDRB, pin);
}

static inline void delayMicroseconds(uint16_t) __attribute__((always_inline, unused));
static inline void delayMicroseconds(uint16_t usec)
{
#if F_CPU != 20000000L
    #error F_CPU != 20mhz. Use a different delayMicroseconds such as http://www.pjrc.com/teensy/beta/delayMicroseconds.h
#endif
    asm volatile(
                 "sbiw	%A0, 1"			"\n\t"	// 2
                 "brcs	L_%=_end"		"\n\t"	// 1
                 "breq	L_%=_end"		"\n\t"	// 1
                 "lsl	%A0"			"\n\t"	// 1
                 "rol	%B0"			"\n\t"	// 1
                 "lsl	%A0"			"\n\t"	// 1
                 "rol	%B0"			"\n\t"	// 1
      
                 "L_%=_loop:"
                 "sbiw	%A0, 1"			"\n\t"	// 2
                 "nop"                  "\n\t"  // 1
                 "brne	L_%=_loop"		"\n\t"	// 2
                 // 5 cycles per loop
                 "L_%=_end: nop\n"
                 : "+w" (usec)
                 : "0" (usec)
                 );
}

/////////////////////////////////////////////////////////////////////////////////

// port pin #s
#define RESET 2
#define STEP 0
#define DIRECTION  1

#define FORWARDS 1
#define BACKWARDS 0

const uint32_t REWIND_NOT_REQUIRED_MAGIC = 0xDEADBEEF;

// will not be initialized. used to let uC know if reset was caused by user after successful rewind
__attribute__ ((section (".noinit"))) volatile uint32_t state;

// rewinds mount
void rewind();

// for faster, non-time-crucial stepping
inline void doStep(const uint16_t rate);

// low power sleep mode, waiting for user reset
void reset_sleep();

/////////////////////////////////////////////////////////////////////////////////

// byte format:
// bit: 7 8 6 5 4 3 2 1 0
//      [  interval   ] C
// interval: nr of intervals until next update
// C: 1 add one to delay, 0 subtract one from delay

// 1 will be subtracted from the initial value to compensate
// makes timing perfect with the cycle delays below
#define DELAY_TWEAK 1
#include "table.h"

const uint16_t quickStepRate = 700; // fast stepping, for rewinding
// 100 @ 1/16
// 300 @ 1/8
// 700 @ 1/4
//1500 @ 1/2
//3200 @ 1
// 7.1 RPM
// any faster and it's possible that it can get stuck unable to step
// YMMV

int main(void) {
    _delay_ms(10); // debounce

	pinMode(STEP, OUTPUT);
    pinMode(DIRECTION, OUTPUT);
    pinMode(RESET, INPUT);
    
    digitalWrite(DIRECTION, FORWARDS);
    
    if (state != REWIND_NOT_REQUIRED_MAGIC)
        rewind(); // rewind the mount; never returns
    
    state = 0; // mark as rewind required
 
    uint16_t delay = baseval;   // number of uS to delay
    uint8_t counter = 0;        // step counter
    uint16_t table_pos = 0;     // table index
    uint8_t skip = first_update;// number of intervals to skip
    
    while(true) {
        bset(PORTB, STEP);  // output a step pulse
        _delay_us(100);
        bclr(PORTB, STEP);
        delayMicroseconds(delay);

        if (!(++counter)) {
            if (skip > 1) { // it's not time to read new tweak value yet
                skip--;
                __builtin_avr_delay_cycles(13 - 3); // above code uses 3 cycles
                continue;
            }
            
            if (table_pos < sizeof(offset_table)) {
                skip = pgm_read_byte((uint16_t)offset_table + table_pos);
                delay += (skip & 1) ? 1 : -1;
                skip = skip >> 1;
                // the above code uses more than 13 cycles, so don't delay at all
            } else
                reset_sleep(); // no more corrections, enter low-power sleep mode
            
            table_pos++;
        } else
            __builtin_avr_delay_cycles(13);
        // 13 cycles makes timekeeping perfect after a uS is taken off base delay
        // THIS WILL REQUIRE ADJUSTMENT FOR OTHER CLOCK SPEEDS/CRYSTALS!
        
    }
    
	return 0;
}

void rewind() {
    
    digitalWrite(DIRECTION, BACKWARDS);
    
    pinMode(RESET, INPUT);
    digitalWrite(RESET, 0); // pullup off
    
    // run backwards at max speed until reset goes low via limit switch
    while(digitalRead(RESET))
        doStep(quickStepRate);
    
    // now advance forward enough to cause reset to be unasserted
    digitalWrite(DIRECTION, FORWARDS);
    
    do {
        // override the limit switch and step forwards
        pinMode(RESET, OUTPUT);
        digitalWrite(RESET, 1);
        _delay_ms(10); // wait for stepper driver to reactivate
        
        for (int i = 0; i < 8; i++) // smaller the number, more consistant the return position but too small and it may be unable to step
            doStep(quickStepRate * 1.3); // slower RPM as more torque is required going up (stepper RPM is inversely porportional to torque)
        
        pinMode(RESET, INPUT);
        digitalWrite(RESET, 0);
        _delay_ms(1); // allow input to settle
    } while(!digitalRead(RESET));
    
    // advance a little more so that we can't rest on the limit switch
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 1);
    
    for (int i = 0; i < 750; i++)
        doStep(quickStepRate * 1.3);
    
    /* okay, reset is done. enter a power saving state, waiting for user input (uC reset) */
    state = REWIND_NOT_REQUIRED_MAGIC;
    
    reset_sleep();
}


// very fast stepping for rewinding
inline void doStep(const uint16_t rate) {
    bset(PORTB, STEP);
    _delay_us(100);
    bclr(PORTB, STEP);
    delayMicroseconds(rate);
}

// shuts avr down until reset
void reset_sleep() {
    pinMode(RESET, OUTPUT); // keep the driver in reset while we sleep
    digitalWrite(RESET, 0); // also stops the stepper from heating up
    
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    asm("cli");
    sleep_cpu();
    
    // no coming back from this sleep!
    while(1);
}

