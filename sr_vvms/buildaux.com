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
$! buildaux - build auxiliary utilities
$! parameters:
$!	p1 = version number
$!	p2 = library (p, d, or b)
$!	p3 = target device and directory
$!	p4 = optional auxillary to build ("gde", "dse", "mupip", "lke", "lmu", "gtmstop",
$!						"cmi", "gtcm_server", "gtcm_stop", "ibi_xfm", "ddp", "dbcertify")
$!		(if none specified, will build all auxillaries)
$!
$ if p1 .eqs. ""
$ then
$	write sys$output "Must supply a version"
$	exit
$ endif
$!
$ lnkimg=f$locate(p2,"PBD")
$ if (f$length(p2) .ne. 1) .or. (lnkimg .eq. 3)
$ then
$	write sys$output "Library must be P, B or D"
$	exit
$ endif
$!
$ if p3 .eqs. ""
$ then
$	write sys$output "Must specify a target directory for the .exe files"
$	exit
$ endif
$!
$ @gtm$tools:gtm_verify_symbols "set"	! sets the global symbols gtm_copy, gtm_delete, gtm_library, gtm_purge
$ @gtm$tools:build_print_stage "buildaux" "begin"
$!
$! ---------- buildaux set linker options -----------
$!
$ lnkopt = f$element(lnkimg,",","/notrace,/debug,/debug")
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ if alpha then lnkopt = lnkopt + "/section"
$!
$! ---------- buildaux determine set of auxillaries to build -----------
$!
$ if p4 .eqs. ""
$ then
$	buildaux_gde        = "yes"
$	buildaux_dse        = "yes"
$	buildaux_mupip      = "yes"
$	buildaux_lke        = "yes"
$	buildaux_lmu        = "yes"
$	buildaux_gtmstop    = "yes"
$	buildaux_cmi        = "yes"
$	buildaux_gtcm_server= "yes"
$	buildaux_gtcm_stop  = "yes"
$	buildaux_ibi_xfm    = "yes"
$	buildaux_ddp        = "yes"
$	buildaux_dbcertify  = "yes"
$ else
$	buildaux_gde        = ""
$	buildaux_dse        = ""
$	buildaux_mupip      = ""
$	buildaux_lke        = ""
$	buildaux_lmu        = ""
$	buildaux_gtmstop    = ""
$	buildaux_cmi        = ""
$	buildaux_gtcm_server= ""
$	buildaux_gtcm_stop  = ""
$	buildaux_ibi_xfm    = ""
$	buildaux_ddp        = ""
$	buildaux_dbcertify  = ""
$	buildaux_'p4'       = "yes"
$ endif
$!
$!  smw 2001/5/2 force no dsf debug files for now - later check axp and logical
$ dsffiles = 0
$!
$ @gtm$tools:setactive_silent 'p1' 'p2'
$ set def gtm$vrt
$ set def gtm$exe
$!
$ set def [.obj]
$ objdir = f$environment("default")
$ set def 'p3'
$ targdir = f$environment("default")
$ set def [.map]
$ mapfile = f$environment("default")
$ set def [-]
$!
$! ---------- buildaux prepare options for the linker in .opt files -----------
$!
$ open/write relnam release_name.opt
$ write relnam "ident=",p1
$ close relnam
$!
$! Because the linker creates image sections on a per-cluster
$! basis, create a cluster for all of the code Psect's (whose
$! pages can be shared among processes) and collect all of the
$! code Psect's into it so the pages corresponding to that image
$! section can be shared.  Note the MACRO/MIGRATION compiler
$! names its code Psect "$CODE" (as do most of the VAX compilers),
$! while the AXP C compiler and MACRO assembler name their code
$! Psect's "$CODE$".
$ open/write clusnam cluster.opt
$ write clusnam "cluster = literal_clust"
$ write clusnam "collect = literal_clust,GTM$LITERALS"
$ write clusnam "cluster = code_clust"
$ write clusnam "collect = code_clust,GTM$CODE,$CODE,$CODE$"
$ close clusnam
$!
$ open/write secshrlink secshrlink.opt
$ if ("" .eqs. f$trnlnm("gtm_no_secshr")) .or. (p2 .nes. "P")
$ then
$ 	write secshrlink "gtm$pro:gtmsecshr.exe/share"
$ endif
$ close secshrlink
$!
$! ---------- buildaux gde -----------
$!
$ if (buildaux_gde .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building GDE" "middle"
$!
$!	use production images to compile GDE*.M and other GT.M utilities
$	@gtm$tools:setactive_silent 'p1' p
$	set def [.obj]
$!
$	set command gtm$src:GTMCOMMANDS.CLDX	! define MUMPS command if .cldx file present
$	mumps gtm$src:gde*.m
$	gtm_library /replace mumps gde*.obj
$	gtm_delete gde*.obj;*
$!
$	gtm_copy gtm$src:lclcol.mpt []_lclcol.m
$	mumps _lclcol
$	gtm_library /replace mumps _lclcol.obj
$	gtm_delete _lclcol.m;,_lclcol.obj;
$!
$	gtm_copy gtm$src:patcode.mpt []_patcode.m
$	mumps _patcode
$	gtm_library /replace mumps _patcode.obj
$	gtm_delete _patcode.m;,_patcode.obj;
$!
$	set def [-]
$	@gtm$tools:setactive_silent 'p1' 'p2' ! revert back to currently building image
$!
$	xtra = "," + f$parse(f$search("gtm$src:gde*.msg"),,,"NAME")
$	set noon
$	search/nooutput gtm$src:gde.m patcode
$	if $severity .eq. 1 then $ xtra = xtra + ",_patcode"
$	define/user gdeobj 'objdir'mumps.olb
$	define/user sys$error nl:
$	define/user sys$output nl:
$	gtm_library/extract=gdeoget/output=nl: 'objdir'mumps.olb
$	if $severity .eq. 1 then $ xtra = xtra + ",gdeoget"
$	set on
$	gdedsf = ""
$	if dsffiles then gdedsf = "/dsf=gde.dsf"
$	define/user gdeobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'gde.map/full/exe='targdir'gde.exe'lnkopt' 'gdedsf' gdeobj/incl=(gde'xtra'),sys$input/opt,target:cluster.opt/opt,target:release_name.opt/opt
gdeobj/incl=(_lclcol,gdeadd,gdechang,gdetempl,gdedelet,gdeexit,gdeget,gdemsgin)
gdeobj/incl=(gdehelp,gdesetgd,gdeinit,gdelocks,gdelog,gdemap,gdeparse,gdeput,gdequit)
gdeobj/incl=(gderenam,gdescan,gdeshow,gdespawn,gdeverif)
SYMBOL=GTM$CTRLC_ENABLE,0
name  = GDE.EXE
$ endif
$!
$! ---------- buildaux dse -----------
$!
$ if (buildaux_dse .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building DSE" "middle"
$	dsedsf = ""
$	if dsffiles then dsedsf = "/dsf=dse.dsf"
$	define/user dseobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'dse.map/full/cross/exe='targdir'dse.exe'lnkopt' 'dsedsf' dseobj/libr/incl=dse,target:secshrlink.opt/opt,target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
name = DSE.EXE
$ endif
$!
$! ---------- buildaux mupip -----------
$!
$ if (buildaux_mupip .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building MUPIP" "middle"
$	mupipdsf = ""
$	if dsffiles then mupipdsf = "/dsf=mupip.dsf"
$	define/user mupipobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'mupip.map/full/cross/exe='targdir'mupip.exe'lnkopt' 'mupipdsf' mupipobj/libr/incl=mupip,target:secshrlink.opt/opt,target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
name = MUPIP.EXE
$ endif
$!
$! ---------- buildaux dbcertify -----------
$!
$ if (buildaux_dbcertify .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building DBCERTIFY" "middle"
$	dbcertifydsf = ""
$	if dsffiles then dbcertifydsf = "/dsf=dbcertify.dsf"
$	define/user dbcertifyobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'dbcertify.map/full/cross/exe='targdir'dbcertify.exe'lnkopt' 'dbcertifydsf' dbcertifyobj/libr/incl=dbcertify,target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
name = DBCERTIFY.EXE
$ endif
$!
$! ---------- buildaux lke -----------
$!
$ if (buildaux_lke .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building LKE" "middle"
$	lkedsf = ""
$	if dsffiles then lkedsf = "/dsf=lke.dsf"
$	define/user lkeobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'lke.map/full/exe='targdir'lke.exe'lnkopt' 'lkedsf' lkeobj/libr/incl=lke,target:secshrlink.opt/opt,target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
name = LKE.EXE
$ endif
$!
$! ---------- buildaux lmu if not NOLICENSE in mdef.h -----------
$!
$ if (buildaux_lmu .nes. "")
$ then
$	search/nooutput gtm$src:mdef.h nolicense
$	if $severity .ne. 1
$	then
$		@gtm$tools:build_print_stage "Building LMU" "middle"
$		define/user lmuobj 'objdir'mumps.olb
$		define/user target 'targdir'
$		link/map='mapfile'lmu.map/full/exe='targdir'lmu.exe'lnkopt' lmuobj/libr/incl=lmu,target:secshrlink.opt/opt,target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
name = LMU.EXE
$	endif
$ endif
$!
$! ---------- skip gtmstop and cmi building for "minimal_build" -----------
$!
$ minimal = f$trnlnm("minimal_build")
$ if (minimal .nes. "")
$ then
$ 	goto minimal_build_skip
$ endif
$!
$! ---------- buildaux gtmstop -----------
$!
$ if (buildaux_gtmstop .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building GTM$STOP" "middle"
$!
$!	use production images to compile GTMSTOP.M
$	@gtm$tools:setactive_silent 'p1' p
$	set def [.obj]
$	set command gtm$src:GTMCOMMANDS.CLDX	! define MUMPS command if .cldx file present
$	mumps gtm$src:gtmstop.m
$	gtm_library /replace mumps gtmstop.obj
$	gtm_delete gtmstop.obj;
$	set def [-]
$	@gtm$tools:setactive_silent 'p1' 'p2' ! revert back to currently building image
$!
$	gtmstopdsf = ""
$	if dsffiles then gtmstopdsf = "/dsf=gtm$stop.dsf"
$	define/user gtmstopobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/exe=gtm$stop.exe/map='mapfile'gtm$stop.map/full 'gtmstopdsf' gtmstopobj/incl=(gtmstop,gtmstopzc,merrors),sys$input/opt,target:cluster.opt/opt,target:release_name.opt/opt
gtm$vrt:[pct]_dh.obj,_do.obj,_exp.obj,_st.obj
name = GTM$STOP.EXE
$ endif
$!
$! ---------- buildaux cmishr -----------
$!
$ if (buildaux_cmi .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building CMISHR" "middle"
$	@gtm$tools:cmicom 'p1' 'p2' 'objdir'
$	xtra = f$element(alpha,",","/incl=cmivector,")
$	cmilink = "cmilink." + f$element(alpha,",","vax,axp")
$	cmidsf = ""
$	if dsffiles then cmidsf = "/dsf=cmishr.dsf"
$	define/user cmiobj 'objdir'cmi.olb
$	define/user target 'targdir'
$	link/map='mapfile'cmishr.map/full/share='targdir'cmishr.exe'lnkopt' 'cmidsf' cmiobj/libr'xtra',gtm$tools:'cmilink'/opt,sys$input/opt,target:release_name.opt/opt
name = CMISHR.EXE
$	set security/protect=(o:rwed,s:rwed,g:re,w:re) 'targdir'cmishr.exe
$	if p2 .eqs. "P"
$	then
$		gtm_copy 'targdir'cmishr.exe 'targdir''p1'_cmishr.exe
$		curpriv=f$setprv("sysprv")
$		gtm_copy/protect=(o:rwed,s:rwed,g:re,w:re) 'targdir'cmishr.exe gtm$sec:'p1'_cmishr.exe
$		curpriv=f$setprv(curpriv)
$		set security/protect=(o:rwed,s:rwed,g:re,w:re) 'targdir''p1'_cmishr.exe
$	endif
$ endif
$!
$! ---------- buildaux gtcm_server -----------
$!
$ if (buildaux_gtcm_server .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building GTCM_SERVER" "middle"
$	gtcmsrvdsf = ""
$	if dsffiles then gtcmsrvdsf = "/dsf=gtcm_server.dsf"
$	define/user gtcmobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'gtcm_server.map/full/exe='targdir'gtcm_server.exe'lnkopt' 'gtcmsrvdsf' gtcmobj/libr/incl=gtcm_server,target:secshrlink.opt/opt,target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
name = GTCM_SERVER.EXE
$ endif
$!
$! ---------- buildaux gtcm_stop -----------
$!
$ if (buildaux_gtcm_stop .nes. "")
$ then
$	@gtm$tools:build_print_stage "Building GTCM_STOP" "middle"
$!
$!	use production images to compile GTCMSTOP.M
$	@gtm$tools:setactive_silent 'p1' p
$	set def [.obj]
$	set command gtm$src:GTMCOMMANDS.CLDX	! define MUMPS command if .cldx file present
$	mumps gtm$src:gtcmstop.m
$	gtm_library /replace mumps gtcmstop.obj
$	gtm_delete gtcmstop.obj;
$	set def [-]
$	@gtm$tools:setactive_silent 'p1' 'p2' ! revert back to currently building image
$!
$	gtcmstpdsf = ""
$	if dsffiles then gtcmstpdsf = "/dsf=gtcm_stop.dsf"
$	define/user gtcmstopobj 'objdir'mumps.olb
$	define/user target 'targdir'
$	link/map='mapfile'gtcm_stop.map/full/exe='targdir'gtcm_stop.exe gtcmstopobj/incl=(gtcmstop,gtmstopzc,merrors),target:cluster.opt/opt,sys$input/opt,target:release_name.opt/opt
gtm$vrt:[pct]_dh.obj,_exp.obj
name = GTCM_STOP.EXE
$ endif
$!
$! ---------- buildaux ibi_xfm -----------
$!
$ if (buildaux_ibi_xfm .nes. "")
$ then
$	set noon
$	define/user sys$error nl:
$	define/user sys$output nl:
$	gtm_library/extract=dsm_api_vector/output=nl: 'objdir'mumps.olb
$	severity = $severity
$	set on
$	if severity .eq. 1
$	then
$		@gtm$tools:build_print_stage "Building IBI_XFM" "middle"
$		define/user ibiobj 'objdir'mumps.olb
$		define/user target 'targdir'
$		link/share='targdir'ibi_xfm.exe/map='mapfile'ibi_xfm.map/full ibiobj/libr/incl=dsm_api_vector,sys$input/opt,target:cluster.opt/opt,target:release_name.opt/opt
GTMSHR/SHARE
GSMATCH=LEQ,4,0
name=DSM$SHARE
$	endif
$ endif
$!
$! ---------- buildaux ddpserver, ddpgvusr and gtcmddpstop -----------
$!
$ if (buildaux_ddp .nes. "")
$ then
$	set noon
$	define/user sys$error nl:
$	define/user sys$output nl:
$	gtm_library/extract=ddpserver/output=nl: 'objdir'mumps.olb
$	severity = $severity
$	set on
$	if severity .eq. 1
$	then
$		@gtm$tools:build_print_stage "Building DDPSERVER" "middle"
$		define/user ddpobj 'objdir'mumps.olb
$		define/user target 'targdir'
$		link/map='mapfile'ddpserver.map/full/exe='targdir'ddpserver.exe'lnkopt' ddpobj/libr/incl=ddpserver,sys$input/opt,target:cluster.opt/opt,target:release_name.opt/opt
gtmshr/share
gtmsecshr/share
name = DDPSERVER.EXE
$		@gtm$tools:build_print_stage "Building DDPGVUSR" "middle"
$		ddplink ="ddplink." + f$element(alpha,",","vax,axp")
$		define/user ddpobj 'objdir'mumps.olb
$		define/user target 'targdir'
$		link/share='targdir'ddpgvusr.exe'lnkopt'/map='mapfile'DDPGVUSR.MAP/full ddpobj/incl=(DDPGVUSR),gtm$tools:'ddplink'/opt,sys$input/opt,target:release_name.opt/opt
name = DDPGVUSR.EXE
$		if p2 .eqs. "P"
$		then
$			@gtm$tools:build_print_stage "Building GTCMDDPSTOP" "middle"
$!
$			set def [.obj]
$			set command gtm$src:GTMCOMMANDS.CLDX	! define MUMPS command if .cldx file present
$!
$			mumps gtm$src:stpimg.m
$			gtm_library /replace mumps stpimg.obj
$			gtm_delete stpimg.obj;
$!
$			mumps gtm$src:pid.m
$			gtm_library /replace mumps pid.obj
$			gtm_delete pid.obj;
$!
$			set def [-]
$!
$			define/user ddpobj 'objdir'mumps.olb
$			define/user target 'targdir'
$			link/exe='targdir'gtcmddpstop.exe/map='mapfile'gtcmddpstop.map/full ddpobj/incl=(stpimg),sys$input/opt,target:cluster.opt/opt,target:release_name.opt/opt
ddpobj/incl=(pid,gtmstopzc,merrors)
gtm$vrt:[pct]_dh.obj,_exp.obj
SYMBOL=GTM$CTRLC_ENABLE,0
name = GTCMDDPSTOP.EXE
$		endif
$	endif
$ endif
$!
$! ---------- buildaux end -----------
$!
$minimal_build_skip:
$ gtm_delete cluster.opt;*,release_name.opt;*,secshrlink.opt;*
$ if p4 .eqs. ""
$ then
$	gtm_purge
$	gtm_purge [.map]*.*
$ endif
$ set security/protect=(o:rwed,s:rwed,g:re,w:re) *.exe
$ @gtm$tools:build_print_stage "buildaux" "end"
$!
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
