#include <stdint.h>
#include <endian.h>

#ifdef __GCC__
# define packed __attribute__((__packed__))
#else
# define packed
#endif

/*
 *	Relocatable Object Format (ROF) Definitions
 */

/* type flags */
#define CODENT		0x04		/* data/code flag */
/* data type flags */
#define DIRENT		0x02		/* global/direct flag */
#define INIENT		0x01		/* clear/init. data flag */
#define TYPMASK		(CODENT|DIRENT|INIENT)

/* Code type flags */
#define CONENT		0x02		/* Constant */
#define SETENT		0x01		/* "Set" label */

/* location flags */
#define CODLOC		0x20		/* data/code flag */
#define DIRLOC		0x10		/* global/direct flag */
#define LOC1BYT		0x08		/* two/one byte size flag */
#define LOCMASK		(CODLOC|DIRLOC)
#define NEGMASK		0x40		/* negate on resolution */
#define RELATIVE	0x80		/* relative reference */

#if BYTE_ORDER == LITTLE_ENDIAN
# define ROFSYNC	0x8723CD62
#else
# define ROFSYNC	0x62CD2387
#endif

#define SYMLEN		63			/* maximum symbol length: same as las (was 9,
						 * which silently merged longer names) */
#define MAXNAME		16			/* maximum "module" name length */

typedef packed struct {	/* rof header structure */
	uint32_t	h_sync;		/* Sync bytes used to identify a ROF */
	uint16_t	h_tylan;	/* Type/Language and Attr/Rev bytes from psect */
	char		h_valid;	/* Assembly valid? */
	char		h_date[5];	/* Date/Time Assembled (OS-9 format) */
	char		h_edit;		/* Edition #, from psect */
	char		h_vers;		/* ROF version, from assembler */
	uint16_t	h_glbl;		/* Size of global storage in object */
	uint16_t	h_dglbl;	/* Size of direct global storage */
	uint16_t	h_data;		/* Size of idata */
	uint16_t	h_ddata;	/* Size of direct idata */
	uint16_t	h_ocode;	/* Size of code */
	uint16_t	h_stack;	/* Size of stack needed, from psect */
	uint16_t	h_entry;	/* Entry point of object, from psect */
} binhead;

typedef packed struct {	/* definition/reference structure */
	char		r_flag;		/* type/location flag */
	uint16_t	r_offset;	/* reference offset */
} def_ref;

#define DEF 1
#define REF 2
