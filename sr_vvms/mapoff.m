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
mapoff(imgoff); get module corresponding to img+offset and print it out
	;
	; ---------- print null string and return if input is null ---------------
	i imgoff="" write ""  quit
	;
	; ---------- get image and offset ------------
	set image=$piece(imgoff,"+",1)
	if (image'=imgoff) set offset=$piece(imgoff,"+",2)
	else  set offset=""
	set image=$tr(image," ","")	; remove blank space
	;
	; ---------- find out if image is part of standard image list -------------
	set imagelst="CMISHR|CRASHANDBURN|DDPGVUSR|DDPSERVER|DSE|GDE|GTCMDDPSTOP|GTCM_SERVER|GTCM_STOP|GTMSECSHR|GTMSHR|LKE|MUPIP"
	set index=0
	for i=1:1:$length(imagelst,"|") quit:index'=0  do
	.	if $piece(imagelst,"|",i)=image set index=i
	;
	; ---------- print imgoff as it was input if image could not be found in imagelst -------
	if index=0 write imgoff  quit
	;
	; ---------- extract module from image map database and print it out ----------
	set image=$tr(image,"_","")
	set str="^"_image
	if (0=$d(str)) write "Error : gtm$map:mapdb.dat not built. Run @gtm$tools:buildmapdb.com first. Exiting...",!  quit
	set offset=$j(offset,8)
	set offset=$tr(offset,"abcdefghijklmnopqrstuvwxyz ","ABCDEFGHIJKLMNOPQRSTUVWXYZ0")
	;
	s endoffset=$order(@str@(""),-1)
	s dendoffset=$$FUNC^%HD(endoffset)
	set doffset=$$FUNC^%HD(offset)
	if doffset>dendoffset write imgoff,!  quit ;
	if (0'=$d(@str@(offset))) set boffset=offset
	else  set boffset=$order(@str@(offset),-1)
	if boffset="" write imgoff,!  quit  ;
	set bdoffset=$$FUNC^%HD(boffset)
	set deltaoff=doffset-bdoffset
	set delta=$$FUNC^%DH(deltaoff)
	set delta=$j(delta,8)
	set delta=$tr(delta,"abcdefghijklmnopqrstuvwxyz ","ABCDEFGHIJKLMNOPQRSTUVWXYZ0")
	set module=@str@(boffset)
	set modulestr=$j(module,20)
	set extension=$piece(module,".",2)
	set extension=$tr(extension,"abcdefghijklmnopqrstuvwxyz ","ABCDEFGHIJKLMNOPQRSTUVWXYZ0")
	if extension'="C" write imgoff,modulestr,!  quit
	if $d(^offset(module))=0  d initoff(module)
	if $d(^offset(module))=0  write "Error creating ^offset(",module,") using initoff^mapoff(",module,")",!  quit
	set origdelta=$extract(delta,5,8)
	if (0=$d(^offset(module,delta))) set delta=$order(^offset(module,delta),-1)
	s sdelta=$extract(delta,5,8)
	write imgoff,modulestr," : line ",$j(^offset(module,delta,0),3)," : ",boffset,"+",origdelta
	write "   [listline ",$j(^offset(module,delta,1),4)," : ",boffset,"+",sdelta,"]",!
	q
	;
initoff(module)
	set dev="nl:"
	open dev
	use dev
	set head=$piece(module,".")
	set name="m"_$e($j,$length($j)-6,$length($j))
	set outfile="gtm$map:"_name_".com"
	open outfile:newversion
	use outfile
	; It is important that NO warnings are issued by the compiler as otherwise this tool's line# mapping scheme fails.
	; Hence the /nowarn below.
	write "$ common_options := /standard=vaxc/share/assume=nowrit/"
	write "float=g_float/inc=(here:,gtm$src:,decw$include,tcpip$examples:)/nowarn",!
	write "$ ccdbg := cc'common_options'/define=(debug,nolicense)/debug/nooptimize",!
	write "$ ccpro := cc'common_options'",!
	write "$    ",!
	write "$ image = f$trnlnm(""gtm$exe"")",!
	write "$ if ((image .nes. ""GTM$PRO"") .and. (image .nes. ""GTM$DBG""))",!
	write "$ then",!
	write "$       write sys$output ""gtm$exe should be either gtm$pro or gtm$dbg. Exiting...""",!
	write "$       exit",!
	write "$ endif ",!
	write "$ set def gtm$obj",!
	write "$ if (image .eqs. ""GTM$PRO"")",!
	write "$ then",!
	write "$        ccpro/machine/list/noobj gtm$src:"_module,!
	write "$ endif ",!
	write "$ if (image .eqs. ""GTM$DBG"")",!
	write "$ then",!
	write "$       ccdbg/machine/list/noobj gtm$src:"_module,!
	write "$ endif ",!
	write "$",!
	write "$ gawk -v module="""_module_""" -f gtm$tools:mapoffset.awk gtm$obj:"_head_".lis >gtm$map:"_name_".m",!
	write "$ gtm",!
	write "if $e($zv,6,9)]""V4.0"" d ^"_name_"  quit",!
	write "; versions <= V4.0 complain mapdb.m has too many literals. so we xecute all contents of mapdb.m instead",!
	write "set file=""gtm$map:"_name_".m"" open file use file read str",!
	write "for  quit:str=""""  xecute str  read str",!
	write "$ ",!
	close outfile
	use $principal
	set zsystr="zsy ""@gtm$map:"_name_""""
	;write zsystr,!
	xecute zsystr
	zsy "delete/nolog gtm$map:"_name_".com.,"_name_".m."
	if $e($zv,6,9)]"V4.0" zsy "delete/nolog gtm$map:"_name_".obj."
	;
	;write $j(module,24)," ",offset," ",boffset," ",delta,!
	;
	use $p
	close dev
	quit
