/* ******************************************************************** *
 * rmabase.h - las's types, constants and prototypes                    *
 *                                                                      *
 * Everything rma.h needs before its global variables, and nothing that *
 * depends on MAIN: globals.c defines the variables (MAIN) but shares   *
 * this part with every other file (for dcc's header images, D50, gone  *
 * since step 20). rma.h includes it first.                             *
 * ******************************************************************** */

#ifdef COCO
#   define const
#   define void int
#   define u_char char
#   define LBLLEN 9
#   define ZILCH    /* To allow void parameter in prototypes */
#else
#  define direct
#    ifndef u_char
#      define u_char unsigned char
#      define __u_char_defined
#     endif
#   define LBLLEN 9
#   define ZILCH void
#endif



/* symbol table types */
/* symbol definition/reference type/location */

/* data type flags */

#define CODENT      0x04        /* data/code flag */

/* location flags */

#define DIRENT      0x02        /* global/direct flag */
#define INIENT      0x01        /* clear/init. data flag */
#define CODLOC      0x20        /* data/code flag */
#define DIRLOC      0x10        /* global/direct flag */
#define LOC1BYT     0x08        /* two/one byte size flag */
#define LOCMASK     (CODLOC | DIRLOC)
#define NEGMASK     0x40        /* negate on resolution */
#define RELATIVE    0x80        /* relative reference */

/* Masks in LblTyp */

#define LT_INPSECT 2
#define LT_GLOBAL 4
#define LT_SET 5
#define LT_EQU 6
#define LT_HASRIGHT 8       /* <=LOC1BYT=>*/
#define LT_HASREFS 0x20     /* <=CODLOC=> */
#define LT_CONST 0x40
#define LT_INIT 0x80        /* Flags that the symblstr has been initialized */

#include "cocotype.h"
#include <stdio.h>
#include <string.h>

/* We could use #include <ctype.h> for non-coco systems, but it's too
 * complicated for just getting one macro definition.
 */

#ifdef NEED_CTYPE
#   define isblank(c) ((c == ' ') || (c == '\t'))
#   define notblank(c) ! isblank(c)
#endif

#include "rstruct.h"
#ifndef COCO
#   include "proto.h"
#endif
#   include "proto.h"
