/**************************************************************************
 *     This file is part of the Bally/Stern OS for Arduino Project.

    I, Dick Hamill, the author of this program disclaim all copyright
    in order to make this program freely available in perpetuity to
    anyone who would like to use it. Dick Hamill, 6/1/2020

    BallySternOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BallySternOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    See <https://www.gnu.org/licenses/>.
 */


 
#include <Arduino.h>
#include <EEPROM.h>
//#define DEBUG_MESSAGES    1
#define BALLY_STERN_CPP_FILE
#include "BSOS_Config.h"
#include "BallySternOS.h"

#ifndef BALLY_STERN_OS_HARDWARE_REV
#define BALLY_STERN_OS_HARDWARE_REV 1
#endif

// To use this library, take the example_BSOS_Config.h, 
// edit it for your hardware and game parameters and put
// it in your game's code folder as BSOS_Config.h
// (so when you fetch new versions of the library, you won't 
// overwrite your config)
#include "BSOS_Config.h"

#if !defined(BSOS_SWITCH_DELAY_IN_MICROSECONDS) || !defined(BSOS_TIMING_LOOP_PADDING_IN_MICROSECONDS)
#error "Must define BSOS_SWITCH_DELAY_IN_MICROSECONDS and BSOS_TIMING_LOOP_PADDING_IN_MICROSECONDS in BSOS_Config.h"
#endif

#ifdef BSOS_USE_EXTENDED_SWITCHES_ON_PB4
#define NUM_SWITCH_BYTES                6
#define NUM_SWITCH_BYTES_ON_U10_PORT_A  5
#define MAX_NUM_SWITCHES                48
#define DEFAULT_SOLENOID_STATE          0x8F
#define ST5_CONTINUOUS_SOLENOID_BIT     0x10
#elif defined(BSOS_USE_EXTENDED_SWITCHES_ON_PB7)
#define NUM_SWITCH_BYTES                6
#define NUM_SWITCH_BYTES_ON_U10_PORT_A  5
#define MAX_NUM_SWITCHES                48
#define DEFAULT_SOLENOID_STATE          0x1F
#define ST5_CONTINUOUS_SOLENOID_BIT     0x80
#else 
#define NUM_SWITCH_BYTES                5
#define NUM_SWITCH_BYTES_ON_U10_PORT_A  5
#define MAX_NUM_SWITCHES                40
#define DEFAULT_SOLENOID_STATE          0x9F
#endif

// Global variables
volatile byte DisplayDigits[5][BALLY_STERN_OS_NUM_DIGITS];
volatile byte DisplayDigitEnable[5];
#ifdef BALLY_STERN_OS_DIMMABLE_DISPLAYS
volatile boolean DisplayDim[5];
#endif
volatile boolean DisplayOffCycle = false;
volatile byte CurrentDisplayDigit=0;
volatile byte LampStates[BSOS_NUM_LAMP_BITS], LampDim0[BSOS_NUM_LAMP_BITS], LampDim1[BSOS_NUM_LAMP_BITS];
volatile byte LampFlashPeriod[BSOS_MAX_LAMPS];
byte DimDivisor1 = 2;
byte DimDivisor2 = 3;

volatile byte SwitchesMinus2[NUM_SWITCH_BYTES];
volatile byte SwitchesMinus1[NUM_SWITCH_BYTES];
volatile byte SwitchesNow[NUM_SWITCH_BYTES];
#ifdef BALLY_STERN_OS_USE_DIP_SWITCHES
byte DipSwitches[4];
#endif


#define SOLENOID_STACK_SIZE 60
#define SOLENOID_STACK_EMPTY 0xFF
volatile byte SolenoidStackFirst;
volatile byte SolenoidStackLast;
volatile byte SolenoidStack[SOLENOID_STACK_SIZE];
boolean SolenoidStackEnabled = true;
volatile byte CurrentSolenoidByte = 0xFF;
volatile byte RevertSolenoidBit = 0x00;
volatile byte NumCyclesBeforeRevertingSolenoidByte = 0;

#define TIMED_SOLENOID_STACK_SIZE 30
struct TimedSolenoidEntry {
  byte inUse;
  unsigned long pushTime;
  byte solenoidNumber;
  byte numPushes;
  byte disableOverride;
};
TimedSolenoidEntry TimedSolenoidStack[TIMED_SOLENOID_STACK_SIZE];

#define SWITCH_STACK_SIZE   60
#define SWITCH_STACK_EMPTY  0xFF
volatile byte SwitchStackFirst;
volatile byte SwitchStackLast;
volatile byte SwitchStack[SWITCH_STACK_SIZE];

#if (BALLY_STERN_OS_HARDWARE_REV==1)
#define ADDRESS_U10_A           0x14
#define ADDRESS_U10_A_CONTROL   0x15
#define ADDRESS_U10_B           0x16
#define ADDRESS_U10_B_CONTROL   0x17
#define ADDRESS_U11_A           0x18
#define ADDRESS_U11_A_CONTROL   0x19
#define ADDRESS_U11_B           0x1A
#define ADDRESS_U11_B_CONTROL   0x1B
#define ADDRESS_SB100           0x10

#elif (BALLY_STERN_OS_HARDWARE_REV==2)
#define ADDRESS_U10_A           0x00
#define ADDRESS_U10_A_CONTROL   0x01
#define ADDRESS_U10_B           0x02
#define ADDRESS_U10_B_CONTROL   0x03
#define ADDRESS_U11_A           0x08
#define ADDRESS_U11_A_CONTROL   0x09
#define ADDRESS_U11_B           0x0A
#define ADDRESS_U11_B_CONTROL   0x0B
#define ADDRESS_SB100           0x10
#define ADDRESS_SB100_CHIMES    0x18
#define ADDRESS_SB300_SQUARE_WAVES  0x10
#define ADDRESS_SB300_ANALOG        0x18

#elif (BALLY_STERN_OS_HARDWARE_REV==3) || (BALLY_STERN_OS_HARDWARE_REV==4)

#define ADDRESS_U10_A           0x88
#define ADDRESS_U10_A_CONTROL   0x89
#define ADDRESS_U10_B           0x8A
#define ADDRESS_U10_B_CONTROL   0x8B
#define ADDRESS_U11_A           0x90
#define ADDRESS_U11_A_CONTROL   0x91
#define ADDRESS_U11_B           0x92
#define ADDRESS_U11_B_CONTROL   0x93
#define ADDRESS_SB100           0xA0
#define ADDRESS_SB100_CHIMES    0xC0
#define ADDRESS_SB300_SQUARE_WAVES  0xA0
#define ADDRESS_SB300_ANALOG        0xC0

#endif 




#if (BALLY_STERN_OS_HARDWARE_REV==1) or (BALLY_STERN_OS_HARDWARE_REV==2)

#if defined(__AVR_ATmega2560__)
#error "ATMega requires BALLY_STERN_OS_HARDWARE_REV of 3, check BSOS_Config.h and adjust settings"
#endif

void BSOS_DataWrite(int address, byte data) {
  
  // Set data pins to output
  // Make pins 5-7 output (and pin 3 for R/W)
  DDRD = DDRD | 0xE8;
  // Make pins 8-12 output
  DDRB = DDRB | 0x1F;

  // Set R/W to LOW
  PORTD = (PORTD & 0xF7);

  // Put data on pins
  // Put lower three bits on 5-7
  PORTD = (PORTD&0x1F) | ((data&0x07)<<5);
  // Put upper five bits on 8-12
  PORTB = (PORTB&0xE0) | (data>>3);

  // Set up address lines
  PORTC = (PORTC & 0xE0) | address;

  // Wait for a falling edge of the clock
  while((PIND & 0x10));

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTC = PORTC | 0x20;
  
  // Wait while clock is low
  while(!(PIND & 0x10));
  
  // Wait while clock is high
  while((PIND & 0x10));

  // Wait while clock is low
  while(!(PIND & 0x10));

  // Set VMA OFF
  PORTC = PORTC & 0xDF;

  // Unset address lines
  PORTC = PORTC & 0xE0;
  
  // Set R/W back to HIGH
  PORTD = (PORTD | 0x08);

  // Set data pins to input
  // Make pins 5-7 input
  DDRD = DDRD & 0x1F;
  // Make pins 8-12 input
  DDRB = DDRB & 0xE0;
}



byte BSOS_DataRead(int address) {
  
  // Set data pins to input
  // Make pins 5-7 input
  DDRD = DDRD & 0x1F;
  // Make pins 8-12 input
  DDRB = DDRB & 0xE0;

  // Set R/W to HIGH
  DDRD = DDRD | 0x08;
  PORTD = (PORTD | 0x08);

  // Set up address lines
  PORTC = (PORTC & 0xE0) | address;

  // Wait for a falling edge of the clock
  while((PIND & 0x10));

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTC = PORTC | 0x20;

  // Wait a full clock cycle to make sure data lines are ready
  // (important for faster clocks)
  // Wait while clock is low
  while(!(PIND & 0x10));

  // Wait for a falling edge of the clock
  while((PIND & 0x10));
  
  // Wait while clock is low
  while(!(PIND & 0x10));

  byte inputData = (PIND>>5) | (PINB<<3);

  // Set VMA OFF
  PORTC = PORTC & 0xDF;

  // Wait for a falling edge of the clock
// Doesn't seem to help  while((PIND & 0x10));

  // Set R/W to LOW
  PORTD = (PORTD & 0xF7);

  // Clear address lines
  PORTC = (PORTC & 0xE0);

  return inputData;
}


