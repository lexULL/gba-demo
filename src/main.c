#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

u32 current_frame = 0u;

void VBlank() {
    ++current_frame;
}

void VCount() {

}

void Present() {

}

int main(void) {
    irqInit();
    irqEnable( IRQ_VBLANK | IRQ_HBLANK );
    irqSet( IRQ_VBLANK, VBlank );
    irqSet( IRQ_HBLANK, VCount );
    SetMode( MODE_4 );
    while( 1 ) {
        VBlankIntrWait();
    }
}