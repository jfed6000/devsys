/* msgs.h - dcc68's messages (devsys, step 17).
 *
 * A message is a 2-byte string: one byte with the high bit set (\200 +
 * its number), then the end. msg() (misc.c) turns it into the text, line
 * N of dcc68.msg, which the GNUmakefile makes from the comments here: so
 * the texts cost no room in dcc68. Keep one message per line, in order,
 * each text in a comment of its own.
 */

#define M_THIRD_EXPRESSION_MISSING      "\201" /* third expression missing */
#define M_OPERAND_EXPECTED              "\202" /* operand expected */
#define M_PRIMARY_EXPECTED              "\203" /* primary expected */
#define M_CONSTANT_REQUIRED             "\204" /* constant required */
#define M_CONSTANT_OPERATOR             "\205" /* constant operator */
#define M_UNDEFINED_STRUCT              "\206" /* undefined struct */
#define M_NAME_IN_A_CAST                "\207" /* name in a cast */
#define M_EXPRESSION_MISSING            "\210" /* expression missing */
#define M_CANT_OPEN_INPUT_FILE          "\211" /* can't open input file */
#define M_ERROR_WRITING_ASSEMBLY        "\212" /* error writing assembly */
#define M_LEA_ARG                       "\213" /* LEA arg */
#define M_CODGEN_LONGS                  "\214" /* codgen - longs */
#define M_CODGEN_FLOATS                 "\215" /* codgen - floats */
#define M_CODGEN_FLOAT_CONV             "\216" /* codgen - float conv */
#define M_COMPILER_TROUBLE              "\217" /* compiler trouble */
#define M_DEREFERENCE                   "\220" /* dereference */
#define M_REL_OP                        "\221" /* rel op */
#define M_TOO_MANY_BRACKETS             "\222" /* too many brackets */
#define M_FUNCTION_HEADER_MISSING       "\223" /* function header missing */
#define M_FUNCTION_TYPE_ERROR           "\224" /* function type error */
#define M_ARGUMENT_STORAGE              "\225" /* argument storage */
#define M_ARGUMENT_ERROR                "\226" /* argument error */
#define M_NOT_AN_ARGUMENT               "\227" /* not an argument */
#define M_DECLARATION_MISMATCH          "\230" /* declaration mismatch */
#define M_ARGUMENT_TYPE_IN_NEWFUNC      "\231" /* argument type in "newfunc" */
#define M_FUNCTION_UNFINISHED           "\232" /* function unfinished */
#define M_NAMED_TWICE                   "\233" /* named twice */
#define M_BAD_TYPE                      "\234" /* bad type */
#define M_NAME_CLASH                    "\235" /* name clash */
#define M_STRUCT_SYNTAX                 "\236" /* struct syntax */
#define M_STRUCT_MEMBER_MISMATCH        "\237" /* struct member mismatch */
#define M_UNDEFINED_STRUCTURE           "\240" /* undefined structure */
#define M_CANNOT_EVALUATE_SIZE          "\241" /* cannot evaluate size */
#define M_IDENTIFIER_MISSING            "\242" /* identifier missing */
#define M_FLOATS                        "\243" /* floats */
#define M_INPUT_LINE_TOO_LONG           "\244" /* input line too long */
#define M_TOO_LONG                      "\245" /* too long */
#define M_TOO_MANY_ELEMENTS             "\246" /* too many elements */
#define M_UNIONS_NOT_ALLOWED            "\247" /* unions not allowed */
#define M_CONSTANT_EXPRESSION_REQUIRED  "\250" /* constant expression required */
#define M_CANNOT_INITIALIZE             "\251" /* cannot initialize */
#define M_BAD_CHARACTER                 "\252" /* bad character */
#define M_CONSTANT_OVERFLOW             "\253" /* constant overflow */
#define M_OUT_OF_MEMORY                 "\254" /* out of memory */
#define M_UNTERMINATED_CHARACTER_CONSTA "\255" /* unterminated character constant */
#define M_CANT_OPEN_STRINGS_FILE        "\256" /* can't open strings file */
#define M_UNTERMINATED_STRING           "\257" /* unterminated string */
#define M_CANT_READ_STRINGS_FILE        "\260" /* can't read strings file */
#define M_DUMPSTRINGS                   "\261" /* dumpstrings */
#define M_LONGS                         "\262" /* longs */
#define M_MULTIPLE_DEFINITION           "\263" /* multiple definition */
#define M_STORAGE_ERROR                 "\264" /* storage error */
#define M_POINTER_MISMATCH              "\265" /* pointer mismatch */
#define M_INDIRECTION                   "\266" /* indirection */
#define M_DIVIDE_BY_ZERO                "\267" /* divide by zero */
#define M_TYPEDEF_NOT_A_VARIABLE        "\270" /* typedef - not a variable */
#define M_CANNOT_CAST                   "\271" /* cannot cast */
#define M_CANT_TAKE_ADDRESS             "\272" /* can't take address */
#define M_POINTER_REQUIRED              "\273" /* pointer required */
#define M_POINTER_OR_INTEGER_REQUIRED   "\274" /* pointer or integer required */
#define M_NOT_A_FUNCTION                "\275" /* not a function */
#define M_BOTH_MUST_BE_INTEGRAL         "\276" /* both must be integral */
#define M_TYPE_MISMATCH                 "\277" /* type mismatch */
#define M_TYPE_CHECK                    "\300" /* type check */
#define M_SHOULD_BE_NULL                "\301" /* should be NULL */
#define M_TYPE_ERROR                    "\302" /* type error */
#define M_LVALUE_REQUIRED               "\303" /* lvalue required */
#define M_UNDECLARED_VARIABLE           "\304" /* undeclared variable */
#define M_STRUCT_MEMBER_REQUIRED        "\305" /* struct member required */
#define M_STRUCTURE_OR_UNION_INAPPROPRI "\306" /* structure or union inappropriate */
#define M_MUST_BE_INTEGRAL              "\307" /* must be integral */
#define M_NO_IF_FOR_ELSE                "\310" /* no 'if' for 'else' */
#define M_ILLEGAL_DECLARATION           "\311" /* illegal declaration */
#define M_SYNTAX_ERROR                  "\312" /* syntax error */
#define M_MULTIPLE_DEFAULTS             "\313" /* multiple defaults */
#define M_NO_SWITCH_STATEMENT           "\314" /* no switch statement */
#define M_WHILE_EXPECTED                "\315" /* while expected */
#define M_BREAK_ERROR                   "\316" /* break error */
#define M_CONTINUE_ERROR                "\317" /* continue error */
#define M_LABEL_REQUIRED                "\320" /* label required */
#define M_ALREADY_A_LOCAL_VARIABLE      "\321" /* already a local variable */
#define M_CONDITION_NEEDED              "\322" /* condition needed */
#define M_TRANSLATION                   "\323" /* translation */
#define M_BINARY_OP                     "\324" /* binary op. */
#define M_X_TRANSLATE                   "\325" /* x translate */
#define M_COMPILER_ERROR                "\326" /* compiler error -  */
#define M_LABEL_UNDEFINED               "\327" /* label undefined :  */
