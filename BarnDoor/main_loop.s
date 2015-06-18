/* C code
    register uint16_t delay = baseval;
    register uint8_t counter = 0;
    uint16_t table_pos = 0;
    uint8_t skip = first_update;
 
    while(true) {
        bset(PORTB, STEP);
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
                reset_sleep();
            
            table_pos++;
        } else
            __builtin_avr_delay_cycles(13);
        // this and a uS off the base delay at 20mhz make the pulse length just right @ 1/4 stepping
    }
    
    AVR ASM quickie
    
    instruction      cycle  desc
    ----------------------------
    add reg, reg     1      adds register to register
    adc reg, reg     1      adds register to register with carry
    adiw reg, 6imm   2      adds 6 bit immediate value to register pair reg, reg+1
    brcs label       1/2    branch if carry flag set
    breq label       1/2    branch if zero flag set
    brsh label       1/2    branches if unsigned greater or equal
    brne label       1/2    branch if zero flag not set. 2 cycles if branch taken
    cbi  ioreg, bit  2      clear bit in io register
    cpi reg, imm     1      compares register to immediate (sets flags)
    cpc reg, imm     1      compares register to immediate with carry
    dec reg          1      decrements register (sets flags)
    ldi  reg, imm    1      load immediate byte into a register r16-r31
    lpm reg, Z       3      load byte from prgm flash indexed by Z(r30:r31) into reg
    lsl reg          1      shifts reg to the left by 1, b7 into C flag
    movw regp, regp  2      move word from register pair to register pair
    nop              1      no operation
    rcall label      3      call subroutine relative to PC
    rjmp label       2      short relative jump
    rol reg          1      rotate left with carry. C->b0, b7->C
    sbi  ioreg, bit  2      set bit in io register
    sbci reg, imm    1      subtract immediate with carry
    sbiw reg, 5imm   2      subtract 6 bit immediate value from register pair reg, reg+1
    sbrs reg, bit    1/2/3  skip if bit in register set - 3 cyc if skipped instr is 2 words
    subi reg, imm    1      subtract immediate from register
*/

    ; skip
	ldi r30,lo8(2)

    ; table pos = 0
	ldi r24,0
	ldi r25,0

	ldi r22,0 ; skip

    ; baseval -> delay
	ldi r18,lo8(34)
	ldi r19,lo8(21)

.L27: ; loop start

    ; 1->step
	sbi 0x18,0

    ; delay ~100uS
	ldi r26,lo8(499)
	ldi r27,hi8(499)
	1: sbiw r26,1
	brne 1b
	rjmp .
	nop

    ; 0->step
	cbi 0x18,0

    ; delayMicroseconds(delay);
	movw r26,r18

	sbiw	r26, 1
	brcs	L_240_end
	breq	L_240_end
	lsl	r26
	rol	r27
	lsl	r26
	rol	r27
	L_240_loop:sbiw	r26, 1
	nop
	brne	L_240_loop
	L_240_end: nop

    ; if (counter++ == 0)
	subi r22,lo8(-(1))
	brne .L21               ; 2 if jmp, 1 if not

    ; if skip > 1
	cpi r30,lo8(2)          ; 1
	brlo .L22               ; 2 / 1
    ; skip --
	subi r30,lo8(-(-1))     ; 1

	;  __builtin_avr_delay_cycles(13 - 3);
	ldi r27,lo8(3)          ; 1
	1: dec r27              ; 1      (3 total)
	brne 1b                 ; 2 / 1  (5 total)
    ; only 9 cycles... should insert a nop

	rjmp .L27               ; continue;
.L22:
    ; bounds check
    ; if (table_pos < sizeof(offset_table)) { ... } else reset_sleep
	cpi r24,30              ; 1
	ldi r27,2               ; 1
	cpc r25,r27             ; 1
	brsh .L24               ; 1

    ; load table pos into indexing register and add table offset
	movw r30,r24            ; 1
	subi r30,lo8(-(_ZL12offset_table)) ; 1
	sbci r31,hi8(-(_ZL12offset_table)) ; 1

    ; skip = pgm_read_byte(...)
	lpm r30, Z              ; 3

 ; @13

    ; if bit 0 in r30 is not set, jmp to L26
	sbrs r30,0              ; 1 / 2
	rjmp .L26               ; 2

    ; load 1 into register to add to delay
	ldi r20,lo8(1)          ; 1
	ldi r21,0               ; 1
	rjmp .L25               ; 2
.L26:
    ; load -1 into register to subtract from delay
	ldi r20,lo8(-1)         ; 1
	ldi r21,lo8(-1)         ; 1
.L25:
    ; add value to delay
	add r18,r20             ; 1
	adc r19,r21             ; 1
    ; shift bit off the end of $skip
	lsr r30                 ; 1

	adiw r24,1              ; 2
    ; table_pos++

    ; continue;
	rjmp .L27
.L24:
	rcall _Z11reset_sleepv
.L21:
    ;  __builtin_avr_delay_cycles(13);
	ldi r20,lo8(4)
	1: dec r20
	brne 1b
	nop

    ; continue;
	rjmp .L27


