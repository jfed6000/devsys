/* **************************************************************** *
 * Part7.c - mostly deals with Numbers                              *
 * NOTE:  this part stack-checks   Compile WITHOUT the "-s" opt     *
 *                                                                  *
 * $Id::                                                        $   *
 * **************************************************************** */

#include "rma.h"

static int do_math (
#ifdef __STDC__
    int
#endif
);

static int mathtyp (
#ifdef __STDC__
    int
#endif
);

static int get_param();
static void e_ilxtRf ();

/* ************************************************************************ *
 * do_long() - This is a new addition to allow for 32-bit numbers           *
 * ************************************************************************ */

int
do_long (ZILCH)
    /* process a 32-bit operand */
{
    if (getNmbr () == 0)
    {
        return 0;
    }

    rmcoderefs ();
    genreflist ();
    return 1;
}

int
proc_int (ZILCH)
    /* process a 16-bit operand */
{
    if (getNmbr () == 0)
    {
        return 0;
    }

    rmcoderefs ();

    if (OptBPtr > d0791)
    {
        genreflist ();
    }
    else
    {
        /* lwasm keeps the low 16 bits of a too-large 16-bit value */

        if ( ! LwasmMode && ((NmbrInt >= 65536) || (NmbrInt < -65536)))
        {
            return e_report ("value out of range");
        }
    }

    LOC1Msk &= ~RELATIVE;
    return 1;
}

int
proc_byte (ZILCH)
    /* process an 8-bit operand */
{
    if (getNmbr () == 0)
    {
        return 0;
    }

    LOC1Msk |= LOC1BYT;
    rmcoderefs ();

    if (OptBPtr > d0791)
    {
        genreflist ();
    }
    else
    {
        /* lwasm keeps the low 8 bits of a too-large fcb value, but an
         * 8-bit instruction operand is still an error */

        if ( ! (LwasmMode && InDataDir) &&
             ((NmbrInt >= 256) || (NmbrInt < -256)))
        {
            return e_report ("value out of range");
        }
    }

    LOC1Msk &= ~(LOC1BYT | RELATIVE);
    return 1;
}

/* ******************************************************************** *
 * getNmbr() - Calculates a mathematical string of operations.          *
 *             Recursively parses the line and retrieves the complete   *
 *             string of operations of precedence equal to or higher    *
 *             than the first op.  The value is stored in NmbrInt.      *
 * Returns : 1 on success - 0 on failure or illegal character following *
 * ******************************************************************** */

int
getNmbr (ZILCH)
{
    SkipSpac ();
    OptBPtr = d0791;

    if (do_math (1))
    {
        switch (*SrcChar)
        {
            case ' ':
            case '\t':
            case '\0':
            case ',':
            case ')':
            case ']':
                return 1;
        }

        e_report ("bad operator");
    }

    return 0;
}

static void
e_ilxtRf (ZILCH)
{
    IsExtrn = NmbrInt = ilExtRef ();
}

/* ************************************************************************ *
 * do_math() - Performs mathematical operations.                            *
 *             On entry, One number has been retrieved (int NmbrInt).       *
 *             If a math operator is the next character, the next number is *
 *             parsed and the result is stored in NmbrInt.                  *
 *             The process recurses for all operations on the line or until *
 *             a lower precedence operator is encountered.                  *
 * ************************************************************************ */

static int
#ifdef __STDC__
do_math (int precedence)
#else
do_math (precedence)
     int precedence;
