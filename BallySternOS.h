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

#ifndef BALLY_STERN_OS_H


#define BALLY_STERN_OS_MAJOR_VERSION  4
#define BALLY_STERN_OS_MINOR_VERSION  2

struct PlayfieldAndCabinetSwitch {
  byte switchNum;
  byte solenoid;
  byte solenoidHoldTime;
};


/*
 * For Arduino wiring, see documentation on
 * GitHub https://ballysternos.github.io/
 */


#define SW_SELF_TEST_SWITCH 0x7F
#define SOL_NONE 0x0F
#define SWITCH_STACK_EMPTY  0xFF
#define CONTSOL_DISABLE_FLIPPERS      0x40
#define CONTSOL_DISABLE_COIN_LOCKOUT  0x20

#define BSOS_INIT_NO_ERRORS                 0
#define BSOS_INIT_U10_PIA_ERROR             0x0001
#define BSOS_INIT_U11_PIA_ERROR             0x0002
#define BSOS_INIT_DIAGNOSTIC_REQUESTED      0x1000
#define BSOS_INIT_SELECTOR_SWITCH_ON        0x2000
#define BSOS_INIT_CREDIT_RESET_BUTTON_HIT   0x4000
#define BSOS_INIT_ORIGINAL_CODE_RUNNING     0x8000

// Function Prototypes

//   Initialization
unsigned long BSOS_InitializeMPU(boolean performBasicBoardCheck=false, byte creditResetSwitch=0xFF);
void BSOS_SetupGameSwitches(int s_numSwitches, int s_numPrioritySwitches, PlayfieldAndCabinetSwitch *s_gameSwitchArray);
byte BSOS_GetDipSwitches(byte index);

// EEProm Helper Functions
byte BSOS_ReadByteFromEEProm(unsigned short startByte);
void BSOS_WriteByteToEEProm(unsigned short startByte, byte value);
unsigned long BSOS_ReadULFromEEProm(unsigned short startByte, unsigned long defaultValue=0);
void BSOS_WriteULToEEProm(unsigned short startByte, unsigned long value);

//   Swtiches
byte BSOS_PullFirstFromSwitchStack();
boolean BSOS_ReadSingleSwitchState(byte switchNum);
void BSOS_PushToSwitchStack(byte switchNumber);

//   Solenoids
void BSOS_PushToSolenoidStack(byte solenoidNumber, byte numPushes, boolean disableOverride = false);
void BSOS_SetCoinLockout(boolean lockoutOff = false, byte solbit = CONTSOL_DISABLE_COIN_LOCKOUT);
void BSOS_SetDisableFlippers(boolean disableFlippers = true, byte solbit = CONTSOL_DISABLE_FLIPPERS);
void BSOS_SetContinuousSolenoidBit(boolean bitOn, byte solBit = 0x10);
boolean BSOS_FireContinuousSolenoid(byte solBit, byte numCyclesToFire);
byte BSOS_ReadContinuousSolenoids();
void BSOS_DisableSolenoidStack();
void BSOS_EnableSolenoidStack();
boolean BSOS_PushToTimedSolenoidStack(byte solenoidNumber, byte numPushes, unsigned long whenToFire, boolean disableOverride = false);
void BSOS_UpdateTimedSolenoidStack(unsigned long curTime);

//   Displays
byte BSOS_SetDisplay(int displayNumber, unsigned long value, boolean blankByMagnitude=false, byte minDigits=2);
void BSOS_SetDisplayBlank(int displayNumber, byte bitMask);
void BSOS_SetDisplayCredits(int value, boolean displayOn = true, boolean showBothDigits=true);
void BSOS_SetDisplayMatch(int value, boolean displayOn = true, boolean showBothDigits=true);
void BSOS_SetDisplayBallInPlay(int value, boolean displayOn = true, boolean showBothDigits=true);
void BSOS_SetDisplayFlash(int displayNumber, unsigned long value, unsigned long curTime, int period=500, byte minDigits=2);
void BSOS_SetDisplayFlashCredits(unsigned long curTime, int period=100);
void BSOS_CycleAllDisplays(unsigned long curTime, byte digitNum=0); // Self-test function
byte BSOS_GetDisplayBlank(int displayNumber);
#if defined(BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT) && defined(BALLY_STERN_OS_ADJUSTABLE_DISPLAY_INTERRUPT)
void BSOS_SetDisplayRefreshConstant(int intervalConstant);
#endif

//   Lamps
void BSOS_SetLampState(int lampNum, byte s_lampState, byte s_lampDim=0, int s_lampFlashPeriod=0);
void BSOS_ApplyFlashToLamps(unsigned long curTime);
void BSOS_FlashAllLamps(unsigned long curTime); // Self-test function
void BSOS_TurnOffAllLamps();
void BSOS_SetDimDivisor(byte level=1, byte divisor=2); // 2 means 50% duty cycle, 3 means 33%, 4 means 25%...
byte BSOS_ReadLampState(int lampNum);
byte BSOS_ReadLampDim(int lampNum);
int BSOS_ReadLampFlash(int lampNum);

//   Sound
#ifdef BALLY_STERN_OS_USE_SQUAWK_AND_TALK
void BSOS_PlaySoundSquawkAndTalk(byte soundByte);
#endif
#ifdef BALLY_STERN_OS_USE_SB100
void BSOS_PlaySB100(byte soundByte);

#if (BALLY_STERN_OS_HARDWARE_REV==2)
void BSOS_PlaySB100Chime(byte soundByte);
#endif 

#endif

#ifdef BALLY_STERN_OS_USE_DASH51
void BSOS_PlaySoundDash51(byte soundByte);
#endif

#if (BALLY_STERN_OS_HARDWARE_REV>=2 && defined(BALLY_STERN_OS_USE_SB300))
void BSOS_PlaySB300SquareWave(byte soundRegister, byte soundByte);
void BSOS_PlaySB300Analog(byte soundRegister, byte soundByte);
#endif 


//   General
byte BSOS_DataRead(int address);


#ifdef BALLY_STERN_CPP_FILE
  int NumGameSwitches = 0;
  int NumGamePrioritySwitches = 0;
  PlayfieldAndCabinetSwitch *GameSwitches = NULL;
#endif


#define BALLY_STERN_OS_H
#endif
