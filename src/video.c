#include "video.h"

void m3_plot(int x, int y, Color color) {
    MEM_VRAM[y*SCRWIDTH+x] = color;
}