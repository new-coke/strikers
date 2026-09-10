/*
 * Special MSL prefix file created so CATS information can 
 * be turned off for Dolphin. This way any calls to MSL
 * that are profiled will not crash the application if they 
 * are called *before* TRK has been initialized.
 */ 
 
 /*
  * Turn off CATS information
  */ 
  
#pragma cats off
  
#include  <ansi_prefix.PPCEABI.bare.h>