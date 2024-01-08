
#include "pico/stdlib.h"

// a maps of BBC scan codes to ps/2 make codes index is C + R * 10

uint16_t ps2codes[81] = {
 0x12 // 00 Left SHIFT
,0x14 // 01 LEFT/RIGHT CTRL (CTRL)
,0,0,0,0,0,0,0,0 // config switches - ignored

,0x15 // 10 Q
,0x26 // 11 3
,0x25 // 12 4
,0x2E // 13 5
,0x0C // 14 F4
,0x3E // 15 8
,0x83 // 16 F7
,0x4E // 17 -
,0x55 // 18 = (^)
,0xE06B // 19 LEFT

,0x09 // 20 F10 (F0)
,0x1D // 21 W
,0x24 // 22 E
,0x2C // 23 T
,0x3D // 24 7
,0x43 // 25 I
,0x46 // 26 9
,0x45 // 27 0
,0x5D // 28 # (_)
,0xE072 // 29 DOWN

,0x16 // 30 1
,0x1E // 31 2
,0x23 // 32 D
,0x2D // 33 R
,0x36 // 34 6
,0x3C // 35 U
,0x44 // 36 O
,0x4D // 37 P
,0x54 // 38 [
,0xE075 // 39 UP

,0x58 // 40 CAPS LOCK
,0x1C // 41 A
,0x22 // 42 X
,0x2B // 43 F
,0x35 // 44 Y
,0x3B // 45 J
,0x42 // 46 K
,0x0E // 47 ` (@)
,0x52 // 48 '
,0x5A // 49 RETURN

,0x11 // 50 LEFT ALT (SHIFT LOCK)
,0x1B // 51 S
,0x21 // 52 C
,0x34 // 53 G
,0x33 // 54 H
,0x31 // 55 N
,0x4B // 56 L
,0x4C // 57 ;
,0x5B // 58 ]
,0x66 // 59 BACKSPACE (DELETE)

,0x0D // 60 TAB
,0x1A // 61 Z
,0x29 // 62 SPACE
,0x2A // 63 V
,0x32 // 64 B
,0x3A // 65 M
,0x41 // 66 ,
,0x49 // 67 .
,0x4A // 68 /
,0x69 // 69 END (COPY)

,0x76 // 70 ESCAPE
,0x05 // 71 F1
,0x06 // 72 F2
,0x04 // 73 F3
,0x03 // 74 F5
,0x0B // 75 F6
,0x0A // 76 F8
,0x01 // 77 F9
,0x61 // 78 backslash 
,0xE074 // 79 RIGHT

,0x07 // xx BREAK - handled separately in top level
};


uint16_t getps2(uint8_t x) {
	if (x <= sizeof(ps2codes)/sizeof(ps2codes[0])) {
		return ps2codes[x];
	} else {
		return 0;
	}
}

/*
                    -- F12 is used for the BREAK key, which in the real BBC asserts
                    -- reset.  Here we pass this out to the top level which may
                    -- optionally OR it in to the system reset
                    when X"07" => BREAK_OUT <= not releasex; -- F12 (BREAK)
*/

/* duplicates map... 

00 //                    when X"59" => // 00 Right SHIFT

*/
