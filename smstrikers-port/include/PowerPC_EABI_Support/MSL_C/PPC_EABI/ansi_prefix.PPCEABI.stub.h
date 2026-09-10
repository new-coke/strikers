/*
 *	ansi_prefix.PPCEABI.stub.h
 *	
 *		Copyright 1999 Metrowerks, Inc.
 *		All rights reserved.
 *
 *		This file is provided as a template when your embedded
 *		board has an OS.  Please see "Porting MSL to an Embedded OS"
 *		for more information.
 */

#ifndef __ansi_prefix__
#define __ansi_prefix__

#include <os_enum.h>
#define __dest_os	__ppc_eabi
#define _Generic_PPC_EABI_OS

#include <ansi_parms.h>


	/* Two macros determine what the floating support will be 	*/
	/* in the MSL and runtime support.							*/
	/*															*/
	/*	_No_Floating_Point 	is defined if we don't want	any		*/
	/*						floating point.  It is undefined 	*/
	/*						for software and hardware support.	*/
	/*															*/
	/*	_No_Floating_Point_Regs	is defined unless we actually	*/
	/*							want hardware floating point	*/
	/*							because the chip has an FPU.	*/
	/*							Note that the 8xx chips do NOT	*/
	/*							have an FPU.					*/

	/* We use the project settings to determine what type of	*/
	/* of floating point support to enable.	 This is a 2 step	*/
	/* process since __option() will only return 0 or 1 and 	*/
	/* there are 3 states to test.								*/
	/* __option(floatingpoint) will return 0 if you have 		*/
	/* selected no floating point support and 1 if you have		*/
	/* selected either software or hardware floating point.		*/
	/*		0 - no floating point								*/
	/*		1 - software or hardware floating point				*/
	/* __option(sfp_emulation) will return 1 if you have 		*/
	/* selected software floating point support and 0			*/
	/* otherwise.		*/
	/*		0 - no floating point or hardware floating point	*/
	/*		1 - software floating point							*/

	
#if __option(floatingpoint)==0

	/* No floating point support 		*/
#define _No_Floating_Point
#define _No_Floating_Point_Regs

#elif __option(floatingpoint)==1

#if __option(sfp_emulation)==1

	/* Software floating point support 	*/
#define _No_Floating_Point_Regs

#else

	/* Hardware floating point support 	*/
	/* Nothing to do					*/

#endif

#endif

	/* comment out macro if service is supplied by your OS 		*/
/*#define _No_Time_OS_Support	*/
/*#define _No_Alloc_OS_Support	*/
/*#define _No_Disk_File_OS_Support	*/

	/* uncomment _No_Console if you do not want to	*/
	/* write and read to a console window.  		*/
/*#define _No_Console*/

#ifndef _No_Console
	/* the serial 1 and 2 UARTlibs have unbuffered	*/
	/* IO; comment out the following line if  		*/
	/* you are either not using either the serial 1 */
	/* or 2 UARTlibs or if your OS has buffered IO.	*/
#define _Unbuffered_Console
#endif

/* Release 4.x of CW EPPC used a different version of allocation */
/* Define _MSL_PRO4_MALLOC if you wish to use that previous method. */
/* Please see alloc.c for more details */
/*#define _MSL_PRO4_MALLOC*/

/* The new allocation method has some optional configuration.  Please */
/* see alloc.c for more details.  By default, the fastest allocation */
/* method has been chosen.  In particular, you may want to define */
/* _MSL_NO_FIX_MALLOC_POOLS if your heap is smaller than 64k. */
#ifndef _MSL_PRO4_MALLOC
	/* define _MSL_NO_MALLOC_DESIGN_2 if you want lower overhead but */
	/* somewhat slower allocation. */
	/* #define _MSL_NO_MALLOC_DESIGN_2 */
	/* define _MSL_NO_FIX_MALLOC_POOLS if your heap will be less than 64k */
	/* #define _MSL_NO_FIX_MALLOC_POOLS */
#endif

#define NEWMODE NEWMODE_MALLOC		/* always use malloc for new */

	/* see ExceptionPPC.cp */
#define __CW_MAX_PROCESSES__ 1

/* __VEC__ is defined to 1 if altivec_model is on */
#if !__VEC__
#define _No_Altivec
#endif

#endif

/*  Change Record
 *	20-Jul-97 MEA   changed __ppc_eabi_os to __ppc_eabi.  Added
 *					specific defines for OS services.
 *	4-Aug-97  MEA   Added automatic support for floating point.
 *	04-May-98 MEA	undefined windows only __tls
 *	24-Aug-99 MEA	Removed outdated defines and added defines and comments
 *					for new allocation methods.
 *	25-Oct-99 MEA	Added _No_Altivec when Altivec isn't enabled
*/
