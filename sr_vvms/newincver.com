$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2005 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! newincver - create incremental version of gtm
$! parameters:
$!	p1 = platform
$!	p2 = newver
$!	p3 = oldver
$!	p4 = copyolb (y or n)
$!	p5 = cms class from which to download following scripts into [.tools] (e.g. "V4.3001B" or "" for V9.9-0 or "n" for none)
$!	p6 = copy all src (y or n)
$!		cms_load.com
$!		cms_load_verify_from_to_version.com
$!		fetch_cms_version.com
$!		vms_cms_load.com
$!
$ interact = (f$mode() .eqs. "INTERACTIVE")
$!
$ set noon
$!
$ if p1 .nes. "AXP" .and. p1 .nes. "VAX"
$ then
$   if interact then inquire p1 "Platform (AXP/VAX)"
$   if p1 .nes. "AXP" .and. p1 .nes. "VAX"
$   then
$     write sys$output "No action taken"
$     exit
$   endif
$ endif
$ if f$getsyi("arch_name") .eqs. "VAX"
$ then
$   platform = "VAX"
$ else
$   platform = "AXP"
$ endif
$ if platform .nes. p1
$ then
$   write sys$output "NEWINCVER-E-PLATFORMUM     Platform does not match. Install requires platform to match."
$   exit
$ endif
$!
$asknew:
$ if p2 .eqs. ""
$ then
$   write sys$output "Must specify a new directory"
$   if interact then inquire p2 "New directory"
$   if p2 .eqs. ""
$   then
$     write sys$output "No action taken"
$     exit
$   endif
$ endif
$!
$ if f$search(p1+"_gtm$gtmdev:[library]"+p2+".dir") .nes. ""
$ then
$   write sys$output "Destination directory "+p2+" already exists. Can't continue on existing directory."
$   p2 :=
$   goto asknew
$ endif
$!
$askold:
$ if p3 .eqs. ""
$ then
$   write sys$output "Must specify an old directory"
$   if interact then inquire p3 "Old directory"
$   if p3 .eqs. ""
$   then
$     write "No action taken"
$     exit
$   endif
$ endif
$!
$ if f$search(p1+"_gtm$gtmdev:[library]"+p3+".dir") .eqs. ""
$ then
$   write sys$output "Old directory not found"
$   p3 :=
$   goto askold
$ endif
$!
$ if interact .and. (p4 .eqs. "")
$ then
$   write sys$output "Must specify whether to copy mumps.olb's"
$   inquire p4 "Copy? <Yes>"
$ endif
$ if p4 .eqs. "" then p4 := y
$!
$! -------------- set up directories for a new version (used to be newverdir.com) -----------------
$!
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log 'p1'_gtm$gtmdev:[library.'p2'] /owner='f$user()'
$ set def 'p1'_gtm$gtmdev:[library.'p2']
$!
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.cmi]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.dist]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.hlp]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.pct]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w="")/log [.src]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tools]
$!
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tcm]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tcx]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tdp]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tdc]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tfi]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.tls]
$!
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.pro]
$ set def [.pro]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w="")/log [.map]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.obj]
$ set def [-]
$!
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.bta]
$ set def [.bta]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w="")/log [.map]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.obj]
$ set def [-]
$!
$ create/dir/prot=(o:rwed,s=rwe,g=re,w=re)/log [.dbg]
$ set def [.dbg]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w="")/log [.map]
$ create/dir/prot=(o:rwed,s=rwe,g=re,w="")/log [.obj]
$ set def [-]
$!
$! ---------- copy over stuff from old directory to new directory structure -----------
$!
$ set def 'p1'_gtm$gtmdev:[library.'p2']
$!
$ open/write newsrc 'p1'_gtm$gtmdev:[library.'p2']gtmsrc.com
$ write newsrc "$ define/nolog/proc gtmsecshr gtm$sec:"+p2+"_gtmsecshr.exe"
$ write newsrc "$ define/nolog/job gtmsecshr gtm$sec:"+p2+"_gtmsecshr.exe"
$ write newsrc "$ define/nolog/proc/exec cmishr gtm$sec:"+p2+"_cmishr.exe"
$ write newsrc "$ define/nolog/job/exec cmishr gtm$sec:"+p2+"_cmishr.exe"
$ write newsrc "$ define gtm$src gtm$root:["+p2+".src]"
$ close newsrc
$ set prot=(w:re) 'p1'_gtm$gtmdev:[library.'p2']gtmsrc.com
$!
$ exclude_list = "gtmsrc.com,*comlist.com,*.log,*.dir,minclude.tlb,dbgflip.com"
$ copy/log 'p1'_gtm$gtmdev:[library.'p3']*.* 'p1'_gtm$gtmdev:[library.'p2']/excl=('exclude_list')
$!
$ if "" .NES. f$search(p1+"_gtm$gtmdev:[library."+p3+"]cmi.dir")
$ then
$! smw 2001/08/15 cmi is small enough so we will just copy it
$    copy/log 'p1'_gtm$gtmdev:[library.'p3'.cmi]*.* 'p1'_gtm$gtmdev:[library.'p2'.cmi]
$ endif
$!
$ copy/log 'p1'_gtm$gtmdev:[library.'p3'.pct]*.* 'p1'_gtm$gtmdev:[library.'p2'.pct]
$ if ((p6 .nes. "y") .and. (p6 .nes. "Y"))
$ then
$	p6 = "N"
$ 	copy/log 'p1'_gtm$gtmdev:[library.'p3'.src]*.%lb/excl=(minclude.tlb) 'p1'_gtm$gtmdev:[library.'p2'.src]
$ else
$ 	copy/log 'p1'_gtm$gtmdev:[library.'p3'.src]*.*/excl=(minclude.tlb) 'p1'_gtm$gtmdev:[library.'p2'.src]
$ endif
$ if ("" .nes. f$search("''p1'_gtm$gtmdev:[library.''p3'.tools]*.*"))
$ then
$	copy/log 'p1'_gtm$gtmdev:[library.'p3'.tools]*.* 'p1'_gtm$gtmdev:[library.'p2'.tools]
$ endif
$!
$ if p4
$ then
$   set noon
$   copy/log 'p1'_gtm$gtmdev:[library.'p3'.bta.obj]*.%lb. 'p1'_gtm$gtmdev:[library.'p2'.bta.obj]
$   copy/log 'p1'_gtm$gtmdev:[library.'p3'.dbg.obj]*.%lb. 'p1'_gtm$gtmdev:[library.'p2'.dbg.obj]
$   copy/log 'p1'_gtm$gtmdev:[library.'p3'.pro.obj]*.%lb. 'p1'_gtm$gtmdev:[library.'p2'.pro.obj]
$   write sys$output " "
$ endif
$!
$!the following copy supplies an environment to bootstrap a build
$ copy/log 'p1'_gtm$gtmdev:[library.'p3'.pro]*.exe,*.mlb,*.olb,*.cld/excl=(lmu.exe,ipcrm.exe) 'p1'_gtm$gtmdev:[library.'p2'.pro]
$ rename/log 'p1'_gtm$gtmdev:[library.'p2'.pro]'p3'_gtmsecshr.exe 'p1'_gtm$gtmdev:[library.'p2'.pro]'p2'_gtmsecshr.exe
$ rename/log 'p1'_gtm$gtmdev:[library.'p2'.pro]'p3'_cmishr.exe 'p1'_gtm$gtmdev:[library.'p2'.pro]'p2'_cmishr.exe
$ if "" .NES. f$search(p1 + "_gtm$gtmdev:[library." + p2 + ".pro]" + p3 + "_crashandburn.exe")
$ then
$   rename/log 'p1'_gtm$gtmdev:[library.'p2'.pro]'p3'_crashandburn.exe 'p1'_gtm$gtmdev:[library.'p2'.pro]'p2'_crashandburn.exe
$ endif
$ set file/protection=w=re 'p1'_gtm$gtmdev:[library.'p2'.pro]'p2'_gtmsecshr.exe
$ curr_priv = f$setprv("cmkrnl,bypas")
$ if f$priv("cmkrnl,bypas")
$ then
$   copy 'p1'_gtm$gtmdev:[library.'p2'.pro]'p2'_gtmsecshr.exe gtm$sec:
$   install replace gtm$sec:'p2'_gtmsecshr.exe/prot/shar/head/open
$   install list gtm$sec:'p2'_gtmsecshr.exe
$   purge gtm$sec:'p2'_gtmsecshr.exe
$   copy 'p1'_gtm$gtmdev:[library.'p2'.pro]'p2'_cmishr.exe gtm$sec:
$   install replace gtm$sec:'p2'_cmishr.exe/prot/shar/head/open
$   install list gtm$sec:'p2'_cmishr.exe
$   purge gtm$sec:'p2'_cmishr.exe
$ else
$   write sys$output "WARNING - not able to install gtmsecshr and cmishr due to privileges"
$ endif
$ curr_priv=f$setprv(curr_priv)
$!
$ dir = ''f$trnlnm("''p1'_gtm$gtmdev") + "[library.''p2']"
$ set default 'dir'
$ purge/log [...]
$!
$! ---------- download cms_load* scripts into [.tools] if necessary -----------
$!
$ if ((p5 .nes. "n") .and. (p5 .nes. "N"))
$ then
$	set def [.tools]
$	cms set lib sl_vvms
$	if (p5 .eqs. "")
$	then
$		classgener = ""
$	else
$		classgener = "/gener=''p5'"
$	endif
$ 	cms fetch 'classgener' cms_load.com
$ 	cms fetch 'classgener' cms_load_verify_from_to_version.com
$ 	cms fetch 'classgener' fetch_cms_version.com
$ 	cms fetch 'classgener' vms_cms_load.com
$	set def [-]
$ endif
$!
$ exit