void WaitClockCycle(int numCycles=1) {
  for (int count=0; count<numCycles; count++) {
    // Wait while clock is low
    while(!(PIND & 0x10));
  
    // Wait for a falling edge of the clock
    while((PIND & 0x10));
  }
}

#elif (BALLY_STERN_OS_HARDWARE_REV==3)

// Rev 3 connections
// Pin D2 = IRQ
// Pin D3 = CLOCK
// Pin D4 = VMA
// Pin D5 = R/W
// Pin D6-12 = D0-D6
// Pin D13 = SWITCH
// Pin D14 = HALT
// Pin D15 = D7
// Pin D16-30 = A0-A14

#if defined(__AVR_ATmega328P__)
#error "BALLY_STERN_OS_HARDWARE_REV 3 requires ATMega2560, check BSOS_Config.h and adjust settings"
#endif


void BSOS_DataWrite(int address, byte data) {
  
  // Set data pins to output
  DDRH = DDRH | 0x78;
  DDRB = DDRB | 0x70;
  DDRJ = DDRJ | 0x01;

  // Set R/W to LOW
  PORTE = (PORTE & 0xF7);

  // Put data on pins
  // Lower Nibble goes on PortH3 through H6
  PORTH = (PORTH&0x87) | ((data&0x0F)<<3);
  // Bits 4-6 go on PortB4 through B6
  PORTB = (PORTB&0x8F) | ((data&0x70));
  // Bit 7 goes on PortJ0
  PORTJ = (PORTJ&0xFE) | (data>>7);  

  // Set up address lines
  PORTH = (PORTH & 0xFC) | ((address & 0x0001)<<1) | ((address & 0x0002)>>1); // A0-A1
  PORTD = (PORTD & 0xF0) | ((address & 0x0004)<<1) | ((address & 0x0008)>>1) | ((address & 0x0010)>>3) | ((address & 0x0020)>>5); // A2-A5
  PORTA = ((address & 0x3FC0)>>6); // A6-A13
  PORTC = (PORTC & 0x3F) | ((address & 0x4000)>>7) | ((address & 0x8000)>>9); // A14-A15

  // Wait for a falling edge of the clock
  while((PINE & 0x20));

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x20;

  // Wait while clock is low
  while(!(PINE & 0x20));

  // Wait while clock is high
  while((PINE & 0x20));

  // Wait while clock is low
  while(!(PINE & 0x20));

  // Set VMA OFF
  PORTG = PORTG & 0xDF;

  // Unset address lines
  PORTH = (PORTH & 0xFC);
  PORTD = (PORTD & 0xF0);
  PORTA = 0;
  PORTC = (PORTC & 0x3F);
  
  // Set R/W back to HIGH
  PORTE = (PORTE | 0x08);

  // Set data pins to input
  DDRH = DDRH & 0x87;
  DDRB = DDRB & 0x8F;
  DDRJ = DDRJ & 0xFE;
  
}



byte BSOS_DataRead(int address) {
  
  // Set data pins to input
  DDRH = DDRH & 0x87;
  DDRB = DDRB & 0x8F;
  DDRJ = DDRJ & 0xFE;

  // Set R/W to HIGH
  DDRE = DDRE | 0x08;
  PORTE = (PORTE | 0x08);

  // Set up address lines
  PORTH = (PORTH & 0xFC) | ((address & 0x0001)<<1) | ((address & 0x0002)>>1); // A0-A1
  PORTD = (PORTD & 0xF0) | ((address & 0x0004)<<1) | ((address & 0x0008)>>1) | ((address & 0x0010)>>3) | ((address & 0x0020)>>5); // A2-A5
  PORTA = ((address & 0x3FC0)>>6); // A6-A13
  PORTC = (PORTC & 0x3F) | ((address & 0x4000)>>7) | ((address & 0x8000)>>9); // A14-A15

  // Wait for a falling edge of the clock
  while((PINE & 0x20));

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x20;

  // Wait a full clock cycle to make sure data lines are ready
  // (important for faster clocks)
  // Wait while clock is low
  while(!(PINE & 0x20));

  // Wait for a falling edge of the clock
  while((PINE & 0x20));
  
  // Wait while clock is low
  while(!(PINE & 0x20));

  byte inputData;
  inputData = (PINH & 0x78)>>3;
  inputData |= (PINB & 0x70);
  inputData |= PINJ << 7;

  // Set VMA OFF
  PORTG = PORTG & 0xDF;

  // Set R/W to LOW
  PORTE = (PORTE & 0xF7);

  // Unset address lines
  PORTH = (PORTH & 0xFC);
  PORTD = (PORTD & 0xF0);
  PORTA = 0;
  PORTC = (PORTC & 0x3F);

  return inputData;
}


void WaitClockCycle(int numCycles=1) {
  for (int count=0; count<numCycles; count++) {
    // Wait while clock is low
    while(!(PINE & 0x20));
  
    // Wait for a falling edge of the clock
    while((PINE & 0x20));
  }
}

#elif (BALLY_STERN_OS_HARDWARE_REV==4)

// Rev 3 connections
// Pin D2 = IRQ
// Pin D3 = CLOCK
// Pin D4 = VMA
// Pin D5 = R/W
// Pin D6-12 = D0-D6
// Pin D13 = SWITCH
// Pin D14 = HALT
// Pin D15 = D7
// Pin D16-30 = A0-A14

#if defined(__AVR_ATmega328P__)
#error "BALLY_STERN_OS_HARDWARE_REV 4 requires ATMega2560, check BSOS_Config.h and adjust settings"
#endif

#define RPU_PROCESSOR_6800

#define RPU_VMA_PIN           40
#define RPU_RW_PIN            3
#define RPU_PHI2_PIN          39
#define RPU_SWITCH_PIN        38
#define RPU_BUFFER_DISABLE    5
#define RPU_HALT_PIN          41
#define RPU_RESET_PIN         42
#define RPU_DIAGNOSTIC_PIN    44
#define BSOS_PINS_OUTPUT true
#define BSOS_PINS_INPUT false

void BSOS_SetAddressPinsDirection(boolean pinsOutput) {  
  for (int count=0; count<16; count++) {
    pinMode(A0+count, pinsOutput?OUTPUT:INPUT);
  }
}

void BSOS_SetDataPinsDirection(boolean pinsOutput) {
  for (int count=0; count<8; count++) {
    pinMode(22, pinsOutput?OUTPUT:INPUT);
  }
}


// REVISION 4 HARDWARE
void BSOS_DataWrite(int address, byte data) {
  
  // Set data pins to output
  DDRA = 0xFF;

  // Set R/W to LOW
  PORTE = (PORTE & 0xDF);

  // Put data on pins
  PORTA = data;

  // Set up address lines
  PORTF = (byte)(address & 0x00FF);
  PORTK = (byte)(address/256);

#ifdef RPU_PROCESSOR_6800
  // Wait for a falling edge of the clock
  while((PING & 0x04));
#else
  // Set clock low (PG2) (if 6802/8)
  PORTG &= ~0x04;
  //delayMicroseconds(3);
#endif  

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x02;

#ifdef RPU_PROCESSOR_6800
  // Wait while clock is low
  while(!(PING & 0x04));

  // Wait while clock is high
  while((PING & 0x04));

  // Wait while clock is low
  while(!(PING & 0x04));  
#else
  // Set clock high
  PORTG |= 0x04;
  //delayMicroseconds(3);

  // Set clock low
  PORTG &= ~0x04;
  //delayMicroseconds(3);

  // Set clock high
  PORTG |= 0x04;
#endif

  // Set VMA OFF
  PORTG = PORTG & 0xFD;

  // Unset address lines
  PORTF = 0x00;
  PORTK = 0x00;
  
  // Set R/W back to HIGH
  PORTE = (PORTE | 0x20);

  // Set data pins to input
  DDRA = 0x00;
  
}



byte BSOS_DataRead(int address) {
  
  // Set data pins to input
  DDRA = 0x00;

  // Set R/W to HIGH
  DDRE = DDRE | 0x20;
  PORTE = (PORTE | 0x20);

  // Set up address lines
  PORTF = (byte)(address & 0x00FF);
  PORTK = (byte)(address/256);

#ifdef RPU_PROCESSOR_6800
  // Wait for a falling edge of the clock
  while((PING & 0x04));
#else
  // Set clock low
  PORTG &= ~0x04;
  //delayMicroseconds(3);
#endif

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x02;

#ifdef RPU_PROCESSOR_6800
  // Wait a full clock cycle to make sure data lines are ready
  // (important for faster clocks)
  // Wait while clock is low
  while(!(PING & 0x04));

  // Wait for a falling edge of the clock
  while((PING & 0x04));
  
  // Wait while clock is low
  while(!(PING & 0x04));
#else
  // Set clock high
  PORTG |= 0x04;
  //delayMicroseconds(3);

  // Set clock low
  PORTG &= ~0x04;
  //delayMicroseconds(3);
  
  // Set clock high
  PORTG |= 0x04;
#endif 

  byte inputData;
  inputData = PINA;

  // Set VMA OFF
  PORTG = PORTG & 0xFD;

  // Set R/W to LOW
  PORTE = (PORTE & 0xDF);

  // Unset address lines
  PORTF = 0x00;
  PORTK = 0x00;

  return inputData;
}

