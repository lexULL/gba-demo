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

// Sprite and background mapping overview
//
// Mapping concerns everything about which tiles to use and where they go. As said, the screen appearence of both sprites and backgrounds are constructed of tiles, laid out side by side.
// You have to tell the GBA which tiles to blit to what position.
// Backgrounds use a tile-map which works just like an ordinary paletted bitmap except that it's a matrix of screenblock entries (with tile indices) instead of pixels (containing color-indices)
// Keep a clear distinction between the entries in the map (the screenblock entries, SE for short) and the image-data (the actual tiles).
// The term "tile" is often used for both.
// each SE has its own tile-index and it contains bits for horizontal and vertical flipping and if it's a 16-color background, an index for the palbank as well.
//
// For sprites, it's a bit different but the basic steps remain.
// You give one tile-index for the whole sprite; the GBA then figures out the other tiles to use by looking at the shape and size of the sprite and the sprite-mapping mode
// the mapping mode is either 1D or 2D, depending on REG_DISPCNT{6}.
// 1D mapping states that the tiles that a sprite should use are consecutive.
// Like backgrounds, there's additional flipping flags and palette-info for 16-color sprites.
// Unlike backgrounds, these work on the whole sprite, not just on one tile.
// Also, the component tiles of sprites are always adjoining, so you can see a sprite as a miniature tiled-background with some imagination.
//
// What belongs to the mapping step as well is the affine transformation matrix. With this 2x2 matrix you can rotate, scale or shear sprites or backgrounds.
// There seems to be a lot of confusion about how this works so I've written a detailed, mathematical description on how this thing works.
// Bottom line: the matrix maps from screen space to texture-space

// Sprite and background image data
//
// Image data is what the GBA actually uses to produce an image. This means two things, tiles and palettes
// -- Tiles --
// Sprites and backgrounds are composed of a matrix of smaller bitmaps called tiles. Your baic tile is an 8x8 bitmap.
// Tiles come in 4bpp (16 colors / 16 palettes) and 8bpp (256 colors / 1 palette) variants.
// In analogy to floating point numbers, these can be referred to as s-tiles (single-size tile) and d-tiles (double-size tiles).
// An s-tile is 32 (20h/0x20) bytes long, a d-tile 64 (40h/0x40) bytes.
// The default type of tile is the 4bpp variant (the s-tile).
//
// There is sometimes a misunderstanding about what working in tiles really means. In tiled modes, VRAM is not a big bitmap out of which tiles are selected, but
// a collection of 8x8 pixel bitmaps (i.e. the tiles). It is important that you understand the differences between these two methods.
// Consider an 8x8 rectangle in a big bitmap, and an 8x8 tile. In the big bitmap, the data after the first 8 pixels contain the next 8 pixels of the same scanline.
// The next line of the tile can be found further on. In tiled mods, the next scanline of the tile immediately follows the current line.
//
// Basically, VRAM works as an 8xN*8 bitmap in the tiled modes. Because such a small width is impractical to work with, they're usually presented as a wider bitmap anyway.
// An example is the VBA tile viewer, which displays char blocks as a 256x256
// You need a tool that can break up a bitmap into 8x8 chunks, or restructure it into a bitmap with a width of 8 pixels
//
// As with all bitmaps, it is the programmer's responsibility that the bit-depth of the tiles that sprites and backgrounds correspond to the bit-depth of the data in VRAM.
// If this is out of sync, graphical errors will occur.

// Tiled Graphics Considerations
//
// Remember and understand the following points:
// 1. The data of each tile are stored sequentially, with the next row of 8 pixels immediately following the previous row. VRAM is basically a big bitmap 8 pixels wide.
// Graphics converters should be able to convert bigger bitmaps into this format.
// 2. As always, watch your bit-depth.

// Tip for Graphics Converters
//
// If you want to make your own conversion tool,s here's a little tip that'll help you with tiles. Work in stages;
// Do not go directly from a normal, linear bitmap to writing the data-file.
// Create a tiling function that takes a bitmap and arranges the tiles into a bitmap 1 tile wide and H tiles high
// This can then be exported normally.
// If you allow for a variable tile-width (not hard-coding the 8-pixel width), you can use it for other purposes as well.
// e.g. to create 16x16 sprites, first arrange with width=16, then width=8

