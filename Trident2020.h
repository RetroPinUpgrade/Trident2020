/**************************************************************************
 *     This file is part of Trident2020.

    I, Dick Hamill, the author of this program disclaim all copyright
    in order to make this program freely available in perpetuity to
    anyone who would like to use it. Dick Hamill, 6/1/2020

    Trident2020 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Trident2020 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    See <https://www.gnu.org/licenses/>.
 */


// Lamp Numbers (defines for lamps)
#define BONUS_1                 0
#define BONUS_2                 1
#define BONUS_3                 2
#define BONUS_4                 3
#define BONUS_5                 4
#define BONUS_6                 5
#define BONUS_7                 6
#define BONUS_8                 7
#define BONUS_9                 8
#define BONUS_10                9
#define DROP_TARGET_5           10
#define STAND_UP_SPECIAL        11
#define LEFT_SPINNER_AMBER      12
#define LEFT_SPINNER_WHITE      13
#define STAND_UP_AMBER          14
#define STAND_UP_WHITE          15
#define RIGHT_SPINNER_YELLOW    16
#define RIGHT_SPINNER_GREEN     17
#define STAND_UP_YELLOW         18
#define STAND_UP_GREEN          19
#define DROP_TARGET_4           20
#define DROP_TARGET_3           21
#define DROP_TARGET_2           22
#define DROP_TARGET_1           23
#define TOP_EJECT_30K           24
#define TOP_EJECT_20K           25 
#define TOP_EJECT_10K           26
#define TOP_EJECT_5K            27
#define BONUS_5X_FEATURE        28
#define BONUS_4X_FEATURE        29
#define BONUS_3X_FEATURE        30
#define BONUS_2X_FEATURE        31
#define BONUS_5X                32
#define BONUS_4X                33
#define BONUS_3X                34
#define BONUS_2X                35
#define LEFT_LANE_8K            36
#define LEFT_LANE_6K            37
#define LEFT_LANE_4K            38
#define LEFT_LANE_2K            39
#define SHOOT_AGAIN             40
#define MATCH                   41
#define STAND_UP_PURPLE         42
#define DROP_TARGET_SPECIAL     43
#define RIGHT_SPINNER_PURPLE    44
#define RIGHT_OUTLANE_SPECIAL   45
#define LEFT_SPINNER_PURPLE     46
#define EXTRA_BALL              47
#define BALL_IN_PLAY            48
#define HIGH_SCORE_TO_DATE      49
#define GAME_OVER               50
#define TILT                    51
#define PLAYER_1                52
#define PLAYER_2                53
#define PLAYER_3                54
#define PLAYER_4                55
#define PLAYER_1_UP             56
#define PLAYER_2_UP             57
#define PLAYER_3_UP             58
#define PLAYER_4_UP             59


// Defines for switches
#define SW_CREDIT_RESET   5
#define SW_TILT           6
#define SW_OUTHOLE        32
#define SW_COIN_1         1
#define SW_COIN_2         0
#define SW_COIN_3         2

#define SW_SLAM           7

#define SW_DROP_TARGET_5  27
#define SW_DROP_TARGET_4  28
#define SW_DROP_TARGET_3  29
#define SW_DROP_TARGET_2  30
#define SW_DROP_TARGET_1  31

#define SW_SAUCER         25

#define SW_RIGHT_INLANE         17
#define SW_LEFT_INLANE          18
#define SW_10_PTS               26
#define SW_RIGHT_OUTLANE        16
#define SW_TOP_BUMPER           14
#define SW_BOTTOM_BUMPER        15

#define SW_WHITE    23
#define SW_GREEN    22
#define SW_AMBER    21
#define SW_YELLOW   20
#define SW_PURPLE   19

#define SW_LEFT_SPINNER   4
#define SW_RIGHT_SPINNER  3

#define SW_UL_SLING       11
#define SW_UR_SLING       10
#define SW_LL_SLING       13
#define SW_LR_SLING       12

#define SW_ROLLOVER       9

// Defines for solenoids
#define SOL_TOP_BUMPER        0
#define SOL_BOTTOM_BUMPER     1
#define SOL_UL_SLING          2
#define SOL_DROP_TARGET_1     3
#define SOL_DROP_TARGET_2     4
#define SOL_KNOCKER           5
#define SOL_LL_SLING          6
#define SOL_DROP_TARGET_3     7
#define SOL_UR_SLING          8
#define SOL_LR_SLING          9
#define SOL_DROP_TARGET_4     10
#define SOL_DROP_TARGET_5     11
#define SOL_SAUCER            12
#define SOL_DROP_TARGET_RESET 13
#define SOL_OUTHOLE           14  

// SWITCHES_WITH_TRIGGERS are for switches that will automatically
// activate a solenoid (like in the case of a chime that rings on a rollover)
// but SWITCHES_WITH_TRIGGERS are fully debounced before being activated
#define NUM_SWITCHES_WITH_TRIGGERS         6

// PRIORITY_SWITCHES_WITH_TRIGGERS are switches that trigger immediately
// (like for pop bumpers or slings) - they are not debounced completely
#define NUM_PRIORITY_SWITCHES_WITH_TRIGGERS 6

// Define automatic solenoid triggers (switch, solenoid, number of 1/120ths of a second to fire)
struct PlayfieldAndCabinetSwitch TriggeredSwitches[] = {
  { SW_TOP_BUMPER, SOL_TOP_BUMPER, 4 },
  { SW_BOTTOM_BUMPER, SOL_BOTTOM_BUMPER, 4 },
  { SW_UL_SLING, SOL_UL_SLING, 4 },
  { SW_LL_SLING, SOL_LL_SLING, 4 },
  { SW_UR_SLING, SOL_UR_SLING, 4 },
  { SW_LR_SLING, SOL_LR_SLING, 4 },
};
