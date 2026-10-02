/* **************************************************************** *
 * module.c - OS-9 module format (lwasm --format=os9 compatible)    *
 *                                                                  *
 * RMA only knew sections (psect/vsect/csect) and ROF output. This  *
 * adds the other way to build a program: the source writes the    *
 * module itself between "mod" and "emod", and las outputs the      *
 * finished module, with no linker.                                 *
 *                                                                  *
 * Location counters, as lwasm does it:                             *
 *   Outside a module there is one counter (AbsCtr). "*" and "."    *
 *   both read it, "org" sets it, "rmb" advances it.                *
 *   Inside a module there are two, both starting at 0: "*" is the  *
 *   code counter (CodeSize, advanced by code and fcb/fdb/...),     *
 *   "." is the data counter (DataCtr, advanced by rmb, set by org).*
 *   "rmb" never writes bytes.                                      *
 *                                                                  *
 * A module is assembled like a psect that starts at address 0:     *
 * the code bytes RMA produces in a psect hold offsets from the     *
 * psect's start, which are exactly the module's own addresses, so  *
 * no relocation is needed. Output is the raw bytes, in order.      *
 * **************************************************************** */

#define NEED_CTYPE
#include "rma.h"

static unsigned char ModCrc[3];
static unsigned char ModBytes[13];

/* ******************************************************************** *
 * crc_byte() - Add one byte to the module CRC. This is the CRC-24 OS-9 *
 *              uses for modules (as in lwasm's os9.c, which took it    *
 *              from the NitrOS9 kernel source).                        *
 * ******************************************************************** */

static void
#ifdef __STDC__
crc_byte (int c)
#else
crc_byte (c)
     int c;
#endif
{
    unsigned char b = (unsigned char) c;

    b ^= ModCrc[0];
    ModCrc[0] = ModCrc[1];
    ModCrc[1] = ModCrc[2];
    ModCrc[1] ^= (unsigned char) (b >> 7);
    ModCrc[2] = (unsigned char) (b << 1);
    ModCrc[1] ^= (unsigned char) (b >> 2);
    ModCrc[2] ^= (unsigned char) (b << 6);
    b ^= (unsigned char) (b << 1);
    b ^= (unsigned char) (b << 2);
    b ^= (unsigned char) (b << 4);

    if (b & 0x80)
    {
        ModCrc[0] ^= 0x80;
        ModCrc[2] ^= 0x21;
    }
}

/* ******************************************************************** *
 * cond_log() - lwasm mode: pass 1 records each conditional's result,   *
 *              pass 2 returns the recorded results in the same order.  *
 *              Both passes read the same lines, so the order matches.  *
 *              One bit per conditional.                                *
 * ******************************************************************** */

#define CONDMAX 16384           /* conditionals per source file */

static unsigned char CondBits[CONDMAX / 8];
static int CondCount, CondNext;

int
#ifdef __STDC__
cond_log (int result)
#else
cond_log (result)
     int result;
#endif
{
    int i;

    if ( ! Pass2)
    {
        if (CondCount >= CONDMAX)
        {
            errexit ("too many conditionals");
        }

        i = CondCount++;

        if (result)
        {
            CondBits[i >> 3] |= (1 << (i & 7));
        }
        else
        {
            CondBits[i >> 3] &= ~(1 << (i & 7));
        }

        return result;
    }

    if (CondNext >= CondCount)
    {
        errexit ("conditionals out of step between passes");
    }

    i = CondNext++;
    return (CondBits[i >> 3] >> (i & 7)) & 1;
}

/* ******************************************************************** *
 * mod_pass() - Reset module state at the start of each pass.           *
 * ******************************************************************** */

/* ******************************************************************** *
 * top_setup() - lwasm mode, outside a module: like lwasm, code and     *
 *               data may appear anywhere and are written as raw bytes  *
 *               at the one location counter. Uses the instruction      *
 *               tables like a module does, with mod/org/rmb/end found  *
 *               through S_TopLw (see AsmblLin).                        *
 * ******************************************************************** */

