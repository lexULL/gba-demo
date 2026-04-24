#include "video.h"

u16* VRAM = (u16*)MEM_VRAM;

void m3_plot(int x, int y, Color color) {
    VRAM[y*SCRWIDTH+x] = color;
}