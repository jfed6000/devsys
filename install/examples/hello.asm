* hello.asm - a NitrOS9 Level 2 program built on the F256 (devsys)
*
*   las hello.asm --args=/dd/sys/n9mod.opt -o=hello
*   attr hello e pe
*   /dd/devsys/ex/hello
*
* "mod" tells las to read lwasm's syntax; n9mod.opt gives NitrOS9 main's
* settings and finds defsfile in /dd/defs/nitros9.

               nam       hello
               use       defsfile

* lwasm's struct: a record type. It defines the offsets line.count and
* line.text, and sizeof{line}, without reserving anything.
line           struct
count          rmb       1
text           rmb       32
               endstruct

tylg           set       Prgrm+Objct
atrv           set       ReEnt+rev
rev            set       $00
edition        set       1

               mod       eom,name,tylg,atrv,start,size

out            rmb       sizeof{line}
               rmb       200                 stack
size           equ       .

name           fcs       /hello/
               fcb       edition

msg            fcc       /hello from las on the F256/
               fcb       C$CR
msglen         equ       *-msg

start          clr       out+line.count,u
               leax      msg,pcr
               ldy       #msglen
               lda       #1                  standard output
               os9       I$WritLn
               bcs       done
               inc       out+line.count,u
               clrb
done           os9       F$Exit

               emod
eom            equ       *
               end