#endif


void TestLightOn() {
  BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) | 0x08);
}

void TestLightOff() {
  BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) & 0xF7);
}



void InitializeU10PIA() {
  // CA1 - Self Test Switch
  // CB1 - zero crossing detector
  // CA2 - NOR'd with display latch strobe
  // CB2 - lamp strobe 1
  // PA0-7 - output for switch bank, lamps, and BCD
  // PB0-7 - switch returns

  BSOS_DataWrite(ADDRESS_U10_A_CONTROL, 0x38);
  // Set up U10A as output
  BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
  // Set bit 3 to write data
  BSOS_DataWrite(ADDRESS_U10_A_CONTROL, BSOS_DataRead(ADDRESS_U10_A_CONTROL)|0x04);
  // Store F0 in U10A Output
  BSOS_DataWrite(ADDRESS_U10_A, 0xF0);
  
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x33);
  // Set up U10B as input
  BSOS_DataWrite(ADDRESS_U10_B, 0x00);
  // Set bit 3 so future reads will read data
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL)|0x04);

}

#ifdef BALLY_STERN_OS_USE_DIP_SWITCHES
void ReadDipSwitches() {
  byte backupU10A = BSOS_DataRead(ADDRESS_U10_A);
  byte backupU10BControl = BSOS_DataRead(ADDRESS_U10_B_CONTROL);

  // Turn on Switch strobe 5 & Read Switches
  BSOS_DataWrite(ADDRESS_U10_A, 0x20);
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, backupU10BControl & 0xF7);
  // Wait for switch capacitors to charge
  delayMicroseconds(BSOS_SWITCH_DELAY_IN_MICROSECONDS);
  DipSwitches[0] = BSOS_DataRead(ADDRESS_U10_B);
 
  // Turn on Switch strobe 6 & Read Switches
  BSOS_DataWrite(ADDRESS_U10_A, 0x40);
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, backupU10BControl & 0xF7);
  // Wait for switch capacitors to charge
  delayMicroseconds(BSOS_SWITCH_DELAY_IN_MICROSECONDS);
  DipSwitches[1] = BSOS_DataRead(ADDRESS_U10_B);

  // Turn on Switch strobe 7 & Read Switches
  BSOS_DataWrite(ADDRESS_U10_A, 0x80);
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, backupU10BControl & 0xF7);
  // Wait for switch capacitors to charge
  delayMicroseconds(BSOS_SWITCH_DELAY_IN_MICROSECONDS);
  DipSwitches[2] = BSOS_DataRead(ADDRESS_U10_B);

  // Turn on U10 CB2 (strobe 8) and read switches
  BSOS_DataWrite(ADDRESS_U10_A, 0x00);
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, backupU10BControl | 0x08);
  // Wait for switch capacitors to charge
  delayMicroseconds(BSOS_SWITCH_DELAY_IN_MICROSECONDS);
  DipSwitches[3] = BSOS_DataRead(ADDRESS_U10_B);

  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, backupU10BControl);
  BSOS_DataWrite(ADDRESS_U10_A, backupU10A);
}
#endif

void InitializeU11PIA() {
  // CA1 - Display interrupt generator
  // CB1 - test connector pin 32
  // CA2 - lamp strobe 2
  // CB2 - solenoid bank select
  // PA0-7 - display digit enable
  // PB0-7 - solenoid data

  BSOS_DataWrite(ADDRESS_U11_A_CONTROL, 0x31);
  // Set up U11A as output
  BSOS_DataWrite(ADDRESS_U11_A, 0xFF);
  // Set bit 3 to write data
  BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL)|0x04);
  // Store 00 in U11A Output
  BSOS_DataWrite(ADDRESS_U11_A, 0x00);
  
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x30);
  // Set up U11B as output
  BSOS_DataWrite(ADDRESS_U11_B, 0xFF);
  // Set bit 3 so future reads will read data
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, BSOS_DataRead(ADDRESS_U11_B_CONTROL)|0x04);
  // Store 9F in U11B Output
  BSOS_DataWrite(ADDRESS_U11_B, DEFAULT_SOLENOID_STATE);
  CurrentSolenoidByte = DEFAULT_SOLENOID_STATE;
  
}


int SpaceLeftOnSwitchStack() {
  if (SwitchStackFirst>=SWITCH_STACK_SIZE || SwitchStackLast>=SWITCH_STACK_SIZE) return 0;
  if (SwitchStackLast>=SwitchStackFirst) return ((SWITCH_STACK_SIZE-1) - (SwitchStackLast-SwitchStackFirst));
  return (SwitchStackFirst - SwitchStackLast) - 1;
}

void PushToSwitchStack(byte switchNumber) {
  //if ((switchNumber>=MAX_NUM_SWITCHES && switchNumber!=SW_SELF_TEST_SWITCH)) return;
  if (switchNumber==SWITCH_STACK_EMPTY) return;

  // If the switch stack last index is out of range, then it's an error - return
  if (SpaceLeftOnSwitchStack()==0) return;

  // Self test is a special case - there's no good way to debounce it
  // so if it's already first on the stack, ignore it
  if (switchNumber==SW_SELF_TEST_SWITCH) {
    if (SwitchStackLast!=SwitchStackFirst && SwitchStack[SwitchStackFirst]==SW_SELF_TEST_SWITCH) return;
  }

  SwitchStack[SwitchStackLast] = switchNumber;
  
  SwitchStackLast += 1;
  if (SwitchStackLast==SWITCH_STACK_SIZE) {
    // If the end index is off the end, then wrap
    SwitchStackLast = 0;
  }
}

void BSOS_PushToSwitchStack(byte switchNumber) {
  PushToSwitchStack(switchNumber);
}


byte BSOS_PullFirstFromSwitchStack() {
  // If first and last are equal, there's nothing on the stack
  if (SwitchStackFirst==SwitchStackLast) return SWITCH_STACK_EMPTY;

  byte retVal = SwitchStack[SwitchStackFirst];

  SwitchStackFirst += 1;
  if (SwitchStackFirst>=SWITCH_STACK_SIZE) SwitchStackFirst = 0;

  return retVal;
}


boolean BSOS_ReadSingleSwitchState(byte switchNum) {
  if (switchNum>=MAX_NUM_SWITCHES) return false;

  int switchByte = switchNum/8;
  int switchBit = switchNum%8;
  if ( ((SwitchesNow[switchByte])>>switchBit) & 0x01 ) return true;
  else return false;
}


int SpaceLeftOnSolenoidStack() {
  if (SolenoidStackFirst>=SOLENOID_STACK_SIZE || SolenoidStackLast>=SOLENOID_STACK_SIZE) return 0;
  if (SolenoidStackLast>=SolenoidStackFirst) return ((SOLENOID_STACK_SIZE-1) - (SolenoidStackLast-SolenoidStackFirst));
  return (SolenoidStackFirst - SolenoidStackLast) - 1;
}


void BSOS_PushToSolenoidStack(byte solenoidNumber, byte numPushes, boolean disableOverride) {
  if (solenoidNumber>14) return;

  // if the solenoid stack is disabled and this isn't an override push, then return
  if (!disableOverride && !SolenoidStackEnabled) return;

  // If the solenoid stack last index is out of range, then it's an error - return
  if (SpaceLeftOnSolenoidStack()==0) return;

  for (int count=0; count<numPushes; count++) {
    SolenoidStack[SolenoidStackLast] = solenoidNumber;
    
    SolenoidStackLast += 1;
    if (SolenoidStackLast==SOLENOID_STACK_SIZE) {
      // If the end index is off the end, then wrap
      SolenoidStackLast = 0;
    }
    // If the stack is now full, return
    if (SpaceLeftOnSolenoidStack()==0) return;
  }
}

void PushToFrontOfSolenoidStack(byte solenoidNumber, byte numPushes) {
  // If the stack is full, return
  if (SpaceLeftOnSolenoidStack()==0  || !SolenoidStackEnabled) return;

  for (int count=0; count<numPushes; count++) {
    if (SolenoidStackFirst==0) SolenoidStackFirst = SOLENOID_STACK_SIZE-1;
    else SolenoidStackFirst -= 1;
    SolenoidStack[SolenoidStackFirst] = solenoidNumber;
    if (SpaceLeftOnSolenoidStack()==0) return;
  }
  
}

byte PullFirstFromSolenoidStack() {
  // If first and last are equal, there's nothing on the stack
  if (SolenoidStackFirst==SolenoidStackLast) return SOLENOID_STACK_EMPTY;
  
  byte retVal = SolenoidStack[SolenoidStackFirst];

  SolenoidStackFirst += 1;
  if (SolenoidStackFirst>=SOLENOID_STACK_SIZE) SolenoidStackFirst = 0;

  return retVal;
}




