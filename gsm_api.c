/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013 doctorxyz
# Copyright 2022 Sestain
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
*/

//#include <kernel.h>

#define make_display_magic_number(dh, dw, magv, magh, dy, dx) \
        (((u64)(dh)<<44) | ((u64)(dw)<<32) | ((u64)(magv)<<27) | \
         ((u64)(magh)<<23) | ((u64)(dy)<<12)   | ((u64)(dx)<<0)     )

#define MAKE_J(func)		(u32)( (0x02 << 26) | (((u32)func) / 4) )	// Jump (MIPS instruction)

// GS Registers
#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

// VMODE TYPES
#define PS1_VMODE	1
#define HDTV_VMODE	3

/// Non-Interlaced Mode
#define GS_NONINTERLACED 0x00
/// Interlaced Mode
#define GS_INTERLACED 0x01

/// Field Mode - Reads Every Other Line
#define GS_FIELD 0x00
/// Frame Mode - Reads Every Line
#define GS_FRAME 0x01

/// DTV 480 Progressive Scan (720x480)
#define GS_MODE_DTV_480P  0x50
/// DTV 1080 Interlaced (1920x1080)
#define GS_MODE_DTV_1080I 0x51
/// DTV 720 Progressive Scan (1280x720)
#define GS_MODE_DTV_720P  0x52
/// DTV 576 Progressive Scan (720x576)
#define GS_MODE_DTV_576P  0x53
/// DTV 1080 Progressive Scan (1920x1080)
#define GS_MODE_DTV_1080P  0x54

// Prototypes for External Functions
#define _GSM_ENGINE_ __attribute__((section(".gsm_engine")))		// Resident section

extern void *Old_SetGsCrt _GSM_ENGINE_;

extern u32 Target_INTERLACE _GSM_ENGINE_;
extern u32 Target_MODE _GSM_ENGINE_;
extern u32 Target_FFMD _GSM_ENGINE_;

extern u64 Target_SMODE2 _GSM_ENGINE_;
extern u64 Target_DISPLAY1 _GSM_ENGINE_;
extern u64 Target_DISPLAY2 _GSM_ENGINE_;
extern u64 Target_SYNCV _GSM_ENGINE_;

extern u8 automatic_adaptation _GSM_ENGINE_;
extern u8 DISPLAY_fix _GSM_ENGINE_;
extern u8 SMODE2_fix _GSM_ENGINE_;
extern u8 SYNCV_fix _GSM_ENGINE_;
extern u8 skip_videos_fix _GSM_ENGINE_;

extern u32 X_offset _GSM_ENGINE_;
extern u32 Y_offset _GSM_ENGINE_;

extern void Hook_SetGsCrt() _GSM_ENGINE_;
extern void GSHandler() _GSM_ENGINE_;

typedef struct predef_vmode_struct {
	u8	category;
	char desc[34];
	u8	interlace;
	u8	mode;
	u8	ffmd;
	u64	display;
	u64	syncv;
} predef_vmode_struct;

// Pre-defined vmodes 
// Some of following vmodes gives BSOD and/or freezing, depending on the console BIOS version, TV/Monitor set, PS2 cable (composite, component, VGA, ...)
// Therefore there are many variables involved here that can lead us to success or fail depending on the circumstances above mentioned.
//
//	category	description								interlace			mode			 	ffmd	   	display							dh		dw		magv	magh	dy		dx		syncv
//	--------	-----------								---------			----			 	----		----------------------------	--		--		----	----	--		--		-----
static const predef_vmode_struct predef_vmode[8] = {
	{  PS1_VMODE, "PS1 NTSC (HDTV 480p @60Hz)     ",	GS_NONINTERLACED,	GS_MODE_DTV_480P,	GS_FRAME,	(u64)make_display_magic_number(	 255,	2559,	0,		1,		 12,	736),	0x00C78C0001E00006},
	{  PS1_VMODE, "PS1 PAL (HDTV 576p @50Hz)      ",	GS_NONINTERLACED,	GS_MODE_DTV_576P,	GS_FRAME,	(u64)make_display_magic_number(	 255,	2559,	0,		1,		 23,	756),	0x00A9000002700005},
	{  HDTV_VMODE,"HDTV 480p @60Hz                ",	GS_NONINTERLACED,	GS_MODE_DTV_480P,	GS_FRAME, 	(u64)make_display_magic_number(	 479,	1279,	0,		1,		 51,	308),	0x00C78C0001E00006},
	{  HDTV_VMODE,"HDTV 576p @50Hz                ",	GS_NONINTERLACED,	GS_MODE_DTV_576P,	GS_FRAME,	(u64)make_display_magic_number(	 575,	1279,	0,		1,		 64,	320),	0x00A9000002700005},
	{  HDTV_VMODE,"HDTV 720p @60Hz                ",	GS_NONINTERLACED,	GS_MODE_DTV_720P,	GS_FRAME, 	(u64)make_display_magic_number(	 719,	1279,	1,		1,		 24,	302),	0x00AB400001400005},
	{  HDTV_VMODE,"HDTV 1080i @60Hz               ",	GS_INTERLACED,		GS_MODE_DTV_1080I,	GS_FIELD, 	(u64)make_display_magic_number(	1079,	1919,	1,		2,		 48,	238),	0x0150E00201C00005},
	{  HDTV_VMODE,"HDTV 1080i @60Hz Non Interlaced",	GS_INTERLACED,		GS_MODE_DTV_1080I,	GS_FRAME, 	(u64)make_display_magic_number(	1079,	1919,	0,		2,		 48,	238),	0x0150E00201C00005},
	{  HDTV_VMODE,"HDTV 1080p @60Hz               ",	GS_NONINTERLACED,	GS_MODE_DTV_1080P,	GS_FRAME, 	(u64)make_display_magic_number(	1079,	1919,	1,		2,		 48,	238),	0x0150E00201C00005},
}; //ends predef_vmode definition

