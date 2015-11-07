;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2013 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
main	;	by default go to prompt
prompt	;
	s lower="abcdefghijklmnopqrstuvwxyz",upper="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	For  Read "V[AX] or A[XP]? ",vaxoraxp,! Do  Quit:$Data(vaxoraxp)
	.	Set vaxoraxp=$Translate($Extract(vaxoraxp,1),lower,upper)
	.	If vaxoraxp="A" Set vaxoraxp="AXP"
	.	Else  If vaxoraxp="V" Set vaxoraxp="VAX"
	.	Else  Write "  Should be VAX or AXP",! Kill vaxoraxp
	If vaxoraxp="AXP" Do
	.	Set vaxc=0
	.	For i=0:1  Set x=$ztrnlnm("gtm$libsrc",,i) Quit:x=""  Do
	.	.	If x="T_VMS" Write "GTM$LIBSRC contains VAX directory for AXP platform, aborting list generation",!  Halt
	Else  Do
	.	For i=0:1  Set x=$ztrnlnm("gtm$libsrc",,i) Quit:x=""  Do
	.	.	If x="T_AVMS" Write "GTM$LIBSRC contains AXP directory for VAX platform, aborting list generation",!  Halt
	.	For  Read "D[EC C] or V[AX C]? ",vaxc,! Do  Quit:$Data(vaxc)
	.	.	Set vaxc=$Translate($Extract(vaxc,1),lower,upper)
	.	.	If vaxc="V" Set vaxc=1
	.	.	Else  If vaxc="D" Set vaxc=0
	.	.	Else  Write "  Should be DECC or VAXC",! Kill vaxc
	For  Read "F[ull] or I[ncremental] Version? ",efori,! Do  Quit:$d(efori)
	.	Set efori=$Translate($Extract(efori,1),lower,upper)
	.	If efori="F" Set efori=1
	.	Else  If efori="I" Set efori=0
	.	Else  Write "  Should be F or I",! Kill efori
	For  Read "multiple compilations per line? [Y] ",mcpl,! Do  Quit:$d(mcpl)
	.	Set mcpl=$tr($e(mcpl,1),lower,upper)
	.	If mcpl="Y"!'$l(mcpl) Set mcpl=1
	.	Else  If mcpl="N" Set mcpl=0
	.	Else  Write "  Should be Y or N",! Kill mcpl
	For  Write "VMS version: [",$Select(vaxoraxp="AXP":"V7.2",1:"V7.2") Read "] ",vmsver,! Do  Quit:$d(vmsver)
	.	If vmsver="" Set vmsver=$Select(vaxoraxp="AXP":"V72",1:"V72") Quit
	.	Set vmsver=$TRanslate(vmsver,lower_".",upper)
	.	If vmsver="V72" Quit
	.	If vaxoraxp="AXP",vmsver="V71"!(vmsver="V62")!(vmsver="V73") Quit
	.	If vaxoraxp="VAX",vmsver="V71"!(vmsver="V61")!(vmsver="V55") Quit
	.	Else  Write "  Should be V7.2 or ",$Select(vaxoraxp="AXP":"V7.3, V7.1 or V6.2",1:"V7.1, V6.1 or V5.5"),! Kill vmsver
	do common
	quit
noprompt;	with no input reads for the alpha
	s lower="abcdefghijklmnopqrstuvwxyz",upper="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	set vaxoraxp="AXP"
	set efori=1
	set mcpl=1
	set vmsver="V72"
	set vaxc=0
	do common
	quit
