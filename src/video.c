#include "video.h"
#include <gba_video.h>

u16 *_VRAM = (u16*)MEM_VRAM;
u16 *vid_page = vid_mem;

void m3_line(s32 x1, s32 y1, s32 x2, s32 y2, Color clr) {
    bmp16_line(x1, y1, x2, y2, clr, vid_mem, SCRWIDTH*2);
}

void m3_fill(Color clr) {
    s32 ii = 0;
    u32 *dst = (u32*)vid_mem;
    u32 wd = (clr << 16) | clr;

    for(ii = 0; ii < M3_SIZE / 4; ++ii) {
        *dst++ = wd;
    }
}

void bmp16_line(s32 x1, s32 y1, s32 x2, s32 y2, Color clr, void *dstBase, u32 dstPitch) {
    s32 ii = 0, dx = 0, dy = 0, xstep = 0, ystep = 0, dd = 0;
    // y1 * dstPitch skips to the correct row, x1 * 2 skips to the correct column (2 = 2 bytes per pixel = 16bpp)
    u16 *dst = (u16*)(dstBase + y1 * dstPitch + x1 * 2);

    // Convert pitch from bytes to pixels so pointer arithmetic works with u16*
    dstPitch /= 2;

    // Normalization
    // xstep is +1 or -1 pixels, ystep is +pitch or -pitch (to move one row up/down)
    if(x1 > x2) {
        xstep = -1;
        dx = x1 - x2;
    } else {
        xstep = +1;
        dx = x2 - x1;
    }

    if(y1>y2) {
        ystep = -dstPitch;
        dy = y1 - y2;
    } else {
        ystep = +dstPitch;
        dy = y2 - y1;
    }

    // Drawing
    if(dy == 0) { // horizontal = walk along x
        for(ii = 0; ii <= dx; ++ii) {
            dst[ii*xstep] = clr;
        }
    } else if(dx == 0) { // vertical = jump by one row at a time using ystep
        for(ii = 0; ii <= dy; ++ii) {
            dst[ii*ystep] = clr;
        }
    } else if(dx >= dy) { // diagonal, slope <= 1: x is the driving axis
        // bresenham algo from here
        // dd is the error accumulator, tracks how far the true line has drifted from the current pixel in the minor axis (y)
        dd = 2 * dy - dx; // initial error

        for(ii = 0; ii <= dx; ++ii) {
            *dst = clr;
            if(dd >= 0) { // error has accumulated enough, step in y and correct the error down
                dd -= 2*dx;
                dst += ystep;
            }

            dd += 2 * dy; // accumulate error by the slope each x step
            dst += xstep;
        }
    } else { // diagonal, slope > 1 = y is the driving axis so same logic but inverted vars
        dd = 2 * dx - dy;

        for(ii = 0; ii <= dy; ++ii) {
            *dst = clr;
            if(dd >= 0) {
                dd -= 2*dy;
                dst += xstep;
            }

            dd += 2 * dx;
            dst += ystep;
        }
    }
}

void bmp16_frame(s32 left, s32 top, s32 right, s32 bottom, Color clr, void *dstBase, u32 dstPitch) {
    // right/bottom are exclusive, so decrement first to get the last included pixel
    right--;
    bottom--;

    bmp16_line(left, top,    right, top,    clr, dstBase, dstPitch); // top edge
    bmp16_line(left, bottom, right, bottom, clr, dstBase, dstPitch); // bottom edge
    bmp16_line(left, top,    left,  bottom, clr, dstBase, dstPitch); // left edge
    bmp16_line(left, top,    right, bottom, clr, dstBase, dstPitch); // right edge
}

u16 *vid_flip() {
    vid_page = (u16*)((u32)vid_page ^ 0x10000); // toggle between 0x06000000 and 0x06010000
    REG_DISPCNT ^= DCNT_PAGE;                   // toggle display to show the other page
    return vid_page;
}

void bmp8_line(s32 x1, s32 y1, s32 x2, s32 y2, u8 clr) {
    s32 ii = 0, dx = 0, dy = 0, xstep = 0, ystep = 0, dd = 0;
    s32 x = x1, y = y1;

    // Normalization
    // xstep is +1 or -1 pixels, ystep is +1 or -1
    if(x1 > x2) {
        xstep = -1;
        dx = x1 - x2;
    } else {
        xstep = +1;
        dx = x2 - x1;
    }

    if(y1>y2) {
        ystep = -1;
        dy = y1 - y2;
    } else {
        ystep = +1;
        dy = y2 - y1;
    }

    // Drawing
    if(dy == 0) { // horizontal = walk along x
        for(ii = 0; ii <= dx; ++ii) {
            m4_plot(x + ii*xstep, y, clr);
        }
    } else if(dx == 0) { // vertical = jump by one row at a time using ystep
        for(ii = 0; ii <= dy; ++ii) {
            m4_plot(x, y + ii*ystep, clr);
        }
    } else if(dx >= dy) { // diagonal, slope <= 1: x is the driving axis
        // bresenham algo from here
        // dd is the error accumulator, tracks how far the true line has drifted from the current pixel in the minor axis (y)
        dd = 2 * dy - dx; // initial error

        for(ii = 0; ii <= dx; ++ii) {
            m4_plot(x, y, clr);
            if(dd >= 0) { // error has accumulated enough, step in y and correct the error down
                dd -= 2*dx;
                y += ystep;
            }

            dd += 2 * dy; // accumulate error by the slope each x step
            x += xstep;
        }
    } else { // diagonal, slope > 1 = y is the driving axis so same logic but inverted vars
        dd = 2 * dx - dy;

        for(ii = 0; ii <= dy; ++ii) {
            m4_plot(x, y, clr);
            if(dd >= 0) {
                dd -= 2*dy;
                x += xstep;
            }

            dd += 2 * dx;
            y += ystep;
        }
    }
}