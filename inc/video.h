#include "common.h"
#include "memory.h"

#define M3_SIZE SCRWIDTH * SCRHEIGHT * 2

// the entire GBA screen is refreshed every 60th of a second
// After a scanline has been drawn (HDraw period, 240 px) there is a pause (HBlank, 68 px) before it starts drawing the next scanline
// After 160 scanlines (VDraw), is a 68 px scanline (VBlank) before it starts over again
//
// @IMPORTANT: To avoid tearing, positional data is usually updated at the VBlank!!! (which is why most games run at 60 or 30 fps)
// (syncing at the VBlank is also why in PAL countries often had slower games, since PAL TVs run at 50 Hz instead of 60 Hz => 17% slower)

// Timing Details Table
//
// Subject       Length        Cycles
//                                  
// Pixel         1             4
// HDraw         240px         960
// HBlank        68px          272
// scanline      HDraw+HBl     1232
// VDraw         160*scanline  197120
// VBlank        68*scanline   83776
// Refresh       VDraw+VBl     280896

// Video Details
//
// Display     - 240x160 px (2.9 inch TFT color LCD display)
// BG layers   - 4 background layers
// BG types    - Tile/map based, or Bitmap based
// OBJ colors  - 256 colors, or 16 colors/16 palettes
// OBJ size    - 12 types (in range 8x8 up to 64x64 dots)
// OBJs/Screen - max. 128 OBJs of any size (up to 64x64 dots each)
// OBJs/Line   - max 128 OBJs of 8x8 dots size (under best circumstances)
// Priorities  - OBJ/OBJ: 0-127, OBJ/BG: 0-3, BG/BG: 0-3
// Effects     - Rotation/Scaling, alpha blending, fade-in/out, mosaic, window
// Backlight   - GBA SP only (optionally by light on/off toggle button)

// Summary:
// 240x160 LCD screen, capable of displaying 32768 (15-bit) colors.
// Refresh rate is just shy of 60 fps (59.73 Hz)
// 5 independent graphics layers: 4 backgrounds and one sprite layer
// capable of doing special effects that include blending two layers, mosaic, rotation and scaling

// Draw and blank periods
//
// GBA screen is refreshed every 60th of a second.
// After a scanline has been drawn (HDraw period, 240 pixels), there is a pause (HBlank, 68 pixels) before it starts drawing the next scanline.
// After 160 scanlines (VDraw period), is a 68 scanline blank (VBlank) before it starts over again.
// To avoid tearing, positional data is usually updated at the VBlank.
// A fullscreen refresh takes 280896 cycles, divided by the clock speed gives a framerate of 59.73.
// From the Draw/Blank periods given, there are 4 cycles per pixel and 1232 cycles per scanline.

// Colors and palettes
//
// The GBA is capable of displaying 16-bit colors in a 5.5.5 format, which means 5 bits for red, 5 bits for green and 5 bits for blue.
// The leftover bit is unused.
// Bit pattern looks like: xbbbbbgggggrrrrr (x - unused)
//
// The GBA has two palettes, one for sprites (objects) and one for backgrounds. Both palettes contain 256 entries of 16-bit colors (512 bytes, each).
// The background palette starts at 0x05000000, immediately followed by the sprite palette at 0x05000200.
// Sprites and backgrounds can use these palettes in two ways: a single palette with 256 colors (8bpp) or as 16 sub-palettes or palette banks of 16 colors (4bpp)
// In palettes, index 0 is the transparency index and in paletted modes pixels with a value of 0 will be transparent.

// Bitmaps, backgrounds and sprites
//
// GBA knows 3 types of graphics representation: bitmaps, tiled backgrounds and sprites (objects).
// The bitmap and the tiled background (a.k.a. background) types affect how the whole screen is built up and as such cannot both be activated at the same time.
// In bitmap mode, video memory works just like a width*height bitmap. To plot a pixel at location (x,y) go to location y*width+x and fill in the color.
// Note: You cannot build a screen-ful of individual pixels each frame on the GBA, there are simply too many of them.
//
// Tiled backgrounds work first by storing 8x8 pixel tiles in one part of video memory, then in another part you build up a tile-map,
// which contains indices that tells the GBA which tiles go into the image you see on the screen.
// To build a screen you'd only need a 30x20 (30 * 8 x 20 x 8 = 240x160) map of numbers and the hardware takes care of drawing the tiles that these numbers point to.
// This way, you can update an entire screen each frame. There are very few games that do not rely on this graphics type.

// Sprites are small (8x8 to 64x64 px) graphical objects that can be transformed independently from each other and can be used in conjunction with either bitmap or tilemap background types.

// The tiles of tiled backgrounds and sprites have the same memory layout (namely, in groups of 8x8 pixel tiles).
// This makes tiled backgrounds and sprites the tiled-types.

// pixel lookup formula: width * y + x

// pitch = number of bytes in a scanline (8bpp pitch = width, 16bpp pitch = 2 * width, 32bpp = 4 * width)

// mode 3, 240x160 16bpp with no page-flipping
// mode 4, 240x160 8 bpp with    page-flipping
// mode 5, 160x128 16bpp with    page-flipping

// size that bitmap requires is width * height * bpp/8
extern u16 *_VRAM;
extern u16 *vid_page;

// Mode 3 pixel plotting
static inline void m3_plot(int x, int y, Color color) {
    _VRAM[y*SCRWIDTH+x] = color;
}

// Mode 3 line drawing
void m3_line(s32 x1, s32 y1, s32 x2, s32 y2, Color clr);

void m3_fill(Color clr);

// 16bpp bresenham for line drawing
void bmp16_line(s32 x1, s32 y1, s32 x2, s32 y2, Color clr, void *dstBase, u32 dstPitch);

// 16bpp hollow rectangle (frame) drawing
void bmp16_frame(s32 left, s32 top, s32 right, s32 bottom, Color clr, void *dstBase, u32 dstPitch);

u16 *vid_flip();

// Mode 4 safe pixel plotting
// You cannot write individual bytes into VRAM (or the paleette or OAM for that matter). Half-words or words only.
// If you want to write single bytes, you have to read the full (half)word, insert the byte and put it back.
static inline void m4_plot(s32 x, s32 y, u8 clr) {
    u16 *dst = &vid_page[( y*SCRWIDTH + x ) / 2]; // divison by 2 due to u8/u16 pointer mismatch
    // x & 1 just checks if x is odd (basically x % 2 == 1)
    if (x & 1) { // each u16 in VRAM memory holds two 8bpp pixels side by side (bits 15-8 = odd pixel, bits 7-0 = even pixel)
        *dst = ( *dst & 0xFF ) | ( clr << 8 ); // odd pixel = keep low byte (even neighbour), put clr in high byte
    } else {
        *dst = ( *dst & ~0xFF ) | clr; // even pixel = keep high byte (odd neighbour), put clr in low byte
    }
}

// 8bpp bresenham for line drawing, also uses u8 for color ofc
void bmp8_line(s32 x1, s32 y1, s32 x2, s32 y2, u8 clr);