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
$!	fetch_cms_version.com - fetch CMS elements for a specific version (class)
$!
$!	p1 - "from version" class name (version name with punctuation; e.g. "V3.2-015")
$!	p2 - CMS library list (e.g., S_SUN or S_UNIX_CM)
$!
$!	This command file fetches the elements from CMS libraries; depending on
$!	its arguments, it will fetch either the most recent generations on the main line
$!	of descent or the generations corresponding to any release for which we have
$!	created a CMS class.  Its complexity is due to the requirement to handle elements
$!	present in old releases that have subsequently been NIX'ed.
$!
$  if (f$extract(0,4,p1) .eqs. "V9.9"  .or.  p1 .eqs. "NEXT" )
$  then
$	write sys$output ""
$	write sys$output "MOST RECENT GENERATIONS IN THE MAIN LINE OF DESCENT ARE BEING DOWNLOADED ..."
$	write sys$output "Fetching version ", p1
$!
$!	Get the most recent generation on the main line of descent of all source files:
$	cms set library 'p2'
$	cms fetch *.* ""
$!
$!	If release_name.h is present, modify the release number to target_version.
$	if (f$search("release_name.h") .nes. "")
$	then
$		if f$search("edrelnam.obj") .nes. ""
$		then
$			delete edrelnam.obj;*	! in case it's for the wrong CPU
$		endif
$		version p p			! production version should always be present and working
$		define/user gtm_new_ver_id 'p1'
$		gtm						! Set to correct version
set $zroutines="[]/src=([],gtm$src,gtm$vrt:[pct])"
d ^edrelnam
$		delete release_name.h;1
$		delete edrelnam.obj;*
$		rename release_name.h;2 release_name.h;1
$	endif
$!
$!	Don't bother processing NIX'ed files; they don't belong in "most recent" version.
$	directory *.*nix
$	if f$search("*.*nix;*") .nes. "" then delete/log *.*nix;*
$     else
$    	write sys$output "Fetching version ", p1
$!
$!	Because this is a configured version and not just the most recent on the
$!	main line of descent, it's possible some of the elements in this version
$!	have been NIX'ed since the version was created.  For this reason, we need
$!	to go through each component CMS source library, one at a time, and change
$!	any NIX'ed files back to their original names.  This is necessary in order
$!	to preserve proper occlusion behavior.
$!
$       lib_ind = 0
$       lib_cnt = f$trnlnm(p2,,,,,"MAX_INDEX")
$lib_loop:
$	  lib = f$trnlnm(p2,,lib_ind)
$	  cms set library 'lib'
$	  cms fetch *.*/gen='p1' ""
$	  show symbol $status
$nix_loop:
$	    nixname = f$search("*.*NIX")
$           if (nixname .eqs. "") then goto end_nixloop
$	    nixname = nixname - f$parse(nixname,,,"VERSION")
$	    basename = nixname - "NIX"
$	    rename/log 'nixname' 'basename'
$	    goto nix_loop
$end_nixloop:
$	  lib_ind = lib_ind + 1
$	  if (lib_ind .le. lib_cnt) then goto lib_loop
$!
$  endif
$  directory
$!
$! Delete duplicates that should have been superseded by previous versions:
$  if f$search("*.*;4") .nes. "" then delete/log *.*;4
$  if f$search("*.*;3") .nes. "" then delete/log *.*;3
$  if f$search("*.*;2") .nes. "" then delete/log *.*;2
$!
$ccfini:
$  directory
