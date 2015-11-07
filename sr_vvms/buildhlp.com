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
$! buildhlp.com - build help files
$!
$ if p1 .eqs. "" then $exit
$ @gtm$tools:gtm_verify_symbols "set"	! sets the global symbols gtm_copy, gtm_delete, gtm_library, gtm_purge
$ @gtm$tools:build_print_stage "buildhlp" "begin"
$ @gtm$tools:setactive_silent 'p1' b
$ set def gtm$help
$!
$ @gtm$tools:build_print_stage "Creating *.hlb files" "middle"
$ gtm_library/create=(block:25,key:60)/help dse
$ gtm_library/help dse gtm$src:dse
$ gtm_library/create=(block:25,key:60)/help LKE
$ gtm_library/help LKE gtm$src:LKE
$ gtm_library/create=(block:25,key:60)/help mupip.hlb
$ gtm_library/help mupip.hlb gtm$src:mupip
$ gtm_library/create=(block:25,key:60)/help gde
$ gtm_library/help gde gtm$src:gde
$ gtm_library/create=(block:25,key:60)/help cce
$ gtm_library/help cce gtm$src:cce
$ gtm_library/create=(block:25,key:60)/help la
$ gtm_library/help la gtm$src:la
$ gtm_library/create=(block:25,key:60)/help lmu
$ gtm_library/help lmu gtm$src:lmu
$ gtm_library/create=(block:25,keysize:60)/help mumps
$ gtm_library/help mumps gtm$src:mumps
$ gtm_purge
$ set prot=w:r *.hlb
$ set def gtm$ver
$!
$ @gtm$tools:build_print_stage "buildhlp" "end"
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