// -- Tile blocks (aka charblocks) --
//
// All the tiles are stored in charblocks. Tradition has it that tiles are characters (not to be confused with 8bit char) and so the critters are called charblock.
// Each charblock is 16kb (4000h bytes) long, so there's room for 512 (4000h/20h) s-tiles or 256 (4000h/40h) d-tiles. You can also consider charblocks to be matrices of tiles
// 32x16 for s-tiles, 16x16 (or 32x8) for d-tiles. The whole 96KB of VRAM can be seen as 6 charblocks.
//
// As said, there are 6 tile-blocks, that is 4 for backgrounds (0-3) and 2 for sprites (4-5).
// For tiled backgrounds, tile-counting starts at a given character base block (block for character base, CBB for short), which are indicated by REG_BGxCNT {2-3}
// Sprite tile-indexing always starts at the lower sprite block (block 4, starting at 0601:0000h)
// It'd be nice if tile-indexing followed the same scheme for backgrounds and sprites, but it doesn't. For sprites, numbering always follows s-tiles (20h offsets) even for d-tiles,
// but backgrounds stick to their indicated tile-size: 20h offsets in 4bpp mode, 40h offsets for 8bpp mode
//
// BG VS SPRITE TILE INDEXING
// Sprites always have 32 bytes between tile indices, bg tile-indexing uses 32 or 64 byte offsets depending on their set bit-depth.

// Now, both regular backgrounds and sprites have 10 bits for tile indices. That means 1024 allowed indices.
// Since each charblock contains 512 s-tiles, you can access not noly the base block, but also the one after that.
// And if your background is using d-tiles, you can actually access a total of four blocks!
// Now, since tiled backgrounds can start counting at any of the four background charblocks, you might be tempted to try to use the sprite charblocks (blocks 4 and 5) as well.
// On emulators it works but on the real GBA it doesn't, which is one of the reasons why you want to test on real hardware.

// Another thing you need to know about available charblocks is that in one of the bitmap modes, the bitmaps extend into the lower sprite block.
// For that reason, you can only use the higher sprite block (containing tiles 512 to 1023) in this case.

// Thanks to typedefs, you can define types for tiles and charblocks so that you can quickly come up with the addresses of tiles by simple array-accesses.
// An alternative to this is using macros or inline functions to calculate the right addresses. In the end it hardly matters which method you choose, though.
// Of course, the typedef method allows the use of the sizeof operator, which can be quite handy when you need to copy a certain amonut of tiles.
// Also struct-copies are faster than simple loops and require less C-code too!!

// tile 8x8@4bpp: 32 bytes, 8 ints
typedef struct { u32 data[8]; }TILE, TILE4;
// d-tile: double-sized tile (8bpp)
typedef struct { u32 data[16]; }TILE8;
// tile block: 32x16 tiles, 16x16 d-tiles
typedef TILE  CHARBLOCK[512];
typedef TILE8 CHARBLOCK8[256];

#define tile_mem  ( (CHARBLOCK*)0x06000000 )
#define tile8_mem ((CHARBLOCK8*)0x06000000 )

// Code example:
// TILE *ptr = &tile_mem[4][12]; // block 4 (== lower object block), tile 12
// Copy a tile from data to sprite-mem, tile 12
// tile_mem[4][12] = *(TILE*)spriteData;

// -- Palettes and tile colors --
//
// Sprite and backgrounds have separate palettes. The background palette goes first at 0500:0000h, immediately followed by the sprite palette (0500:0200h).
// Both palettes contain 256 entries of 15-bit colors.
// In 8-bit color mode, the pixel value in the tiles is palette-index for that pixel.
// In 4-bit color mode, the pixel value contains the lower nibble of the palette index. The high nibble is the palbank index, which can be found in either the sprite's attributes
// or the upper nibble of the tiles. If the pixel value is 0, then that pixel won't be rendered (i.e. will be transparent)

// Because of 16-color mode and the transparency issue, it is essential that your bitmap editor leaves the palette intact.


// Subject	Backgrounds	Sprites
// Number	4 (2 affine)	128 (32 affine)
// Max size	reg: 512x512
// aff: 1024x1024	64x64
// Control	REG_BGxCNT	OAM
// Base tile block	0-3	4
// Available tiles ids	reg: 0-1023
// aff: 0-255	modes 0-2: 0-1023
// modes 3-5: 512-1023
// Tile memory offsets	Per tile size:
// 4bpp: start= base + id*32
// 8bpp: start= base + id*64	Always per 4bpp tile size:
// start= base + id*32
// Mapping	reg: the full map is divided into map-blocks of 32×32 tegels. (banked map)
// aff: one matrix of tegels, just like a normal bitmap (flat map)	If a sprite is m × n tiles in size:
// 1D mapping: the m*n successive tiles are used, starting at id
// 2D mapping: tile-blocks are 32×32 matrices; the used tiles are the n columns of the m rows of the matrix, starting at id.
// Flipping	Each tile can be flipped individually	Flips the whole sprite
// Palette	0500:0000h	0500:0200h