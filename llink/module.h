#ifndef _MODULE_H_
#define _MODULE_H_
/* OS-9 module header definitions */

#ifndef _ANSI_H_
# include "ansi.h"
#endif

#ifndef _SYS_TYPES_H_
# include <sys/types.h>
#endif

#ifndef _TYPES_H_
# include "types.h"
#endif

/* Module sync bytes */
#ifndef MODSYNC		/* devsys: rlink.h, included first, sets the host's order */
#define MODSYNC	0x87CD		/* Module header sync bytes */
#endif
#define CRCCON	0x800063	/* CRC polynomial constant */

/* Module Type values */
#define MT_ANY		0	/* not used -- wildcard in syscalls */
#define MT_PROGRAM	1	/* Module type: Program */
#define MT_SUBROUT	2	/* Subroutine module */
#define MT_MULTI	3	/* Multi-module (reserved) */
#define MT_DATA		4	/* Data module */
#define MT_CSDDATA	5	/* "Config data block" data module */
#define MT_CDBDATA	MT_CSDDATA
#define MT_TRAPLIB	11	/* User trap library (68K+) */
#define MT_SYSTEM	12	/* System module */
#define MT_FILEMAN	13	/* File manager module */
#define MT_DEVDRVR	14	/* Device driver */
#define MT_DEVDESC	15	/* Device descriptor */

/* Module Language values */
#define ML_ANY		0	/* not used -- wildcard in syscalls */
#define ML_OBJECT	1	/* Language: Object (machine) code */
#define ML_ICODE	2	/* Basic09 I-code */
#define ML_PCODE	3	/* Pascal P-code */
#define ML_CCODE	4	/* C I-code (does not exist) */
#define ML_CBLCODE	5	/* COBOL code */
#define ML_FRTNCODE	6	/* FORTRAN code */
#define ML_JAVACODE	7	/* Java code (yes, really) */

#define mktypelang(type,lang)	(((type)<<4)|(lang))

/* Module Attribute values */
#define MA_REENT	0x80	/* Module is reentrant */
#define MA_GHOST	0x40	/* Module is sticky */
#define MA_SUPER	0x20	/* Module is for system state */

#define mkattrevs(attr,revs)	(((attr)<<4)|(revs))

/* Macro definitions for accessing module header fields */
#define MODNAME(mod)	(char*)((char*)(mod) + (mod)->m_name)
#define MODSIZE(mod)	((mod)->m_size)

/* Module header common section -- found in all OS-9 module types */
typedef struct mod_com {
	_USHORT	m_sync,			/* sync bytes ($87cd) */
			m_size,			/* module size */
			m_name;			/* offset to module name */
	_BYTE	m_tylan,		/* type & language */
			m_attrev,		/* attributes & revision */
			m_parity;		/* header parity */
} mod_com;

/* Executable memory module */
typedef struct mod_exec {
	_USHORT	m_sync,			/* sync bytes ($87cd) */
			m_size,			/* module size */
			m_name;			/* offset to module name */
	_BYTE	m_tylan,		/* type & language */
			m_attrev,		/* attributes & revision */
			m_parity;		/* header parity */

	_USHORT	m_exec,			/* offset to execution start */
			m_store;		/* initial storage size */
} mod_exec;

typedef struct mod_driver {
	_USHORT	m_sync,			/* sync bytes ($87cd) */
			m_size,			/* module size */
			m_name;			/* offset to module name */
	_BYTE	m_tylan,		/* type & language */
			m_attrev,		/* attributes & revision */
			m_parity;		/* header parity */

	_USHORT	m_exec,			/* offset to function jump table */
			m_ddata;		/* data storage size */
	_BYTE	m_mode;			/* device mode capabilities */
} mod_driver;

#define MODDINIT(mod)	(char *)((char *)(mod) + (mod)->m_exec)
#define MODDREAD(mod)	(char *)((char *)(mod) + (mod)->m_exec + 3)
#define MODDWRITE(mod)	(char *)((char *)(mod) + (mod)->m_exec + 6)
#define MODDGST(mod)	(char *)((char *)(mod) + (mod)->m_exec + 9)
#define MODDPST(mod)	(char *)((char *)(mod) + (mod)->m_exec + 12)
#define MODDTERM(mod)	(char *)((char *)(mod) + (mod)->m_exec + 15)

/* Device descriptor module */
typedef struct mod_dev {
	_USHORT	m_sync,			/* sync bytes ($87cd) */
			m_size,			/* module size */
			m_name;			/* offset to module name */
	_BYTE	m_tylan,		/* type & language */
			m_attrev,		/* attributes & revision */
			m_parity;		/* header parity */

	_USHORT	m_fmname,		/* offset to file manager name */
			m_ddname;		/* offset to device driver name */
	_BYTE	m_mode,			/* device mode capabilities */
			m_control[3],	/* device controller address (24 bit)*/
			m_tabsize;		/* option table size */
} mod_dev;

/* Configuration module */
typedef struct mod_config {
	_USHORT	m_sync,			/* sync bytes ($87cd) */
			m_size,			/* module size */
			m_name;			/* offset to module name */
	_BYTE	m_tylan,		/* type & language */
			m_attrev,		/* attributes & revision */
			m_parity;		/* header parity */

	_BYTE	m_ramtop[3],	/* top limit of free ram */
			m_irqno,		/* IRQ polling entry count */
			m_devno;		/* device entries */
	_USHORT	m_startup,		/* offset to startup mod. name */
			m_sysdrive,		/* offset to default drive name */
			m_consol,		/* offset to console terminal name */
			m_boot;			/* offset to bootstrap module name */
	_BYTE	m_wprot,		/* "Write protect flag (?)" */
			m_os9lvl[4],	/* operating system level/version/edition */
			m_feat1,		/* Feature byte #1 */
			m_feat2;		/* Feature byte #2 */
	_USHORT	m_os9rev,		/* offset to OS-9 level/revision string */
			m_install;		/* installation name offset */
								/* CoCo 3 section */
	_BYTE	m_montype,		/* Monitor type */
			m_hrmous,		/* Mouse high-res? */
			m_lrmous,		/* Mouse left/right port */
			m_krstart,		/* Key repeat: start constant */
			m_krdelay;		/* Key repeat: delay constant */
} mod_config;

/* Data module */
typedef struct mod_data {
	_USHORT	m_sync,			/* sync bytes ($87cd) */
			m_size,			/* module size */
			m_name;			/* offset to module name */
	_BYTE	m_tylan,		/* type & language */
			m_attrev,		/* attributes & revision */
			m_parity;		/* header parity */

	_USHORT	m_data,			/* offset to data */
			m_dsize;		/* size of data */
} mod_data;

#endif /* _MODULE_H_ */
