/* **************************************************************** *
 * globals.c - las's global variables and tables                    *
 *                                                                  *
 * rma.h and rtables.h declare them; with MAIN defined they define  *
 * them, here and nowhere else. They used to be defined in          *
 * rma_main.c, but a file that defines all of them and also holds   *
 * the assembler's main loop is more than the native dcc68 can      *
 * compile in its 16K of data on the F256 (step 17, DESIGN.md D49). *
 * **************************************************************** */

#define MAIN

#include "rma.h"
