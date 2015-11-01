;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdeget:	;read in an existing GD or create a default
CONVERT	n accmeth,hdrlab,MAXSEGLN,MAXREGLN,SIZEOF,RMS,csegs,cregs,contents d gdeinit
	;s label=$e(rec,1,12)
	s (v23,v24,v25)=0 										; to allow autoconvert
	i label="GTCGBLDIR001" s label=hdrlab,(v23,v24)=1,update=1					; to allow autoconvert
	i label="GTCGBLDIR002" s label=hdrlab,v24=1,update=1						; to allow autoconvert
	i label="GTCGBLDIR003" s label=hdrlab,v25=1,update=1						; to allow autoconvert
	i label'=hdrlab zm gdeerr("GDUNKNFMT"):file,gdeerr("INPINTEG")
	s filesize=$$bin2num($ze(rec,13,16))
	s header=$ze(rec,17,SIZEOF("gd_header")),abs=abs+SIZEOF("gd_header")
; contents
	i $ze(rec,abs,abs+3)'=$c(0,0,0,0) zm gdeerr("INPINTEG")						; filler
	s abs=abs+4
	s contents("maxrecsize")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("mapcnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("maps")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("regioncnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("regions")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("segmentcnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("segments")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("gdscnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("gdsfiledata")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("rmscnt")=$$bin2num($ze(rec,abs,abs+1)),abs=abs+2
	s contents("rmsfiledata")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s contents("end")=$$bin2num($ze(rec,abs,abs+3)),abs=abs+4
	s abs=abs+22
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
	s x=x+(contents("segmentcnt")*SIZEOF("gd_segment"))
	i x'=contents("gdsfiledata") zm gdeerr("INPINTEG")
	s x=x+(contents("gdscnt")*SIZEOF("gds_filedata"))
	i x'=contents("rmsfiledata") zm gdeerr("INPINTEG")
	s x=x+(contents("rmscnt")*SIZEOF("rms_filedata"))
	i x'=contents("end") zm gdeerr("INPINTEG")
	s rel=abs
; maps - verify that mapped regions and regions are 1-to-1
	k reglist
	f i=1:1:contents("mapcnt") d map
	s s=""
	f i=1:1:contents("regioncnt") s s=$o(reglist(s))
	i $l($o(reglist(s))) zm gdeerr("INPINTEG")
; regions
	k cregs,xregs s cregs=0
	f i=1:1:contents("regioncnt") d region
	i cregs'=contents("regioncnt") zm gdeerr("INPINTEG")
; segments
	k csegs,xsegs s csegs=0
	f i=1:1:contents("segmentcnt") d segment
	i csegs'=contents("segmentcnt") zm gdeerr("INPINTEG")
; gdsfiledata
	f i=1:1:contents("gdscnt") d fabdata(SIZEOF("gds_filedata"),0)
; rmsfiledata
	f i=1:1:contents("rmscnt") d fabdata(SIZEOF("rms_filedata"),SIZEOF("struct RAB"))
; names
	k nams s nams=0
	i $zl(rec)-(rel-1)<3 d nextrec
	s nams=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	f i=1:1:nams d name
; regions
	k regs s regs=0
	i $zl(rec)-(rel-1)<3 d nextrec
	s regs=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i regs<contents("regioncnt") zm gdeerr("INPINTEG")
	f i=1:1:regs d gdereg
; template access method
	s tmpacc=$$gderead(4)
	i accmeth'[("\"_tmpacc) zm gdeerr("INPINTEG")
; segments
	k segs s segs=0
	i $zl(rec)-(rel-1)<3 d nextrec
	s segs=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i segs<contents("segmentcnt") zm gdeerr("INPINTEG")
	f i=1:1:segs d gdeseg
; templates
	;k tmpreg,tmpseg
	f s="ALLOCATION","BEFORE_IMAGE","BUFFER_SIZE","EXTENSION","FILE_NAME" d tmpreg(s)
	i v24 d tmpreg(s) s tmpreg("ALLOCATION")=100,tmpreg("BEFORE_IMAGE")=1			; to allow autoconvert
	i v24 s tmpreg("BUFFER_SIZE")=128,tmpreg("EXTENSION")=100,tmpreg("FILE_NAME")=""	; to allow autoconvert
	;											; ,tmpreg("STOP_ENABLE")=0
	f s="JOURNAL","KEY_SIZE","LOCK_WRITE","NULL_SUBSCRIPTS","RECORD_SIZE" d tmpreg(s) 	;,"STOP_ENABLE"
	f s="ACCESS_METHOD","ALLOCATION","BLOCK_SIZE" d tmpseg("BG",s)
	f s="EXTENSION_COUNT","FILE_TYPE","GLOBAL_BUFFER_COUNT" d tmpseg("BG",s)
	i v23 s tmpseg("BG","LOCK_SPACE")=20							; to allow autoconvert
	e  s s="LOCK_SPACE" d tmpseg("BG",s)							; remove else
	f s="ACCESS_METHOD","ALLOCATION","BLOCK_SIZE","DEFER","EXTENSION_COUNT","FILE_TYPE" d tmpseg("MM",s)
	i v23 s tmpseg("MM","LOCK_SPACE")=20							; to allow autoconvert
	e  s s="LOCK_SPACE" d tmpseg("MM",s)							; remove else
	f s="ACCESS_METHOD","ALLOCATION","BLOCK_SIZE","BUCKET_SIZE","EXTENSION_COUNT" d tmpseg("RMS",s)
	f s="FILE_TYPE","GLOBAL_BUFFER_COUNT","WINDOW_SIZE" d tmpseg("RMS",s)
	i v25 s tmpseg("USER","ACCESS_METHOD")="USER",tmpseg("USER","FILE_TYPE")="DYNAMIC"	; to allow autoconvert
	e  f s="ACCESS_METHOD","FILE_TYPE" d tmpseg("USER",s)					; remove else
; resolve
	s s=""
	f  s s=$o(cregs(s)) q:'$l(s)  i '$d(regs(s)) zm gdeerr("INPINTEG")
	f  s s=$o(csegs(s)) q:'$l(s)  i '$d(segs(s)) zm gdeerr("INPINTEG")
	f  s s=$o(cregs(s)) q:'$l(s)  i '$d(xsegs(cregs(s,"DYNAMIC_SEGMENT"))) zm gdeerr("INPINTEG")
	f  s s=$o(regs(s)) q:'$l(s)  i '$d(segs(regs(s,"DYNAMIC_SEGMENT"))) zm gdeerr("INPINTEG")
	c file
	d tmpres
	f  s s=$o(regs(s)) q:'$l(s)  k regs(s,"LOCK_WRITE") d regres f x=$o(regs(s,x)) q:'$l(x)  d
	. s:$d(minreg(x))&(regs(s,x)<minreg(x)) regs(s,x)=minreg(x)
	. s:$d(maxreg(x))&(regs(s,x)>maxreg(x)) regs(s,x)=maxreg(x)
	f  s s=$o(segs(s)) q:'$l(s)  s am=segs(s,"ACCESS_METHOD"),x="" s:am="RMS" am="BG" d
	. d segres f  s x=$o(segs(s,am,x)) q:x=""  s segs(s,x)=segs(s,am,x) k segs(s,am,x) d
	. . s:$d(minseg(am,x))&(segs(s,x)<minseg(am,x)) segs(s,x)=minseg(am,x)
	. . s:$d(maxseg(am,x))&(segs(s,x)>maxseg(am,x)) segs(s,x)=maxseg(am,x)
	. f i=1:1:$l(accmeth,"\") s x=$p(accmeth,"\",i) i am'=x k segs(s,x)
	k minseg("RMS"),maxseg("RMS"),tmpseg("RMS")
	i tmpacc="RMS" s tmpacc="BG"
	q

tmpres:	k tmpreg("LOCK_WRITE")
	s x="" s x=$o(tmpreg(x)) q:x=""  d
	. s:$d(minreg(x))&(tmpreg(x)<minreg(x)) tmpreg(x)=minreg(x)
	. s:$d(maxreg(x))&(tmpreg(x)>maxreg(x)) tmpreg(x)=maxreg(x)
	s am="" f  s am=$o(tmpseq(am)) q:am=""  s x="" f  s x=$o(tmpseq(am,x)) q:x=""  d
	. s:$d(minseg(am,x))&(tmpseg(am,x)<minseg(am,x)) tmpseg(am,x)=minseg(am,x)
	. s:$d(maxseg(am,x))&(tmpseg(am,x)>maxseg(am,x)) tmpseg(am,x)=maxseg(am,x)
	q

regres:	s x="" f  s x=$o(tmpreg(x)) q:x=""  s:'$d(regs(s,x)) regs(s,x)=tmpreg(x)
	q

segres:	s x="" f  s x=$o(tmpseg(am,x)) q:x=""  s:'$d(segs(s,am,x)) segs(s,am,x)=tmpseg(am,x)
	q

; verify
	s x=$$ALL^GDEVERIF
	i 'x zm gdeerr("INPINTEG")
	q

;----------------------------------------------------------------------------------------------------------------------------------

badfile ;file access failed
	s:'debug $et="" s abortzs=$zs zm gdeerr("GDREADERR"):file,+abortzs
	h
	;
bin2num:(bin)	; binary number -> number
	n num,i
	s num=0
	f i=1:1:$zl(bin) s num=$za(bin,i)*HEX(i-1*2)+num
	q num
	;
str2hex:(in)
	n i,j,out
	s out=""
	f i=1:1 s j=$a(in,i) q:j=-1  f k=j\16,j#16 s out=out_$s(k<10:k,1:$c(k+55))
	q out
	;
num2long:(num)
	i num<0 zm gdeerr("INPINTEG")
	i num'<TWO(32) zm gdeerr("INPINTEG")
	q $c(num/TWO(24),num/TWO(16)#256,num/256#256,num#256)
	;
num2shrt:(num)
	i num<0 zm gdeerr("INPINTEG")
	i num'<TWO(16) zm gdeerr("INPINTEG")
	q $c(num\256,num#256)
	;
dec2hex:(in)
	q $$str2hex($$num2long(in))

;----------------------------------------------------------------------------------------------------------------------------------

map:
	i $zl(rec)-(rel-1)<SIZEOF("gd_map") d nextrec
	s x=$$bin2num($ze(rec,rel+SIZEOF("mident"),rel+SIZEOF("gd_map")-1))
	s reglist(x)="",x=x-contents("regions")
	i x#SIZEOF("gd_region") zm gdeerr("INPINTEG")
	i x\SIZEOF("gd_region")'<contents("regioncnt") zm gdeerr("INPINTEG")
	s rel=rel+SIZEOF("gd_map")
	s abs=abs+SIZEOF("gd_map")
	q
region:
	i $zl(rec)-(rel-1)<SIZEOF("gd_region") d nextrec
	s cregs=cregs+1
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s s=$ze(rec,rel,rel+l-1),rel=rel+MAXSEGLN,xregs(abs-1-SIZEOF("gd_header"))=s
	s cregs(s,"DYNAMIC_SEGMENT")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s x=cregs(s,"DYNAMIC_SEGMENT")-contents("segments")
	i x#SIZEOF("gd_segment") zm gdeerr("INPINTEG")
	i x\SIZEOF("gd_segment")'<contents("segmentcnt") zm gdeerr("INPINTEG")
	i $ze(rec,rel,rel+3)'=$c(0,0,0,0) zm gdeerr("INPINTEG")						; static segment
	s rel=rel+4
	s cregs(s,"RECORD_SIZE")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	s cregs(s,"KEY_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	i $ze(rec,rel)'=ZERO zm gdeerr("INPINTEG")							; OPEN state
	s rel=rel+1
	s cregs(s,"LOCK_WRITE")=$$bin2num($ze(rec,rel)),rel=rel+1
	s cregs(s,"NULL_SUBSCRIPTS")=$$bin2num($ze(rec,rel)),rel=rel+1
	s cregs(s,"JOURNAL")=$$bin2num($ze(rec,rel)),rel=rel+1
	i $ze(rec,rel,rel+7)'=$tr($j("",8)," ",ZERO) zm gdeerr("INPINTEG")				; gbl_lk_root, lcl_lk_root
	s rel=rel+8
	s cregs(s,"ALLOCATION")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4					; journal options
	s cregs(s,"EXTENSION")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s cregs(s,"BUFFER_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s cregs(s,"BEFORE_IMAGE")=$$bin2num($ze(rec,rel)),rel=rel+1
	i 'v23,'v24,$ze(rec,rel,rel+10)'=$tr($j("",11)," ",ZERO) zm gdeerr("INPINTEG")			; 7 filler + 4 for jnllsb
	s rel=rel+11
	s l=$$bin2num($ze(rec,rel)),rel=rel+1
	s cregs(s,"FILE_NAME")=$ze(rec,rel,rel+l-1),rel=rel+SIZEOF("jnl_filename")
	i $ze(rec,rel,rel+46)'=$tr($j("",47)," ",ZERO) zm gdeerr("INPINTEG")				; reserved
	s rel=rel+47
	s abs=abs+SIZEOF("gd_region")
	q
segment:
	i $zl(rec)-(rel-1)<SIZEOF("gd_segment") d nextrec
	s csegs=csegs+1
	s x=$$bin2num($ze(rec,rel+SIZEOF("am_offset"),rel+SIZEOF("am_offset")+3))
	s am=$s(x=0:"RMS",x=1:"BG",x=2:"MM",x=4:"USER",1:"ERROR")
	i am="ERROR" zm gdeerr("INPINTEG")
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s s=$ze(rec,rel,rel+l-1),rel=rel+MAXSEGLN,xsegs(abs-1-SIZEOF("gd_header"))=s
	s (csegs(s,"ACCESS_METHOD"),csegs(s,am,"ACCESS_METHOD"))=am
	s l=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	s csegs(s,am,"FILE_NAME")=$ze(rec,rel,rel+l-1),rel=rel+SIZEOF("file_spec")
	s x=$$bin2num($ze(rec,rel)),rel=rel+1
	s csegs(s,am,"FILE_TYPE")=$s(x=0:"DYNAMIC",1:"ERROR")
	i csegs(s,am,"FILE_TYPE")="ERROR" zm gdeerr("INPINTEG")
	i "USER"=am s rel=rel+8
	e  s csegs(s,am,"BLOCK_SIZE")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	e  s csegs(s,am,"ALLOCATION")=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	e  s csegs(s,am,"EXTENSION_COUNT")=$$bin2num($ze(rec,rel,rel+1)),rel=rel+2
	i $ze(rec,rel,rel+3)'=$tr($j("",4)," ",ZERO) zm gdeerr("INPINTEG")			; reserved for clb
	s rel=rel+4
	i "BG|MM"[am s csegs(s,am,"LOCK_SPACE")=$s(v23:20,1:$$bin2num($ze(rec,rel)))		; allow autoconv
	s rel=rel+1
	i 'v24 i $ze(rec,rel,rel+3)'=".DAT" zm gdeerr("INPINTEG")				; if to allow autoconv
	s rel=rel+4
	i $ze(rec,rel,rel+3)'=$tr($j("",4)," ",ZERO) zm gdeerr("INPINTEG")			; filler
	s rel=rel+4
	s rel=rel+4									; access method already processed
	s xfile(s)=$$bin2num($ze(rec,rel,rel+3)),rel=rel+4
	i "MM"=am s csegs(s,am,"DEFER")=$$bin2num($ze(rec,rel+36))
	s rel=rel+96
	s abs=abs+SIZEOF("gd_segment")
	q
fabdata:(datasize,offset)
	n fna,x,y
	i $zl(rec)-(rel-1)<datasize d nextrec
	s x=$ze(rec,rel+1+offset,rel+datasize-1)
	s fna=$$bin2num($e(x,RMS("FAB$L_FNA"),RMS("FAB$L_FNA")+3))
	s y=fna-contents("segments")
	i y#SIZEOF("gd_segment")'=SIZEOF("fna_offset") zm gdeerr("INPINTEG")
	i y\SIZEOF("gd_segment")'<contents("segmentcnt") zm gdeerr("INPINTEG")
	s y=fna-SIZEOF("fna_offset")
	i '$d(xsegs(y)) zm gdeerr("INPINTEG")
	s s=xsegs(y)
	i '$d(csegs(s,"ACCESS_METHOD")) zm gdeerr("INPINTEG")
	s am=csegs(s,"ACCESS_METHOD")
	i csegs(s,am,"ALLOCATION")'=$$bin2num($e(x,RMS("FAB$L_ALQ"),RMS("FAB$L_ALQ")+3)) zm gdeerr("INPINTEG")
	i csegs(s,am,"EXTENSION_COUNT")'=$$bin2num($e(x,RMS("FAB$W_DEQ"),RMS("FAB$W_DEQ")+1)) zm gdeerr("INPINTEG")
	i "RMS"=am s csegs(s,am,"WINDOW_SIZE")=$$bin2num($e(x,RMS("FAB$B_RTV")))
	s y=$$bin2num($e(x,RMS("FAB$L_DNA"),RMS("FAB$L_DNA")+3))-contents("segments")
	i 'v24 i y#SIZEOF("gd_segment")'=SIZEOF("dna_offset") zm gde("INPINTEG")		; if to allow autoconvert
	i $l(csegs(s,am,"FILE_NAME"))'=$$bin2num($e(x,RMS("FAB$B_FNS"))) zm gdeerr("INPINTEG")
	i 'v24 i $$bin2num($e(x,RMS("FAB$B_DNS")))'=4						; if to allow autoconvert
	i csegs(s,am,"BLOCK_SIZE")'=$$bin2num($e(x,RMS("FAB$W_BLS"),RMS("FAB$W_BLS")+1)) zm gdeerr("INPINTEG")
	i "RMS"=am s csegs(s,am,"BUCKET_SIZE")=$$bin2num($e(x,RMS("FAB$B_BKS")))
	i "BG|RMS"[am s csegs(s,am,"GLOBAL_BUFFER_COUNT")=$$bin2num($e(x,RMS("FAB$W_GBC"),RMS("FAB$W_GBC")+1))
	s rel=rel+datasize,abs=abs+datasize
	q
name:
	i $zl(rec)-(rel-1)<3 d nextrec
	s l=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i l>SIZEOF("mident") zm gdeerr("INPINTEG")
	i $zl(rec)-(rel-1)<l d nextrec
	s s=$ze(rec,rel,rel+l-1),rel=rel+l,abs=abs+l
	i $zl(rec)-(rel-1)<3 d nextrec
	s l=$ze(rec,rel,rel+2),rel=rel+3,abs=abs+3
	i l>MAXREGLN zm gdeerr("INPINTEG")
	i $zl(rec)-(rel-1)<l d nextrec
	s nams(s)=$ze(rec,rel,rel+l-1),rel=rel+l,abs=abs+l
	q
gdereg:
	s s=$$gderead(MAXREGLN)
	s regs(s,"DYNAMIC_SEGMENT")=$$gderead(MAXSEGLN)
	s regs(s,"ALLOCATION")=$$gderead(10)
	s regs(s,"EXTENSION")=$$gderead($l(maxreg("EXTENSION")))
	s regs(s,"FILE_NAME")=$$gderead(SIZEOF("jnl_filename"))
	s regs(s,"BUFFER_SIZE")=$$gderead(5)
	i v24 s regs(s,"ALLOCATION")=100,regs(s,"EXTENSION")=100,regs(s,"FILE_NAME")=""		; to allow autoconvert
	i  s regs(s,"BUFFER_SIZE")=128,regs(s,"BEFORE_IMAGE")=1	;,regs(s,"STOP_ENABLE")=0	; to allow autoconvert
	i  s x=$$gderead(5),x=$$gderead(5)							; to allow autoconvert
	e  s regs(s,"BEFORE_IMAGE")=$$gderead(1) ;s regs(s,""STOP_ENABLE")=$$gderead(1)		; remove else
	s regs(s,"JOURNAL")=$$gderead(1)
	s regs(s,"KEY_SIZE")=$$gderead(5)
	s regs(s,"LOCK_WRITE")=$$gderead(1)
	s regs(s,"NULL_SUBSCRIPTS")=$$gderead(1)
	s regs(s,"RECORD_SIZE")=$$gderead(5)
	q
gdeseg:
	s s=$$gderead(MAXSEGLN)
	s segs(s,"ACCESS_METHOD")=$$gderead(4)
	s segs(s,"BG","ACCESS_METHOD")=$$gderead(4)
	s segs(s,"BG","ALLOCATION")=$$gderead(10)
	s segs(s,"BG","BLOCK_SIZE")=$$gderead($l(maxseg("BG","BLOCK_SIZE")))
	s segs(s,"BG","EXTENSION_COUNT")=$$gderead($l(maxseg("BG","EXTENSION_COUNT")))
	s segs(s,"BG","FILE_NAME")=$$gderead(SIZEOF("file_spec"))
	s segs(s,"BG","FILE_TYPE")=$$gderead(7)
	s segs(s,"BG","GLOBAL_BUFFER_COUNT")=$$gderead(5)
	s segs(s,"BG","LOCK_SPACE")=$s(v23:20,1:$$gderead(3))				; to allow autoconvert
	s segs(s,"MM","ACCESS_METHOD")=$$gderead(4)
	s segs(s,"MM","ALLOCATION")=$$gderead(10)
	s segs(s,"MM","BLOCK_SIZE")=$$gderead($l(maxseg("MM","BLOCK_SIZE")))
	s segs(s,"MM","DEFER")=$$gderead(1)
	s segs(s,"MM","EXTENSION_COUNT")=$$gderead($l(maxseg("MM","EXTENSION_COUNT")))
	s segs(s,"MM","FILE_NAME")=$$gderead(SIZEOF("file_spec"))
	s segs(s,"MM","FILE_TYPE")=$$gderead(7)
	s segs(s,"MM","LOCK_SPACE")=$s(v23:20,1:$$gderead(3))				; to allow autoconvert
	s segs(s,"RMS","ACCESS_METHOD")=$$gderead(4)
	s segs(s,"RMS","ALLOCATION")=$$gderead(10)
	s segs(s,"RMS","BLOCK_SIZE")=$$gderead(5)
	s segs(s,"RMS","BUCKET_SIZE")=$$gderead(5)
	s segs(s,"RMS","EXTENSION_COUNT")=$$gderead($l(maxseg("RMS","EXTENSION_COUNT")))
	s segs(s,"RMS","FILE_NAME")=$$gderead(SIZEOF("file_spec"))
	s segs(s,"RMS","FILE_TYPE")=$$gderead(7)
	s segs(s,"RMS","GLOBAL_BUFFER_COUNT")=$$gderead(5)
	s segs(s,"RMS","WINDOW_SIZE")=$$gderead(5)
	s segs(s,"USER","ACCESS_METHOD")=$s(v25:"USER",1:$$gderead(4))				; to allow autoconvert
	s segs(s,"USER","FILE_NAME")=$s(v25:"MUMPS",1:$$gderead(SIZEOF("file_spec")))		; to allow autoconvert
	s segs(s,"USER","FILE_TYPE")=$s(v25:"DYNAMIC",1:$$gderead(7))				; to allow autoconvert
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
	s tmpseg(a,s)=$ze(rec,rel,rel+l-1),rel=rel+l,abs=abs+l
	q
nextrec:
	n nextrec
	u file r nextrec
	i debug u @useio
	s rec=$ze(rec,rel,$zl(rec))_nextrec,rel=1
	q

;----------------------------------------------------------------------------------------------------------------------------------

gdeinit:
	s accmeth="\BG\MM\RMS\USER"
	s hdrlab="GTCGBLDIR004"
	s SIZEOF("am_offset")=296
	s SIZEOF("blk_hdr")=7
	s SIZEOF("dna_offset")=288
	s SIZEOF("file_spec")=255
	s SIZEOF("fna_offset")=19
	s SIZEOF("gd_header")=288
	s SIZEOF("gd_contents")=64
	s SIZEOF("gd_map")=12
	s SIZEOF("gd_region")=160
	s SIZEOF("gd_segment")=400
	s SIZEOF("gds_filedata")=80
	s SIZEOF("jnl_filename")=49
	s SIZEOF("mident")=8
	s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	s SIZEOF("rms_filedata")=224
	s SIZEOF("struct FAB")=80,SIZEOF("struct RAB")=68
;
	s MAXNAMLN=SIZEOF("mident"),MAXREGLN=15,MAXSEGLN=15
;define offsets into rms structures
	;fab
	;s RMS("FAB$B_BID")=0
	s RMS("FAB$B_BLN")=1
	;s RMS("FAB$W_IFI")=2
	s RMS("FAB$L_FOP")=4
	;s RMS("FAB$L_STS")=8
	s RMS("FAB$L_STV")=12
	s RMS("FAB$L_ALQ")=16
	s RMS("FAB$W_DEQ")=20
	s RMS("FAB$B_FAC")=22
	;s RMS("FAB$B_SHR")=23
	;s RMS("FAB$L_CTX")=24
	s RMS("FAB$B_RTV")=28
	;s RMS("FAB$B_ORG")=29
	;s RMS("FAB$B_RAT")=30
	;s RMS("FAB$B_RFM")=31
	;s RMS("FAB$L_JNL")=32
	s RMS("FAB$L_XAB")=36
	;s RMS("FAB$L_NAM")=40
	s RMS("FAB$L_FNA")=44
	s RMS("FAB$L_DNA")=48
	s RMS("FAB$B_FNS")=52
	s RMS("FAB$B_DNS")=53
	;s RMS("FAB$W_MRS")=54
	;s RMS("FAB$L_MRN")=56
	s RMS("FAB$W_BLS")=60
	s RMS("FAB$B_BKS")=62
	s RMS("FAB$B_FSZ")=63
	;s RMS("FAB$L_DEV")=64
	;s RMS("FAB$L_SDC")=68
	s RMS("FAB$W_GBC")=72
	;s RMS("FAB$B_ACMODES")=74
	;s RMS("FAB$B_RCF")=75
	;rab
	s RMS("RAB$L_FAB")=60
	s RMS("RAB$L_XAB")=64
; rms
	s minseg("RMS","ALLOCATION")=10,minseg("RMS","BLOCK_SIZE")=SIZEOF("dsk_blk"),minseg("RMS","BUCKET_SIZE")=0
	s minseg("RMS","EXTENSION_COUNT")=0,minseg("RMS","GLOBAL_BUFFER_COUNT")=0,minseg("RMS","WINDOW_SIZE")=0
	s maxseg("RMS","ALLOCATION")=TWO(24),maxseg("RMS","BLOCK_SIZE")=HEX(4)-SIZEOF("dsk_blk"),maxseg("RMS","BUCKET_SIZE")=63
	s maxseg("RMS","EXTENSION_COUNT")=HEX(4)-1,maxseg("RMS","GLOBAL_BUFFER_COUNT")=32767,maxseg("RMS","WINDOW_SIZE")=255
	q
