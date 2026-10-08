+ -S=/home/dlb/coco/defs/miscos9lbl pd0 -S=rl.lbl -S=rof.lbl

>Y D; U @; #D &

" D 09 'End of init DP
'
" D 0067 'End of DP data
'
" D 01e4 'End of Non-DP Init data
'

A 0d/5; S &
W 2c2a-2c2d; W 2c6a-2c6d
*A 30d6-30da
W 2b9d-2ba0; W 077e/4

" L 016A '
End of cstart
Begin of part 1
lnk_01.a
'

> X D 2b; R & 57; #1 ^ 81-89; #1 ^ 9f
> Y & d1
A 10f-126; S &
> #1 I 0124; #Y & 0126; #1 E 012b
" L 0184 'while (argc--)'
" L 0187 / if (*(*argv++) == '-')/
" L 019f / while (* argv++) - (parsing argument)/
> #1 ^ 0193 - 01b9
" L 01c3 /switch (*argv)/
' L 01c8 'b'
' L 01da 'E' or 'e' (Edition)
' L 01e6 'l'
' L 020f 'M' (memory)
' L 0223 'm' (map)
' L 0228 'n' (modname)
' L 022f 'o' (filename)
' L 0247 's' (no stkcheck)
' L 0251 't'
' L 0259 'z'
' L 0264 default
> #X ^ 027b-02c1
" L 02cf /end while (*argv...)/
" L 02d1 /Not "-", it's a module name/
" L 02f0 ' End "while argc--"
----------------------------

Setup complete, now link
'
" L 03ca /
End main()
/
' L 03f4 No files to process
' L 03fa Already have a file opened
' L 042d Read failed
" L 0439 "
L0439()"
> #D ^ 0533

A 548-0557; S &; A -570; B /2; A -0589; S &
A -059f; S &; A -05b4; S &; A -05c7; S &
A -05d6; S &; A -05f9; S &; A -0604; S &
A -0613; S &; A -0627; S &; A -0642; S &
A; S &; A -065e; S &; A -066f; S &; A -0683; S &
A -0693; S &; A -06b4; S &; A -06d2; S &
A -0700; S &; A -0721; S &; A -73d; S &

" L 073f '
Begin of part 2
lnk_02.a
'
" L 074f /
InFilePD=freopen(ModName,"r",InFilePD)/
*> X R (+ &2) 0774; X R 0779; S R (+ &2) 078e-07bc
" L 07a2 "
Check that we don't already have a mainline
"
" L 07bc "
We don't already have a mainline, so we need one here
"
*> S R (+ &2) 07db-07d5; S R (+ &2) 07db; S R (+ &4) 07e5
*> S R (+ &4) 0829; S R (+ &8) 0864; X R (+ &2) 08a9; X R 08ad
W 08b3 /4
*> S R (+ &2) 08c3-08d8
' L 0790 h_valid ?
' L 07e1 (L0937 ModHead, 0)
" L 0856 'Read ModHead'
" L 0885 /End "while (D0043=L03ca)"/

" L 0937 /
check modhead - parameters (modhd *, int)
43 bytes static storage/
" L 0942 'Add Buffer (49 bytes) if not already initialized'
" L 0967 'getname (buffer, 16)'
" L 097d ' _strass (&buffer, [43,s+12], 16) (memcpy)'

A 1031 /5; S &; A /5; S &; A /3; S &
2 A /5; S &
* Begin of "r"
A; S &; A -1067; S &; A; S &
A -108c; B /2; A -10a2; S &
A -10b7; S &; A -10d3; S &
>#D $ 10ec

" L 10d5 '
Begin of part 3
lnk_03.a
'

L & 125f /4; L & 1282 /4
> Y & (+ D 0081) 13f8-140d
> #1 S 1a37

A 1a46 /3; S &; A -1a61; S &; A -1a75; S &
A -1a9a; B /2; A; S &; A -1ab8; S &
A -1ad6; S &; A -1af0; S &; A -1aff; B /2
A -1b37; S &; a -1b4d; S &; A -1b63; S &

" L 1b65 '
Begin of part 4
lnk_04.a
'
> #D ^ 1cbc-1cc2
" L 1da9 /
Allocate memory - Passed : Size of memory to request
Returns : Address of new mem or NULL on failure
/
" L 1fa2 "
Read Null-terminated string from file into buffer
Passed: (1) Address of string buffer, (2) Max count"
" L 1fd9 "
_fgets() Similar to fgets() except it replaces newline w/NULL
Passed: (1) Storage buffer (2) File ptr
Returns: NULL on error, ptr to buffer end on success
"
> #D ^ 1ff2
A 203d-204a; B /3; A -2066; B /2; A -2075; S &
7 A /4; S &
A /3; S &; A -20ae; B /2; A -20cd; B /3
A -20fe; B /2; A -2123; B /2; A -2133; B /2
A -2157; S &; A -2182; B /2; A -219a; B /2
A -21b4; B /2

" L 21b7 '
Begin of part 5
lnk_05.a
'

A 23ad-23c1; B /2; A -23d4; S &


" L 23d6 '
End split
Begin of Library routines
'
* ****************
* Library routines
* ****************

> U & 23e7; #D E 23f3; #1 ^ 2416
> #1 ^ 241c; #1 ^ 242c-2431; U & 2444; #1 ^ 246b
> X ^ 2489 - 248e; #X ^ 2489-248f
> #1 E 249d; #X ^ 24f8-250d
> #D E 24ed
" L 2572 'End of fopen module'
> #X ^ 2863-28b9

A 2b08-2b0d; S &
S & 30da; S & 30e5

> #X ^ 30c1-30ce
> #1 ^ 325e-327e; #1 S 32e2-32ef; #1 E 32f2
> #1 S 331c; #1 E 3320; #1 E 3380; #1 S 339A; #1 E 33D0
> #1 E 342d; #1 S 3445-3451
> Y & 3510; Y & 3612; Y D (+&2) 3592

" L 3797 'Init data area

Init DP values
'
L & 3797; B; W; B -379d; L L; L D
" L 37a2 '
Non-Dp Init Data  @ D0067
'
L &; L D -37a7; B -37b3; L L -37bd
'L 37be _Crc
"L 37c3 'Begin Library functions'
'L 37c3 TenMults
'L 37cb D008e
B -37c0; L D; L & -37ca; L D
B -37cc; A -37ce; B
" L 37d0 'STDIO path descriptors

stdin'
" L 37dd 'stdout'
" L 37ea 'stderr'
16 W /10; B; L &
B -3920

"L 3921 '
Data-text references
'
L &; L D /12

"L 392f '
Data-data references
'
L &; L & (+D0067); L D /8

A 393b/5; S &