#endif
{
    struct ref_ent *var1;
    long frstnum;
    int _oldxtrn,
        _opratr,
        _oplevel;

    if (get_param () == 0)
    {
        return 0;
    }

    while ((_oplevel = mathtyp (*SrcChar)) >= precedence)
    {
        _opratr = (*(SrcChar++));

        if ((_opratr == '<') || (_opratr == '>'))
        {
            ++SrcChar;          /* the second character of << or >> */
        }
        frstnum = NmbrInt;
        var1 = OptBPtr;
        _oldxtrn = IsExtrn;

        if (do_math (_oplevel + 1) == 0)
        {
            return 0;
        }

        IsExtrn |= _oldxtrn;

        switch (_opratr)
        {
            case '+':
                NmbrInt += frstnum;
                break;
            case '-':
                NmbrInt = frstnum - NmbrInt;

                while (var1 < OptBPtr)
                {
                    (var1++)->ETyp ^= _HEXDIG;
                }

                break;
            case '*':
                if (IsExtrn)
                {
                    e_ilxtRf ();
                }
                else
                {
                    NmbrInt *= frstnum;
                }

                break;
            case '/':
                if (IsExtrn)
                {
                    e_ilxtRf ();
                }
                else
                {
                    if (NmbrInt == 0)
                    {
                        return e_report ("zero division");
                    }

                    NmbrInt = frstnum / NmbrInt;
                }

                break;
            case '&':
                if (IsExtrn)
                {
                    e_ilxtRf ();
                }
                else
                {
                    NmbrInt &= frstnum;
                }

                break;
            case '!':
            case '|':           /* lwasm mode only (see mathtyp) */
                if (IsExtrn)
                {
                    e_ilxtRf ();
                }
                else
                {
                    NmbrInt |= frstnum;
                }

                break;

            /* lwasm mode only (see mathtyp) */

            case '^':           /* binary ^ is exclusive or */
            case '%':           /* remainder */
            case '<':           /* << */
            case '>':           /* >> */
                if (IsExtrn)
                {
                    e_ilxtRf ();
                }
                else if (_opratr == '^')
                {
                    NmbrInt ^= frstnum;
                }
                else if (_opratr == '%')
                {
                    if (NmbrInt == 0)
                    {
                        return e_report ("zero division");
                    }

                    NmbrInt = frstnum % NmbrInt;
                }
                else if (_opratr == '<')
                {
                    NmbrInt = frstnum << NmbrInt;
                }
                else
                {
                    NmbrInt = frstnum >> NmbrInt;
                }

                break;
        }
    }

    return 1;
}

static int
#ifdef __STDC__
mathtyp (int tst_char)
#else
mathtyp (tst_char)
     char tst_char;
#endif
{
    /* lwasm mode: lwasm's operators and precedence, lowest first:
     *     | ! ^      or, or, exclusive or
     *     &          and
     *     + -
     *     * / % << >>
     * RMA's own order is different: & and ! bind tighter than * and +,
     * so "A+B&C" is A+(B&C) in RMA but (A+B)&C in lwasm. */

    if (LwasmMode)
    {
        switch (tst_char)
        {
            case '|':
            case '!':
            case '^':
                return 1;
            case '&':
                return 2;
            case '+':
            case '-':
                return 3;
            case '*':
            case '/':
            case '%':
                return 4;
            case '<':
            case '>':
                return (SrcChar[1] == tst_char) ? 4 : 0;
        }

        return 0;
    }

    switch (tst_char)
    {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
        case '&':
        case '!':
            return 3;
    }

    return 0;                   /* Default to "no match" */
}

/* ******************************************************************** *
 * get_param() - Retrieves a mathematical parameter from the source     *
 *               buffer.  If a math operator or parentheses is          *
 *               encountered, it recurses into itself until the entire  *
 *               expression is handled.                                 *
 *               It handles math precedence.                            *
 * ******************************************************************** */