boolean BSOS_PushToTimedSolenoidStack(byte solenoidNumber, byte numPushes, unsigned long whenToFire, boolean disableOverride) {
  for (int count=0; count<TIMED_SOLENOID_STACK_SIZE; count++) {
    if (!TimedSolenoidStack[count].inUse) {
      TimedSolenoidStack[count].inUse = true;
      TimedSolenoidStack[count].pushTime = whenToFire;
      TimedSolenoidStack[count].disableOverride = disableOverride;
      TimedSolenoidStack[count].solenoidNumber = solenoidNumber;
      TimedSolenoidStack[count].numPushes = numPushes;
      return true;
    }
  }
  return false;
}


void BSOS_UpdateTimedSolenoidStack(unsigned long curTime) {
  for (int count=0; count<TIMED_SOLENOID_STACK_SIZE; count++) {
    if (TimedSolenoidStack[count].inUse && TimedSolenoidStack[count].pushTime<curTime) {
      BSOS_PushToSolenoidStack(TimedSolenoidStack[count].solenoidNumber, TimedSolenoidStack[count].numPushes, TimedSolenoidStack[count].disableOverride);
      TimedSolenoidStack[count].inUse = false;
    }
  }
}



volatile int numberOfU10Interrupts = 0;
volatile int numberOfU11Interrupts = 0;
volatile byte InsideZeroCrossingInterrupt = 0;


#if defined (BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT)

ISR(TIMER1_COMPA_vect) {    //This is the interrupt request
  // Backup U10A
  byte backupU10A = BSOS_DataRead(ADDRESS_U10_A);
  
  // Disable lamp decoders & strobe latch
  BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) | 0x08);
  BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) & 0xF7);
#ifdef BALLY_STERN_OS_USE_AUX_LAMPS
  // Also park the aux lamp board 
  BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) | 0x08);
  BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) & 0xF7);    
#endif

  // Blank Displays
  BSOS_DataWrite(ADDRESS_U10_A_CONTROL, BSOS_DataRead(ADDRESS_U10_A_CONTROL) & 0xF7);
  // Set all 5 display latch strobes high
  BSOS_DataWrite(ADDRESS_U11_A, (BSOS_DataRead(ADDRESS_U11_A) & 0x03) | 0x01);
  BSOS_DataWrite(ADDRESS_U10_A, 0x0F);

  // Write current display digits to 5 displays
  for (int displayCount=0; displayCount<5; displayCount++) {

    if (CurrentDisplayDigit<BALLY_STERN_OS_NUM_DIGITS) {
      // The BCD for this digit is in b4-b7, and the display latch strobes are in b0-b3 (and U11A:b0)
      byte displayDataByte = ((DisplayDigits[displayCount][CurrentDisplayDigit])<<4) | 0x0F;
      byte displayEnable = ((DisplayDigitEnable[displayCount])>>CurrentDisplayDigit)&0x01;

      // if this digit shouldn't be displayed, then set data lines to 0xFX so digit will be blank
      if (!displayEnable) displayDataByte = 0xFF;
#ifdef BALLY_STERN_OS_DIMMABLE_DISPLAYS        
      if (DisplayDim[displayCount] && DisplayOffCycle) displayDataByte = 0xFF;
#endif        

      // Set low the appropriate latch strobe bit
      if (displayCount<4) {
        displayDataByte &= ~(0x01<<displayCount);
      }
      // Write out the digit & strobe (if it's 0-3)
      BSOS_DataWrite(ADDRESS_U10_A, displayDataByte);
      if (displayCount==4) {            
        // Strobe #5 latch on U11A:b0
        BSOS_DataWrite(ADDRESS_U11_A, BSOS_DataRead(ADDRESS_U11_A) & 0xFE);
      }

      // Need to delay a little to make sure the strobe is low for long enough
      //WaitClockCycle(4);
      delayMicroseconds(8);

      // Put the latch strobe bits back high
      if (displayCount<4) {
        displayDataByte |= 0x0F;
        BSOS_DataWrite(ADDRESS_U10_A, displayDataByte);
      } else {
        BSOS_DataWrite(ADDRESS_U11_A, BSOS_DataRead(ADDRESS_U11_A) | 0x01);
        
        // Set proper display digit enable
#ifdef BALLY_STERN_OS_USE_7_DIGIT_DISPLAYS          
        byte displayDigitsMask = (0x02<<CurrentDisplayDigit) | 0x01;
#else
        byte displayDigitsMask = (0x04<<CurrentDisplayDigit) | 0x01;
#endif          
        BSOS_DataWrite(ADDRESS_U11_A, displayDigitsMask);
      }
    }
  }

  // Stop Blanking (current digits are all latched and ready)
  BSOS_DataWrite(ADDRESS_U10_A_CONTROL, BSOS_DataRead(ADDRESS_U10_A_CONTROL) | 0x08);

  // Restore 10A from backup
  BSOS_DataWrite(ADDRESS_U10_A, backupU10A);    

  CurrentDisplayDigit = CurrentDisplayDigit + 1;
  if (CurrentDisplayDigit>=BALLY_STERN_OS_NUM_DIGITS) {
    CurrentDisplayDigit = 0;
    DisplayOffCycle ^= true;
  }
}

void InterruptService3() {
  byte u10AControl = BSOS_DataRead(ADDRESS_U10_A_CONTROL);
  if (u10AControl & 0x80) {
    // self test switch
    if (BSOS_DataRead(ADDRESS_U10_A_CONTROL) & 0x80) PushToSwitchStack(SW_SELF_TEST_SWITCH);
    BSOS_DataRead(ADDRESS_U10_A);
  }

  // If we get a weird interupt from U11B, clear it
  byte u11BControl = BSOS_DataRead(ADDRESS_U11_B_CONTROL);
  if (u11BControl & 0x80) {
    BSOS_DataRead(ADDRESS_U11_B);    
  }

  byte u11AControl = BSOS_DataRead(ADDRESS_U11_A_CONTROL);
  byte u10BControl = BSOS_DataRead(ADDRESS_U10_B_CONTROL);

  // If the interrupt bit on the display interrupt is on, do the display refresh
  if (u11AControl & 0x80) {
    BSOS_DataRead(ADDRESS_U11_A);
    numberOfU11Interrupts+=1;
  }

  // If the IRQ bit of U10BControl is set, do the Zero-crossing interrupt handler
  if ((u10BControl & 0x80) && (InsideZeroCrossingInterrupt==0)) {
    InsideZeroCrossingInterrupt = InsideZeroCrossingInterrupt + 1;

    byte u10BControlLatest = BSOS_DataRead(ADDRESS_U10_B_CONTROL);

    // Backup contents of U10A
    byte backup10A = BSOS_DataRead(ADDRESS_U10_A);

    // Latch 0xFF separately without interrupt clear
    BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) | 0x08);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) & 0xF7);
    // Read U10B to clear interrupt
    BSOS_DataRead(ADDRESS_U10_B);

    // Turn off U10BControl interrupts
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x30);

    // Copy old switch values
    byte switchCount;
    byte startingClosures;
    byte validClosures;
    for (switchCount=0; switchCount<NUM_SWITCH_BYTES; switchCount++) {
      SwitchesMinus2[switchCount] = SwitchesMinus1[switchCount];
      SwitchesMinus1[switchCount] = SwitchesNow[switchCount];

      // Enable switch strobe
#if defined(BSOS_USE_EXTENDED_SWITCHES_ON_PB4) or defined(BSOS_USE_EXTENDED_SWITCHES_ON_PB7)
      if (switchCount<NUM_SWITCH_BYTES_ON_U10_PORT_A) {
        BSOS_DataWrite(ADDRESS_U10_A, 0x01<<switchCount);
      } else {
        BSOS_SetContinuousSolenoidBit(true, ST5_CONTINUOUS_SOLENOID_BIT);
      }
#else       
      BSOS_DataWrite(ADDRESS_U10_A, 0x01<<switchCount);
#endif        

      // Turn off U10:CB2 if it's on (because it strobes the last bank of dip switches
      BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x34);

      // Delay for switch capacitors to charge
      delayMicroseconds(BSOS_SWITCH_DELAY_IN_MICROSECONDS);
      
      // Read the switches
      SwitchesNow[switchCount] = BSOS_DataRead(ADDRESS_U10_B);

      //Unset the strobe
      BSOS_DataWrite(ADDRESS_U10_A, 0x00);
#if defined(BSOS_USE_EXTENDED_SWITCHES_ON_PB4) or defined(BSOS_USE_EXTENDED_SWITCHES_ON_PB7)
      BSOS_SetContinuousSolenoidBit(false, ST5_CONTINUOUS_SOLENOID_BIT);
