* settle.asm - lwasm-mode features the corpus doesn't use, assembled by
* native las in suite "las" (native.py) and compared with the host las,
* which matches lwasm on it (D46):
*  - a count defined after its use ("rmb N*8" before "N equ 9"), as in
*    NitrOS9 main's wildbits_vtio.d: las settles the values before pass 2
*  - lwasm's zero register "0" in tfr/exg (grfdrv256's "tfr 0,dp")
*  - ",pcr" sizing across the module
* No flags: "mod" selects lwasm mode (D40); the build's pragmas are given.

Prgrm    equ   $10
Objct    equ   1

         mod   eom,name,Prgrm+Objct,$81,start,dsize

         org   0
tbl      rmb   N*8                 count defined below
tail     rmb   1
dsize    equ   .

name     fcs   /settle/

start    tfr   0,dp
         exg   a,0
         tfr   0,x
         leax  tail,u
         leay  tbl,u
         lda   #N
         leax  msg,pcr
         ldb   tail,x
         swi2
         fcb   $06                 F$Exit
msg      fcc   /settled/
         fcb   $0d
N        equ   9

         emod
eom      equ   *
         end
