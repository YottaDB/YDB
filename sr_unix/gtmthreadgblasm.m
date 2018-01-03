;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2014-2017 Fidelity National Information		;
;  Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Routine to read gtm_threadgbl_deftypes.h and for the given platform and the threadgbl variables defined in the input
; file gtm_threadgbl_asm_access.txt, generate the proper #define type declarations to enable assembler routine access
; to those variables.
;
; Argument: Takes three file names as arguments
;		$gtm_tools/gtm_threadgbl_asm_access.txt
;		<gtm_threadgbl_defs generated input filename>
;		<asm output filename>
;
; Output: This routine writes the following symbol defininitions for each field defined in gtm_threadgbl_asm_access.txt
;
;   - ggo_<fieldname> - Defines the variable's offset within the threadgbl structure.
;   - ggl_<fieldname> - Defines the variable's data length (in bytes)
;
	set accesstxt=$piece($zcmdline," ",1)
	set defsin=$piece($zcmdline," ",2)
	set defsout=$piece($zcmdline," ",3)
	set TAB=$char(9)
	set months="January,February,March,April,May,June,July,August,September,October,November,December"
	;
	; Prefixes (for the desired variable name) we want to collect. All must be the same length
	;
	set varprefixcnt=2			; Count of variables below
	set varprefix("# define ggo_")=1	; Defines variable offset
	set varprefix("# define ggl_")=1	; Defines variable length
	set varprefixlen=$zlength($order(varprefix("")))
	;
	; See which platform we are running on so we know how to generate records for variables
	;
	set platform("AIX","RS6000")="AIXOnPSeries"
	set platform("HP-UX","IA64")="HPUXOnIA64"
	set platform("Linux","x86")="LinuxOnX8632"
	set platform("Linux","x86_64")="LinuxOnX8664"
	set platform("Solaris","SPARC")="SolarisOnSPARC"
	;
	; Note AIX comments handled separated as their comments are the same as C.
	;
	set commentchar("HPUXOnIA64")="//"
	set commentchar("LinuxOnX8632")="#"
	set commentchar("LinuxOnX8664")="#"
	set commentchar("SolarisOnSPARC")="!"
	set gtmzv=$ZVersion
	set gtmos=$ZPiece(gtmzv," ",3)
	set gtmhdwr=$ZPiece(gtmzv," ",4)
	if (0=$get(platform(gtmos,gtmhdwr))) do
	. write "gtmthreadgblasm: Not running on a recognized platform",!
	. set $etrap="zgoto 0"
	. zhalt 1
	set platform=platform(gtmos,gtmhdwr)
	set fmtrtn="formatrecfor"_platform
	;
	; Check argument
	;
	if ($zlength($zcmdline," ")'=3) do
	. write "gtmthreadgblasm: Invalid arguments ",!
	. write "gtmthreadgblasm: $gtm_tools/gtm_threadgbl_asm_access.txt <input file name>.in <output file name>.si",!
	. set $etrap="zgoto 0"
	. zhalt 1
	;
	; Read in variable list to make available to assembler routines
	;
	set infile=accesstxt
	open infile:readonly
	use infile
	set maxvarlen=0
	for i=1:1 do  quit:$zeof
	. read line
	. quit:$zeof
	. quit:(("#"=$extract(line,1))!(""=line))
	. do verify(.line)
	. set globacc(line)=0
	. set globvarlen=$zlength(line)
	. if (maxvarlen<globvarlen) set maxvarlen=globvarlen	; For formatting output
	close infile
	;
	; Now that we have a list of variables to look for, open/read the gtm_threadgbl_deftypes.h file to look for entries
	; to reformat for assembler usage.
	;
	set infile=defsin
	set outfile=defsout
	open infile:readonly
	open outfile:new
	use outfile
	if ("AIXOnPSeries"=platform) do
	. write "/*",!
	. write " * Created by gtmthreadgblasm for ",gtmos," on ",gtmhdwr," (",defsout,")",!
	. write " */",!
	else  do
	. write commentchar(platform),!
	. write commentchar(platform)," Created by gtmthreadgblasm for ",gtmos," on ",gtmhdwr," (",defsout,")",!
	. write commentchar(platform),!
	for  use infile read line quit:$zeof  do
	. quit:("#"'=$zextract(line,1))
	. ;
	. ; We now have a record to check if we need to gen
	. ;
	. quit:(0=$data(varprefix($zextract(line,1,varprefixlen))))	; Not the prefix we were looking for
	. set varname=$zpiece($zextract(line,varprefixlen+1,999)," ",1)
	. quit:(0=$data(globacc(varname)))			; Not for our variable
	. ;
	. ; We now have a record that we want to produce an assembler record for. How we do that depends on our platform
	. ; so do the appropriate platform code.
	. ;
	. set globacc(varname)=globacc(varname)+1		; Bump count for this var
	. set varname=$zpiece(line," ",3)			; This is the full varname with the ggX_ prefix
	. set offorlen=$zpiece(line," ",4)			; offset or length
	. do @fmtrtn@(varname,offorlen)
	close infile
	close outfile
	use $p
	;
	; Now check that each variable we wanted to make sure it was found at least once (may or may not have had a length).
	;
	set idx=""
	for  set idx=$order(globacc(idx)) quit:(""=idx)  do
	. if (0=globacc(idx)) do
	. . write "gtmthreadgblasm: Variable ",idx," was not found in the input file: ",infile," - output file may not be correct",!
	quit


;
; Routine to take a line of input from the gtm_threadgbl_asm_access.txt file and syntax check it. We allow the following:
;
;   - trailing white space
;   - trailing comments preceded by a '#' character potentially preceded by white space
;
; In either of the above conditions, the input line (passed by reference) is stripped of its trailing junk so only the
; variable name itself is left when we return.
;
verify(line)
	new newline,var
	set newline=$translate(line,TAB," ")		; Change TABs to spaces to simplify checks
	set var=$zpiece(newline," ",1)
	if (""=var) do
	. use $principal
	. write "gtmthreadgblasm: invalid character(s) in variable specification on line "_i,!
	. set $etrap="zgoto 0"
	. zhalt 1
	set var=$zpiece(var,"#",1)			; Isolate variable name
	set line=var
	quit

;
; Routine to format record for AIX
;
formatrecforAIXOnPSeries(varname,offorlen)
	use outfile
	write "#define ",varname,?(maxvarlen+8)," ",offorlen,!
	use infile
	quit

;
; Routine to format record for HPUX on IA64
;
formatrecforHPUXOnIA64(varname,offorlen)
	use outfile
	write varname,?(maxvarlen+5),"= ",offorlen,!
	use infile
	quit

;
; Routine to format record for Linux x86-32
;
formatrecforLinuxOnX8632(varname,offorlen)
	use outfile
	write varname,?(maxvarlen+5),"= ",offorlen,!
	use infile
	quit

;
; Routine to format record for Linux x86-64
;
formatrecforLinuxOnX8664(varname,offorlen)
	use outfile
	write varname,?(maxvarlen+5),"= ",offorlen,!
	use infile
	quit

;
; Routine to format record for Solaris SPARC
;
formatrecforSolarisOnSPARC(varname,offorlen)
	use outfile
	write "#define ",varname,?(maxvarlen+8)," ",offorlen,!
	use infile
	quit
