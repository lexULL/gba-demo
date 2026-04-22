#include "common.h"

// CPU Modes Details
//
// ARM   Mode - ARM7TDMI 32-bit RISC CPU, 16.78 MHz, 32-bit opcodes (GBA)
// THUMB Mode - ARM7TDMI 32-bit RISC CPU, 16.78 MHz, 16-bit opcodes (GBA)
// CGB   Mode - Z80/8080-style 8-bit CPU, 4.2 MHz or 8.4 MHz (CGB compatibility)
// DMG   Mode - Z80/8080-style 8-bit CPU, 4.2 MHz (monochrome Game Boy compatibility)

// Internal Memory Details
//
// BIOS ROM    - 16  KB
// Work RAM    - 288 KB (Fast 32K on-chip, plus Slow 256K on-board)
// VRAM        - 96  KB
// OAM         - 1   KB (128 OBJs 3x16-bit, 32 OBJ-Rotation/Scalings 4x16-bit)
// Palette RAM - 1   KB (256 BG colors, 256 OBJ colors)

// GBA's video system has 96KB of video memory available besides a multitude of registers in I/O memory
#define MEM_IO   0x04000000
#define MEM_VRAM 0x06000000

// ################ I/O registers for graphical control ################
//
// display control register - primary control of the screen
#define REG_DISPCNT  *((volatile u32*)(MEM_IO+0x0000))
// display status register
#define REG_DISPSTAT *((volatile u32*)(MEM_IO+0x0004))
// scanline counter register
#define REG_VCOUNT   *((volatile u32*)(MEM_IO+0x0006))

#define DCNT_MODE0 0x0000
#define DCNT_MODE1 0x0001
#define DCNT_MODE2 0x0002
#define DCNT_MODE3 0x0003
#define DCNT_MODE4 0x0004
#define DCNT_MODE5 0x0005

// Layers
// GBA has 5 independent layers that can contain graphics: 
// - 4 background layers (DCNT_BG0 to DCNT_BG3)
// - 1 sprite layer
//
// The layering is capable of doing special effects that include:
// - blending two layers
// - mosaic
// - rotation
// - scaling
#define DCNT_BG0 0x0100
#define DCNT_BG1 0x0200
#define DCNT_BG2 0x0400
#define DCNT_BG3 0x0800
#define DCNT_OBJ 0x1000