void
top_setup (ZILCH)
{
    codsetup ();
    DLocField = RELATIVE | DIRENT;  /* labels are plain numbers */
    TmpLocMsk = 0;
    S_Addrs = &AbsCtr;
    CurntOrg = &AbsCtr;
    InTop = 1;
}

void
mod_pass (ZILCH)
{
    if ( ! Pass2)
    {
        SawPsect = UsedModule = 0;
        CondCount = 0;
    }

    CondNext = 0;

    AbsCtr = DataCtr = 0;
    LocalCtx = NextCtx = 0;
    InModule = InTop = TopPending = 0;
    CurntOrg = &AbsCtr;

    if (LwasmMode)
    {
        top_setup ();
    }
}

/* ******************************************************************** *
 * mod_write() - Called by w_ocod() for bytes generated in a module.    *
 *               On pass 2, adds them to the CRC and writes them out.   *
 * ******************************************************************** */

/* Bytes written in a module, or at the top level in lwasm mode */

void
#ifdef __STDC__
mod_write (char *bytes, int len)
#else
mod_write (bytes, len)
     char *bytes;
     int len;
#endif
{
    int i;

    UsedModule = 1;     /* raw output: no ROF header or refs */

    if (Pass2)
    {
        for (i = 0; i < len; ++i)
        {
            crc_byte (bytes[i]);
        }

        if (OutFile && (OFlg > 0))
        {
            fwrite (bytes, len, 1, OutFile);
        }
    }
}

/* ******************************************************************** *
 * do_mod() - "mod size,name,type,attr[,exec,mem]"                      *
 *            Starts a module: both counters to 0, then the header.     *
 * ******************************************************************** */

int
do_mod (ZILCH)
{
    int vals[6],
        count = 0,
        parity,
        i;

    if (SawPsect)
    {
        return e_report ("mod and psect can't be mixed");
    }

    /* Operands may be forward references (the size, usually), so
     * pass 1 may see 0; that's fine, the header is the same length. */

    while (count < 6)
    {
        OptBPtr = &d0791[0];

        if (getNmbr () == 0)
        {
            return 0;
        }

        vals[count++] = NmbrInt;

        if (*SrcChar != ',')
        {
            break;
        }

        ++SrcChar;
    }

    OptBPtr = &d0791[0];

    if ((count != 4) && (count != 6))
    {
        return e_report ("mod needs 4 or 6 operands");
    }

    /* Enter the module: a psect at address 0 */

    InTop = 0;
    codsetup ();
    DLocField = RELATIVE | DIRENT;  /* labels are plain numbers, as in lwasm */
    TmpLocMsk = 0;
    CodeSize = 0;
    DataCtr = 0;
    CurntOrg = &DataCtr;
    InModule = 1;
    UsedModule = 1;
    ModCrc[0] = ModCrc[1] = ModCrc[2] = 0xff;

    ModBytes[0] = 0x87;
    ModBytes[1] = 0xcd;
    ModBytes[2] = (vals[0] >> 8) & 0xff;
    ModBytes[3] = vals[0] & 0xff;
    ModBytes[4] = (vals[1] >> 8) & 0xff;
    ModBytes[5] = vals[1] & 0xff;
    ModBytes[6] = vals[2] & 0xff;
    ModBytes[7] = vals[3] & 0xff;

    for (parity = 0, i = 0; i < 8; ++i)
    {
        parity ^= ModBytes[i];
    }

    ModBytes[8] = (~parity) & 0xff;

    if (count == 6)
    {
        ModBytes[9] = (vals[4] >> 8) & 0xff;
        ModBytes[10] = vals[4] & 0xff;
        ModBytes[11] = (vals[5] >> 8) & 0xff;
        ModBytes[12] = vals[5] & 0xff;
    }

    /* The caller writes CmdCod[0..NumBytes-1] through w_ocod() */

    CmdCod = (char *) ModBytes;
    NumBytes = (count == 6) ? 13 : 9;
    return 1;
}