u32 predef_vmode_size = 	sizeof( predef_vmode ) / sizeof( predef_vmode[0] );

#define DI2	DIntr
#define EI2	EIntr

_GSM_ENGINE_ int ee_kmode_enter2() {
	u32 status, mask;

	__asm__ volatile (
		".set\tpush\n\t"		\
		".set\tnoreorder\n\t"		\
		"mfc0\t%0, $12\n\t"		\
		"li\t%1, 0xffffffe7\n\t"	\
		"and\t%0, %1\n\t"		\
		"mtc0\t%0, $12\n\t"		\
		"sync.p\n\t"
		".set\tpop\n\t" : "=r" (status), "=r" (mask));

	return status;
}

_GSM_ENGINE_ int ee_kmode_exit2() {
	int status;

	__asm__ volatile (
		".set\tpush\n\t"		\
		".set\tnoreorder\n\t"		\
		"mfc0\t%0, $12\n\t"		\
		"ori\t%0, 0x10\n\t"		\
		"mtc0\t%0, $12\n\t"		\
		"sync.p\n\t" \
		".set\tpop\n\t" : "=r" (status));

	return status;
}

_GSM_ENGINE_ void SetSyscall2(int number, void (*functionPtr)(void)) {
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $3, 0x74\n"
    "add $4, $0, %0    \n"   // Specify the argument #1
    "add $5, $0, %1    \n"   // Specify the argument #2
   	"syscall\n"
	"jr $31\n"
	"nop\n"
	".set at\n"
	".set reorder\n"
    :
    : "r"( number ), "r"( functionPtr )
    );
}

_GSM_ENGINE_ u32* GetROMSyscallVectorTableAddress(void) {
	//Search for Syscall Table in ROM
	u32 i;
	u32 startaddr;
	u32* ptr;
	u32* addr;
	startaddr = 0;
	for (i = 0x1FF00000; i < 0x1FFFFFFF; i+= 4)
	{
		if ( *(u32*)(i + 0) == 0x40196800 )
		{
			if ( *(u32*)(i + 4) == 0x3C1A8001 )
			{
				startaddr = i - 8;
				break;
			}
		}
	}
	ptr = (u32 *) (startaddr + 0x02F0);
	addr = (u32*)((ptr[0] << 16) | (ptr[2] & 0xFFFF));
	addr = (u32*)((u32)addr & 0x1fffffff);
	addr = (u32*)((u32)addr + startaddr);
	return addr;
}

