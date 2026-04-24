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
#define MEM_IO   0x04000000 // 16-bit register
// MEM_IO Specs
//
// Bit 0-2   - BG mode (video modes)
// Bit 3     - can only be set by BIOS opcodes for GBA or CGB mode
// Bit 4     - Display Frame Select (0-1 = Frame 0-1) (for video modes 4 and 5 only)
// Bit 5     - H-Blank Interval Free (1 = Allow access to OAM during H-Blank)
// Bit 6     - OBJ/Sprite Character VRAM Mapping (0 = Two Dimensional, 1 = One Dimensional)
// Bit 7     - Forced Blank (1 = Allow FAST access to VRAM, Palette, OAM)
// Bit 8-11  - Screen Display from BG 0 to BG 3 (0 = Off, 1 = On)
// Bit 12    - Screen Display OBJ/Sprite (0 = Off, 1 = On)
// Bit 13-14 - Window 0-1 Display Flag   (0 = Off, 1 = On)
// Bit 15    - OBJ Window Display Flag   (0 = Off, 1 = On)
//
//
// BG/Video Mode Capability Table
// 
// Mode  Rot/Scal  Layers  Size                Tiles  Colors        Features
// 0     No        0123    256x256..512x515    1024   16/16..256/1  SFMABP
// 1     Mixed     012-    (BG0,BG1 as Above   Mode 0, BG2 as below Mode 2)
// 2     Yes       --23    128x128..1024x1024  256    256/1         S-MABP
// 3     Yes       --2-    240x160             1      32768         --MABP
// 4     Yes       --2-    240x160             2      256/1         --MABP
// 5     Yes       --2-    160x128             2      32768         --MABP
//
// S - Scrolling, F - Flip, M - Mosaic, A - AlphaBlending, B - Brightness, P - Priority
//
//
// Blanking Bits
// 
// For MEM_IO, Setting Forced Blank (Bit 7) causes the video controller to display white lines, and all VRAM, Palette RAM and OAM may be accessed
// "When the internal HV synchronous counter cancels a forced blank during a display period, the display begins from the beginning, following the display of two vertical lines"
// Setting H-Blank Interval Free (Bit 5) allows to access OAM during H-Blank time - using this feature reduces the number of sprites that can be displayed per line.
//
//
// Display Enable Bits
//
// By default, BG0-3 and OBJ Display Flags (Bit 8-12) are used to enable/disable BGs and OBJ. When enabling Window 0 and/or Window 1 (Bit 13-14),
// color special effects may be used, and BG0-3 and OBJ are controlled by the window(s).
//
//
// Frame Selection
//
// In BG Modes 4 and 5 (Bitmap Modes), either one of the two bitmaps/frames may be displayed (Bit 4), allowing the user to update the other (invisible) frame in background.
// ^ I think you can use this for 3D? for framebuffers and stuff
// In BG Mode 3, only one frame exists.
// In BG Modes 0-2 (Tile/Map based modes), a similar effect may be gained by altering the base address(es) of BG Map and/or BG Charater data.


#define MEM_PAL  0x05000000 // palette RAM
#define MEM_VRAM 0x06000000 // 16-bit register
#define MEM_OAM  0x07000000 // object attribute memory (sprites)

// ################ I/O registers for graphical control ################
//
// display control register - primary control of the screen (General LCD Status - Read/Write)
// Display status and interrupt control. The H-Blank conditions are generated once per scanline, including for the "hidden" scanlines during V-Blank.
#define REG_DISPCNT  *((volatile u16*)(MEM_IO+0x0000))
// Bit  Expl.
// 0    V-Blank flag               (1=VBlank) (set in line 160..226; not 227) (Read-Only)
// 1    H-Blank flag               (1=HBlank) (toggled in all lines, 0..227)  (Read-Only)
// 2    V-Counter flag             (1=Match)  (set in selected line)          (Read-Only)
// 3    V-Blank IRQ enable         (1=Enable)                                 (Read/Write)
// 4    H-Blank IRQ enable         (1=Enable)                                 (Read/Write)
// 5    V-Counter IRQ enable       (1=Enable)                                 (Read/Write)
// 6    Used on the DSi for LCD Initialization Ready (0=Busy, 1=Ready),       (Read-Only)
// 7    Used on the NDS for MSB of V-VCount Setting (LYC.Bit8) (0..262)       (Read/Write)
// 8-15 V-Count Setting (LYC) (0..227)                                        (Read/Write)
//
// Note: IRQ means Interrupt Request, and it's a signal that tells the CPU "stop what you're doing and handle this event now"
//
// The V-Count-Setting value is much the same as the LYC (Line Y Compare) of older gameboys, when its value is identical to the content of the VCOUNT register then the V-Counter flag is set (Bit 2)
// and (if enabled in Bit 5) an interrupt is requested.
// Although the drawing time is only 960 cycles (240*4), the H-Blank flag is "0" for a total of 1006 cycles.
// MSB = Most Significant Bit

