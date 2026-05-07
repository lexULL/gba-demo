#include "common.h"
#include "memory.h"

#include <gba_input.h>

// Key Status Register (Read-Only)
// Recommended to read-out
// #define REG_KEYINPUT 0x04000130
// Bit 0 - Button A (0 = Pressed, 1 = Released)
// Bit 1 - Button B (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 2 - Select   (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 3 - Start    (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 4 - Right    (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 5 - Left     (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 6 - Up       (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 7 - Down     (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 8 - Button R (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 9 - Button L (^^^^^^^^^^^^^^^^^^^^^^^^^)
// Bit 10-15 are unused.

// Key Interrupt Control (Read/Write)
// The Keypad IRQ function is intended to terminate the very-low-power Stop mode, it is not suitable for processing normal user input, to do this, most programs
// are invoking their keypad handlers from within VBlank IRQ.
//
// 
// #define REG_KEYCNT 0x04000132

// The defines below can be used either for REG_KEYINPUT (0 = Pressed, 1 = Released) or for REG_KEYCNT (0 = Ignore, 1 = Enable)
#define KEY_A        0x0001
#define KEY_B        0x0002
#define KEY_SELECT   0x0004
#define KEY_START    0x0008
#define KEY_RIGHT    0x0010
#define KEY_LEFT     0x0020
#define KEY_UP       0x0040
#define KEY_DOWN     0x0080
#define KEY_R        0x0100
#define KEY_L        0x0200
#define KEY_MASK     0x03FF
#define KEY_IRQ_ON   0x4000 // Enable Keypad Interrupt - REG_KEYCNT ONLY
#define KEY_IRQ_OFF  0x8000 // Disable Keypad Interrupt - REG_KEYCNT ONLY

// async key handling (not the fastest and most optimal way to listen for key input)
// this causes key bounce (which happens when a physical button press doesn't produce a claen digital transition. It produces a rapid series of open/close cycles lasting ~1-50ms)
// KEY_DOWN_NOW causes problem because it samples REG_KEYINPUT exactly once per call, with no state comparison to a previous frame.
// On the GBA's 60Hz loop, one "press" can span ~3-4 frames of bounce, each frame alternating between pressed and released reads.
//
// Concrete consequences:
// Single-press actions fire multiple times: a menu selection triggers 3 times in 4 frames
// Toggle logic flips back and forth: boolean state ends up where it started
// "Press" detection is frame-order sensitive: whether you sample at the start or end of a frame changes whether you catch a bounce high or low
#define KEY_DOWN_NOW(key) (~(REG_KEYINPUT) & key)

extern u16 __key_curr, __key_prev;

// polling function
INLINE void key_poll() {
    __key_prev = __key_curr;
    __key_curr = ~REG_KEYINPUT & KEY_MASK;
}

INLINE u32 key_curr_state()      { return __key_curr; }
INLINE u32 key_prev_state()      { return __key_prev; }
INLINE u32 key_is_down(u32 key)  { return __key_curr & key ; }
INLINE u32 key_is_up(u32 key)    { return ~__key_curr & key; }
INLINE u32 key_was_down(u32 key) { return __key_prev & key ; }
INLINE u32 key_was_up(u32 key)   { return ~__key_prev & key; }

/// Transitional state checks
// Key is changing state.
INLINE u32 key_transit(u32 key) {
    return ( __key_curr ^ __key_prev) & key; 
}

// Key is held (down now and before)
INLINE u32 key_held(u32 key) {
    return ( __key_curr & __key_prev) & key;
}

// Key is being hit (down now, but not before)
INLINE u32 key_hit(u32 key) {
    return ( __key_curr &~ __key_prev) & key;
}

// Key is being released (up now but down before)
INLINE u32 key_released(u32 key) {
    return (~__key_curr & __key_prev) & key;
}

// Notes
//
// In 8-bit GB Compatibility Mode, L and R buttons are used to toggle the screen size  between normal 160x144 pixels and stretched 240x144 pixels.
// The GBA SP is additionally having a * Button used to toggle the backlight on and off (controlled by separate hardware logic, there's no way to detect or change the current backlight state by software)

// tribool: 1 if {plus} on, -1 if {minus} on, 0 if {plus}=={minus}
INLINE s32 bit_tribool(u32 x, s32 plus, s32 minus) {
    return ((x>>plus)&1) - ((x>>minus)&1);
}

enum eKeyIndex {
    KI_A=0,KI_B,KI_SELECT,KI_START,
    KI_RIGHT,KI_LEFT,KI_UP,KI_DOWN,
    KI_R,KI_L,KI_MAX
};

// tristates
INLINE s32 key_tri_horizontal() { // right/left : +/-
    return bit_tribool(__key_curr, KI_RIGHT, KI_LEFT);
}

INLINE s32 key_tri_vertical() { // down/up : +/-
    return bit_tribool(__key_curr, KI_DOWN, KI_UP);
}

INLINE s32 key_tri_shoulder() { // R/L : +/-
    return bit_tribool(__key_curr, KI_R, KI_L);
}

INLINE s32 key_tri_fire() { // B/A : -/+
    return bit_tribool(__key_curr, KI_A, KI_B);
}

// Thanks to tribools, instead of doing the good old:
// if(key_is_down(KEY_RIGHT))
//     x += dx;
// else if(key_is_down(KEY_LEFT))
//     x -= dx;
// You can now just do:
// x+= dx*key_tri_horizontal();
// Or:
// x += dx * bit_tribool(key_hit(-1), KI_RIGHT, KI_LEFT); // increase/decrease x on a right/left hit