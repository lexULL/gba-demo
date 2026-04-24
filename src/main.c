#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include "video.h"

u32 current_frame = 0u;

void VBlank() {
    ++current_frame;
    GREEN_SWAP = !GREEN_SWAP;
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

    SetMode( MODE_3 | BG2_ENABLE );

    for(s16 i = 10; i <= 32; ++i) {
        for(s16 j = 10; j <= 32; ++j) {
            if(i % 2 == 0 || j % 2 == 0) m3_plot(i, j, (u16)CLR_WHITE);
            else m3_plot(i, j, (u16)CLR_RED);
        }
    }

    while( 1 ) {
        VBlankIntrWait();
    }
}