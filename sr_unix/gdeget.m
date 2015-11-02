;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2006, 2012 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdeget:	;read in an existing GD or create a default
LOAD
	n abs,contents,rel,xregs,xsegs,reglist,map,$et
	i debug s $et="b"
	e  s $et="g ABORT^GDE:($p($p($zs,"","",3),""-"")'=""%GDE"") u io w !,$p($zs,"","",3,999),! h"
	; if zchset is UTF-8 open in raw mode to avoid BADCHAR errors
	; For OS390 aka z/OS, use BINARY mode
	s abs=1,update=0,chset=$SELECT($ZV["OS390":"BINARY",$ZV["VMS":"",$ZCHSET="UTF-8":"M",1:"")
	o file:(exc="g badfile":rewind:recordsize=SIZEOF("dsk_blk"):readonly:fixed:blocksize=SIZEOF("dsk_blk"):ichset=chset)
	u file r rec
	i debug u @useio
; header
	s label=$ze(rec,1,12)
	s olabel=label
	s mach=$p($zver," ",4)
	s seghasencrflag=FALSE
	if ($e(label,0,9)="GTCGBDUNX"),$e(label,$l(label)-1,99)>5 s seghasencrflag=TRUE
	s reghasv550fields=FALSE
	if ($e(label,0,9)="GTCGBDUNX"),$e(label,$l(label)-1,99)>6 s reghasv550fields=TRUE
	s reghasv600fields=FALSE
	if ($e(label,0,9)="GTCGBDUNX"),$e(label,$l(label)-1,99)>7 s reghasv600fields=TRUE
	set v550=0
	i (label="GTCGBDUNX007")!(label="GTCGBDUNX107") s label=hdrlab,v550=1,update=1	;autoconvert
	i (v550=1) n SIZEOF d v550init
	set v542=0
	i (label="GTCGBLDIR009")!(label="GTCGBDUNX006")!(label="GTCGBDUNX106") s label=hdrlab,v542=1,update=1	;autoconvert
	i (v542=1) n SIZEOF d v542init
	set v534=0
	i (label="GTCGBDUNX105")!((label="GTCGBDUNX005")&(mach="IA64")) s label=hdrlab,v534=1,update=1   ;autoconvert
	i (v534=1) n SIZEOF d v534init
	set v533=0
	i (gtm64=TRUE),(label="GTCGBDUNX006") s label=hdrlab,v533=1,update=1   ;autoconvert
	i (v533=1) n SIZEOF d v533init
	set v532=0
	i (label="GTCGBLDIR008")!(label="GTCGBDUNX005") s label=hdrlab,v532=1,update=1	;autoconvert
	i (v532=1),(mach'="IA64") n gtm64 s gtm64=FALSE
	i (v532=1),(mach'="IA64") n SIZEOF d v532init
	set v5ft1=0
	i (label="GTCGBLDIR008")!(label="GTCGBDUNX004") s label=hdrlab,v5ft1=1,update=1 ;autoconvert
	i v5ft1=1 n gtm64 s gtm64=FALSE
	i v5ft1=1 n SIZEOF d v5ft1init
	set v44=0
	i (label="GTCGBLDIR007")!(label="GTCGBDUNX003") s label=hdrlab,v44=1,update=1	;autoconvert
	i v44=1 n gtm64 s gtm64=FALSE
	i v44=1 n MAXNAMLN,MAXSEGLN,MAXREGLN,SIZEOF d v44init
	s v30=0
	i (label="GTCGBLDIR006")!(label="GTCGBDUNX002") s label=hdrlab,v30=4,update=1 ;autoconvert
	i v30=4 n gtm64 s gtm64=FALSE
	i label'=hdrlab d cretmps,CONVERT^GDEOGET,verify s encryptsupported=FALSE,update=1 q ;autoconvert
	s filesize=$$bin2num($ze(rec,13,16))
	s abs=abs+SIZEOF("gd_header")
; contents
	i (gtm64=TRUE) d
	. i $ze(rec,abs,abs+7)'=$c(0,0,0,0,0,0,0,0) zm gdeerr("INPINTEG")						; filler
	. s abs=abs+8
	e  d
	. i $ze(rec,abs,abs+3)'=$c(0,0,0,0) zm gdeerr("INPINTEG")						; filler
	. s abs=abs+4
	s contents("maxrecsize")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("mapcnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("regioncnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("segmentcnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	i $ze(rec,abs,abs+1)'=$tr($j("",2)," ",ZERO) zm gdeerr("INPINTEG")				; filler
	i (gtm64=TRUE) d
	. s abs=abs+6											; including padding
	. s contents("maps")=$$bin2num($ze(rec,abs,abs+7)),abs=abs+8
	. s contents("regions")=$$bin2num($ze(rec,abs,abs+7)),abs=abs+8
	. s contents("segments")=$$bin2num($ze(rec,abs,abs+7)),abs=abs+8
	. s abs=abs+24								;skip link, tab_ptr and id pointers
	. s contents("end")=$$bin2num($ze(rec,abs,abs+7)),abs=abs+8
	e  d
	. s abs=abs+2
	. s contents("maps")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	. s contents("regions")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	. s contents("segments")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	. s abs=abs+12								;skip link, tab_ptr and id pointers
	. s contents("end")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	i contents("regioncnt")'=contents("segmentcnt") zm gdeerr("INPINTEG")
	i contents("regioncnt")-1>contents("mapcnt") zm gdeerr("INPINTEG")
; verify offsets
	i abs'=(SIZEOF("gd_header")+SIZEOF("gd_contents")+1) zm gdeerr("INPINTEG")
	s x=contents("maps")
	i x+1'=(abs-SIZEOF("gd_header")) zm gdeerr("INPINTEG")
	s x=x+(contents("mapcnt")*SIZEOF("gd_map"))
	i x'=contents("regions") zm gdeerr("INPINTEG")
	s x=x+(contents("regioncnt")*SIZEOF("gd_region"))
	i x'=contents("segments") zm gdeerr("INPINTEG")
	s x=x+(contents("segmentcnt")*(SIZEOF("gd_segment")-v30))
	i x'=contents("end") zm gdeerr("INPINTEG")
	s rel=abs
; maps - verify that mapped regions and regions are 1-to-1
	k reglist
	f i=1:1:contents("mapcnt") d map
	zm:'$$MAP2NAM^GDEMAP(.map) gdeerr("INPINTEG")
	s s=""
	f i=1:1:contents("regioncnt") s s=$o(reglist(s))
	i $l($o(reglist(s))) zm gdeerr("INPINTEG")
; regions
	k regs,xregs s regs=0
	f i=1:1:contents("regioncnt") d region
	i regs'=contents("regioncnt") zm gdeerr("INPINTEG")
; segments
	k segs,xsegs s segs=0
	f i=1:1:contents("segmentcnt") d segment
	i segs'=contents("segmentcnt") zm gdeerr("INPINTEG")
; template access method
	s tmpacc=$$gderead(4)
	i accmeth'[("\"_tmpacc) zm gdeerr("INPINTEG")
; templates
	k tmpreg,tmpseg
	d cretmps
	i (reghasv550fields=TRUE) d tmpreg("ALIGNSIZE")
	f s="ALLOCATION" d tmpreg(s)
	i (reghasv550fields=TRUE) d tmpreg("AUTOSWITCHLIMIT")
	f s="BEFORE_IMAGE","BUFFER_SIZE" d tmpreg(s)
	i tmpreg("BUFFER_SIZE")<minreg("BUFFER_SIZE")  s tmpreg("BUFFER_SIZE")=minreg("BUFFER_SIZE"),update=1
	i 'v30 d tmpreg("COLLATION_DEFAULT")
	i (reghasv550fields=TRUE) d tmpreg("EPOCH_INTERVAL")
	f s="EXTENSION","FILE_NAME" d tmpreg(s)
	i (reghasv600fields=TRUE) d tmpreg("INST_FREEZE_ON_ERROR")
	f s="JOURNAL","KEY_SIZE","NULL_SUBSCRIPTS" d tmpreg(s)
	i (reghasv600fields=TRUE) d tmpreg("QDBRUNDOWN")
	f s="RECORD_SIZE" d tmpreg(s) ;,"STOP_ENABLE"
	; need to handle versioning
	i 'v44&'v30 d tmpreg("STDNULLCOLL")
	i (reghasv550fields=TRUE) d tmpreg("SYNC_IO")
	i (reghasv550fields=TRUE) d tmpreg("YIELD_LIMIT")
	; minimum allocation/extension was changed in V54001; check if default current value is lower and if so adjust it
	i tmpreg("ALLOCATION")<minreg("ALLOCATION")  s tmpreg("ALLOCATION")=minreg("ALLOCATION"),update=1
	i tmpreg("EXTENSION")<minreg("EXTENSION")  s tmpreg("EXTENSION")=minreg("EXTENSION"),update=1
	i (reghasv550fields=TRUE) i tmpreg("ALIGNSIZE")<minreg("ALIGNSIZE")  s tmpreg("ALIGNSIZE")=minreg("ALIGNSIZE"),update=1
	i (reghasv550fields=TRUE) i tmpreg("AUTOSWITCHLIMIT")<minreg("AUTOSWITCHLIMIT")  d
	. s tmpreg("AUTOSWITCHLIMIT")=minreg("AUTOSWITCHLIMIT"),update=1
	f i=2:1:$l(accmeth,"\") s am=$p(accmeth,"\",i) d
	. i am="MM" d:$zl(rec)-(rel-1)<3 nextrec i +$ze(rec,rel,rel+2)'=2 d tmpmm q
	. f s="ACCESS_METHOD","ALLOCATION","BLOCK_SIZE","BUCKET_SIZE","DEFER" d tmpseg(am,s)
	. i (seghasencrflag=TRUE) d tmpseg(am,"ENCRYPTION_FLAG")
	. f s="EXTENSION_COUNT","FILE_TYPE","GLOBAL_BUFFER_COUNT","LOCK_SPACE" d tmpseg(am,s)
	. i 'v30 d tmpseg(am,"RESERVED_BYTES")					;autoconvert, can be condensed someday
	. d tmpseg(am,"WINDOW_SIZE")
	.
	c file
; resolve
	s s=""
	f  s s=$o(nams(s)) q:'$l(s)  zm:'$d(xregs(nams(s))) gdeerr("INPINTEG") s nams(s)=xregs(nams(s))
	f  s s=$o(regs(s)) q:'$l(s)  zm:'$d(xsegs(regs(s,"DYNAMIC_SEGMENT"))) gdeerr("INPINTEG") d
	. s regs(s,"DYNAMIC_SEGMENT")=xsegs(regs(s,"DYNAMIC_SEGMENT"))
	f  s s=$o(segs(s)) q:'$l(s)  s am=segs(s,"ACCESS_METHOD") d
	. s x=""
	. f  s x=$o(segs(s,x)) q:x=""  d
	. . i x'="FILE_NAME",'$l(tmpseg(am,x)) zm:segs(s,x) gdeerr("INPINTEG") s segs(s,x)=""
	; fall through !
verify:	s x=$$ALL^GDEVERIF
	i 'x zm gdeerr("INPINTEG")
	q

;----------------------------------------------------------------------------------------------------------------------------------

badfile ;file access failed
	s:'debug $et="" u file:exc="" s abortzs=$zs zm gdeerr("GDREADERR"):file,+abortzs
	h
	;
bin2num:(bin)	; binary number -> number
	n num,i
	s num=0
	i endian=TRUE f i=$zl(bin):-1:1 s num=$za(bin,i)*HEX($zl(bin)-i*2)+num
	e  f i=1:1:$zl(bin) s num=$za(bin,i)*HEX(i-1*2)+num
	q num
	;

;----------------------------------------------------------------------------------------------------------------------------------
map:
	i $zl(rec)-(rel-1)<SIZEOF("gd_map") d nextrec
	s s=$ze(rec,rel,rel+SIZEOF("mident")-1),rel=rel+SIZEOF("mident")
	s x=$zf(s,$c(0))-2 i x=-2 s x=SIZEOF("mident")
	s s=$ze(s,1,x)
	i (gtm64=TRUE) s x=$$bin2num($ze(rec,rel,rel+3)),rel=rel+8 ; read 4 bytes, but skip 8 bytes
	e  s x=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s map(s)=x
	s reglist(x)="",x=x-contents("regions")
	i x#SIZEOF("gd_region") zm gdeerr("INPINTEG")
	i x\SIZEOF("gd_region")'<contents("regioncnt") zm gdeerr("INPINTEG")
	s abs=abs+SIZEOF("gd_map")
	q
region:
	i $zl(rec)-(rel-1)<SIZEOF("gd_region") d nextrec
	s regs=regs+1
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s s=$ze(rec,rel,rel+l-1),rel=rel+MAXREGLN,xregs(abs-1-SIZEOF("gd_header"))=s
	s regs(s,"KEY_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s regs(s,"RECORD_SIZE")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	i (gtm64=TRUE) s regs(s,"DYNAMIC_SEGMENT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+8  ; read 4 bytes, but skip 8
	e  s regs(s,"DYNAMIC_SEGMENT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s x=regs(s,"DYNAMIC_SEGMENT")-contents("segments")
	i x#(SIZEOF("gd_segment")-v30) zm gdeerr("INPINTEG")						; autoconvert
	i x\(SIZEOF("gd_segment")-v30)'<contents("segmentcnt") zm gdeerr("INPINTEG")			; autoconvert
	i (gtm64=TRUE) d
	. i $ze(rec,rel,rel+7)'=$c(0,0,0,0,0,0,0,0) zm gdeerr("INPINTEG")				; static segment
	. s rel=rel+8
	e  d
	. i $ze(rec,rel,rel+3)'=$c(0,0,0,0) zm gdeerr("INPINTEG")					; static segment
	. s rel=rel+4
	i $ze(rec,rel)'=ZERO zm gdeerr("INPINTEG")							; OPEN state
	s rel=rel+1
	i $ze(rec,rel)'=ZERO zm gdeerr("INPINTEG")							; lock_write
	s rel=rel+1
	s regs(s,"NULL_SUBSCRIPTS")=$$bin2num($ze(rec,rel)),rel=rel+1
	s regs(s,"JOURNAL")=$$bin2num($ze(rec,rel)),rel=rel+1
	s regs(s,"ALLOCATION")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4					; journal options
	; check if allocation is below new minimums (changed in V54001) if so adjust it to be at least that much
	i regs(s,"ALLOCATION")<minreg("ALLOCATION")  s regs(s,"ALLOCATION")=minreg("ALLOCATION"),update=1
	s regs(s,"EXTENSION")=$$bin2num($ze(rec,rel,rel+SIZEOF("reg_jnl_deq")-1)),rel=rel+SIZEOF("reg_jnl_deq")
	; check if allocation is below new minimums (changed in V54001) if so adjust it to be at least that much
	i regs(s,"EXTENSION")<minreg("EXTENSION")  s regs(s,"EXTENSION")=minreg("EXTENSION"),update=1
	i (reghasv550fields=TRUE)  d
	. s regs(s,"AUTOSWITCHLIMIT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	. s regs(s,"ALIGNSIZE")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	. i regs(s,"ALIGNSIZE")<minreg("ALIGNSIZE")  s regs(s,"ALIGNSIZE")=minreg("ALIGNSIZE"),update=1
	. i regs(s,"AUTOSWITCHLIMIT")<minreg("AUTOSWITCHLIMIT")  s regs(s,"AUTOSWITCHLIMIT")=minreg("AUTOSWITCHLIMIT"),update=1
	. s regs(s,"EPOCH_INTERVAL")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	. s regs(s,"SYNC_IO")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	. s regs(s,"YIELD_LIMIT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	e  d
	. s regs(s,"AUTOSWITCHLIMIT")=8388600
	. s regs(s,"ALIGNSIZE")=4096
	. s regs(s,"EPOCH_INTERVAL")=300
	. s regs(s,"SYNC_IO")=0
	. s regs(s,"YIELD_LIMIT")=8
	s regs(s,"BUFFER_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	i regs(s,"BUFFER_SIZE")<minreg("BUFFER_SIZE")  s regs(s,"BUFFER_SIZE")=minreg("BUFFER_SIZE"),update=1
	s regs(s,"BEFORE_IMAGE")=$$bin2num($ze(rec,rel)),rel=rel+1
	i $ze(rec,rel,rel+3)'=$tr($j("",4)," ",ZERO) zm gdeerr("INPINTEG")				; 4 chars
	s rel=rel+4
	s regs(s,"COLLATION_DEFAULT")=$$bin2num($ze(rec,rel)),rel=rel+1					; default collating type
	; stdnullcoll is applicable from V5
	i 'v44&'v30 s regs(s,"STDNULLCOLL")=$$bin2num($ze(rec,rel))
	e  d
	. i $ze(rec,rel)'=$tr($j("",1)," ",ZERO) zm gdeerr("INPINTEG")					; 1 chars
	. s regs(s,"STDNULLCOLL")=0
	s rel=rel+1
	i (reghasv600fields=TRUE)  d
	. s regs(s,"INST_FREEZE_ON_ERROR")=$$bin2num($ze(rec,rel)),rel=rel+1
	. s regs(s,"QDBRUNDOWN")=$$bin2num($ze(rec,rel)),rel=rel+1
	e  d
	. s regs(s,"INST_FREEZE_ON_ERROR")=0
	. s regs(s,"QDBRUNDOWN")=0
	s l=$$bin2num($ze(rec,rel)),rel=rel+1 ;jnl_file_len
	s regs(s,"FILE_NAME")=$ze(rec,rel,rel+l-1),rel=rel+SIZEOF("file_spec")
	s rel=rel+SIZEOF("gd_region_padding")								; padding
	i $ze(rec,rel,rel+7)'=$tr($j("",8)," ",ZERO) zm gdeerr("INPINTEG")				; reserved
	s rel=rel+8											; reserved
	s abs=abs+SIZEOF("gd_region")
	q
segment:
	i $zl(rec)-(rel-1)<(SIZEOF("gd_segment")-v30) d nextrec						; autoconvert
	s segs=segs+1
	s x=$$bin2num($ze(rec,rel+SIZEOF("am_offset")-v30,rel+SIZEOF("am_offset")-v30+3))		; autoconvert
	s am=$s(x=1:"BG",x=2:"MM",x=4:"USER",1:"ERROR")
	i am="ERROR" zm gdeerr("INPINTEG")
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s s=$ze(rec,rel,rel+l-1),rel=rel+MAXSEGLN,xsegs(abs-1-SIZEOF("gd_header"))=s
	s segs(s,"ACCESS_METHOD")=am
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s segs(s,"FILE_NAME")=$ze(rec,rel,rel+l-1),rel=rel+SIZEOF("file_spec")
	s segs(s,"BLOCK_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s segs(s,"EXTENSION_COUNT")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s segs(s,"ALLOCATION")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	i (gtm64=TRUE) d
	. i $ze(rec,rel,rel+7)'=$tr($j("",8)," ",ZERO) zm gdeerr("INPINTEG")			; reserved for clb
	. s rel=rel+12						; padding
	e  d
	. i $ze(rec,rel,rel+3)'=$tr($j("",4)," ",ZERO) zm gdeerr("INPINTEG")			; reserved for clb
	. s rel=rel+4
	i $ze(rec,rel,rel+3)'=".DAT" zm gdeerr("INPINTEG")
	s rel=rel+4
	s segs(s,"DEFER")=$$bin2num($ze(rec,rel))
	s rel=rel+1
	s x=$$bin2num($ze(rec,rel)),rel=rel+1
	s segs(s,"FILE_TYPE")=$s(x=0:"DYNAMIC",1:"ERROR")
	i segs(s,"FILE_TYPE")="ERROR" zm gdeerr("INPINTEG")
	s segs(s,"BUCKET_SIZE")=$$bin2num($ze(rec,rel))
	s rel=rel+1
	s segs(s,"WINDOW_SIZE")=$$bin2num($ze(rec,rel))
	s rel=rel+1
	s segs(s,"LOCK_SPACE")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s segs(s,"GLOBAL_BUFFER_COUNT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	i 'v30 s segs(s,"RESERVED_BYTES")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4		;autoconvert
	e  s segs(s,"RESERVED_BYTES")=0
	s rel=rel+4										; access method already processed
	i (gtm64=TRUE) d
	. i $ze(rec,rel,rel+7)'=$tr($j("",8)," ",ZERO) zm gdeerr("INPINTEG")			; file_cntl pointer
	. s rel=rel+8
	. i $ze(rec,rel,rel+7)'=$tr($j("",8)," ",ZERO) zm gdeerr("INPINTEG")			; repl_list pointer
	. s rel=rel+8
	e  d
	. i $ze(rec,rel,rel+3)'=$tr($j("",4)," ",ZERO) zm gdeerr("INPINTEG")			; file_cntl pointer
	. s rel=rel+4
	. i $ze(rec,rel,rel+3)'=$tr($j("",4)," ",ZERO) zm gdeerr("INPINTEG")			; repl_list pointer
	. s rel=rel+4
	; If the gld file has the encrytion flag, read it. Also read only if
	; the current platform is encrpytion enabled. Otherwise default to 0
	s segs(s,"ENCRYPTION_FLAG")=$S(((encsupportedplat=TRUE)&(seghasencrflag=TRUE)):$$bin2num($ze(rec,rel,rel+3)),1:0)
	i (seghasencrflag=TRUE) d
	. s rel=rel+4
        . if (gtm64=TRUE) s rel=rel+4 ; Padding bytes for 64 bit platforms
	s abs=abs+SIZEOF("gd_segment")-v30
	q
gderead:(max)
	n s
	i $zl(rec)-(rel-1)<3 d nextrec
	s l=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i l>max zm gdeerr("INPINTEG")
	i $zl(rec)-(rel-1)<l d nextrec
	s s=$ze(rec,rel,rel+l-1),rel=rel+l,abs=abs+l
	q s
	;
tmpreg:(s)
	i $zl(rec)-(rel-1)<3 d nextrec
	s l=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i $zl(rec)-(rel-1)<l d nextrec
	s tmpreg(s)=$ze(rec,rel,rel+l-1),rel=rel+l,abs=abs+l
	q
tmpseg:(a,s)
	i $zl(rec)-(rel-1)<3 d nextrec
	s l=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i $zl(rec)-(rel-1)<l d nextrec
	s tmpseg(a,s)=$s($l(tmpseg(a,s)):$ze(rec,rel,rel+l-1),1:"") s rel=rel+l,abs=abs+l
	q
nextrec:
	n nextrec
	u file r nextrec
	i debug u @useio
	s rec=$ze(rec,rel,$zl(rec))_nextrec,rel=1
	q
;----------------------------------------------------------------------------------------------------------------------------------

CREATE
	k contents,nams,regs,segs,tmpreg,tmpseg
	s update=1
	s header=$tr($j("",SIZEOF("gd_header")-16)," ",ZERO)
	s nams=2,(nams("*"),nams("#"))=defreg
	s regs=1,regs(defreg,"DYNAMIC_SEGMENT")=defseg,reg="regs(defreg)"
	d cretmps
	s x=""
	f  s x=$o(tmpreg(x)) q:'$l(x)  s @reg@(x)=tmpreg(x)
	s segs=1
	s am=tmpacc d maktseg
	q
cretmps:
	s tmpreg("ALIGNSIZE")=4096
	s tmpreg("ALLOCATION")=2048
	s tmpreg("AUTOSWITCHLIMIT")=8386560
	s tmpreg("BEFORE_IMAGE")=1
	s tmpreg("BUFFER_SIZE")=2308
	s tmpreg("COLLATION_DEFAULT")=0
	s tmpreg("EPOCH_INTERVAL")=300
	s tmpreg("EXTENSION")=2048
	s tmpreg("FILE_NAME")=""
	s tmpreg("JOURNAL")=0
	s tmpreg("INST_FREEZE_ON_ERROR")=0
	s tmpreg("KEY_SIZE")=64
	s tmpreg("NULL_SUBSCRIPTS")=0
	s tmpreg("QDBRUNDOWN")=0
	s tmpreg("RECORD_SIZE")=256
	s tmpreg("STDNULLCOLL")=0
	s tmpreg("SYNC_IO")=0
	s tmpreg("YIELD_LIMIT")=8
	;s tmpreg("STOP_ENABLED")=1
	s tmpseg("BG","ACCESS_METHOD")="BG"
	s tmpseg("BG","ALLOCATION")=100
	s tmpseg("BG","BLOCK_SIZE")=1024
	s tmpseg("BG","BUCKET_SIZE")=""
	s tmpseg("BG","DEFER")=""
	s tmpseg("BG","EXTENSION_COUNT")=100
	s tmpseg("BG","FILE_TYPE")="DYNAMIC"
	s tmpseg("BG","GLOBAL_BUFFER_COUNT")=defglo
	s tmpseg("BG","RESERVED_BYTES")=0
	s tmpseg("BG","LOCK_SPACE")=40
	s tmpseg("BG","WINDOW_SIZE")=""
	s tmpseg("BG","ENCRYPTION_FLAG")=0
	d tmpmm
	s tmpseg("USER","ACCESS_METHOD")="USER"
	s tmpseg("USER","ALLOCATION")=""
	s tmpseg("USER","BLOCK_SIZE")=""
	s tmpseg("USER","BUCKET_SIZE")=""
	s tmpseg("USER","DEFER")=""
	s tmpseg("USER","EXTENSION_COUNT")=""
	s tmpseg("USER","FILE_TYPE")="DYNAMIC"
	s tmpseg("USER","GLOBAL_BUFFER_COUNT")=""
	s tmpseg("USER","RESERVED_BYTES")=0
	s tmpseg("USER","LOCK_SPACE")=""
	s tmpseg("USER","WINDOW_SIZE")=""
	s tmpseg("USER","ENCRYPTION_FLAG")=0
	s tmpacc="BG"
	q
tmpmm:	s tmpseg("MM","ACCESS_METHOD")="MM"
	s tmpseg("MM","ALLOCATION")=100
	s tmpseg("MM","BLOCK_SIZE")=1024
	s tmpseg("MM","BUCKET_SIZE")=""
	s tmpseg("MM","DEFER")=1
	s tmpseg("MM","EXTENSION_COUNT")=100
	s tmpseg("MM","FILE_TYPE")="DYNAMIC"
	s tmpseg("MM","GLOBAL_BUFFER_COUNT")=1024
	s tmpseg("MM","RESERVED_BYTES")=0
	s tmpseg("MM","LOCK_SPACE")=40
	s tmpseg("MM","WINDOW_SIZE")=""
	s tmpseg("MM","ENCRYPTION_FLAG")=0
	q
maktseg:	s segs(defseg,"FILE_NAME")=defdb
	s seg="segs(defseg)",x=""
	f  s x=$o(tmpseg(am,x)) q:'$l(x)  s @seg@(x)=tmpseg(am,x)
	q
v44init:
	s SIZEOF("am_offset")=308
	s SIZEOF("file_spec")=256
	s SIZEOF("gd_header")=16
	s SIZEOF("gd_contents")=44
	s SIZEOF("gd_map")=12
	s SIZEOF("gd_region")=316
	s SIZEOF("gd_segment")=320
	s SIZEOF("mident")=8
	s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=32767
	s SIZEOF("reg_jnl_deq")=2
	s SIZEOF("gd_region_padding")=0
	s MAXNAMLN=SIZEOF("mident"),MAXREGLN=16,MAXSEGLN=16
	i ver'="VMS" s SIZEOF("blk_hdr")=8
	e  s SIZEOF("blk_hdr")=7
	q
v5ft1init:
	s SIZEOF("am_offset")=324
	s SIZEOF("file_spec")=256
	s SIZEOF("gd_header")=16
	s SIZEOF("gd_contents")=44
	s SIZEOF("gd_map")=36
	s SIZEOF("gd_region")=332
	s SIZEOF("gd_segment")=336
	s SIZEOF("mident")=32
	s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=32767
	s SIZEOF("reg_jnl_deq")=2
	s SIZEOF("gd_region_padding")=0
	i ver'="VMS" s SIZEOF("blk_hdr")=8
	e  s SIZEOF("blk_hdr")=7
	q
v532init:
	s SIZEOF("am_offset")=324
	s SIZEOF("file_spec")=256
	s SIZEOF("gd_header")=16
	s SIZEOF("gd_contents")=44
	s SIZEOF("gd_map")=36
	s SIZEOF("gd_region")=332
	s SIZEOF("gd_segment")=336
	s SIZEOF("mident")=32
	s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=32767
	s SIZEOF("reg_jnl_deq")=2
	i (gtm64=TRUE) s SIZEOF("gd_region_padding")=4
	e  s SIZEOF("gd_region_padding")=0
	;s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32
	;s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	i ver'="VMS" s SIZEOF("blk_hdr")=16
	e  s SIZEOF("blk_hdr")=7
	q
v533init:
	s SIZEOF("am_offset")=324
	s SIZEOF("file_spec")=256
	s SIZEOF("gd_header")=16
	s SIZEOF("gd_contents")=44
	s SIZEOF("gd_map")=36
	s SIZEOF("gd_region")=332
	s SIZEOF("gd_segment")=340
	s SIZEOF("mident")=32
	s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=32767
	s SIZEOF("reg_jnl_deq")=2
	i (gtm64=TRUE) s SIZEOF("gd_region_padding")=4
	e  s SIZEOF("gd_region_padding")=0
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	i ver'="VMS" s SIZEOF("blk_hdr")=16
	e  s SIZEOF("blk_hdr")=7
	q
v534init:
        s SIZEOF("am_offset")=332
        s SIZEOF("file_spec")=256
        s SIZEOF("gd_header")=16
        s SIZEOF("gd_contents")=80
        s SIZEOF("gd_map")=40
        s SIZEOF("gd_region")=344
        s SIZEOF("gd_segment")=352
        s SIZEOF("mident")=32
        s SIZEOF("blk_hdr")=16
        s SIZEOF("rec_hdr")=3
        s SIZEOF("dsk_blk")=512
        s SIZEOF("max_str")=32767
        s SIZEOF("reg_jnl_deq")=2
	i (gtm64=TRUE) s SIZEOF("gd_region_padding")=4
	e  s SIZEOF("gd_region_padding")=0
        s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32   ; maximum name length allowed is 31 characters
        s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
        q
v542init:
        i (olabel="GTCGBLDIR009")!(olabel="GTCGBDUNX006") d		; 32-bit
        . s SIZEOF("am_offset")=324
        . s SIZEOF("file_spec")=256
        . s SIZEOF("gd_header")=16
        . s SIZEOF("gd_contents")=44
        . s SIZEOF("gd_map")=36
        . s SIZEOF("gd_region")=332
        . s SIZEOF("gd_region_padding")=0
        . if (olabel="GTCGBLDIR009") s SIZEOF("gd_segment")=336		; VMS
        . e  s SIZEOF("gd_segment")=340
        e  d								; 64-bit
        . s SIZEOF("am_offset")=332
        . s SIZEOF("file_spec")=256
        . s SIZEOF("gd_header")=16
        . s SIZEOF("gd_contents")=80
        . s SIZEOF("gd_map")=40
        . s SIZEOF("gd_region")=344
        . s SIZEOF("gd_region_padding")=4
        . s SIZEOF("gd_segment")=360
        s SIZEOF("mident")=32
        s SIZEOF("blk_hdr")=16
        s SIZEOF("rec_hdr")=3
        s SIZEOF("dsk_blk")=512
        s SIZEOF("max_str")=32767
        s SIZEOF("reg_jnl_deq")=2
        s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32   ; maximum name length allowed is 31 characters
        s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
        q
v550init:
	i (olabel="GTCGBDUNX007") d
	. s SIZEOF("am_offset")=324
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=44
	. s SIZEOF("gd_map")=36
	. s SIZEOF("gd_region")=356
	. s SIZEOF("gd_region_padding")=2
	. s SIZEOF("gd_segment")=340
	e  d
	. s SIZEOF("am_offset")=332
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=80
	. s SIZEOF("gd_map")=40
	. s SIZEOF("gd_region")=368
	. s SIZEOF("gd_region_padding")=6
	. s SIZEOF("gd_segment")=360
	s SIZEOF("mident")=32
	s SIZEOF("blk_hdr")=16
	s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=32767
	s SIZEOF("reg_jnl_deq")=4
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32	; maximum name length allowed is 31 characters
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	q