/* ******************************************************************** *
 * mod_peek() - Is this line's operation "mod"? Called before the label *
 *              is defined: lwasm gives the mod line itself address 0,  *
 *              so a label on it is 0, not the old counter.             *
 * ******************************************************************** */

int
mod_peek (ZILCH)
{
    register char *p = SrcChar;

    while (*p && ! isblank (*p))        /* past the label */
    {
        ++p;
    }

    while (isblank (*p))
    {
        ++p;
    }

    return ((_tolower (p[0]) == 'm') && (_tolower (p[1]) == 'o') &&
            (_tolower (p[2]) == 'd') && ((p[3] == '\0') || isblank (p[3])));
}

/* ******************************************************************** *
 * mod_emod() - "emod": append the CRC and leave the module. Writes the *
 *              3 bytes itself (they must not go into the CRC) and      *
 *              leaves S_Addrs at 0, so the caller's w_ocod() only      *
 *              lists them.                                             *
 * ******************************************************************** */

int
mod_emod (ZILCH)
{
    ModBytes[0] = Pass2 ? (ModCrc[0] ^ 0xff) : 0;
    ModBytes[1] = Pass2 ? (ModCrc[1] ^ 0xff) : 0;
    ModBytes[2] = Pass2 ? (ModCrc[2] ^ 0xff) : 0;

    if (Pass2 && OutFile && (OFlg > 0))
    {
        fwrite (ModBytes, 3, 1, OutFile);
    }

    CodeSize += 3;

    /* Back to the top level; its counter carries on from the module's end */

    InModule = 0;
    precodeset ();          /* S_Addrs = 0: the caller's w_ocod only lists */
    AbsCtr = CodeSize;
    TopPending = LwasmMode; /* back to top-level code at the next line */

    CmdCod = (char *) ModBytes;
    NumBytes = 3;
    return 1;
}

/* ******************************************************************** *
 * mod_rmb() - "rmb" outside any section, or inside a module: labels    *
 *             the current counter (data counter in a module) and       *
 *             advances it. The label is a plain number.                *
 * ******************************************************************** */

int
mod_rmb (ZILCH)
{
    NumBytes = 0;
    WhereLoc = CODENT | DIRENT;
    return do_rmb ();
}

/* ******************************************************************** *
 * mod_org() - "org" outside any section (sets the one counter) or      *
 *             inside a module (sets the data counter).                 *
 * ******************************************************************** */

int
mod_org (ZILCH)
{
    NumBytes = 0;

    /* lwasm, outside any section: an org to a symbol it can't resolve
     * (scf.d's "ORG M$DTyp" without os9.d) isn't an error; offsets after
     * it are still right relative to each other. Count from 0. */

    if (LwasmMode && InTop && ! InModule)
    {
        if ( ! getNmbr ())
        {
            return 0;
        }

        if (OptBPtr != d0791)
        {
            OptBPtr = d0791;
            NmbrInt = 0;
        }

        *CurntOrg = NmbrInt;
        *(Adrs_Ptr = &CodeAddrs) = NmbrInt;
        return 1;
    }

    if (l1723 ())
    {
        *CurntOrg = NmbrInt;
        *(Adrs_Ptr = &CodeAddrs) = NmbrInt;
        return 1;
    }

    return 0;
}

/* ******************************************************************** *
 * mod_dot() - Value of "." in an expression: the data counter in a     *
 *             module, otherwise the same as "*".                       *
 * ******************************************************************** */

int
mod_dot (ZILCH)
{
    if (InModule)
    {
        return DataCtr;
    }

    if (InTop)
    {
        return AbsCtr;
    }

    if (S_Addrs)
    {
        return *S_Addrs;
    }

    return CurntOrg ? *CurntOrg : AbsCtr;
}
