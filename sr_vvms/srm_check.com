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
$! check for invalid alpha code sequences in .exe files found in directory p1
$!
$ proc_verify = f$environment("VERIFY_PROCEDURE")
$ set noverify
$!
$ if "" .nes. p1 then $ set def 'p1'
$ srmck := $ sys$system:srm_check.exe
$ exe = f$search("foo.exe")	! clear any active search list
$loop:
$ exe = f$search("*.exe.")
$ if "" .nes. exe
$  then
$   srmck 'exe'
$   goto loop
$ endif
$!
$ temp = f$verify(proc_verify)
$ exit
