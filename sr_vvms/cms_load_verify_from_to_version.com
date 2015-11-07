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
$! This DCL command file verifies the validity of the "from version" and
$! "to version" for a dowload from CMS to a target platform.  The rules
$! are:
$!
$!	if "to version" is "V9.x-x" and not "V9.9-0", then "from version"
$!		can be any valid version ("V9.9-0" must always be the most
$!		recent on the main line of descent),
$!	otherwise, "from version" must match "to version"
$!
$!	if "from version" is not specified and there is a choice, this
$!	script, if invoked interactively, will prompt for it.  The default
$!	is to set "from version" to "to version".
$!
$!
$! Input:
$!
$!	p1 - "to version", the version of GT.M to populate on the target platform
$!	p2 - "from version", the version of GT.M to fetch from CMS
$!
$!
$! Output:
$!
$!	from_version - set to version of GT.M to fetch from CMS, compatible
$!			with requested "to version"
$!
$!
$! Error:
$!
$!	exit
$!
$  interact = (F$MODE() .eqs. "INTERACTIVE")
$!  set verify
$!
$! Determine whether the user wishes to fetch a different version (the "from version") that that being populated.
$! This is only legal when the "to version" is a V9.9-x version other than V9.9-0 (which must always be the most
$! recent on the main line of descent).
$!
$!
$  to_version = p1
$  from_version == p2
$!
$  if ( from_version .nes. ""  .and.  from_version .nes. to_version )
$    then
$      if ( f$extract(0,3,to_version) .nes. "V9."  .and.  f$extract(0,6,from_version) .nes. "NEXT32" )
$        then
$          write sys$output "%CMS_LOAD-I-MISMATCH, You cannot overwrite ''to_version' with any other version"
$          exit 9
$        else
$          if ( to_version .eqs. "V9.9-0" )
$            then
$              write sys$output "%CMS_LOAD-I-ILLDEVELOP, V9.9-0 must always be the most recent version the main line of descent"
$              exit 9
$          endif
$      endif
$  endif
$!
$!
$! If user didn't specify a different "from version", determine whether it's allowed;
$! if so, and this is an interactive invocation, prompt the user for a "from version".
$!
$  if ( from_version .eqs. "" )
$    then
$      if ( f$extract(0,3,to_version) .nes. "V9."  .or.  to_version .eqs. "V9.9-0" )
$        then
$          from_version == to_version	! no comment needed about defaulting; there's no choice
$        else
$          if ( interact )
$            then
$              write sys$output "%CMS_LOAD-I-NOFROMVERSION, You have specified a null ""from version""; the default is ",to_version
$              inquire spec_version "Enter ""from version"" (with all punctuation) or enter <CR> to retain ''from_version'"
$              from_version == spec_version
$          endif
$          if ( from_version .eqs. "" )
$            then
$              write sys$output "%CMS_LOAD-I-DEFAULTVER, Defaulting ""from version"" to ",to_version
$              from_version == to_version
$          endif
$      endif
$  endif
$!
$  if ( F$EXTRACT(0,3,from_version) .eqs. "V9." )
$    then
$      write sys$output ""
$      write sys$output "You have selected ",from_version," which contains the most recent versions in the main line of descent"
$      write sys$output ""
$  endif
$!
$  exit 1
