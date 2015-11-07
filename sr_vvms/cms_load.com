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
$! This DCL command file is a front end to launch a platform_CMS_LOAD, which in turn fetches
$! files from CMS libraries and puts them into the appropriate source directory(ies) for the
$! specified platform.
$!
$! 	p1 - target platform or CMS library specification from which the target can be determined
$!	p2 - "to version", version of GT.M to populate on target platform (including punctuation, e.g., class name)
$!	p3 - library's password (for Unix platforms)
$!	p4 - "from version", version of GT.M to fetch (defaults to p2)
$!
$ interact = (F$MODE() .eqs. "INTERACTIVE")
$!
$askplat:
$!
$! Determine platform.
$! Since VMS DCL doesn't have an "elseif" command, the following are
$! all single "if"'s to improve readability and maintainability by
$! keeping down the number of nesting levels.
$!
$! check by specific platform name
$!
$  platform :=
$  if ((p1 .eqs. "ALPHA2") .or. (p1 .eqs. "WIGLAF") .or (p1 .eqs. "ASGARD"))
$   then
$    cms_lib = "S_AVMS"
$    dl_type = "VMS"
$    platform = "''p1'"
$  endif
$!
$  if ( p1 .eqs. "CETUS" )
$   then
$    cms_lib = "S_VMS"
$    dl_type = "VMS"
$    platform = "CETUS"
$  endif
$!
$  if ( platform .eqs. "" )
$  then
$    if ( interact )
$    then
$      inquire p1 "Enter valid CMS library or target platform name"
$      if ( p1 .eqs. "" )
$      then
$        write sys$output "%CMS_LOAD-E-BADPLATFORM, Invalid platform or CMS library specified"
$        exit
$      endif
$      goto askplat		! verify platform or CMS library specification
$    else
$        write sys$output "%CMS_LOAD-E-BADPLATFORM, Invalid platform or CMS library specified"
$        exit
$    endif
$  endif
$!
$! Verify CMS library.
$  cms set library 'cms_lib'
$  if ( $severity .ne. 1 )
$   then
$    write sys$output "%CMS_LOAD-E-BADCMSLIB, Invalid platform or CMS library specified for first argument"
$    exit
$  endif
$!
$! If we get to this point, we should have a valid platform and CMS library; now verify the "to" version.
$! This is the version on the target platform to which the sources will be copied.
$!
$  to_version = p2
$askver:
$  if ( to_version .eqs. "" )
$  then
$    if ( interact )
$    then
$      write sys$output "%CMS_LOAD-I-NOTOVERSION, You have specified a null ""to version""; the default is V9.9-0"
$      inquire to_version "Enter ""to version"" (with all punctuation) or enter <CR> to retain V9.9-0"
$    endif
$    if ( to_version .eqs. "" )
$    then
$      write sys$output "%CMS_LOAD-I-DEFAULTVER, Defaulting ""to version"" to V9.9-0"
$      to_version = "V9.9-0"
$    endif
$  endif
$!
$  from_version == p4
$!
$  cmsver = to_version - "." - "-"
$  toolsdir = "user:[library.''cmsver'.tools]"
$  gtmverdir = "user:[library.''cmsver']"
$  @'toolsdir'cms_load_verify_from_to_version 'to_version' 'from_version'
$  if ( $status .ne. 1 )
$    then
$      exit
$  endif
$!
$ when = f$trnlnm("test_run_time")
$ if when .nes. "" then $ when = "/after=""" + when + """"
$  submit/noprint/notify/queue='dl_type'_download_hiq/log='gtmverdir''platform'_cms_load.log 'toolsdir''dl_type'_cms_load.com -
	'when' /parameters=("''to_version'","''platform'","''cms_lib'","''p3'","''from_version'")
