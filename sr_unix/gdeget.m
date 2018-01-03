;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2006-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdeget:	;read in an existing GD or create a default
LOAD
	n abs,contents,rel,xregs,xsegs,reglist,map,$et,ptrsize
	i debug s $et="b"
	e  s $et="g ABORT^GDE:($p($p($zs,"","",3),""-"")'=""%GDE"") u io w !,$p($zs,"","",3,999),! d GETOUT^GDEEXIT zg 0"
	; if zchset is UTF-8 open in raw mode to avoid BADCHAR errors
	; For OS390 aka z/OS, use BINARY mode
	s abs=1,update=0,chset=$SELECT($ZV["OS390":"BINARY",$ZCHSET="UTF-8":"M",1:"")
	o file:(exc="g badfile^"_$t(+0):rewind:recordsize=SIZEOF("dsk_blk"):readonly:fixed:blocksize=SIZEOF("dsk_blk"):ichset=chset)
	u file r rec
	i debug u @useio
; header
	s label=$ze(rec,1,12)
	s olabel=label
	s mach=$p($zver," ",4)
	n gldfmt
	i ($ze(label,1,9)'="GTCGBDUNX") zm gdeerr("GDUNKNFMT"):file,gdeerr("INPINTEG")
	s gldfmt=+$ze(label,11,12)	; 10th byte could be 1 indicating 64-bit platform so ignore that for format check
	s seghasencrflag=FALSE
	i gldfmt>5 s seghasencrflag=TRUE
	s reghasv550fields=FALSE
	i gldfmt>6 s reghasv550fields=TRUE
	s reghasv600fields=FALSE
	i gldfmt>7 s reghasv600fields=TRUE
	s v631=0
	i (label="GTCGBDUNX011")!(label="GTCGBDUNX111") s label=hdrlab,v631=1,update=1  ;autoconvert
	i (v631=1) n SIZEOF d v631init
	s v63a=0
	i (label="GTCGBDUNX010")!(label="GTCGBDUNX110") s label=hdrlab,v63a=1,update=1  ;autoconvert
	i (v63a=1) n SIZEOF d v63ainit
	s v621=0
	i (label="GTCGBDUNX009")!(label="GTCGBDUNX109") s label=hdrlab,v621=1,update=1  ;autoconvert
	i (v621=1) n SIZEOF d v621init
	s v600=0
	i (label="GTCGBDUNX008")!(label="GTCGBDUNX108") s label=hdrlab,v600=1,update=1	;autoconvert
	i (v600=1) n SIZEOF d v600init
	s v550=0
	i (label="GTCGBDUNX007")!(label="GTCGBDUNX107") s label=hdrlab,v550=1,update=1	;autoconvert
	i (v550=1) n SIZEOF d v550init
	s v542=0
	i (label="GTCGBLDIR009")!(label="GTCGBDUNX006")!(label="GTCGBDUNX106") s label=hdrlab,v542=1,update=1	;autoconvert
	i (v542=1) n SIZEOF d v542init
	s v534=0
	i (label="GTCGBDUNX105")!((label="GTCGBDUNX005")&(mach="IA64")) s label=hdrlab,v534=1,update=1   ;autoconvert
	i (v534=1) n SIZEOF d v534init
	s v533=0
	i (gtm64=TRUE),(label="GTCGBDUNX006") s label=hdrlab,v533=1,update=1   ;autoconvert
	i (v533=1) n SIZEOF d v533init
	s v532=0
	i (label="GTCGBLDIR008")!(label="GTCGBDUNX005") s label=hdrlab,v532=1,update=1	;autoconvert
	i (v532=1),(mach'="IA64") n gtm64 s gtm64=FALSE
	i (v532=1),(mach'="IA64") n SIZEOF d v532init
	s v5ft1=0
	i (label="GTCGBLDIR008")!(label="GTCGBDUNX004") s label=hdrlab,v5ft1=1,update=1 ;autoconvert
	i v5ft1=1 n gtm64 s gtm64=FALSE
	i v5ft1=1 n SIZEOF d v5ft1init
	s v44=0
	i (label="GTCGBLDIR007")!(label="GTCGBDUNX003") s label=hdrlab,v44=1,update=1	;autoconvert
	i v44=1 n gtm64 s gtm64=FALSE
	i v44=1 n MAXNAMLN,MAXSEGLN,MAXREGLN,SIZEOF d v44init
	s v30=0
	i (label="GTCGBLDIR006")!(label="GTCGBDUNX002") s label=hdrlab,v30=4,update=1 ;autoconvert
	i v30=4 n gtm64 s gtm64=FALSE
	i label'=hdrlab zm gdeerr("GDUNKNFMT"):file,gdeerr("INPINTEG")
	s filesize=$$bin2num($ze(rec,13,16))
	s abs=abs+SIZEOF("gd_header")
; contents
	s ptrsize=$s((gtm64=TRUE):8,1:4)
	i $ze(rec,abs,abs+ptrsize-1)'=$tr($j("",ptrsize)," ",ZERO) zm gdeerr("INPINTEG")	; gd_addr.local_locks
	s abs=abs+ptrsize
	s contents("maxrecsize")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	n cntsize  s cntsize=$s((gldfmt>8):4,1:2)				; counters are 4-bytes since V6.1
	s contents("mapcnt")=$$bin2num($ze(rec,abs,abs+cntsize-1)),abs=abs+cntsize
	s contents("regioncnt")=$$bin2num($ze(rec,abs,abs+cntsize-1)),abs=abs+cntsize
	s contents("segmentcnt")=$$bin2num($ze(rec,abs,abs+cntsize-1)),abs=abs+cntsize
	i '(gldfmt>8) d
	. i $ze(rec,abs,abs+cntsize-1)'=$tr($j("",cntsize)," ",ZERO) zm gdeerr("INPINTEG") ; filler
	. s abs=abs+cntsize
	. i gtm64=TRUE s abs=abs+4						; including 4-byte padding
	e  d
	. s contents("gblnamecnt")=$$bin2num($ze(rec,abs,abs+cntsize-1)),abs=abs+cntsize
	. s contents("varmapslen")=$$bin2num($ze(rec,abs,abs+cntsize-1)),abs=abs+cntsize
	s contents("maps")=$$bin2num($ze(rec,abs,abs+ptrsize-1)),abs=abs+ptrsize
	s contents("regions")=$$bin2num($ze(rec,abs,abs+ptrsize-1)),abs=abs+ptrsize
	s contents("segments")=$$bin2num($ze(rec,abs,abs+ptrsize-1)),abs=abs+ptrsize
	i (gldfmt>8) s contents("gblnames")=$$bin2num($ze(rec,abs,abs+ptrsize-1)),abs=abs+ptrsize
	i (gldfmt>11) s contents("inst")=$$bin2num($ze(rec,abs,abs+ptrsize-1)),abs=abs+ptrsize
	s abs=abs+(3*ptrsize)							;skip link, tab_ptr and id pointers
	s contents("end")=$$bin2num($ze(rec,abs,abs+ptrsize-1)),abs=abs+ptrsize
	i (gldfmt>8) s abs=abs+16	; reserved for runtime fillers
	i contents("regioncnt")'=contents("segmentcnt") zm gdeerr("INPINTEG")
	i contents("regioncnt")-1>contents("mapcnt") zm gdeerr("INPINTEG")
; verify offsets
	i abs'=(SIZEOF("gd_header")+SIZEOF("gd_contents")+1) zm gdeerr("INPINTEG")
	s x=contents("maps")
	i x+1'=(abs-SIZEOF("gd_header")) zm gdeerr("INPINTEG")
	s x=x+(contents("mapcnt")*SIZEOF("gd_map"))
	i (gldfmt>8)  s x=x+contents("varmapslen")		; add variable maps section too if available
	i x'=contents("regions") zm gdeerr("INPINTEG")
	s x=x+(contents("regioncnt")*SIZEOF("gd_region"))
	i x'=contents("segments") zm gdeerr("INPINTEG")
	s x=x+(contents("segmentcnt")*(SIZEOF("gd_segment")-v30))
	i (gldfmt>8) d
	. i x'=contents("gblnames") zm gdeerr("INPINTEG")
	. s x=x+(contents("gblnamecnt")*(SIZEOF("gd_gblname")))
	i (gldfmt>11),(contents("inst")>0) d
	. i x'=contents("inst") zm gdeerr("INPINTEG")
	. s x=x+(SIZEOF("gd_inst_info"))
	i x'=contents("end") zm gdeerr("INPINTEG")
	s rel=abs
; maps - verify that mapped regions and regions are 1-to-1
	k reglist
	i '(gldfmt>8) d
	. f i=1:1:contents("mapcnt") d mapPreV61
	e  d
	. n maparray,tmpabs,newabs
	. f i=1:1:contents("mapcnt") d mapfixed(i)	; read through fixed    section of MAPS
	. s tmpabs=abs
	. f i=1:1:contents("mapcnt") d mapvariable(i)	; read through variable section of MAPS
	. s newabs=(((abs-1)+(ptrsize-1))\ptrsize*ptrsize)+1 ; make "abs" 8-byte aligned for 64bit platforms (and 4-byte for 32-bit)
	. s rel=rel+(newabs-abs)			; adjust "rel" to take "abs"-alignment-adjustment into account
	. s abs=newabs
	. i (abs-tmpabs)'=contents("varmapslen") zm gdeerr("INPINTEG")
	s s=""
	f i=1:1:contents("regioncnt") s s=$o(reglist(s))
	i $zl($o(reglist(s))) zm gdeerr("INPINTEG")
	i i'=contents("regioncnt") zm gdeerr("INPINTEG")
; regions
	k regs,xregs s regs=0
	f i=1:1:contents("regioncnt") d region
	i regs'=contents("regioncnt") zm gdeerr("INPINTEG")
; segments
	k segs,xsegs s segs=0
	f i=1:1:contents("segmentcnt") d segment
	i segs'=contents("segmentcnt") zm gdeerr("INPINTEG")
; gblnames
	i (gldfmt>8) d
	. k gnams s gnams=0
	. f i=1:1:contents("gblnamecnt") d gblname(i)
	e  s gnams=0
	; wait until "gnams" is setup before checking maps, as "gnams" is used in case of subscripted gvns in map entries
	zm:'$$MAP2NAM^GDEMAP(.map) gdeerr("INPINTEG")
	i (gldfmt'>10) do
	. ; remove any %Y* name mappings in old gld (unsubscripted OR subscripted) as documented
	. n s s s="" f  s s=$o(nams(s)) q:s=""  i $ze(s,1,2)="%Y"  k nams(s)  i $incr(nams,-1)
; instance
	i (gldfmt>11) d
	. k inst s inst=0
	. i contents("inst")>0 d inst
	e  s inst=0
; template access method
	s tmpacc=$$gderead(4)
	i accmeth'[("\"_tmpacc) zm gdeerr("INPINTEG")
; templates
	k tmpreg,tmpseg
	d cretmps
	i (reghasv550fields=TRUE) d tmpreg("ALIGNSIZE")
	s s="ALLOCATION" d tmpreg(s)
	if (gldfmt>10) d tmpreg("AUTODB") s tmpreg("STATS")='(tmpreg("AUTODB")\2#2),tmpreg("AUTODB")=tmpreg("AUTODB")#2
	i (reghasv550fields=TRUE) d tmpreg("AUTOSWITCHLIMIT")
	f s="BEFORE_IMAGE","BUFFER_SIZE" d tmpreg(s)
	i tmpreg("BUFFER_SIZE")<minreg("BUFFER_SIZE")  s tmpreg("BUFFER_SIZE")=minreg("BUFFER_SIZE"),update=1
	i 'v30 d tmpreg("COLLATION_DEFAULT")
	i (gldfmt>9) d tmpreg("EPOCHTAPER")
	i (reghasv550fields=TRUE) d tmpreg("EPOCH_INTERVAL")
	f s="EXTENSION","FILE_NAME" d tmpreg(s)
	i (reghasv600fields=TRUE) d tmpreg("INST_FREEZE_ON_ERROR")
	f s="JOURNAL","KEY_SIZE" d tmpreg(s)
	if (gldfmt>10) d tmpreg("LOCK_CRIT") s tmpreg("LOCK_CRIT")='tmpreg("LOCK_CRIT")
	f s="NULL_SUBSCRIPTS" d tmpreg(s)
	i (reghasv600fields=TRUE) d tmpreg("QDBRUNDOWN")
	f s="RECORD_SIZE" d tmpreg(s)
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
	f i=2:1:$zl(accmeth,"\") s am=$p(accmeth,"\",i) d
	. i am="MM" d:$zl(rec)-(rel-1)<3 nextrec i +$ze(rec,rel,rel+2)'=2 n tmpsegcommon d tmpmm q
	. f s="ACCESS_METHOD","ALLOCATION" d tmpseg(am,s)
	. i (gldfmt>10) d tmpseg(am,"ASYNCIO")
	. f s="BLOCK_SIZE","BUCKET_SIZE","DEFER" d tmpseg(am,s)
	. i (gldfmt>9) d tmpseg(am,"DEFER_ALLOCATE")
	. i (seghasencrflag=TRUE) d tmpseg(am,"ENCRYPTION_FLAG")
	. f s="EXTENSION_COUNT","FILE_TYPE","GLOBAL_BUFFER_COUNT","LOCK_SPACE" d tmpseg(am,s)
	. i (gldfmt>8) d tmpseg(am,"MUTEX_SLOTS")
	. i 'v30 d tmpseg(am,"RESERVED_BYTES")					;autoconvert, can be condensed someday
	. d tmpseg(am,"WINDOW_SIZE")
	.
	c file
; resolve
	s s=""
	f  s s=$o(nams(s)) q:'$zl(s)  zm:'$d(xregs(nams(s))) gdeerr("INPINTEG") s nams(s)=xregs(nams(s)) d:nams(s)?1L.E
	. i $i(nams,-1),$d(regs(nams(s))),$i(regs,-1),$i(segs,-1) k regs(nams(s)),segs(nams(s))
	. k nams(s)
	f  s s=$o(regs(s)) q:'$zl(s)  zm:'$d(xsegs(regs(s,"DYNAMIC_SEGMENT"))) gdeerr("INPINTEG") d
	. s regs(s,"DYNAMIC_SEGMENT")=xsegs(regs(s,"DYNAMIC_SEGMENT"))
	f  s s=$o(segs(s)) q:'$zl(s)  s am=segs(s,"ACCESS_METHOD") d
	. s x=""
	. f  s x=$o(segs(s,x)) q:x=""  d
	. . i x'="FILE_NAME",'$zl(tmpseg(am,x)) zm:segs(s,x) gdeerr("INPINTEG") s segs(s,x)=""
	; fall through !
verify:	s x=$$ALL^GDEVERIF
	i 'x zm gdeerr("INPINTEG")
	q

;----------------------------------------------------------------------------------------------------------------------------------

badfile ;file access failed
	s:'debug $et="" u file:exc="" s abortzs=$zs zm gdeerr("GDREADERR"):file,+abortzs
	d GETOUT^GDEEXIT
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
regoffchk:(x)	; check region offset
	s reglist(x)="",x=x-contents("regions")
	i x#SIZEOF("gd_region") zm gdeerr("INPINTEG")
	i x\SIZEOF("gd_region")'<contents("regioncnt") zm gdeerr("INPINTEG")
	q

mapPreV61:
	i $zl(rec)-(rel-1)<SIZEOF("gd_map") d nextrec
	s s=$ze(rec,rel,rel+SIZEOF("mident")-1),rel=rel+SIZEOF("mident")
	s x=$zf(s,ZERO)-2 i x=-2 s x=SIZEOF("mident")
	s s=$ze(s,1,x)
	s x=$$bin2num($ze(rec,rel,rel+3)),rel=rel+ptrsize ; read 4 bytes, but skip 8 bytes if gtm64
	s map(s)=x
	d regoffchk(x)
	s abs=abs+SIZEOF("gd_map")
	q

mapfixed:(i)
	n regoffset,keyoffset,gvnamelen,gvkeylen
	i $zl(rec)-(rel-1)<SIZEOF("gd_map") d nextrec
	s keyoffset=$$bin2num($ze(rec,rel,rel+3)),rel=rel+ptrsize ; read 4 bytes, but skip 8 bytes if gtm64
	s regoffset=$$bin2num($ze(rec,rel,rel+3)),rel=rel+ptrsize ; read 4 bytes, but skip 8 bytes if gtm64
	s gvnamelen=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s gvkeylen=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s maparray(i,1)=keyoffset
	s maparray(i,2)=regoffset
	s maparray(i,3)=gvnamelen
	s maparray(i,4)=gvkeylen+1	; include second null byte as well
	d regoffchk(regoffset)
	s abs=abs+SIZEOF("gd_map")
	q

mapvariable:(i)
	n keyoffset,regoffset,gvnamelen,gvkeylen,s
	s keyoffset=maparray(i,1)
	s regoffset=maparray(i,2)
	s gvnamelen=maparray(i,3)
	s gvkeylen=maparray(i,4)
	i (keyoffset+1+SIZEOF("gd_header"))'=abs zm gdeerr("INPINTEG")
	i (keyoffset+gvkeylen)>contents("regions") zm gdeerr("INPINTEG")
	f  q:(($zl(rec)-(rel-1))'<gvkeylen)  d nextrec
	s s=$ze(rec,rel,rel+gvkeylen-1)
	i $ze(s,gvnamelen+1)'=ZERO zm gdeerr("INPINTEG")
	i $ze(s,gvkeylen-1)'=ZERO zm gdeerr("INPINTEG")
	i $ze(s,gvkeylen)'=ZERO zm gdeerr("INPINTEG")
	s s=$ze(s,1,gvkeylen-2)
	s map(s)=regoffset
	s rel=rel+gvkeylen
	s abs=abs+gvkeylen
	q

region:
	i $zl(rec)-(rel-1)<SIZEOF("gd_region") d nextrec
	s regs=regs+1
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s s=$ze(rec,rel,rel+l-1),rel=rel+MAXREGLN,xregs(abs-1-SIZEOF("gd_header"))=s
	s regs(s,"KEY_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s regs(s,"RECORD_SIZE")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s regs(s,"DYNAMIC_SEGMENT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+ptrsize
	s x=regs(s,"DYNAMIC_SEGMENT")-contents("segments")
	i x#(SIZEOF("gd_segment")-v30) zm gdeerr("INPINTEG")						; autoconvert
	i x\(SIZEOF("gd_segment")-v30)'<contents("segmentcnt") zm gdeerr("INPINTEG")			; autoconvert
	i $ze(rec,rel,rel+ptrsize-1)'=$tr($j("",ptrsize)," ",ZERO) zm gdeerr("INPINTEG")		; static segment
	s rel=rel+ptrsize
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
	i $ze(rec,rel,rel+7)'=$tr($j("",8)," ",ZERO) zm gdeerr("INPINTEG")				; reserved
	s rel=rel+8											; reserved
	i (gldfmt>8) s rel=rel+4 ; "gd_region.is_spanned" not needed by GDE
	i (gldfmt>10) s rel=rel+4 ; "gd_region.statsDB_reg_index" never used by GDE (generated by gdeput if needed)
	s regs(s,"EPOCHTAPER")=1
	s regs(s,"AUTODB")=0,regs(s,"STATS")=1,regs(s,"LOCK_CRIT")=0
	i (gldfmt>9) do
	. s regs(s,"EPOCHTAPER")=$$bin2num($ze(rec,rel)),rel=rel+1
	. i (gldfmt>10) do
	. . s x=$$bin2num($ze(rec,rel)),regs(s,"AUTODB")=x#2,regs(s,"STATS")='(x\2#2),rel=rel+1
	. . s regs(s,"LOCK_CRIT")='$$bin2num($ze(rec,rel)),rel=rel+1
	. . s rel=rel+34	; reserved for runtime fillers
	. s rel=rel+11		; reserved for runtime fillers
	e  i (gldfmt=9) s rel=rel+12	; reserved for runtime fillers
	s rel=rel+SIZEOF("gd_region_padding")								; padding
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
	i $ze(rec,rel,rel+ptrsize-1)'=$tr($j("",ptrsize)," ",ZERO) zm gdeerr("INPINTEG")	; reserved for clb
	s rel=rel+$s((gtm64=TRUE):12,1:4)							; padding
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
	i 'v30 d
	. s segs(s,"RESERVED_BYTES")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4		;autoconvert
	e  s segs(s,"RESERVED_BYTES")=0
	i (gldfmt>8) s segs(s,"MUTEX_SLOTS")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	e  s segs(s,"MUTEX_SLOTS")=defseg("MUTEX_SLOTS")
	i (gldfmt>9) s segs(s,"DEFER_ALLOCATE")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	e  s segs(s,"DEFER_ALLOCATE")=defseg("DEFER_ALLOCATE")
	s rel=rel+4										; access method already processed
	i (gldfmt=9)&(gtm64=TRUE) s rel=rel+4 							; 4-byte filler
	i $ze(rec,rel,rel+ptrsize-1)'=$tr($j("",ptrsize)," ",ZERO) zm gdeerr("INPINTEG")	; file_cntl pointer
	s rel=rel+ptrsize
	i $ze(rec,rel,rel+ptrsize-1)'=$tr($j("",ptrsize)," ",ZERO) zm gdeerr("INPINTEG")	; repl_list pointer
	s rel=rel+ptrsize
	; If the gld file has the encrytion flag, read it. Also read only if
	; the current platform is encrpytion enabled. Otherwise default to 0
	s segs(s,"ENCRYPTION_FLAG")=$S(((encsupportedplat=TRUE)&(seghasencrflag=TRUE)):$$bin2num($ze(rec,rel,rel+3)),1:0)
	i (seghasencrflag=TRUE) s rel=rel+4
	i (gldfmt>10) s segs(s,"ASYNCIO")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	e  do
	. s segs(s,"ASYNCIO")=defseg("ASYNCIO")
	. if (seghasencrflag=TRUE)&(gtm64=TRUE) s rel=rel+4 ; Padding bytes for 64 bit platforms
	i (gldfmt>8) s rel=rel+16	; reserved for runtime fillers
	s abs=abs+SIZEOF("gd_segment")-v30
	q
gblname:(i)
	n x,y
	i $zl(rec)-(rel-1)<SIZEOF("gd_gblname") d nextrec
	s gnams=gnams+1
	s s=$ze(rec,rel,rel+SIZEOF("mident")-1),rel=rel+SIZEOF("mident")
	s x=$zf(s,ZERO)-2 i x=-2 zm gdeerr("INPINTEG")	; it better be null terminated
	s s=$ze(s,1,x)
	s x=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4 	; read 4 bytes
	i x>maxgnam("COLLATION") zm gdeerr("INPINTEG")	; collation # should be <= 255
	s y=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4 	; read 4 bytes
	d chkcoll^GDEPARSE(x,s,y)
	s gnams(s,"COLLATION")=x
	s gnams(s,"COLLVER")=y
	s abs=abs+SIZEOF("gd_gblname")
	q
inst:
	n x,y
	i $zl(rec)-(rel-1)<SIZEOF("gd_inst_info") d nextrec
	i $zl(rec)-(rel-1)<SIZEOF("gd_inst_info") zm gdeerr("INPINTEG")
	s y=$ze(rec,rel,rel+SIZEOF("gd_inst_info")),rel=rel+SIZEOF("gd_inst_info")
	s x=$zf(y,ZERO)-2 i x=-2 zm gdeerr("INPINTEG")	; it better be null terminated
	s y=$ze(y,1,x)
	s inst("FILE_NAME")=y,inst=1
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
	s tmpseg(a,s)=$s($zl(tmpseg(a,s)):$ze(rec,rel,rel+l-1),1:"") s rel=rel+l,abs=abs+l
	q
nextrec:
	n nextrec
	u file r nextrec
	i debug u @useio
	s rec=$ze(rec,rel,$zl(rec))_nextrec,rel=1
	q
;----------------------------------------------------------------------------------------------------------------------------------

CREATE
	k contents,nams,regs,segs,tmpreg,tmpseg,gnams,inst
	s update=1
	s header=$tr($j("",SIZEOF("gd_header")-16)," ",ZERO)
	s nams=2,(nams("*"),nams("#"))=defreg
	s regs=1,regs(defreg,"DYNAMIC_SEGMENT")=defseg,reg="regs(defreg)"
	s (gnams,inst)=0
	d cretmps
	s x=""
	f  s x=$o(tmpreg(x)) q:'$zl(x)  s @reg@(x)=tmpreg(x)
	s segs=1
	s am=tmpacc d maktseg
	q
cretmps:
	s tmpreg("ALIGNSIZE")=4096
	s tmpreg("ALLOCATION")=2048
	s tmpreg("AUTOSWITCHLIMIT")=8386560
	s tmpreg("AUTODB")=0
	s tmpreg("BEFORE_IMAGE")=1
	s tmpreg("BUFFER_SIZE")=2312
	s tmpreg("COLLATION_DEFAULT")=0
	s tmpreg("EPOCHTAPER")=1
	s tmpreg("EPOCH_INTERVAL")=300
	s tmpreg("EXTENSION")=2048
	s tmpreg("FILE_NAME")=""
	s tmpreg("JOURNAL")=0
	s tmpreg("INST_FREEZE_ON_ERROR")=0
	s tmpreg("KEY_SIZE")=dflreg("KEY_SIZE")
	s tmpreg("LOCK_CRIT")=1
	s tmpreg("NULL_SUBSCRIPTS")=0
	s tmpreg("QDBRUNDOWN")=0
	s tmpreg("RECORD_SIZE")=256
	s tmpreg("STATS")=1
	s tmpreg("STDNULLCOLL")=0
	s tmpreg("SYNC_IO")=0
	s tmpreg("YIELD_LIMIT")=8
	n tmpsegcommon
	; First define segment characteristics that are identical to BG and MM access methods (done inside "tmpmm")
	; Then define overrides specific to BG and MM
	d tmpmm
	m tmpseg("BG")=tmpsegcommon	; copy over all common templates into BG access method first
	; now add BG specific overrides
	s tmpseg("BG","ACCESS_METHOD")="BG"
	s tmpseg("BG","DEFER")=""
	s tmpseg("BG","GLOBAL_BUFFER_COUNT")=defglo
	s tmpacc="BG"	; set default access method to BG
	q
tmpmm:	;
	d tmpsegcommon
	m tmpseg("MM")=tmpsegcommon	; copy over all common stuff into MM access method first
	; now add MM specific overrides
	s tmpseg("MM","ACCESS_METHOD")="MM"
	s tmpseg("MM","DEFER")=1
	s tmpseg("MM","GLOBAL_BUFFER_COUNT")=1024
	q
tmpsegcommon:
	m tmpsegcommon=defseg
	q
	;
maktseg: s segs(defseg,"FILE_NAME")=defdb
	s seg="segs(defseg)",x=""
	f  s x=$o(tmpseg(am,x)) q:'$zl(x)  s @seg@(x)=tmpseg(am,x)
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
	s SIZEOF("blk_hdr")=8
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
	s SIZEOF("blk_hdr")=8
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
	s SIZEOF("blk_hdr")=16
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
	s SIZEOF("blk_hdr")=16
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
v600init:
	i (olabel="GTCGBDUNX008") d
	. s SIZEOF("am_offset")=324
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=44
	. s SIZEOF("gd_map")=36
	. s SIZEOF("gd_region")=356
	. s SIZEOF("gd_region_padding")=0
	. s SIZEOF("gd_segment")=340
	e  d
	. s SIZEOF("am_offset")=332
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=80
	. s SIZEOF("gd_map")=40
	. s SIZEOF("gd_region")=368
	. s SIZEOF("gd_region_padding")=4
	. s SIZEOF("gd_segment")=360
	s SIZEOF("mident")=32
	s SIZEOF("blk_hdr")=16
	s SIZEOF("rec_hdr")=4
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=1048576
	s SIZEOF("reg_jnl_deq")=4
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32	; maximum name length allowed is 31 characters
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	q
v621init:
	i (olabel="GTCGBDUNX009") d
	. s SIZEOF("am_offset")=328
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=76
	. s SIZEOF("gd_map")=16
	. s SIZEOF("gd_region")=372
	. s SIZEOF("gd_region_padding")=0
	. s SIZEOF("gd_segment")=360
	e  d
	. s SIZEOF("am_offset")=336
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=112
	. s SIZEOF("gd_map")=24
	. s SIZEOF("gd_region")=384
	. s SIZEOF("gd_region_padding")=4
	. s SIZEOF("gd_segment")=384
	s SIZEOF("gd_gblname")=40
	s SIZEOF("mident")=32
	s SIZEOF("blk_hdr")=16
	s SIZEOF("rec_hdr")=4
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=1048576
	s SIZEOF("reg_jnl_deq")=4
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32	; maximum name length allowed is 31 characters
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	q
v63ainit:
	i (olabel="GTCGBDUNX010") d
	. s SIZEOF("am_offset")=332
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=76
	. s SIZEOF("gd_map")=16
	. s SIZEOF("gd_region")=372
	. s SIZEOF("gd_region_padding")=0
	. s SIZEOF("gd_segment")=364
	e  d
	. s SIZEOF("am_offset")=340
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=112
	. s SIZEOF("gd_map")=24
	. s SIZEOF("gd_region")=384
	. s SIZEOF("gd_region_padding")=4
	. s SIZEOF("gd_segment")=384
	s SIZEOF("gd_gblname")=40
	s SIZEOF("mident")=32
	s SIZEOF("blk_hdr")=16
	s SIZEOF("rec_hdr")=4
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=1048576
	s SIZEOF("reg_jnl_deq")=4
	d Init^GDEINITSZ
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32	; maximum name length allowed is 31 characters
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	q
v631init:
	i (olabel="GTCGBDUNX011") d
	. s SIZEOF("am_offset")=332
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=76
	. s SIZEOF("gd_map")=16
	. s SIZEOF("gd_region")=412
	. s SIZEOF("gd_region_padding")=0
	. s SIZEOF("gd_segment")=368
	e  d
	. s SIZEOF("am_offset")=340
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=112
	. s SIZEOF("gd_map")=24
	. s SIZEOF("gd_region")=424
	. s SIZEOF("gd_region_padding")=4
	. s SIZEOF("gd_segment")=384
	s SIZEOF("gd_gblname")=40
	s SIZEOF("mident")=32
	s SIZEOF("blk_hdr")=16
	s SIZEOF("rec_hdr")=4
	s SIZEOF("dsk_blk")=512
	s SIZEOF("max_str")=1048576
	s SIZEOF("reg_jnl_deq")=4
	d Init^GDEINITSZ
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32	; maximum name length allowed is 31 characters
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
	q
