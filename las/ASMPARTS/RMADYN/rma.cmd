* rma.cmd : for the disassembly by DYNAMITE of "rma" Edition 11

+ a z pd0 ll=9 S=rma.lbl d=/home/dlb/coco/defs S=rma.strs S=miscos9lbl

* Redefine Y and U references for "C"
> Y D; U &; #D &

" D 15 '
End Init DPD - Begin Uninit DPD'

" D ea '
Begin NonDP init Data'

" D 32c 'End of opcode table
'

" D 0368 '
Begin of commandline options
'

" D 039e '
End of commandline opts
Begin of jumptable for pseudo-opts
'

" D 03dc 'End of pseudo-opts jumptable
'

" D 0404 '
Begin of data for Library functions
'
" D 561 '
End of Init Data
'
" D 0573 '
End of Data created by cstart
'
" D 0971 '
Begin of Library data
'

" L 11 'Begin of cstart'
A 0d-0f;S &
A 010d-0125

*Floating-point defs (for (dinc and ddec))

B 1d3e/4; B 2d67/4; B 3e93/4; B 3f20/4; B 3f60/4

B 43d2; B 43dd
A 4ea8/3

*---------------------

* Program code definitions

> X D 29; U D 3b; R & 55
> U D 69; Y & 8f-a6; #1 ^ 77-b6
> Y & c9-d0
> #1 E 0129; #Y & 012f

" L 0168 '
Begin of part 1
rma.a
'

> #1 D (+ &1) 0192-0197; #1 D (+ &8) 01cf-1d3
> #1 ^ 032f-0338; #1 ^ 035d-0375
> D D (+ &1) 045b; Y D (+&1) 0524; D D (+ &1) 0555
> D D (+ &3) 0559; D D (+ &2) 055b;

C 0791
2 A; B
A -07a5; B; A -07af; B; A -07bc; B; A -07c1; B
A -07c7; B; A -07cd; B; A -07d2; B; A /2; B
A -07e0; B -07e4; A -07f0; B; A /2; B; A -07f9; B
A -07fd; B; A -080f; B; A -0821; B; A -083a; B
A -084d; B /2; A -0856; B /2

" L 0859 '
Begin of part 2
part2.a
'

> #1 ^ 04a7-04db; #D ^ 0628-062f

C 08d6
3 A /5; B
* psect...
A /3; B;
3 A /4; B
* "andcc"
A -0900; B
3 A /4; B
* "ldd"
3 A /3; B
A -0920; B
4 A /3; B
* "cmpu"
4 A /4; B
10 A /3; B
* "ld"
2 A /2; B
3 A /3; B
A -0981; B; A /3; B; A -098c; B
15 A /3; B
* "sync"
A -09cd; B
4 A /3; B
2 A /4; B
A -09eb; B
4 A /4; B
2 A /3; B
4 A /4; B
A -0a1e; B
* "fcc"
5 A /3; B
2 A /5; B
A -0a43; B; A -0a49; B
2 A /3; B
A /4; B
6 A /3; B
* "ends" @ 0a70
2 A /4; B; A /3; B
5 A /3; B
12 A /4; B
* "equ" @ 0ad2
2 A /3; B
A - 0ade; B; A /4; B
19 A /3; B


" L 0b31 '
Begin of part 3
part3.a
'

> D D (+ &2) 0b57; D D (+ &2) 0b71
> D D (+ &1) 0b7e-0b85; D D (+ &2) 0b9f
> D D (+ &2) 0ba1; D D (+ &1) 0bbb-0bcc
> D D (+ &1) 0bed-0bfb
> #X ^ 0bd8-0bdf; #1 ^ 0c2c; D D (+ &2) 0c59; #1 ^ 0c8b
> D D (+ &2) 0c7e-0c85; Y D (+ &1) 0cbb
> D D (+ &1) 0cce; D D (+ &2) 0d2e
> D D (+ &1) 0d41; D D (+ &1) 0d52; D D (+ &1) 0d6e-0d9a
> D D (+ &1) 0dd2

A 0f3d-0f52; B; A -0f5f; B; A -0f73; B
A -0f8e; B; A -0fa8; B; A -0fb6; B; A -0fc9; B

" L 0fcb '
Begin of part 4
part4.a
'

> #D ^ 0fd0; D D (+ &2) 100f-1011
> #1 ^ 10da-110a; D D (+ &2) 1123; #X ^ 112b-1137; #1 ^ 1145-1185
> D D (+ &2) 11ce; D D (+ &3) 11d2; D D (+ &2) 11df
> D D (+ &1) 11e1; D D (+ &2) 1200; D D (+ &1) 1202-1210
> #1 D (+ &1) 1210; #1 ^ 1226
> #1 ^ 1235; D D (+ &2) 124b; #1 ^ 126e-127e
> D D (+ &2) 1288-1296; #D ^ 12ce-12da; #X ^ 12df-12f0
>  D D (+ &2) 1300-130c
> D D (+ &1) 1328-132d; D D (+ &2) 1341-1346; #1 ^ 134c; #X ^ 137a
> #D ^ 14e7; R D 1557; D D (+ &3) 159e; D D (+ &2) 15a7; D D (+ &2) 15b7
> D D (+ &3) 15d7; D D (+ &2) 15e7-15ef; D D (+ &3) 1665
> D D (+ &2) 166e; D D (+&3) 1683; D D (+ &2) 1685-168a; #1 ^ 18e4

