#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include "video.h"
#include "input.h"

u32 current_frame = 0u;

// Using the V-Blank as a timing mechanism is called V-Sync (Vertical Synchronisation)
// Two common methods are: use a while loop and check REG_VCOUNT or REG_DISPSTAT
// Since VBlank starts at scanline 160, you could see when REG_VCOUNT goes beyond this value.
//
// Waiting for VBlank isn't enough, you may still be in VBlank when you call vid_sync() again, which will be blazed through immediately
// To do this, you have to wait until the next VDraw
void VBlank() {
    ++current_frame;
    // GREEN_SWAP = !GREEN_SWAP;
}

void VCount() {
}

void Present() {

}

int main(void) {
    irqInit();
    irqEnable( IRQ_VBLANK /*| IRQ_HBLANK*/ );
    irqSet( IRQ_VBLANK, VBlank );
    // irqSet( IRQ_HBLANK, VCount );

    SetMode( MODE_3 | BG2_ENABLE );

    s32 ii = 0, jj = 0;

    m3_fill(RGB15(12, 12, 14));

    Color clr;
    u32 btn;

    // Lines in top right frame
    // for(ii=0; ii<=8; ii++)
    // {
    //     jj= 3*ii+7;
    //     m3_line(132+11*ii, 9, 226, 12+7*ii, RGB15(jj, 0, jj));
    //     m3_line(226-11*ii,70, 133, 69-7*ii, RGB15(jj, 0, jj));
    // }

    // // Lines in bottom left frame
    // for(ii=0; ii<=8; ii++)
    // {
    //     jj= 3*ii+7;
    //     m3_line(15+11*ii, 88, 104-11*ii, 150, RGB15(0, jj, jj));
    // }

    while( 1 ) {
        VBlankIntrWait(); // v-sync here

        // if((current_frame & 7) == 0u) // this makes the key polling happen every 8 frame so it looks slower (to fully demonstrate the input stuff)
        key_poll();

        for(ii=0; ii<KI_MAX; ++ii) {
            clr = 0u;
            btn = 1 << ii;
            if(key_hit(btn)) clr = CLR_RED;
            else if(key_released(btn)) clr = CLR_YELLOW;
            else if(key_held(btn)) clr = CLR_LIME;
            else clr = CLR_UP;
        }

        // void m3_line(s32 x1, s32 y1, s32 x2, s32 y2, Color clr);
        m3_line(0,0,100,100, clr);
        m3_line(0,100,100,100, clr);

        // what happens behind the scenes:
        // while(REG_VCOUNT >= 160); // wait till VDraw
        // while(REG_VCOUNT < 160); // wait till VBlank
    }
}