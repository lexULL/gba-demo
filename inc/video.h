#include "common.h"
#include "memory.h"

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