static int
get_param (ZILCH)
{
    struct ref_ent *refminus;
    unsigned var3;

    IsExtrn = 0;

    switch (var3 = *(SrcChar++))
    {
        case '+':
            if (!get_param ())
            {
                return 0;
            }

            break;
        case '-':
            refminus = OptBPtr;

            if (!get_param ())
            {
                return 0;
            }

            NmbrInt = -NmbrInt;

            while (refminus < OptBPtr)
            {
                (refminus++)->ETyp ^= NEGMASK;
            }

            break;
        case '^':
            if (!get_param ())
            {
                return 0;
            }

            if (IsExtrn)
            {
                e_ilxtRf ();
            }
            else
            {
                NmbrInt = ~NmbrInt;
            }

            break;
        case '(':
            if (!do_math (1))
            {
                return 0;
            }

            if (*SrcChar == ')')
            {
                ++SrcChar;
                break;
            }
            else
            {
                e_report ("parenthesis missing");
            }

            break;
        case '"':       /* lwasm: "xy is a two-character 16-bit value */
            if ( ! LwasmMode)
            {
                --SrcChar;
                return (NmbrInt = e_report ("bad operand"));
            }

            if ((SrcChar[0] == '\0') || (SrcChar[1] == '\0'))
            {
                return (NmbrInt = e_report ("bad operand"));
            }

            NmbrInt = ((SrcChar[0] & 0xff) << 8) | (SrcChar[1] & 0xff);
            SrcChar += 2;

            if (*SrcChar == '"')
            {
                ++SrcChar;      /* optional closing quote */
            }

            break;
        case '~':       /* lwasm: complement (RMA spells it ^) */
            if ( ! LwasmMode)
            {
                --SrcChar;
                return (NmbrInt = e_report ("bad operand"));
            }

            if ( ! get_param ())
            {
                return 0;
            }

            if (IsExtrn)
            {
                e_ilxtRf ();
            }
            else
            {
                NmbrInt = ~NmbrInt;
            }

            break;
        case '.':       /* data counter in a module, else same as '*' */
            NmbrInt = mod_dot ();
            break;
        case '*':
            if (InModule || InTop)
            {
                NmbrInt = *S_Addrs;     /* plain numbers, no relocation */
            }
            else if (S_Addrs == 0)
            {
                if (CurntOrg == 0)
                {
                    return (NmbrInt = e_report ("undefined org"));
                }
                else
                {
                    NmbrInt = *CurntOrg;
                }
            }
            else
            {
                NmbrInt = *(int *) S_Addrs;

                /* Add a ref to the reflist */

                OptBPtr->ETyp = WhereLoc;
                /*(OptBPtr++)->RAddr = CurLclRef;*/
                OptBPtr->RAddr = CurLclRef;
                OptBPtr++;
                IsExtrn = 1;
            }

            break;
        case '\'':
            if ((NmbrInt = *(SrcChar++)) == '\0')
            {
                --SrcChar;
                return 0;
            }

            if (LwasmMode && (*SrcChar == '\''))
            {
                ++SrcChar;      /* lwasm allows a closing quote: 'A' */
            }

            break;
        default:
            --SrcChar;

            if (!_getnum ())
            {
                if (MovLbl (d0587) == 0)
                {
                    return (NmbrInt = e_report ("bad operand"));
                }

                lblnumeric ();
            }

            break;
    }

    return 1;                   /* Success */
}

/* **************************************************************** *
 * _getnum () - Reads in number from input buffer up to the end     *
 *          of the numeric string, and stores it in NmbrInt.        *
 *          If first char is "%", base is binary,                   *
 *                           "$", base is hex                       *
 *                         digit, base is 10                        *
 * Returns : number of digits in string                             *
 * **************************************************************** */

int
_getnum (ZILCH)
{
    int base,
      _lgth,
      _digit;
    char CTyp;

    _lgth = base = 0;
    CTyp = 8;

    switch (_digit = *SrcChar)
    {
        case '%':
            _digit = *(++SrcChar);
            base = 2;
            CTyp = 0x10;
            break;
        case '$':
            _digit = *(++SrcChar);
            base = 16;
            CTyp = 0x20;
            break;
        default:
            if (LwasmMode && (_digit == '0') &&
                ((SrcChar[1] == 'x') || (SrcChar[1] == 'X')) &&
                (_chcodes[(int) SrcChar[2]] & 0x20))
            {
                SrcChar += 2;           /* lwasm: 0x hex */
                _digit = *SrcChar;
                base = 16;
                CTyp = 0x20;
            }
            else if (isdigit (_digit))
            {
                base = 10;
            }
    }

    if (base == 0)
    {
        return 0;
    }

    NmbrInt = 0;

    while ( _chcodes[_digit = (islower (_digit) ? _toupper (_digit) :
                                                  _digit)] & CTyp)
    {
        NmbrInt = (NmbrInt * base) + (_digit - ((_digit < 'A') ?
                                                        '0' : ('A' - 10)));
        ++_lgth;
        _digit = *(++SrcChar);
    }

    return _lgth;
}
