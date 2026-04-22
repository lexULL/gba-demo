#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include "common.h"


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