$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2013 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$!	p1 - comma separated set of C files (wildcards supported '*' and '%')
$!
$ if (p1 .eqs. "")
$ then
$	write sys$output ""
$ 	write sys$output "Syntax :  runall file1,file2,... "
$	write sys$output ""
$	write sys$output "            where wild-cards are accepted "
$	write sys$output ""
$	write sys$output "      e.g. runall t_end.c,gds*.c,gvcst_init.c,w*.c"
$	write sys$output ""
$ 	exit
$ endif
$!
$ proc_verify = f$environment("VERIFY_PROCEDURE")
$ image_verify = f$environment("VERIFY_IMAGE")
$!
$ set noverify
$ pwd = f$environment("DEFAULT")
$!
$ on control_y then goto TERMINATE
$ on severe_error then goto TERMINATE
$ on error then goto TERMINATE
$!
$ minimal = f$trnlnm("minimal_build")
$ bypass_vercheck = f$trnlnm("runall_bypass_version_check")
$ stop_with_compile = f$trnlnm("runall_stop_with_compile")
$!
$ common_options := /standard=vaxc/share/assume=nowrit/float=g_float
$ common_options := 'common_options'/warn=disable=(signedknown,signedmember)/inc=(gtm$src:,tcpip$examples:)
$ ccdbg := cc'common_options'/define=(debug,nolicense)/debug/nooptimize
$ ccbta := cc'common_options'/debug/nooptimize
$ ccpro := cc'common_options'
$!
$ verno = f$trnlnm("gtm$verno")
$ image = f$trnlnm("gtm$exe")
$ img = f$extract(4,3,image)
$!
$ if (bypass_vercheck .eqs. ""  .and.  (f$extract(1,1,verno) .nes. "9"  .or.  f$extract(1,3,verno) .eqs. "990"))
$ then
$	write sys$output " "
$	write sys$output "RUNALL-E-WRONGVERSION -- Cannot Runall Non-Developmental version :: [7m",verno,"[0m"
$	write sys$output "Define the logical, RUNALL_BYPASS_VERSION_CHECK to bypass this check"
$	write sys$output " "
$	goto TERMINATE
$ endif
$!
$! rebuild this which won't cause any recompilations itself but may be usable.
$ @gtm$tools:gen_gtm_threadgbl_deftypes
$!
$ write sys$output ""
$ write sys$output "---------------------------------------------------------------------------------------------------"
$ write sys$output " ****  Compiling from   USER:[LIBRARY.''verno'.SRC]  ------------>   USER:[LIBRARY.''verno'.''img'.OBJ]"
$ write sys$output "---------------------------------------------------------------------------------------------------"
$ write sys$output ""
$ len_cur = 0
$ len_index = 0
$ len_max = f$length(p1)
$ offset = 0
$!
$ star_element = ""
$!
$outer_loop:
$	element = ""
$	cur_index = 0
$!
$inner_loop:
$	if (star_element .eqs. "")
$	then
$		t_element = f$element(len_index,",",p1)
$		len_index = len_index + 1
$		if (t_element .eqs. ",") then goto end_inner_loop
$	endif
$!
$	if (f$locate("*",t_element) .eqs. f$length(t_element) .and. f$locate("%",t_element) .eqs. f$length(t_element))
$	then
$		leaf_element = t_element
$	else
$		star_element = f$search(t_element,1)
$		leaf_element = star_element
$	endif
$!
$	if (leaf_element .nes. "")
$	then
$		leaf_element_head = f$element(0,"]",leaf_element)
$		if (leaf_element_head .nes. leaf_element)
$		then
$			leaf_element_tail = f$element(1,"]",leaf_element)
$		else
$			leaf_element_tail = leaf_element
$		endif
$!
$		leaf_element = f$element(0,";",leaf_element_tail)
$!
$		if (f$location(".CLD",leaf_element) .nes. f$length(leaf_element))
$		then
$			set def gtm$vrt:['img'.obj]
$			write sys$output "        ---->    set command/object/nolist gtm$src:''leaf_element'"
$			set command/object/nolist gtm$src:'leaf_element'
$			set def 'pwd'
$		else
$			if (f$location(".MSG",leaf_element) .nes. f$length(leaf_element))
$			then
$				set def gtm$vrt:['img'.obj]
$				write sys$output "        ---->    message object/nolist gtm$src:''leaf_element'"
$				message /object/nolist gtm$src:'leaf_element'
$				set def 'pwd'
$			else
$				if (leaf_element .nes. "SECSHR_DB_CLNUP.C" .and. leaf_element .nes. "SEC_SHR_BLK_BUILD.C")
$				then
$					element = element + "," + leaf_element
$					cur_index = cur_index + 1
$				endif
$			endif
$		endif
$	endif
$!
$	if (cur_index .eqs. 5) then goto end_inner_loop
$	goto inner_loop
$!
$end_inner_loop:
$	if (element .nes. "")
$	then
$		element = element - ","
$		set def gtm$vrt:['img'.obj]
$		write sys$output "        ---->    cc''img' gtm$src:''element'"
$		cc'img' gtm$src:'element'
$		set def 'pwd'
$		goto outer_loop
$	endif
$!
$ write sys$output ""
$!
$end_outer_loop:
$!
$ set def gtm$vrt:['img'.obj]
$ libr/repl mumps *.obj
$ delete/nolog *.obj;*
$!
$ if (stop_with_compile .nes. "")
$ then
$ 	goto TERMINATE
$ endif
$!
$ write sys$output ""
$ write sys$output "----------------------------------------------------------------------------------"
$ write sys$output " ****  Linking and forming executables in  USER:[LIBRARY.''verno'.''img']"
$ write sys$output "----------------------------------------------------------------------------------"
$ write sys$output ""
$!
$ @gtm$tools:build'img' 'verno' V72
$!
$ write sys$output ""
$ write sys$output ""
$!
$TERMINATE:
$ set def 'pwd'
$ temp = f$verify(proc_verify, image_verify)
$!
