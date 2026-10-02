               nam       hello
               ifp1
               use       defsfile
               endc
tylg           set       Prgrm+Objct
atrv           set       ReEnt+rev
rev            set       $00
edition        set       1
               mod       eom,name,tylg,atrv,start,size
count          rmb       1
               rmb       250
size           equ       .
name           fcs       /hello/
               fcb       edition
msg            fcc       /Hi/
               fcb       C$CR
start          leax      msg,pcr
               ldy       #3
               lda       #1
               os9       I$WritLn
               clrb
               os9       F$Exit
               emod
eom            equ       *