#endif 

      // Some switches need to trigger immediate closures (bumpers & slings)
      startingClosures = (SwitchesNow[switchCount]) & (~SwitchesMinus1[switchCount]);
      boolean immediateSolenoidFired = false;
      // If one of the switches is starting to close (off, on)
      if (startingClosures) {
        // Loop on bits of switch byte
        for (byte bitCount=0; bitCount<8 && immediateSolenoidFired==false; bitCount++) {
          // If this switch bit is closed
          if (startingClosures&0x01) {
            byte startingSwitchNum = switchCount*8 + bitCount;
            // Loop on immediate switch data
            for (int immediateSwitchCount=0; immediateSwitchCount<NumGamePrioritySwitches && immediateSolenoidFired==false; immediateSwitchCount++) {
              // If this switch requires immediate action
              if (GameSwitches && startingSwitchNum==GameSwitches[immediateSwitchCount].switchNum) {
                // Start firing this solenoid (just one until the closure is validate
                PushToFrontOfSolenoidStack(GameSwitches[immediateSwitchCount].solenoid, 1);
                immediateSolenoidFired = true;
              }
            }
          }
          startingClosures = startingClosures>>1;
        }
      }

      immediateSolenoidFired = false;
      validClosures = (SwitchesNow[switchCount] & SwitchesMinus1[switchCount]) & ~SwitchesMinus2[switchCount];
      // If there is a valid switch closure (off, on, on)
      if (validClosures) {
        // Loop on bits of switch byte
        for (byte bitCount=0; bitCount<8; bitCount++) {
          // If this switch bit is closed
          if (validClosures&0x01) {
            byte validSwitchNum = switchCount*8 + bitCount;
            // Loop through all switches and see what's triggered
            for (int validSwitchCount=0; validSwitchCount<NumGameSwitches; validSwitchCount++) {

              // If we've found a valid closed switch
              if (GameSwitches && GameSwitches[validSwitchCount].switchNum==validSwitchNum) {

                // If we're supposed to trigger a solenoid, then do it
                if (GameSwitches[validSwitchCount].solenoid!=SOL_NONE) {
                  if (validSwitchCount<NumGamePrioritySwitches && immediateSolenoidFired==false) {
                    PushToFrontOfSolenoidStack(GameSwitches[validSwitchCount].solenoid, GameSwitches[validSwitchCount].solenoidHoldTime);
                  } else {
                    BSOS_PushToSolenoidStack(GameSwitches[validSwitchCount].solenoid, GameSwitches[validSwitchCount].solenoidHoldTime);
                  }
                } // End if this is a real solenoid
              } // End if this is a switch in the switch table
            } // End loop on switches in switch table
            // Push this switch to the game rules stack
            PushToSwitchStack(validSwitchNum);
          }
          validClosures = validClosures>>1;
        }        
      }

      // There are no port reads or writes for the rest of the loop, 
      // so we can allow the display interrupt to fire
      interrupts();
      
      // Wait so total delay will allow lamp SCRs to get to the proper voltage
      delayMicroseconds(BSOS_TIMING_LOOP_PADDING_IN_MICROSECONDS);
      
      noInterrupts();
    }
    BSOS_DataWrite(ADDRESS_U10_A, backup10A);

    if (NumCyclesBeforeRevertingSolenoidByte!=0) {
      NumCyclesBeforeRevertingSolenoidByte -= 1;
      if (NumCyclesBeforeRevertingSolenoidByte==0) {
        CurrentSolenoidByte |= RevertSolenoidBit;
        RevertSolenoidBit = 0x00;
      }
    }

#ifdef BALLY_STERN_OS_USE_DASH32
    // mask out sound E line
    byte curDisplayDigitEnableByte = BSOS_DataRead(ADDRESS_U11_A);
    BSOS_DataWrite(ADDRESS_U11_A, curDisplayDigitEnableByte | 0x02);
#endif    

    // If we need to turn off momentary solenoids, do it first
    byte momentarySolenoidAtStart = PullFirstFromSolenoidStack();
    if (momentarySolenoidAtStart!=SOLENOID_STACK_EMPTY) {
      CurrentSolenoidByte = (CurrentSolenoidByte&0xF0) | momentarySolenoidAtStart;
      BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte);
#ifdef BALLY_STERN_OS_USE_DASH32
      // Raise CB2 so we don't unset the solenoid we just set
      BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x3C);
      // Mask off sound lines
      BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte | SOL_NONE);
      // Put CB2 back low
      BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x34);
      // Put solenoids back again
      BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte);
#endif    
    } else {
      CurrentSolenoidByte = (CurrentSolenoidByte&0xF0) | SOL_NONE;
      BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte);
    }

#ifdef BALLY_STERN_OS_USE_DASH32
    // put back U11 A without E line
    BSOS_DataWrite(ADDRESS_U11_A, curDisplayDigitEnableByte);
#endif    

#ifndef BALLY_STERN_OS_USE_AUX_LAMPS
    for (int lampBitCount = 0; lampBitCount<BSOS_NUM_LAMP_BITS; lampBitCount++) {
      byte lampData = 0xF0 + lampBitCount;

      interrupts();
      BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
      noInterrupts();
      
      // Latch address & strobe
      BSOS_DataWrite(ADDRESS_U10_A, lampData);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

      BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x38);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

      BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x30);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

      // Use the inhibit lines to set the actual data to the lamp SCRs 
      // (here, we don't care about the lower nibble because the address was already latched)
      byte lampOutput = LampStates[lampBitCount];
      // Every other time through the cycle, we OR in the dim variable
      // in order to dim those lights
      if (numberOfU10Interrupts%DimDivisor1) lampOutput |= LampDim0[lampBitCount];
      if (numberOfU10Interrupts%DimDivisor2) lampOutput |= LampDim1[lampBitCount];

      BSOS_DataWrite(ADDRESS_U10_A, lampOutput);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      
    }

    // Latch 0xFF separately without interrupt clear
    BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) | 0x08);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) & 0xF7);

#else 

    for (int lampBitCount=0; lampBitCount<15; lampBitCount++) {
      byte lampData = 0xF0 + lampBitCount;

      interrupts();
      BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
      noInterrupts();
      
      // Latch address & strobe
      BSOS_DataWrite(ADDRESS_U10_A, lampData);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

      BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x38);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

      BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x30);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

      // Use the inhibit lines to set the actual data to the lamp SCRs 
      // (here, we don't care about the lower nibble because the address was already latched)
      byte lampOutput = LampStates[lampBitCount];
      // Every other time through the cycle, we OR in the dim variable
      // in order to dim those lights
      if (numberOfU10Interrupts%DimDivisor1) lampOutput |= LampDim0[lampBitCount];
      if (numberOfU10Interrupts%DimDivisor2) lampOutput |= LampDim1[lampBitCount];

      BSOS_DataWrite(ADDRESS_U10_A, lampOutput);
#ifdef BSOS_SLOW_DOWN_LAMP_STROBE      
//      WaitClockCycle();
      delayMicroseconds(2);
#endif      

    }
    // Latch 0xFF separately without interrupt clear
    // to park 0xFF in main lamp board
    BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) | 0x08);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, BSOS_DataRead(ADDRESS_U10_B_CONTROL) & 0xF7);

    for (int lampBitCount=15; lampBitCount<22; lampBitCount++) {
      byte lampOutput = (LampStates[lampBitCount]&0xF0) | (lampBitCount-15);
      // Every other time through the cycle, we OR in the dim variable
      // in order to dim those lights
      if (numberOfU10Interrupts%DimDivisor1) lampOutput |= LampDim0[lampBitCount];
      if (numberOfU10Interrupts%DimDivisor2) lampOutput |= LampDim1[lampBitCount];

      interrupts();
      BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
      noInterrupts();

      BSOS_DataWrite(ADDRESS_U10_A, lampOutput | 0xF0);
      BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) | 0x08);
      BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) & 0xF7);    
      BSOS_DataWrite(ADDRESS_U10_A, lampOutput);
    }
    
    BSOS_DataWrite(ADDRESS_U10_A, 0xFF);
    BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) | 0x08);
    BSOS_DataWrite(ADDRESS_U11_A_CONTROL, BSOS_DataRead(ADDRESS_U11_A_CONTROL) & 0xF7);

#endif 

    interrupts();
    noInterrupts();

    InsideZeroCrossingInterrupt = 0;
    BSOS_DataWrite(ADDRESS_U10_A, backup10A);
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, u10BControlLatest);

    // Read U10B to clear interrupt
    BSOS_DataRead(ADDRESS_U10_B);
    numberOfU10Interrupts+=1;
  }
}


#else
#error "Must define BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT in BSOS_Config.h -- hardware display interrupt no longer supported"
#endif 




byte BSOS_SetDisplay(int displayNumber, unsigned long value, boolean blankByMagnitude, byte minDigits) {
  if (displayNumber<0 || displayNumber>4) return 0;

  byte blank = 0x00;

  for (int count=0; count<BALLY_STERN_OS_NUM_DIGITS; count++) {
    blank = blank * 2;
    if (value!=0 || count<minDigits) blank |= 1;
    DisplayDigits[displayNumber][(BALLY_STERN_OS_NUM_DIGITS-1)-count] = value%10;
    value /= 10;    
  }

  if (blankByMagnitude) DisplayDigitEnable[displayNumber] = blank;

  return blank;
}

void BSOS_SetDisplayBlank(int displayNumber, byte bitMask) {
  if (displayNumber<0 || displayNumber>4) return;
    
  DisplayDigitEnable[displayNumber] = bitMask;
}

// This is confusing -
// Digit mask is like this
//   bit=   b7 b6 b5 b4 b3 b2 b1 b0
//   digit=  x  x  6  5  4  3  2  1
//   (with digit 6 being the least-significant, 1's digit
//  
// so, looking at it from left to right on the display
//   digit=  1  2  3  4  5  6
//   bit=   b0 b1 b2 b3 b4 b5


byte BSOS_GetDisplayBlank(int displayNumber) {
  if (displayNumber<0 || displayNumber>4) return 0;
  return DisplayDigitEnable[displayNumber];
}