A 1693-16a9; B; A -16c0; B; A -16d0; B

" L16d2 '
Begin of part 5
part5.a
'

> Y D (+ &1) 18a1; D D (+ &1) 1967; D D (+ &2) 196c; D D (+ &3) 1970
> #1 ^ 197b; #1 ^ 1a21; #X ^ 1aaf-1ab6; #1 ^ 1ac3; #1 ^ 1b2c
> #1 ^ 1c03; #1 ^ 1c25

L & 1cf2 /4

A 1d4c-1d58; B; A -1d66; B; A -1d7a; B; A -1d89; B
A -1d94; B; A -1d9f; B; A; B; A -1db0; B
A -1db5; B; A -1dc1; B; A -1dd3; B

" L 1dd5 '
Begin of part 6
part6.a
'

> #1 ^ 1e69

A 2311-231d; B; A-232c; B; A -2343; B; A -2351; B; A -2367; B

" L 2369 '
Begin of part 7
part7.a
'

> #X ^ 23fc-2417; D D (+ &1) 24ea; D D (+ &1) 24F8; #X ^ 24fe-251f
> #X ^ 2554-2573; #1 ^ 2605
> #X ^ 26a4-26cd; #X ^ 2737-273e; #1 ^ 275f

A 27bf-27d0; B; A -27dd; B; A -27eb; B; A -27ff; B
A -280d; B; A -2819; B

" L 281b '
Begin of part 8
part8.a
'

> #1 ^ 2833; #1 ^ 2859
> #1 ^ 2b4c

A 3075-3089; B; A -309e; B; A -30ac; B
A -30b5; B; A -30bd; B; A -30cd; B
A -30f8; B; A -3103; B; A -3115; B /2
A -311e; B /3; A -3125; B; A -3136; B
B; A -3144; B -314a; A /3; B; A -3163; B /3
A -316d; B; A -3181; B


" L 3183 '
Begin of part 9
part9.a
'

' L 33e1 while (getc() > 0)
' L 33f7 while (MacFlg)
> #D ^ 344b
> #X ^ 348c-34a1; #1 ^ 351a; #X ^ 3567-3585; #1 ^ 359f; #D ^ 35f7

A 3668 /2; B; A -3684; B; A -369b; B
A -36af; B; A -36b4; B; A -36b9; B
A -36ca; B; A -36dd; B; A -36eb; B
A -36fc; B; A -370d; B


" L 370f '
End split
Begin of Library routines

fopen.a
'
> Y D (+& d0) 3724
> #D E 372c; #1 ^ 374f-3756; #1 ^ 3765-376a; #1 ^ 37a4; #X ^ 37c2-37c9
> #D E 3826; #X ^ 3831-3846
"L 38ab '
puts.a'
" L 38e9 '
fwrite.a'
"L 3933 '
printf.a'
> #1 ^ 399d-39ae; #1 ^ 39c6; #D ^ 39ca; #1 ^ 3a04
> #X ^ 3b59-3baf
> #D ^ 3c4b; #D ^ 3c66-3c8a; #D ^ 3ce2-3ce9; #D ^ 3cef

* Printf constant

A 3dfe /6; B
"L 3e05 '
fseek
'
"L 3fb6 '
putc.a
'
"L 4307 '
setbase.a
'
"L 4393 '
pfldummy.a
'
> #X ^ 43b9-43c7
" L 43d3 '
pffdummy.a
'
" L 43de '
strings.a
'
" L 4509 '
io.a
'
> #1 E 44c7; #1 E 4517; #1 E 4574; #1 S 458c-4598
> Y D (+&2) 45b2; Y & 461d-4626
> Y D (+&2) 4692; Y D (+&2) 46a7; Y & 4723-4729
"L 45c5 '
mem.a
'
" L 4656 '
time.a
'
" L 4672 '
syscommon.a
'
" L 468e '
claddsub.a
'
*----------------------

" L 4864 'Init DP Data'
' L 4864 Count
L & 4864
B
' L 4866 Placeholder by c.start
' L 4867 e_flg,ListMacs
' L 4869 PgWd, PgDpth
B /2; L & /4; L D /14
*S &
*B /2; L & /4; L D /14

" L 487b 'Init Non-DP Data'
' L 487b Count
L & 487b

A /14; B /16
" L 487D 'TmpNam - next 14 bytes'
' L 488b D00f8
*A /7; S & /7; S & /16
" L 489b 'Following are 137 refs to
routines to process opcodes
'
137 L L; B /2
" L 4abf 'End opcodes table

Begin of Dirctives
'
L L -4afa; B -4b18
8 A /1; L D

* Data-Text refs
" L 4b31 '
Jumptable for pseudo-opts
'
L L -4b6e

' L 4b6f CurFPos
W /4
' L 4b73 Header Sync bytes
W /4
B /24

*L L/62; B /32;
L D/4; B /140; L D; B /211

" L 4cf4 '
Data-Text refs
'

L &; L D -4e81

" L 4e82 '
Data-Data refs
'
L &
L D (+&1); L D /6
7 L D (+&1)
L D /14
