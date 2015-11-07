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
$! This DCL command file fetches a version of GT.M and GT.CM (GDP) from the CMS libraries
$! into the appropriate VMS source directory.
$!
$!	p1 - "to version", version of GT.M to populate on the target platform
$!	p2 - name of the platform (e.g., CETUS, ASGARD, etc.)
$!	p3 - CMS library specification for the target platform (e.g., S_VMS, S_AVMS, etc.)
$!      p4 - ignored, only used by UNIX_CMS_LOAD.COM
$!      p5 - "from version", version of GT.M to fetch from the CMS libraries
$!
$  set verify
$  set noon
$  interact = (f$mode() .eqs. "INTERACTIVE")
$!
$  if ( p1 .eqs. "" )
$  then
$!     N.B., there is no need to download V9.9-0 on VMS targets.
$      write sys$output "%CMS_LOAD-E-NOPARAM, Must provide a Version"
$      if ( interact )
$      then
$          inquire "Version ",p1
$      endif
$      if ( p1 .eqs. "" )
$      then
$          write sys$output "No action taken"
$          exit
$      endif
$  endif
$  to_version = ''p1'
$!
$  if ( (p2 .nes. "ASGARD") .and.  (p2 .nes. "ALPHA2") .and. (p2 .nes. "CETUS") .and. (p2 .nes. "WIGLAF") )
$  then
$      write sys$putput "%CMS_LOAD-E-NOTARGET, Must provide a Platform"
$      if ( interact )
$      then
$          inquire "Platform ",p2
$      endif
$      if ( (p2 .nes. "ASGARD") .and. (p2 .nes. "ALPHA2") .and. (p2 .nes. "CETUS") .and. (p2 .nes. "WIGLAF") )
$      then
$          write sys$output "No action taken"
$          exit
$      endif
$  endif
$!
$  dirname = to_version - "." - "-"
$  if ( dirname .eqs. "V990" )
$  then
$      write sys$output "%CMS_LOAD-E-V990, Cannot download to V9.9-0 on VMS"
$      exit
$  endif
$!
$  device = f$trnlnm("gtm$gtmdev")
$  if ( f$locate("_",device) .eq. f$length(device) )
$  then
$      device := gtm$gtmdev
$  endif
$!
$  if ( (p2 .eqs. "ASGARD") .or. (p2 .eqs. "ALPHA2") .or. (p2 .eqs. "WIGLAF") )
$  then
$      type := AXP
$  endif
$!
$  if ( p2 .eqs. "CETUS" )
$  then
$      type := VAX
$  endif
$!
$  toolsdir = "user:[library.''dirname'.tools]"
$!
$  version 'dirname' p
$  if ( f$trnlnm("gtm$verno") .nes. dirname )
$  then
$      write sys$output "%CMS_LOAD-E-VERSIONFAIL, Version command failed"
$      exit
$  endif
$!
$!  set default gtm$vrt:[src]
$  if ( type .eqs. "VAX")
$  then
$      set default 'type'_gtm$gtmdev:[library.'dirname'.src]
$   else
$!     'type'_gtm$gtmdev should ideally be used here, but it is a search list and edrelnam fails for search lists
$!     in fetch_cms_version.com. hence using alternate "user" logical.
$      set default user:[library.'dirname'.src]
$  endif
$!
$!  clean up the source directory
$  if ( f$search("*.c") .nes. "" )
$  then
$      delete/log *.*;*/exclude=(maclib.mlb)
$  endif
$!
$  rename/log maclib.mlb; maclib.mlb;1
$!
$  if ( p3 .eqs. "" )
$  then
$      if ( type .eqs. "AXP" )
$      then
$          p3 = S_AVMS
$      endif
$      if ( type .eqs. "VAX" )
$      then
$          p3 = S_VMS
$      endif
$  endif
$!
$  @'toolsdir'cms_load_verify_from_to_version 'to_version' 'p5'
$  if ( $status .ne. 1 )
$  then
$      exit $status
$  endif
$!
$  t1 = "=" + from_version
$  if (f$extract(0, 3, from_version) .eqs. "V9.") .or. ("NEXT" .eqs. from_version) then $ t1 :=
$  cms set library sl_cmi
$  cms fetch cmierrors.msg /generation't1' ""
$  @'toolsdir'fetch_cms_version 'from_version' 'p3'
$  @'toolsdir'fetch_cms_version 'from_version' S_VMS_CM
$  set def [-]
$!
$! -----------------------------------------------------------------------------
$!           download the CMI sources onto gtm$vrt:[cmi] directory
$! -----------------------------------------------------------------------------
$  if ( f$search("cmi.dir") .eqs. "" )
$  then
$      create/dir [.cmi]
$  else
$      set def [.cmi]
$      delete/log *.*;*
$      set def [-]
$  endif
$  set def [.cmi]
$  @'toolsdir'fetch_cms_version 'from_version' SL_CMI
$  set def [-]
$!
$! ----------------------------------------------------------------------------------
$!           move kit building stuff into gtm$vrt:[tcm,tcx,tdp,tfi,tls,tdc] directories
$! ----------------------------------------------------------------------------------
$  set def [.tcm]
$  rename/log [-.src]GTCMKITHLP.COM []
$  rename/log [-.src]GTCM_SPKITBLD.DAT []
$  rename/log [-.src]GTCMKITINSTAL.COM []KITINSTAL.COM
$  set def [-]
$  set def [.tcx]
$  rename/log [-.src]GTCXKITHLP.COM []
$  rename/log [-.src]GTCX_SPKITBLD.DAT []
$  rename/log [-.src]GTCXKITINSTAL.COM []KITINSTAL.COM
$  set def [-]
$  set def [.tdp]
$  rename/log [-.src]DDPKITHLP.COM []
$  rename/log [-.src]DDP_SPKITBLD.DAT []
$  rename/log [-.src]DDPKITINSTAL.COM []KITINSTAL.COM
$  set def [-]
$  set def [.tdc]
$  rename/log [-.src]GTMDCKITHLP.COM []
$  rename/log [-.src]GTMDC_SPKITBLD.DAT []
$  rename/log [-.src]GTMDCKITINSTAL.COM []KITINSTAL.COM
$  set def [-]
$  set def [.tfi]
$  rename/log [-.src]GTMFIKITHLP.COM []
$  rename/log [-.src]GTMFI_SPKITBLD.DAT []
$  rename/log [-.src]GTMFIKITINSTAL.COM []KITINSTAL.COM
$  set def [-]
$  set def [.tls]
$  rename/log [-.src]GTMKITHLP.COM []
$  rename/log [-.src]GTM_SPKITBLD.DAT []
$  rename/log [-.src]GTMKITINSTAL.COM []KITINSTAL.COM
$  rename/log [-.src]GTM$IVP.TLB []
$  rename/log [-.src]GTM$CE.H []
$  copy/log   [-.src]GTM$DEFAULTS.M64 []	! gtm$src copy is used by the build so take only a copy
$  rename/log [-.src]GTMCOLLECT.OPT []
$  set def [-]
$!
$! -----------------------------------------------------------------------------
$!           move scripts into gtm$vrt:[tools] directory
$! -----------------------------------------------------------------------------
$  set def [.tools]
$  rename/log [-.src]*.com []
$  rename/log [-.src]*.axp []
$  rename/log [-.src]*.awk []
$  purge/log *.*/(excl=vms_cms_load.com,cms_load.com)	! remove versions of *.com files copied over by newincver.com
$				! except cms_load.com and vms_cms_load.com as they are the currently running scripts
$  set def [-]
$!
$! -----------------------------------------------------------------------------
$!           edit gtm$vrt:[t%%]*_spkitbld.dat version ids
$! -----------------------------------------------------------------------------
$  set def [.src]
$  ver p p
$  curr_priv = f$setprv("bypas")
$  gtma := $ gtm$exe:gtm$dmod.exe
$  define/user gtm$routines "[]/src=(gtm$root:[''dirname'.src],gtm$root:[''dirname'.pct])"
$  gtma "user:[library.''dirname']"
d ^spkitbld
$  curr_priv=f$setprv(curr_priv)
$  delete/nolog/since spkitbld.obj.,_ucase.obj.
$!
$!  write sys$output "Please review the version ids in gtm$vrt:[t%%]*_spkitbld.dat"
$!  write sys$output "Please edit GTMSRC.COM and README.TXT as appropriate"
$!
$  exit