#if defined(BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT) && defined(BALLY_STERN_OS_ADJUSTABLE_DISPLAY_INTERRUPT)
void BSOS_SetDisplayRefreshConstant(int intervalConstant) {
  cli();
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for selected increment
  OCR1A = intervalConstant;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();
}
#endif


void BSOS_SetDisplayFlash(int displayNumber, unsigned long value, unsigned long curTime, int period, byte minDigits) {
  // A period of zero toggles display every other time
  if (period) {
    if ((curTime/period)%2) {
      BSOS_SetDisplay(displayNumber, value, true, minDigits);
    } else {
      BSOS_SetDisplayBlank(displayNumber, 0);
    }
  }
  
}


void BSOS_SetDisplayFlashCredits(unsigned long curTime, int period) {
  if (period) {
    if ((curTime/period)%2) {
      DisplayDigitEnable[4] |= 0x06;
    } else {
      DisplayDigitEnable[4] &= 0x39;
    }
  }
}


void BSOS_SetDisplayCredits(int value, boolean displayOn, boolean showBothDigits) {
#ifdef BALLY_STERN_OS_USE_6_DIGIT_CREDIT_DISPLAY_WITH_7_DIGIT_DISPLAYS
  DisplayDigits[4][2] = (value%100) / 10;
  DisplayDigits[4][3] = (value%10);
#else
  DisplayDigits[4][1] = (value%100) / 10;
  DisplayDigits[4][2] = (value%10);
#endif 
  byte enableMask = DisplayDigitEnable[4] & BALLY_STERN_OS_MASK_SHIFT_1;

  if (displayOn) {
    if (value>9 || showBothDigits) enableMask |= BALLY_STERN_OS_MASK_SHIFT_2; 
    else enableMask |= 0x04;
  }

  DisplayDigitEnable[4] = enableMask;
}

void BSOS_SetDisplayMatch(int value, boolean displayOn, boolean showBothDigits) {
  BSOS_SetDisplayBallInPlay(value, displayOn, showBothDigits);
}

void BSOS_SetDisplayBallInPlay(int value, boolean displayOn, boolean showBothDigits) {
#ifdef BALLY_STERN_OS_USE_6_DIGIT_CREDIT_DISPLAY_WITH_7_DIGIT_DISPLAYS
  DisplayDigits[4][5] = (value%100) / 10;
  DisplayDigits[4][6] = (value%10); 
#else
  DisplayDigits[4][4] = (value%100) / 10;
  DisplayDigits[4][5] = (value%10); 
#endif
  byte enableMask = DisplayDigitEnable[4] & BALLY_STERN_OS_MASK_SHIFT_2;

  if (displayOn) {
    if (value>9 || showBothDigits) enableMask |= BALLY_STERN_OS_MASK_SHIFT_1;
    else enableMask |= 0x20;
  }

  DisplayDigitEnable[4] = enableMask;
}



void BSOS_SetDimDivisor(byte level, byte divisor) {
  if (level==1) DimDivisor1 = divisor;
  if (level==2) DimDivisor2 = divisor;
}

void BSOS_SetLampState(int lampNum, byte s_lampState, byte s_lampDim, int s_lampFlashPeriod) {
  if (lampNum>=BSOS_MAX_LAMPS || lampNum<0) return;
  
  if (s_lampState) {
    int adjustedLampFlash = s_lampFlashPeriod/50;
    
    if (s_lampFlashPeriod!=0 && adjustedLampFlash==0) adjustedLampFlash = 1;
    if (adjustedLampFlash>250) adjustedLampFlash = 250;
    
    // Only turn on the lamp if there's no flash, because if there's a flash
    // then the lamp will be turned on by the ApplyFlashToLamps function
    if (s_lampFlashPeriod==0) LampStates[lampNum/4] &= ~(0x10<<(lampNum%4));
    LampFlashPeriod[lampNum] = adjustedLampFlash;
  } else {
    LampStates[lampNum/4] |= (0x10<<(lampNum%4));
    LampFlashPeriod[lampNum] = 0;
  }

  if (s_lampDim & 0x01) {    
    LampDim0[lampNum/4] |= (0x10<<(lampNum%4));
  } else {
    LampDim0[lampNum/4] &= ~(0x10<<(lampNum%4));
  }

  if (s_lampDim & 0x02) {    
    LampDim1[lampNum/4] |= (0x10<<(lampNum%4));
  } else {
    LampDim1[lampNum/4] &= ~(0x10<<(lampNum%4));
  }

}

byte BSOS_ReadLampState(int lampNum) {
  if (lampNum>=BSOS_MAX_LAMPS || lampNum<0) return 0x00;
  byte lampStateByte = LampStates[lampNum/4];
  return (lampStateByte & (0x10<<(lampNum%4))) ? 0 : 1;
}

byte BSOS_ReadLampDim(int lampNum) {
  if (lampNum>=BSOS_MAX_LAMPS || lampNum<0) return 0x00;
  byte lampDim = 0;
  byte lampDimByte = LampDim0[lampNum/4];
  if (lampDimByte & (0x10<<(lampNum%4))) lampDim |= 1;

  lampDimByte = LampDim1[lampNum/4];
  if (lampDimByte & (0x10<<(lampNum%4))) lampDim |= 2;

  return lampDim;
}

int BSOS_ReadLampFlash(int lampNum) {
  if (lampNum>=BSOS_MAX_LAMPS || lampNum<0) return 0;

  return LampFlashPeriod[lampNum]*50;
}



void BSOS_ApplyFlashToLamps(unsigned long curTime) {
  for (int count=0; count<BSOS_MAX_LAMPS; count++) {
    if ( LampFlashPeriod[count]!=0 ) {
      unsigned long adjustedLampFlash = (unsigned long)LampFlashPeriod[count] * (unsigned long)50;
      if ((curTime/adjustedLampFlash)%2) {
        LampStates[count/4] &= ~(0x10<<(count%4));
      } else {
        LampStates[count/4] |= (0x10<<(count%4));
      }
    } // end if this light should flash
  } // end loop on lights
}


void BSOS_FlashAllLamps(unsigned long curTime) {
  for (int count=0; count<BSOS_MAX_LAMPS; count++) {
    BSOS_SetLampState(count, 1, 0, 500);  
  }

  BSOS_ApplyFlashToLamps(curTime);
}

void BSOS_TurnOffAllLamps() {
  for (int count=0; count<BSOS_MAX_LAMPS; count++) {
    BSOS_SetLampState(count, 0, 0, 0);  
  }
}


void BSOS_ClearVariables() {
  // Reset solenoid stack
  SolenoidStackFirst = 0;
  SolenoidStackLast = 0;

  // Reset switch stack
  SwitchStackFirst = 0;
  SwitchStackLast = 0;

  CurrentDisplayDigit = 0; 

  // Set default values for the displays
  for (int displayCount=0; displayCount<5; displayCount++) {
    for (int digitCount=0; digitCount<6; digitCount++) {
      DisplayDigits[displayCount][digitCount] = 0;
    }
    DisplayDigitEnable[displayCount] = 0x03;
#ifdef BALLY_STERN_OS_DIMMABLE_DISPLAYS    
    DisplayDim[displayCount] = false;
#endif
  }

  // Turn off all lamp states
  for (int lampNibbleCounter=0; lampNibbleCounter<BSOS_NUM_LAMP_BITS; lampNibbleCounter++) {
    LampStates[lampNibbleCounter] = 0xFF;
    LampDim0[lampNibbleCounter] = 0x00;
    LampDim1[lampNibbleCounter] = 0x00;
  }

  for (int lampFlashCount=0; lampFlashCount<BSOS_MAX_LAMPS; lampFlashCount++) {
    LampFlashPeriod[lampFlashCount] = 0;
  }

  // Reset all the switch values 
  // (set them as closed so that if they're stuck they don't register as new events)
  byte switchCount;
  for (switchCount=0; switchCount<NUM_SWITCH_BYTES; switchCount++) {
    SwitchesMinus2[switchCount] = 0xFF;
    SwitchesMinus1[switchCount] = 0xFF;
    SwitchesNow[switchCount] = 0xFF;
  }
}


void BSOS_HookInterrupts() {
  // Hook up the interrupt
#if not defined (BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT)
#error "Must define BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT in BSOS_Config.h -- hardware display interrupt no longer supported"
#else
/*
  cli();
  TCCR2A|=(1<<WGM21);     //Set the CTC mode
  OCR2A=0xBA;            //Set the value for 3ms
  TIMSK2|=(1<<OCIE2A);   //Set the interrupt request
  TCCR2B|=(1<<CS22);     //Set the prescale 1/64 clock
  sei();                 //Enable interrupt
*/  

  cli();
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for selected increment
  OCR1A = BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT_INTERVAL;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();
  
  attachInterrupt(digitalPinToInterrupt(2), InterruptService3, LOW);
#endif  
}


