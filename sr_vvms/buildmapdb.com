$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! -----------------------------------------------------------------------------------------------------------
$!	This utility builds a database out of each *.map file created in gtm$map.
$!	The database is named mapdb.dat with its global directory mapdb.gld both in gtm$map.
$! -----------------------------------------------------------------------------------------------------------
$ if (f$trnlnm("gtm$map") .eqs. "")
$ then
$	write sys$output "Logical gtm$map not defined. Exiting..."
$	exit
$ endif
$!
$ define sys$output nl:
$ define sys$error  nl:
$ savegbldir = f$trnlnm("gtm$gbldir")
$ savepwd = f$environment("default")
$ set def gtm$map
$ curpwd = f$environment("default")
$ delete/log mapdb.gld.*
$ delete/log mapdb.dat.*
$ define gtm$gbldir gtm$map:mapdb.gld
$!
$ open/write outfile mapdbtmp.com
$ write outfile "change /seg $DEFAULT /file=''curpwd'mapdb.dat"
$ write outfile "change /seg $DEFAULT /allocation=200"
$ close outfile
$ gde
@mapdbtmp.com
exit
$ delete/log mapdbtmp.com.*
$!
$ mupip create
$!
$ delete/log mapdb.m.*
$ delete/log mapdb.obj.*
$maploop:
$ mapfile = f$search("*.map")
$ if (mapfile .eqs. "") then goto end_maploop
$ imagename = f$parse(mapfile,,,"NAME")
$ if (imagename .eqs. "IPCRM") then goto maploop
$ if (imagename .eqs. "MCOMPILE") then goto maploop
$ if (imagename .eqs. "GTM$DMOD") then goto maploop
$ if (imagename .eqs. "GTM$STOP") then goto maploop
$ if (imagename .eqs. "GTCM_STOP") then goto maploop
$ if (imagename .eqs. "CRASHANDBURN") then goto maploop
$ if (imagename .eqs. "GTCMDDPSTOP") then goto maploop
$ if (imagename .eqs. "CCP") then goto maploop
$ deassign sys$output
$ write sys$output "  --> Building in gtm$map:mapdb.dat : global ^''imagename'"
$ define sys$output nl:
$ gawk /input=gtm$tools:mapdb.awk /var=("image=''imagename'") 'mapfile' /out=mapdb.m
$ gtm
if $e($zv,6,9)]"V4.0" d ^mapdb  quit
; versions <= V4.0 complain mapdb.m has too many literals. so we xecute all contents of mapdb.m instead
set file="mapdb.m" open file use file read str
for  quit:str=""  xecute str  read str
$ mumps gtm$src:mapoff.m
$ purge mapoff.obj
$ delete/log gtm$obj:*.lis.*/excl=xref.lis
$ delete/log mapdb.m.*
$ delete/log mapdb.obj.*
$ goto maploop
$!
$end_maploop:
$ define gtm$gbldir 'savegbldir'
$ deassign sys$output
$ deassign sys$error
$ set def 'savepwd'
$!