_GSM_ENGINE_ void InitGSM(u32 interlace, u32 mode, u32 ffmd, u64 display, u64 syncv, u64 smode2, int dx_offset, int dy_offset, u8 skip_videos)
 {

	u32* ROMSyscallTableAddress;

	// Update GSM params
	DI2();
	ee_kmode_enter2();

	Target_INTERLACE		= interlace;
	Target_MODE				= mode;
	Target_FFMD				= ffmd;
	Target_DISPLAY1			= display;
	Target_DISPLAY2			= display;
	Target_SYNCV			= syncv;
	Target_SMODE2			= smode2;
	X_offset				= dx_offset;		// X-axis offset -> Use it only when automatic adaptations formulas don't fit into your needs
	Y_offset				= dy_offset;		// Y-axis offset -> Use it only when automatic adaptations formulas dont't fit into your needs
	skip_videos_fix			= skip_videos ^ 1;	// Skip Videos Fix ------------> 0 = On, 1 = Off ; Default = 0 = On
	
	automatic_adaptation	= 0;				// Automatic Adaptation -> 0 = On, 1 = Off ; Default = 0 = On
	DISPLAY_fix				= 0;				// DISPLAYx Fix ---------> 0 = On, 1 = Off ; Default = 0 = On
	SMODE2_fix				= 0;				// SMODE2 Fix -----------> 0 = On, 1 = Off ; Default = 0 = On
	SYNCV_fix				= 0;				// SYNCV Fix ------------> 0 = On, 1 = Off ; Default = 0 = On

	ee_kmode_exit2();
	EI2();

	// Hook SetGsCrt
	ROMSyscallTableAddress = GetROMSyscallVectorTableAddress();
	Old_SetGsCrt = (void*)ROMSyscallTableAddress[2];
	SetSyscall2(2, &Hook_SetGsCrt);

	// Remove all breakpoints (even when they aren't enabled)
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $k0, 0x8000\n"
	"mtbpc $k0\n"			// All breakpoints off (BED = 1)
	"sync.p\n"				// Await instruction completion
	".set at\n"
	".set reorder\n"
	);

	// Replace Core Debug Exception Handler (V_DEBUG handler) by GSHandler
	DI2();
	ee_kmode_enter2();
	*(u32 *)0x80000100 = MAKE_J((int)GSHandler);
	*(u32 *)0x80000104 = 0;
	ee_kmode_exit2();
	EI2();

	SetVCommonHandler(8, (void *)0x80000280);	//TPIIG Fix

}

/*---------------------------------------------------------*/
/* Disable Graphics Synthesizer Mode Selector (a.k.a. GSM) */
/*---------------------------------------------------------*/
static inline void DeInitGSM(void)
{
	//Search for Syscall Table in ROM
	u32 i;
	u32 KernelStart;
	u32* Pointer;
	u32* SyscallTable;
	KernelStart = 0;
	for (i = 0x1fc00000+0x300000; i < 0x1fc00000+0x3fffff; i+=4)
	{
		if ( *(u32*)(i+0) == 0x40196800 )
		{
			if ( *(u32*)(i+4) == 0x3c1a8001 )
			{
				KernelStart = i - 8;
				break;
			}
		}
	}
	if (KernelStart == 0)
	{
		GS_BGCOLOUR = 0x00ffff;	// Yellow	
		while (1) {;}
	}
	Pointer = (u32 *) (KernelStart + 0x2f0);
	SyscallTable = (u32*)((Pointer[0] << 16) | (Pointer[2] & 0xFFFF));
	SyscallTable = (u32*)((u32)SyscallTable & 0x1fffffff);
	SyscallTable = (u32*)((u32)SyscallTable + KernelStart);
	
	DI();
	ee_kmode_enter();
	// Restore SetGsCrt (even when it isn't hooked)
	SetSyscall(2, (void*)SyscallTable[2]);
	// Remove all breakpoints (even when they aren't enabled)
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $k0, 0x8000\n"
	"mtbpc $k0\n"			// All breakpoints off (BED = 1)
	"sync.p\n"				// Await instruction completion
	".set at\n"
	".set reorder\n"
	);
	ee_kmode_exit();
	EI();
}

//----------------------------------------------------------------------------
int main(void)
{   
	// Other values: Value chosen by user (0-7)
	int predef_vmode_idx = 3;

	//----------------------------------------------------------------------------
	//---------- Start of coding stuff ----------

	DeInitGSM();

	InitGSM(predef_vmode[predef_vmode_idx].interlace, \
					predef_vmode[predef_vmode_idx].mode, \
					predef_vmode[predef_vmode_idx].ffmd, \
					predef_vmode[predef_vmode_idx].display, \
					predef_vmode[predef_vmode_idx].syncv, \
					((predef_vmode[predef_vmode_idx].ffmd)<<1)|(predef_vmode[predef_vmode_idx].interlace), \
					0, \
					0, \
					0); 
					//XOffset
					//YOffset
					//Skip videos

	// Call sceSetGsCrt syscall in order to "bite" the new video mode
	__asm__ __volatile__(
		"li  $3, 0x02\n"   // Syscall Number = 2 (sceGsCrt)
		"add $4, $0, %0\n"   // interlace
		"add $5, $0, %1\n"   // mode
		"add $6, $0, %2\n"   // ffmd

		"syscall\n"			// Perform the syscall
		"nop\n"				// nop for Branch delay slot

		:
		:	"r" (predef_vmode[predef_vmode_idx].interlace), \
			"r" (predef_vmode[predef_vmode_idx].mode), \
			"r" (predef_vmode[predef_vmode_idx].ffmd)
	);

	// Exit to PS2 Browser
	Exit(0);

	return 0;
}