unsigned long BSOS_InitializeMPU(boolean performBasicBoardCheck, byte creditResetSwitch) {
  unsigned int retResult = BSOS_INIT_NO_ERRORS;
  // Wait for board to boot
  delayMicroseconds(50000);
  delayMicroseconds(50000);

#if (BALLY_STERN_OS_HARDWARE_REV==1) or (BALLY_STERN_OS_HARDWARE_REV==2)
  (void)performBasicBoardCheck;
  (void)creditResetSwitch;
  // Assume Arduino pins all start as input
  unsigned long startTime = millis();
  boolean sawHigh = false;
  boolean sawLow = false;
  // for one second, look for activity on the VMA line (A5)
  // If we see anything, then the MPU is active so we shouldn't run
  while ((millis()-startTime)<1000) {
    if (PINC&0x20) sawHigh = true;
    else sawLow = true;
  }
  // If we saw both a high and low signal, then someone is toggling the 
  // VMA line, so we should hang here forever (until reset)
  if (sawHigh && sawLow) {
    while (1);
  }
#elif (BALLY_STERN_OS_HARDWARE_REV==3)
  (void)performBasicBoardCheck;
  (void)creditResetSwitch;
  for (byte count=2; count<32; count++) pinMode(count, INPUT);

  // Decide if halt should be raised (based on switch)   
  pinMode(13, INPUT);
  if (digitalRead(13)==0) {
    // Switch indicates the Arduino should run, so HALT the 6800
    pinMode(14, OUTPUT); // Halt
    digitalWrite(14, LOW);
  } else {
    // Let the 6800 run 
    pinMode(14, OUTPUT); // Halt
    digitalWrite(14, HIGH);
    while(1);
  }  

#elif (BALLY_STERN_OS_HARDWARE_REV==4)
  // put the 6800 buffers into tri-state
  pinMode(RPU_BUFFER_DISABLE, OUTPUT);
  digitalWrite(RPU_BUFFER_DISABLE, 1);

  // Set /HALT low so the processor doesn't come online
  // (on some hardware, HALT & RESET are combined)
  pinMode(RPU_HALT_PIN, OUTPUT); 
  digitalWrite(RPU_HALT_PIN, 0);  
  pinMode(RPU_RESET_PIN, OUTPUT); 
  digitalWrite(RPU_RESET_PIN, 0);  

  // Set VMA, R/W, and PHI2 to OUTPUT
  pinMode(RPU_VMA_PIN, OUTPUT);
  pinMode(RPU_RW_PIN, OUTPUT);
  BSOS_SetAddressPinsDirection(BSOS_PINS_OUTPUT);  
  
#ifdef RPU_PROCESSOR_6800
  pinMode(RPU_PHI2_PIN, INPUT);
#else
  pinMode(RPU_PHI2_PIN, OUTPUT);
#endif  
  delay(1000);
//  BSOS_DataWrite(ADDRESS_SB100, 0x01);

  boolean bootToOldCode = false;

  if (creditResetSwitch!=0xFF) {
    // Check for credit button
    InitializeU10PIA();
    InitializeU11PIA();
  
    byte strobeNum = 0x01 << (creditResetSwitch/8);
    byte switchNum = 0x01 << (creditResetSwitch%8);
    BSOS_DataWrite(ADDRESS_U10_A, strobeNum);
    // Turn off U10:CB2 if it's on (because it strobes the last bank of dip switches
    BSOS_DataWrite(ADDRESS_U10_B_CONTROL, 0x34);
  
    // Delay for switch capacitors to charge
    delayMicroseconds(BSOS_SWITCH_DELAY_IN_MICROSECONDS);
        
    // Read the switches
    byte curSwitchByte = BSOS_DataRead(ADDRESS_U10_B);
  
    //Unset the strobe
    BSOS_DataWrite(ADDRESS_U10_A, 0x00);
  
    if (curSwitchByte & switchNum) {
      bootToOldCode = true;
      retResult |= BSOS_INIT_CREDIT_RESET_BUTTON_HIT;
    }
  }

  pinMode(RPU_SWITCH_PIN, INPUT);
  if (digitalRead(RPU_SWITCH_PIN)) retResult |= BSOS_INIT_SELECTOR_SWITCH_ON;
  
  // See if the switch is off
  if (digitalRead(RPU_SWITCH_PIN)==0 || bootToOldCode) {

    // If the switch is off, allow 6808 to boot
    pinMode(RPU_BUFFER_DISABLE, OUTPUT); // IRQ
    // Turn on the tri-state buffers
    digitalWrite(RPU_BUFFER_DISABLE, 0);
    
    pinMode(RPU_PHI2_PIN, INPUT); // CLOCK
    pinMode(RPU_VMA_PIN, INPUT); // VMA
    pinMode(RPU_RW_PIN, INPUT); // R/W

    // Set all the pins to input so they'll stay out of the way
    BSOS_SetDataPinsDirection(BSOS_PINS_INPUT);
    BSOS_SetAddressPinsDirection(BSOS_PINS_INPUT);

    // Set /HALT high
    pinMode(RPU_HALT_PIN, OUTPUT);
    digitalWrite(RPU_HALT_PIN, 1);
    pinMode(RPU_RESET_PIN, OUTPUT);
    digitalWrite(RPU_RESET_PIN, 1);

    retResult |= BSOS_INIT_ORIGINAL_CODE_RUNNING;
    if (!performBasicBoardCheck) while (1);
    else return retResult;    
  }

#endif  

#if (BALLY_STERN_OS_HARDWARE_REV==1)
  // Arduino A0 = MPU A0
  // Arduino A1 = MPU A1
  // Arduino A2 = MPU A3
  // Arduino A3 = MPU A4
  // Arduino A4 = MPU A7
  // Arduino A5 = MPU VMA
  // Set up the address lines A0-A7 as output
  DDRC = DDRC | 0x3F;
  // Set up D13 as address line A5 (and set it low)
  DDRB = DDRB | 0x20;
  PORTB = PORTB & 0xDF;
  // Set up control lines & data lines
  DDRD = DDRD & 0xEB;
  DDRD = DDRD | 0xE8;
  // Set VMA OFF
  PORTC = PORTC & 0xDF;
  // Set R/W to HIGH
  PORTD = (PORTD | 0x08);  
#elif (BALLY_STERN_OS_HARDWARE_REV==2) 
  // Set up the address lines A0-A7 as output
  DDRC = DDRC | 0x3F;
  // Set up D13 as address line A7 (and set it high)
  DDRB = DDRB | 0x20;
  PORTB = PORTB | 0x20;
  // Set up control lines & data lines
  DDRD = DDRD & 0xEB;
  DDRD = DDRD | 0xE8;
  // Set VMA OFF
  PORTC = PORTC & 0xDF;
  // Set R/W to HIGH
  PORTD = (PORTD | 0x08);  
#elif (BALLY_STERN_OS_HARDWARE_REV==3) 
  pinMode(3, INPUT); // CLK
  pinMode(4, OUTPUT); // VMA
  pinMode(5, OUTPUT); // R/W
  for (byte count=6; count<13; count++) pinMode(count, INPUT); // D0-D6
  pinMode(13, INPUT); // Switch
  pinMode(14, OUTPUT); // Halt
  pinMode(15, INPUT); // D7
  for (byte count=16; count<32; count++) pinMode(count, OUTPUT); // Address lines are output
  digitalWrite(5, HIGH);  // Set R/W line high (Read)
  digitalWrite(4, LOW);  // Set VMA line LOW
#endif

  // Prep the address bus (all lines zero)
  BSOS_DataRead(0);
  // Set up the PIAs
  InitializeU10PIA();
  InitializeU11PIA();

  // Read values from MPU dip switches
#ifdef BALLY_STERN_OS_USE_DIP_SWITCHES  
  ReadDipSwitches();
#endif 
  
  // Reset address bus
  BSOS_DataRead(0);
  BSOS_ClearVariables();
  BSOS_HookInterrupts();
  BSOS_DataRead(0);  // Reset address bus

  // Cleary all possible interrupts by reading the registers
  byte result;
  result = BSOS_DataRead(ADDRESS_U11_A);
  if (result!=0x02) retResult |= BSOS_INIT_U11_PIA_ERROR;
  result = BSOS_DataRead(ADDRESS_U11_B);
  if (result!=DEFAULT_SOLENOID_STATE) retResult |= BSOS_INIT_U11_PIA_ERROR;
  result = BSOS_DataRead(ADDRESS_U10_A);
  if (result!=0xF0) retResult |= BSOS_INIT_U10_PIA_ERROR;
  result = BSOS_DataRead(ADDRESS_U10_B);
  if (result!=0x00) retResult |= BSOS_INIT_U10_PIA_ERROR;
  BSOS_DataRead(0);  // Reset address bus

  return retResult;
}

byte BSOS_GetDipSwitches(byte index) {
#ifdef BALLY_STERN_OS_USE_DIP_SWITCHES
  if (index>3) return 0x00;
  return DipSwitches[index];
#else
  return 0x00 & index;
#endif
}


void BSOS_SetupGameSwitches(int s_numSwitches, int s_numPrioritySwitches, PlayfieldAndCabinetSwitch *s_gameSwitchArray) {
  NumGameSwitches = s_numSwitches;
  NumGamePrioritySwitches = s_numPrioritySwitches;
  GameSwitches = s_gameSwitchArray;
}



void BSOS_SetCoinLockout(boolean lockoutOff, byte solbit) {
  if (!lockoutOff) {
    CurrentSolenoidByte = CurrentSolenoidByte & ~solbit;
  } else {
    CurrentSolenoidByte = CurrentSolenoidByte | solbit;
  }
  BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte);
}


