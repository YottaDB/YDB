$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2013 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$! The purpose of this script is to generate save the full directory
$! information to dir_full.txt and record the checksum for each file.
$
$!	p1 - version to get kits for (e.g. V50000D)
$
$ if (p1 .eqs. "")
$ then
$	write sys$output ""
$	write sys$output "With yeti or bgfoot as the build system of record"
$ 	write sys$output "Syntax :  @gtm$tools:kitprepare <version>"
$	write sys$output "      e.g. @gtm$tools:kitprepare V60002"
$	write sys$output ""
$ 	exit
$ endif
$
$! Check if the kits we expect to see are present in GTM$VRT:[DIST]
$ gtmverno = p1
$ distdir = "gtm$root:[" + gtmverno + ".dist]"
$
$ set def gtm$root:['gtmverno]
$ set def [.dist]
$ purge/log *.*
$ if (f$search("dir_full.txt") .nes. "")
$ then
$	delete/log dir_full.txt;
$ endif
$ dir/full /out=dir_full.txt
$
$ open/append dirlist dir_full.txt
$
$! the following list has to be maintained in sync with "distrib_kits_vms" defined in $cms_tools/server_list
$ kitlist = "GTCDxxxxx.A,GTCMxxxxx.A,GTDCxxxxx.A,GTMxxxxx.A,GTMxxxxx.B,GTMxxxxx.C"
$!
$ numkits = f$length(kitlist)
$ partial_list = "''kitlist'"
$ verlength = f$length(gtmverno)
$ kitver = f$extract(1, 5, gtmverno)
$kitcheckloop:
$       index = f$locate(",",partial_list)
$	curkit = f$extract(0, index, partial_list)
$	partial_list = f$extract(index + 1, numkits, partial_list)
$	if (curkit .eqs. "") then goto endkitcheckloop
$	length = f$length(curkit)
$	xxindex = f$locate("xxxxx",curkit)
$	kitprefix = f$extract(0, xxindex, curkit)
$	kitsuffix = f$extract(xxindex+5, length, curkit)
$	kitfile = distdir + kitprefix + kitver + kitsuffix
$	if (f$search(kitfile) .eqs. "")
$	then
$		msg = "''kitfile' does not exist. First run @gtm$com:kitstart all ''gtmverno' user:[library.''gtmverno'.dist]"
$		write sys$output "KITPREP-E-NOKITS : ''msg'"
$		write dirlist "KITPREP-E-NOKITS : ''msg'"
$		exit 0	! to signal error
$	endif
$	checksum 'kitfile
$	filename = f$parse(kitfile,,,"NAME") + f$parse(kitfile,,,"TYPE")
$	write sys$output "KITPREP-I-CHECKSUM : ''filename' : checksum = [''checksum$checksum'] "
$	write dirlist "KITPREP-I-CHECKSUM : ''filename' : checksum = [''checksum$checksum'] "
$	goto kitcheckloop
$endkitcheckloop:
$
$ write sys$output "KITPREP-S-SUCCESS : KITPREP completed successfully"
$ write dirlist "KITPREP-S-SUCCESS : KITPREP completed successfully"