common	;
	k ar,br
	Set ofil=$zparse($ztrnlnm("gtm$libsrc"),"directory")
	If $p(ofil,".")="[LIBRARY",$p(ofil,".",3)="SRC]" s tfil=$p(ofil,".",2)
	Else  If $extract(ofil,1,2)="[V",$piece(ofil,".",2)="SRC]" Set tfil=$extract($piece(ofil,".",1),2,$length(ofil))
	Else  Read "GT.M Version: ",tfil,!
	If tfil="" Write !,"No version specified",! Quit
	Set ofil=$zparse(vaxoraxp_"_gtm$gtmdev:[LIBRARY."_tfil_"]"_vaxoraxp_"_comlist.com")
	if ofil="" w !,"No version structure for "_tfil,! quit
	s (mtables,relname)=0
	f  s x=$zsearch("gtm$libsrc:*.*") q:x=""  s ar($p($p(x,";",1),"]",2))=""
	f  s x=$o(ar(x)) q:x=""  s br($p(x,".",2),$p(x,".",1))="" s:x["MTABLES.C" mtables=1 s:x["RELEASE_NAME.H" relname=1
	i 'relname w !,"WARNING: No RELEASE_NAME.H"
	e  i 'mtables s br("C","MTABLES")="" w !,"Added MTABLES.C to the compile list"
	;
	o ofil:newv u ofil
	d proc
	c ofil
	q
proc	;
	s count=0
	w "$! comlist.com : p1=macro params, p2=cc params, p3=gtm$xxx, p4 = version number, p5 = YES to build",!
	w "$ set verify",!
	w "$ show time",!
	w "$!",!
	Write "$! default to building development version",!
	w "$ if p4 .eqs. """" then p4 = ""d""",!
	w "$!",!
	Write "$! use production images to compile MSG.M",!
	w "$ @gtm$com:setactive 'p4' p",!
	w "$ set noon",!
	w "$ install list gtmsecshr",!
	w "$ if $severity .ne. 1",!
	w "$  then",!
	w "$   pdir = ""gtm$root:["" + f$trnlnm(""gtm$curpro"") + "".pro]""",!
	w "$   define gtmsecshr gtm$sec:'f$trnlnm(""gtm$curpro"")'_gtmsecshr.exe",!
	w "$   define gtmshr 'pdir'gtmshr.exe",!
	w "$   define mcompile 'pdir'mcompile.exe",!
	w "$ endif",!
	w "$ set default 'p3'",!
	w "$ set default [.obj]",!
	w "$!",!
	;
	Set listopts="/nolist"
	s srcdir="gtm$src:",srctail=""
	Set maxlen=0,comend="",search="CLD",exclude="",command="$ set command/object"_listopts Do genlst
	w "$",!
	Set maxlen=0,search="MSG",exclude="",command="$ message/object"_listopts Do genlst
	w "$",!
	w "$! define MUMPS command if .cldx file present",!
	s srcdir="gtm$src:",srctail=""
	Set maxlen=0,comend="",search="CLDX",exclude="",command="$ set command" Do genlst
	w "$",!
	Set search="MSG"
	Write "$ if f$extract(1,2,p4) .lt. 43 then $ goto skpmsg",!
	Write "$ mumps 'f$search(""gtm$src:MSG.M"")'",!
	Write "$ link msg",!
	Write "$ gtm_process_msg == ""$"" + f$environment(""DEFAULT"") + ""msg.exe""",!
	Set pname="",errctl=0
	For  Set pname=$Order(br(search,pname)) Quit:""=pname  Do:pname["ERRORS"
	. Write "$ gtm_process_msg ",srcdir,pname,".",search," VMS",!
	. Set br("C",pname_"_CTL")="",errctl=1
	. If "MERRORS"=pname Write "$ rename/log merrors_ansi.h gtm$src",!
	If errctl Write "$ rename/log *errors_ctl.c gtm$src",!
	Write "$ delete/log msg.obj;",!
	Write "$ delete/log msg.exe;",!
	Write "$ @gtm$tools:gen_gtm_threadgbl_deftypes",!
	Write "$ if 1 .ne. $status",!
	Write "$ then",!
	Write "$        write sys$output ""Failed to build gtm_threadgbl_deftypes.h - aborting build""",!
	Write "$        exit",!
	Write "$ endif",!
	Write "$",!
	Write "$skpmsg:",!
	If efori write "$!",!
	Set srcdir="gtm$src:",srctail=""
	s maxlen=0
	i mcpl s maxlen=100
	w "$ compiler = ""cc""",!
	Set vaxcopts="/vaxc"
	Set deccslit="/assume=nowritable_string_literals"
	Set deccopts="/standard=vaxc/share_globals/float=g_float"
	Set deccopts=deccopts_"/warn=disable=(signedknown,signedmember,questcompare,questcompare1)/inc=(gtm$src:,tcpip$examples:)"
	If (vaxoraxp="VAX")&(vaxc=1) Set opts=vaxcopts
	Else  Set opts=deccopts_deccslit
	Set opts=opts_listopts_"'p2'"
	Write "$ opts = """_opts_"""",!
	Set axpexcl="GTCM_NETERR,GTCM_SERVER,"        ; these sources need different options and header files on AXP VMS
	Set deccexcl="MEMMOVE,SECSHR_DB_CLNUP,SEC_SHR_BLK_BUILD,"	; these need to omit "DECC$" prefix from "MEMMOVE"
	; avoid compiling GTM_THREADGBL_DEFTYPES.C since it is used by gtm$tools:gen_gtm_threadgbl_deftypes.com to
	; build GTM_THREADGBL_DEFTYPES.H. It is not part of the GTM runtime. SE 11/10.
	Set exclude="GTM_THREADGBL_DEFTYPES,"
	If vaxoraxp="AXP" Set exclude=exclude_axpexcl; Do
	.	Set deccaxp="MUPIP_SET_FILE,",exclude=exclude_deccaxp
	If vaxc=0 Set exclude=exclude_deccexcl
	s comend="",search="C",command="$ 'compiler''opts'" d genlst
	If vaxoraxp="AXP" Do
	.	;Write "$ write sys$output ""DECC-E-NFG - REMOVE this nonsense when DECC can compile MUPIP_SET_FILE /optimize""",!
	.	;Write "$ opts = """_deccopts_listopts_"'p2'/noopti""",!
	.	;Write command_$p(deccaxp,",")_".C"_comend,!
	.	; end of MUPIP_SET_FILE nonsense
	.	; N.B.  On AXP VMS V6.1 or later, <NBFDEF.H> has been moved to SYS$LIB_C.TLB; recompile source files that need it.
	.	Write "$ opts = """_deccopts_listopts_"'p2'""",!	; N.B. omit "/assume=nowritable_string_literals"
	.	Set comend="+sys$library:sys$lib_c.tlb/library",command="$ 'compiler''opts'"
	.	For i=1:1 Set pname=$Piece(axpexcl,",",i) Quit:pname=""  Do
	.	.	If $Data(br("C",pname)) Write command_" "_srcdir_pname_".C"_comend,!
	If vaxc=0 Do
	.	w "$! Turn off prefix generation for the name memmove in order to generate references to local version.",!
	.	w "$! Then compile memmove and anything in gtmsecshr that refers to it in order to eliminate",!
	.	w "$! any outbound calls from gtmsecshr (other modules that refer to memmove should use the one",!
	.	w "$! in the DEC C shared executable because it's probably faster).",!
	.	Write "$ opts = """_deccopts_deccslit_listopts_"/prefix=except=(MEMMOVE)'p2'""",!
	.	s comend="",search="C",command="$ 'compiler''opts'"
	.	For i=1:1 Set pname=$Piece(deccexcl,",",i) Quit:pname=""  Do
	.	.	If $Data(br("C",pname)) Write command_" "_srcdir_pname_".C"_comend,!
	If efori Write "$ library/macro/create/log maclib",!  Set libopt="/insert"
	Else     Write "$ copy/log gtm$src:maclib.mlb []",!   Set libopt=""
	Set maxlen=0,comend="",search="MAX",exclude="",command="$ library/macro"_libopt_"/log maclib" Do genlst
	Write "$ copy maclib.mlb gtm$src",!
	Write "$ purge/log maclib.mlb,gtm$src:maclib.mlb",!
	Write "$ library/macro/list gtm$src:maclib",!
	;
	; Native VAX MACRO dialect modules.
	Write "$ compiler = ""macro""",!
	If vaxoraxp="VAX" Write "$ opts = """_listopts_"'p1'""",!
	Else  Write "$ opts = ""/migration/flag=(hints)"_listopts_"'p1'""",!
	Set maxlen=0,comend="+maclib/lib",search="MAR",exclude="",command="$ 'compiler''opts'" Do genlst
	;
	; Native AXP MACRO dialect modules.
	If vaxoraxp="AXP" Do
	.	Write "$ compiler = ""macro""",!
	.	Write "$ opts = ""/alpha_axp"_listopts_"'p1'""",!
	.	Set maxlen=0,comend="+maclib/lib",search="M64",exclude="",command="$ 'compiler''opts'" Do genlst
	If efori Do newolb
	w "$!",!
	If $Data(br("C","DDPGVUSR")) Do
	.	Write "$ library/log/noglobals mumps ddpgvusr.obj",!
	.	Write "$ delete/log ddpgvusr.*;*",!
	w "$ dir *.obj;2",!	; look for duplicates (usually caused by both C and MACRO versions)
	; Macro64 V1.2-108 has a bug which leaves dummy symbols from bndsym
	; gtm_main.m64 does not use bndsym but library/list/name showed it
	; also generated dummy symbols incorrectly.  See mails in C9E07-002614 folder
	; The problem in gtm_main.m64 is using the $call macro with an argument of type /A
	; This bug was fixed by -118 which we no longer have
	For m64bug="CMI_SYMBOLS","DDPGVUSR_SYMBOLS","GTM_MAIN"  Do
	. If $Data(br("M64",m64bug)) Do
	. .	Write "$ library/log mumps "_m64bug_".obj",!
	. .	Write "$ library/list="_m64bug_".name/name/only="_m64bug_" mumps",!
	. .	Write "$ search/noout/nowarn "_m64bug_".name macro64$",!
	. .	Write "$ if $status .ne. %X08D78053  ! NOMATCHES",!
	. .	Write "$ then",!
	. .	Write "$     library/remove=macro64$* mumps",!
	. .	Write "$ endif",!
	. .	Write "$ delete/log "_m64bug_".obj;*,"_m64bug_".name;*",!
	If 'efori Write "$ library/log mumps *.obj",!
	If efori  Write "$ library mumps *.obj",!
	If efori  write "$ library/compress/data=reduce mumps.olb",!
	If efori  Write "$ purge/log mumps.olb",!
	write "$ delete *.obj;*",!
	write "$ show time",!
	write "$ if p5 .nes. ""YES"" then exit",!
	write "$ if p3 .eqs. ""GTM$DBG"" then $@gtm$tools:builddbg 'p4' ",vmsver,!
	write "$ if p3 .eqs. ""GTM$BTA"" then $@gtm$tools:buildbta 'p4' ",vmsver,!
	write "$ if p3 .eqs. ""GTM$PRO"" then $@gtm$tools:buildpro 'p4' ",vmsver,!
	write "$ show time",!
	quit
genlst	s command=command_" "_srcdir,str=command,pname="",linecnt=0
	f  s pname=$o(br(search,pname)) q:pname=""  d
	.	If '(exclude[(pname_",")) Do
	.	.	s count=count+1,linecnt=linecnt+1
	.	.	s x=pname_"."_search_comend
	.	.	i maxlen,$l(x)+$l(str)>maxlen d flush s linecnt=1
	.	.	s str=str_x_","
	.	.	i 'maxlen d flush
	d flush:(maxlen)&(linecnt)
	q
flush	w $e(str,1,$l(str)-1)_srctail,!
	s str=command,linecnt=0
	q
newolb	;
	w "$",!
	w "$ ! ----- create new mumps.olb -------",!
	w "$",!
	w "$ show status",!
	w "$ set noverify",!
	w "$ size = 0",!
	w "$ count = 0",!
	w "$ cntblk:",!
	w "$ x = f$search(""*.obj"")",!
	w "$ if x .nes. """"",!
	w "$  then ",!
	w "$   count = count + 1",!
	w "$   size = size + f$file_attributes(x,""EOF"")",!
	w "$   goto cntblk",!
	w "$ endif",!
	w "$ gblcnt = count + count / 10",!
	w "$ size = size + size / 10",!
	w "$ set verify",!
	w "$ purge/log",!
	w "$ library/create=(history:0,module:'count',global:'gblcnt',block:'size') mumps",!
	q