void BSOS_SetDisableFlippers(boolean disableFlippers, byte solbit) {
  if (disableFlippers) {
    CurrentSolenoidByte = CurrentSolenoidByte | solbit;
  } else {
    CurrentSolenoidByte = CurrentSolenoidByte & ~solbit;
  }
  
  BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte);
}


void BSOS_SetContinuousSolenoidBit(boolean bitOn, byte solbit) {

  if (bitOn) {
    CurrentSolenoidByte = CurrentSolenoidByte | solbit;
  } else {
    CurrentSolenoidByte = CurrentSolenoidByte & ~solbit;
  }
  BSOS_DataWrite(ADDRESS_U11_B, CurrentSolenoidByte);
}


boolean BSOS_FireContinuousSolenoid(byte solBit, byte numCyclesToFire) {
  if (NumCyclesBeforeRevertingSolenoidByte) return false;

  NumCyclesBeforeRevertingSolenoidByte = numCyclesToFire;

  RevertSolenoidBit = solBit;
  BSOS_SetContinuousSolenoidBit(false, solBit);
  return true;
}


byte BSOS_ReadContinuousSolenoids() {
  return BSOS_DataRead(ADDRESS_U11_B);
}


void BSOS_DisableSolenoidStack() {
  SolenoidStackEnabled = false;
}


void BSOS_EnableSolenoidStack() {
  SolenoidStackEnabled = true;
}



void BSOS_CycleAllDisplays(unsigned long curTime, byte digitNum) {
  int displayDigit = (curTime/250)%10;
  unsigned long value;
#ifdef BALLY_STERN_OS_USE_7_DIGIT_DISPLAYS
  value = displayDigit*1111111;
#else  
  value = displayDigit*111111;
#endif

  byte displayNumToShow = 0;
  byte displayBlank = BALLY_STERN_OS_ALL_DIGITS_MASK;

  if (digitNum!=0) {
    displayNumToShow = (digitNum-1)/6;
#ifdef BALLY_STERN_OS_USE_7_DIGIT_DISPLAYS
    displayBlank = (0x40)>>((digitNum-1)%7);
#else    
    displayBlank = (0x20)>>((digitNum-1)%6);
#endif    
  }

  for (int count=0; count<5; count++) {
    if (digitNum) {
      BSOS_SetDisplay(count, value);
      if (count==displayNumToShow) BSOS_SetDisplayBlank(count, displayBlank);
      else BSOS_SetDisplayBlank(count, 0);
    } else {
      BSOS_SetDisplay(count, value, true);
    }
  }
}

#ifdef BALLY_STERN_OS_USE_SQUAWK_AND_TALK

void BSOS_PlaySoundSquawkAndTalk(byte soundByte) {

  byte oldSolenoidControlByte, soundLowerNibble, soundUpperNibble;

  // mask further zero-crossing interrupts during this 
  noInterrupts();

  // Get the current value of U11:PortB - current solenoids
  oldSolenoidControlByte = BSOS_DataRead(ADDRESS_U11_B);
  soundLowerNibble = (oldSolenoidControlByte&0xF0) | (soundByte&0x0F); 
  soundUpperNibble = (oldSolenoidControlByte&0xF0) | (soundByte/16); 
    
  // Put 1s on momentary solenoid lines
  BSOS_DataWrite(ADDRESS_U11_B, oldSolenoidControlByte | 0x0F);

  // Put sound latch low
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x34);

  // Let the strobe stay low for a moment
  delayMicroseconds(32);

  // Put sound latch high
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x3C);
  
  // put the new byte on U11:PortB (the lower nibble is currently loaded)
  BSOS_DataWrite(ADDRESS_U11_B, soundLowerNibble);
        
  // wait 138 microseconds
  delayMicroseconds(138);

  // put the new byte on U11:PortB (the uppper nibble is currently loaded)
  BSOS_DataWrite(ADDRESS_U11_B, soundUpperNibble);

  // wait 76 microseconds
  delayMicroseconds(145);

  // Restore the original solenoid byte
  BSOS_DataWrite(ADDRESS_U11_B, oldSolenoidControlByte);

  // Put sound latch low
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x34);

  interrupts();
}
#endif

// With hardware rev 1, this function relies on D13 being connected to A5 because it writes to address 0xA0
// A0  - A0   0
// A1  - A1   0   
// A2  - n/c  0
// A3  - A2   0
// A4  - A3   0
// A5  - D13  1
// A6  - n/c  0
// A7  - A4   1
// A8  - n/c  0
// A9  - GND  0
// A10 - n/c  0
// A11 - n/c  0
// A12 - GND  0
// A13 - n/c  0
#ifdef BALLY_STERN_OS_USE_SB100
void BSOS_PlaySB100(byte soundByte) {

#if (BALLY_STERN_OS_HARDWARE_REV==1)
  PORTB = PORTB | 0x20;
#endif 

  BSOS_DataWrite(ADDRESS_SB100, soundByte);

#if (BALLY_STERN_OS_HARDWARE_REV==1)
  PORTB = PORTB & 0xDF;
#endif 
  
}

#if (BALLY_STERN_OS_HARDWARE_REV==2)
void BSOS_PlaySB100Chime(byte soundByte) {

  BSOS_DataWrite(ADDRESS_SB100_CHIMES, soundByte);

}
#endif 
#endif


#ifdef BALLY_STERN_OS_USE_DASH51
void BSOS_PlaySoundDash51(byte soundByte) {

  // This device has 32 possible sounds, but they're mapped to 
  // 0 - 15 and then 128 - 143 on the original card, with bits b4, b5, and b6 reserved
  // for timing controls.
  // For ease of use, I've mapped the sounds from 0-31
  
  byte oldSolenoidControlByte, soundLowerNibble, displayWithSoundBit4, oldDisplayByte;

  // mask further zero-crossing interrupts during this 
  noInterrupts();

  // Get the current value of U11:PortB - current solenoids
  oldSolenoidControlByte = BSOS_DataRead(ADDRESS_U11_B);
  oldDisplayByte = BSOS_DataRead(ADDRESS_U11_A);
  soundLowerNibble = (oldSolenoidControlByte&0xF0) | (soundByte&0x0F); 
  displayWithSoundBit4 = oldDisplayByte;
  if (soundByte & 0x10) displayWithSoundBit4 |= 0x02;
  else displayWithSoundBit4 &= 0xFD;
    
  // Put 1s on momentary solenoid lines
  BSOS_DataWrite(ADDRESS_U11_B, oldSolenoidControlByte | 0x0F);

  // Put sound latch low
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x34);

  // Let the strobe stay low for a moment
  delayMicroseconds(68);

  // put bit 4 on Display Enable 7
  BSOS_DataWrite(ADDRESS_U11_A, displayWithSoundBit4);

  // Put sound latch high
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x3C);
  
  // put the new byte on U11:PortB (the lower nibble is currently loaded)
  BSOS_DataWrite(ADDRESS_U11_B, soundLowerNibble);
        
  // wait 180 microseconds
  delayMicroseconds(180);

  // Restore the original solenoid byte
  BSOS_DataWrite(ADDRESS_U11_B, oldSolenoidControlByte);

  // Restore the original display byte
  BSOS_DataWrite(ADDRESS_U11_A, oldDisplayByte);

  // Put sound latch low
  BSOS_DataWrite(ADDRESS_U11_B_CONTROL, 0x34);

  interrupts();
}

#endif

#if (BALLY_STERN_OS_HARDWARE_REV>=2 && defined(BALLY_STERN_OS_USE_SB300))

void BSOS_PlaySB300SquareWave(byte soundRegister, byte soundByte) {
  BSOS_DataWrite(ADDRESS_SB300_SQUARE_WAVES+soundRegister, soundByte);
}

void BSOS_PlaySB300Analog(byte soundRegister, byte soundByte) {
  BSOS_DataWrite(ADDRESS_SB300_ANALOG+soundRegister, soundByte);
}

#endif 

// EEProm Helper functions

void BSOS_WriteByteToEEProm(unsigned short startByte, byte value) {
  EEPROM.write(startByte, value);
}

byte BSOS_ReadByteFromEEProm(unsigned short startByte) {
  byte value = EEPROM.read(startByte);

  // If this value is unset, set it
  if (value==0xFF) {
    value = 0;
    BSOS_WriteByteToEEProm(startByte, value);
  }
  return value;
}



unsigned long BSOS_ReadULFromEEProm(unsigned short startByte, unsigned long defaultValue) {
  unsigned long value;

  value = (((unsigned long)EEPROM.read(startByte+3))<<24) | 
          ((unsigned long)(EEPROM.read(startByte+2))<<16) | 
          ((unsigned long)(EEPROM.read(startByte+1))<<8) | 
          ((unsigned long)(EEPROM.read(startByte)));

  if (value==0xFFFFFFFF) {
    value = defaultValue; 
    BSOS_WriteULToEEProm(startByte, value);
  }
  return value;
}


void BSOS_WriteULToEEProm(unsigned short startByte, unsigned long value) {
  EEPROM.write(startByte+3, (byte)(value>>24));
  EEPROM.write(startByte+2, (byte)((value>>16) & 0x000000FF));
  EEPROM.write(startByte+1, (byte)((value>>8) & 0x000000FF));
  EEPROM.write(startByte, (byte)(value & 0x000000FF));
}