// display status register
#define REG_DISPSTAT *((volatile u16*)(MEM_IO+0x0004))
// scanline counter register (Vertical Counter) (Read-Only)
// Indicates the currently drawn scanline, values in range from 160..227 indicate "hidden" scanlines within the VBlank area.
#define REG_VCOUNT   *((volatile u16*)(MEM_IO+0x0006))
// Bit  Expl.
// 0-7  Current Scanline                                           (0..227) (Read-Only)
// 8    Only used on the NDS for MSB of Current Scanline (LY.Bit8) (0..262) (Read-Only)
// 9-15 Not Used (0)
//
// Note: This is much the same than the "LY" register of older gameboys.

// What is a window?
//
// A window is a rectangular region you define on screen that controls where certain layers are visible.
// You define up to 2 rectangular windows (WIN0, WIN1) by setting their left/right/top/bottom coordinates in other IO registers.
// Then you configure which backgrounds and sprites are visible **inside** vs **outside** each window.
// This is used for HUD/status bars, spotlight effects or transition wipes
// The separate registers are WININ, WINOUT
#define REG_WININ   (*(volatile u16*)0x04000048)
#define REG_WINOUT  (*(volatile u16*)0x0400004A)
#define REG_WIN0H   (*(volatile u16*)0x04000040)
#define REG_WIN1H   (*(volatile u16*)0x04000042)
#define REG_WIN0V   (*(volatile u16*)0x04000044)
#define REG_WIN1V   (*(volatile u16*)0x04000046)

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
// DISPCNT flags
#define DCNT_MODE0   0x0000
#define DCNT_MODE1   0x0001
#define DCNT_MODE2   0x0002
#define DCNT_MODE3   0x0003
#define DCNT_MODE4   0x0004
#define DCNT_MODE5   0x0005
#define DCNT_FB      (1<<4)   // frame select
#define DCNT_OAM_HBL (1<<5)  // OAM access during H-Blank
#define DCNT_OBJ_1D  (1<<6)  // 1D sprite mapping
#define DCNT_BLANK   (1<<7)  // forced blank
#define DCNT_BG0     (1<<8)
#define DCNT_BG1     (1<<9)
#define DCNT_BG2     (1<<10)
#define DCNT_BG3     (1<<11)
#define DCNT_OBJ     (1<<12)
#define DCNT_WIN0    (1<<13)
#define DCNT_WIN1    (1<<14)
#define DCNT_WINOBJ  (1<<15)

// 4000002h - Undocumented - Green Swap (R/W)
// Normally, red green blue intensities for a group of two pixels is output as BGRbgr (uppercase for left pixel at even xloc, lowercase for right pixel at odd xloc). When the Green Swap bit is set, each pixel group is output as BgRbGr (ie. green intensity of each two pixels exchanged).
//   Bit   Expl.
//   0     Green Swap  (0=Normal, 1=Swap)
//   1-15  Not used
// This feature appears to be applied to the final picture (ie. after mixing the separate BG and OBJ layers). Eventually intended for other display types (with other pin-outs). With normal GBA hardware it is just producing an interesting dirt effect.
// The NDS DISPCNT registers are 32bit (4000000h..4000003h), so Green Swap doesn't exist in NDS mode, however, the NDS does support Green Swap in GBA mode.
#define GREEN_SWAP (*(volatile u16*)0x04000002) // <-- free chromatic abberration by setting this to 1 lol

// backgrounds
#define REG_BG0CNT  *((volatile u16*)(MEM_IO+0x0008))
#define REG_BG1CNT  *((volatile u16*)(MEM_IO+0x000A))
#define REG_BG2CNT  *((volatile u16*)(MEM_IO+0x000C))
#define REG_BG3CNT  *((volatile u16*)(MEM_IO+0x000E))

// background scroll
#define REG_BG0HOFS *((volatile u16*)(MEM_IO+0x0010))
#define REG_BG0VOFS *((volatile u16*)(MEM_IO+0x0012))
#define REG_BG1HOFS *((volatile u16*)(MEM_IO+0x0014))
#define REG_BG1VOFS *((volatile u16*)(MEM_IO+0x0016))
#define REG_BG2HOFS *((volatile u16*)(MEM_IO+0x0018))
#define REG_BG2VOFS *((volatile u16*)(MEM_IO+0x001A))
#define REG_BG3HOFS *((volatile u16*)(MEM_IO+0x001C))
#define REG_BG3VOFS *((volatile u16*)(MEM_IO+0x001E))

// keypad input
#define REG_KEYINPUT *((volatile u16*)(MEM_IO+0x0130))

// interrupts
#define REG_IE      *((volatile u16*)(MEM_IO+0x0200))  // interrupt enable
#define REG_IF      *((volatile u16*)(MEM_IO+0x0202))  // interrupt flags
#define REG_IME     *((volatile u16*)(MEM_IO+0x0208))  // interrupt master enable

// interrupt flags
#define IRQ_VBLANK   (1<<0)
#define IRQ_HBLANK   (1<<1)
#define IRQ_VCOUNT   (1<<2)

// mode 3/4/5 framebuffer
#define VRAM_FRONT  ((volatile u16*)MEM_VRAM)
#define VRAM_BACK   ((volatile u16*)(MEM_VRAM + 0xA000))  // mode 4/5 only