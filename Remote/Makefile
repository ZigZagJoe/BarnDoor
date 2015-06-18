### AVR executable Makefile stub for Posix

PROGRAMMER = -cusbtiny
#PROGRAMMER = -cstk500v1 -P/dev/tty.usbmodem34521

F_CPU 	= 1000000L
MCU    = attiny85

LIBS 	= 
INC	= 

# clockdiv/8, 8mhz RC oscillator, 4ms startup
LFUSE	= 0x52
HFUSE	= 0xDF
EFUSE 	= 0xFF

#######################################################
# Variables:
# F_CPU = CPU speed (such as 8000000L)
# MCU	= MCU name (atmega324pa)

# LIBS 	= any additional libraries. -lname Optional.
# INC	= -I /additional/path Optional.

# LFUSE/HFUSE/EFUSE - for fuses target. Optional

#######################################################
ifndef BIN
	# Get directory name, to use as output filename.
  	BIN = $(notdir $(CURDIR))
endif

ifndef OPTIMIZE
       OPTIMIZE = s
endif

# Program names

RM   	= rm -f
CPP  	= avr-g++
AR   	= avr-ar
OBJCOPY = avr-objcopy

# Compiler and linker flags

LSTFLAGS += -S -fverbose-asm 
CXXFLAGS += -c -g -O$(OPTIMIZE) -Wall -fno-exceptions -ffunction-sections -fdata-sections -mmcu=$(MCU) -DF_CPU=$(F_CPU) -MMD $(INC)
ARFLAGS += -ar rcs $(BIN)
ADFLAGS += -v -v -p$(MCU) $(PROGRAMMER) -e -D 
LST_COND = sed -i "" -e '/\.stab[s,n,d].*/d' -e '/\.LB[B,E][[:digit:]]\{1,4\}.*/d' -e '/\.LM[[:digit:]]\{1,4\}.*/d'

# List of source files
SRCS=$(wildcard *.cpp)
OBJS=$(SRCS:.cpp=.o)
LSTS=$(SRCS:.cpp=.s)

# need to clear default rules
.SUFFIXES:
.SUFFIXES: .cpp .o .s

DATE=$(shell date "+%m-%d-%y_%H-%M-%S")

####### TARGETS ########

all:	$(BIN)

listing: $(LSTS)
	
upload: all
	avrdude $(ADFLAGS) -Uflash:w:$(BIN).hex:i

# fuses case: check that fuse variables are set, first...
fuses:

ifeq ($(strip $(LFUSE)),)
	@echo ERROR: LFUSE UNDEFINED/NULL

else ifeq ($(strip $(HFUSE)),)
	@echo ERROR: HFUSE UNDEFINED/NULL

else ifeq ($(strip $(EFUSE)),)
	@echo ERROR: EFUSE UNDEFINED/NULL

else
	avrdude $(ADFLAGS) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m 

endif

# end fuses case

zip:	
	zip -vr ../$(BIN)-$(DATE).zip .

clean:
	$(RM) $(LSTS) $(OBJS) *.d ./$(BIN).hex ./$(BIN).elf ./$(BIN).eep

# Open source in default editor, on mac.
open:
	-open $(SRCS) 

$(BIN): clean $(OBJS)    
	$(CPP) -Os -Wl,--gc-sections -mmcu=$(MCU) -o $(BIN).elf $(OBJS) $(LIBPATH) $(LIBS)
#	$(OBJCOPY) -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 $(BIN).elf $(BIN).eep 
	$(OBJCOPY) -O ihex -R .eeprom $(BIN).elf $(BIN).hex 
	@echo
	@avr-size --format sysv $(BIN).hex | grep Total | sed "s/$$/ bytes/" | sed "s/Total/binary size:/"
	
.cpp.o:
	$(CPP) $(CXXFLAGS) -c -o $@ $<

.cpp.s:
	$(CPP) $(CXXFLAGS) $(LSTFLAGS) -c -o $@ $< 
	$(LST_COND) $@
