#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef u16      Color;

// GBA has a 240x160 LCD screen, capable of displaying 32768 (15 bit) colors
// Refresh rate is 59.73Hz
#define SCRWIDTH 240
#define SCRHEIGHT 160
#define GBA_WIDTH SCRWIDTH
#define GBA_HEIGHT SCRHEIGHT

#define CLR_BLACK   0x0000
#define CLR_RED     0x001F
#define CLR_LIME    0x03E0
#define CLR_YELLOW  0x03FF
#define CLR_BLUE    0x7C00
#define CLR_MAG     0x7C1F
#define CLR_CYAN    0x7FE0
#define CLR_WHITE   0x7FFF

