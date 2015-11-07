$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2014 Fidelity Information Services Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! spkitupdate.com p1=version
$!
$ vno = p1
$ p1 = ""
$ say = "write sys$output"
$ gawk:=$gtm$bin:gawk.exe
$ say "Fixing the version in packaging config files to "'vno'
$ ver 'vno' p
$ curr_priv = f$setprv("sysprv")
$ gtma := $ gtm$exe:gtm$dmod.exe
$ gtma "user:[library.''vno']"
set $zro=$piece($zroutines,"=(",1)_"=(gtm$vrt:[src],"_$piece($zroutines,"=(",2)
d ^spkitbld
$ curr_priv=f$setprv(curr_priv)
$ delete/nolog/since spkitbld.obj.,_ucase.obj.
$ say "Should show the correct version"
$ pipe gawk /commands ="/KITNAME/{printf(""%-32s\t%s\n"", FILENAME, $2);}" /field_sep=":=" gtm$vrt:[t%%]*_spkitbld.dat
$ exit
