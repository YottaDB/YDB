$ set noon
$! Define old library logicals to support linking against older versions of
$! VMS.
$! John Francini 17 Apr 1997
$! Roger Partridge 15 Dec 1999
$! Xianguan Li 05 Jan 2000
$! Sam Weiner 11 Jul 2004
$!
$! If on a AXP, call with P1 = "V62" to define for V6.2 libraries.
$! If on a AXP, call with P1 = "V71" to define for V7.1 libraries.
$! Default on AXP is V7.2-1.
$!
$! If on a VAX, call with P1 = "V55" to define for V5.5-2 libraries.
$!              P1 = "V61 for V6.1 libraries
$! Default on VAX is V7.1.
$!
$! If called with P1 = "CHECK", a display of the current state of the libraries
$! is given.
$!
$! If called with P1 = "REMOVE", then the logicals are deleted.
$!
$! These last two options may be abbrieviated to a single letter.
$!
$! All logicals are defined in the PROCESS logical name table.
$ say := write sys$output
$ myarch = f$getsyi("arch_type")
$ on_vax := false
$ on_alpha := false
$ nosys := false
$ dpn := define/process/nolog
$ if myarch .eq. 1
$ then
$ 	on_vax := true
$	arch_str := "VAX"
$ endif
$ if myarch .eq. 2
$ then
$	on_alpha := true
$	arch_str := "Alpha"
$	node = f$edit(f$getsyi("nodename"),"upcase")
$	if node .eqs. "YETI" then nosys := true
$	if node .eqs. "BGFOOT" then nosys := true
$ endif
$ if f$extract(0,1,p1) .eqs. "C" then goto check_logicals
$ if f$extract(0,1,p1) .eqs. "R" then goto undefine_logicals
$!
$! Here to define the logicals
$!
$ if on_alpha
$ then
$	axplib := V721LIB
$	verstr := V7.2-1
$	if p1 .eqs. "V62"
$	then
$		axplib := V62LIB
$		verstr := V6.2
$       endif
$	if p1 .eqs. "V71"
$	then
$		axplib := V71LIB
$		verstr := V7.1
$       endif
$	say "[Defining old library logicals for OpenVMS Alpha ''verstr']"
$	if nosys
$	then
$		dpn alpha$library gtm$root:[alpha$'axplib'.sys$library]
$		dpn sys$library gtm$root:[alpha$'axplib'.sys$library]
$		dpn alpha$loadable_images gtm$root:[alpha$'axplib'.sys$ldr]
$		dpn sys$loadable_images gtm$root:[alpha$'axplib'.sys$ldr]
$	else
$		dpn alpha$library disk$launch-box:[alpha$'axplib'.sys$library]
$		dpn sys$library disk$launch-box:[alpha$'axplib'.sys$library]
$		dpn alpha$loadable_images disk$launch-box:[alpha$'axplib'.sys$ldr]
$		dpn sys$loadable_images disk$launch-box:[alpha$'axplib'.sys$ldr]
$	endif
$	dpn old_library_logicals 'verstr'
$ endif
$ if on_vax
$ then
$	vaxlib := V71LIB
$	verstr := V7.1
$	if p1 .eqs. "V61"
$	then
$		vaxlib := V61LIB
$		verstr := V6.1
$	endif
$	if p1 .eqs. "V55"
$	then
$		vaxlib := V55LIB
$		verstr := V5.5-2
$	endif
$	say "[Defining old library logicals for OpenVMS VAX ''verstr']"
$	dpn sys$library disk$cetus:[vax$'vaxlib'.sys$library]
$	dpn vax$library disk$cetus:[vax$'vaxlib'.sys$library]
$	dpn sys$loadable_images disk$cetus:[vax$'vaxlib'.sys$ldr]
$	dpn vax$loadable_images disk$cetus:[vax$'vaxlib'.sys$ldr]
$       dpn mthrtl disk$cetus:[vax$'vaxlib'.sys$library]uvmthrtl.exe
$       dpn uvmthrtl disk$cetus:[vax$'vaxlib'.sys$library]uvmthrtl.exe
$       dpn librtl2 disk$cetus:[vax$'vaxlib'.sys$library]librtl2.exe
$	dpn old_library_logicals 'verstr'
$ endif
$ exit
$!
$! Here to check to see if the logicals are defined...
$!
$check_logicals:
$ verstr = f$trnlnm("OLD_LIBRARY_LOGICALS","LNM$PROCESS")
$ if verstr .eqs. ""
$ then
$	say "[No old library logicals are defined]"
$ else
$	say "[Currently using old library logicals for OpenVMS ''arch_str' ''verstr]"
$ endif
$ exit
$!
$! Here to undefine the old library logicals...
$!
$undefine_logicals:
$ call undefiner sys$library
$ call undefiner vax$library
$ call undefiner sys$loadable_images
$ call undefiner vax$loadable_images
$ call undefiner mthrtl
$ call undefiner uvmthrtl
$ call undefiner librtl2
$ call undefiner alpha$library
$ call undefiner alpha$loadable_images
$ call undefiner old_library_logicals
$ say "[Old library logicals have been removed from your process table]"
$ exit
$!
$! Subroutine to undefine a logical only if it exists in the PROCESS logical
$! name table...
$undefiner: SUBROUTINE
$ if f$trnlnm(P1,"LNM$PROCESS") .NES. "" THEN DEASSIGN/PROCESS 'P1'
$ exit
$ endsubroutine
