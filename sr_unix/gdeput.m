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
gdeput:	;output the result of the session to the global directory file
GDEPUT()
	n rec,gds,cregs,csegs,cregcnt,csegcnt,maxrecsize,mapcnt,map,hasSpanGbls,isSpanned,curMapSpanning,prevMapSpanning
	n varmapslen,vargblnamelen,tmplen,ptrsize,varmapoff,gnamcnt,gblnamelen,filler16byte,filler12byte,filler45byte
	d gblstatmap,CREATEGLDMAP^GDEMAP
	s ptrsize=$s((gtm64=TRUE):8,1:4)
	s s="",gdeputzs="",varmapslen=0,mapcnt=0,hasSpanGbls=0
	f  s s=$o(map(s)),tmplen=$zl(s) q:'tmplen  d
	. s cregs(map(s))=""
	. s varmapslen=(varmapslen+tmplen+2) ; varmapslen needs to account 2 null terminating bytes
	. s varmapslen($incr(mapcnt))=tmplen
	. s gblnamelen=$zf(s,ZERO)
	. i gblnamelen'=0 s hasSpanGbls=1
	. s gblnamelen=$s(gblnamelen=0:tmplen,1:gblnamelen-2)
	. s vargblnamelen(mapcnt)=gblnamelen
	s varmapslen=(varmapslen+ptrsize-1)\ptrsize*ptrsize	; do 8-byte or 4-byte rounding up as appropriate
	s maxrecsize=0
	f cregcnt=0:1 s s=$o(cregs(s)) q:'$l(s)  d
	. s csegs(regs(s,"DYNAMIC_SEGMENT"))=s i maxrecsize<regs(s,"RECORD_SIZE") s maxrecsize=regs(s,"RECORD_SIZE")
	. s isSpanned(s)=0
	f csegcnt=0:1 s s=$o(csegs(s)) q:'$l(s)  d fdatum
	i cregcnt'=csegcnt d error1
	s x=SIZEOF("gd_contents")+(mapcnt*SIZEOF("gd_map"))+varmapslen,s=""
	f i=0:1 s s=$o(cregs(s)) q:'$l(s)  s cregs(s,"offset")=i*SIZEOF("gd_region")+x
	s x=x+(cregcnt*SIZEOF("gd_region"))
	f i=0:1 s s=$o(csegs(s)) q:'$l(s)  s csegs(s,"offset")=i*SIZEOF("gd_segment")+x
	s x=x+(csegcnt*SIZEOF("gd_segment"))
	s gnamcnt=gnams
	s rec=""
	s $p(filler45byte,ZERO,45)=ZERO				; this may be used by the runtime logic
	s $p(filler12byte,ZERO,12)=ZERO
	s $p(filler16byte,ZERO,16)=ZERO
; contents
	s rec=rec_$tr($j("",ptrsize)," ",ZERO)			; not used (gd_addr.local_locks)
	s rec=rec_$$num2bin(4,maxrecsize)			; max rec size
	s filesize=SIZEOF("gd_contents")
	s rec=rec_$$num2bin(4,mapcnt)_$$num2bin(4,cregcnt)	; maps,regs
	s rec=rec_$$num2bin(4,csegcnt)_$$num2bin(4,gnamcnt) 	; segs,gblnames
	s rec=rec_$$num2bin(4,varmapslen)			; varmapslen
	s rec=rec_$$num2bin(ptrsize,filesize)			; mapptr
	s varmapoff=filesize+(mapcnt*SIZEOF("gd_map"))     	; offset of variable length map section
	s filesize=varmapoff+varmapslen
	s rec=rec_$$num2bin(ptrsize,filesize)			; regionptr
	s filesize=filesize+(cregcnt*SIZEOF("gd_region"))
	s rec=rec_$$num2bin(ptrsize,filesize)			; segmentptr
	s filesize=filesize+(csegcnt*SIZEOF("gd_segment"))
	s rec=rec_$$num2bin(ptrsize,filesize)			; gblnameptr
	s filesize=filesize+(gnamcnt*SIZEOF("gd_gblname"))
	i inst>0 d
	. s rec=rec_$$num2bin(ptrsize,filesize)			; instptr
	. s filesize=filesize+SIZEOF("gd_inst_info")
	e  s rec=rec_$tr($j("",ptrsize)," ",ZERO)		; no inst info
	s rec=rec_$tr($j("",(3*ptrsize))," ",ZERO)		; reserved
	s rec=rec_$$num2bin(ptrsize,filesize)			; end
	s rec=rec_$$num2bin(4,hasSpanGbls)			; has_span_gbls
	s rec=rec_filler12byte					; for runtime filler
	s rec=hdrlab_$$num2bin(4,$l(hdrlab)+4+filesize)_rec
	i create zm gdeerr("GDCREATE"):file
	e  zm gdeerr("GDUPDATE"):file
	s gdexcept="s gdeputzs=$zs  zgoto "_$zl_":writeerr^GDEPUT"
	s tempfile=file_"inprogress"
	;if zchset is UTF-8 open in raw mode to avoid BADCHAR errors
	; For OS390 aka z/OS, use BINARY mode
	s chset=$SELECT($ZV["OS390":"BINARY",$ZCHSET="UTF-8":"M",1:"")
	o tempfile:(rewind:noreadonly:newversion:recordsize=512:fixed:blocksize=512:exception=gdexcept:ochset=chset)
; maps
	s s="",curMapSpanning=0,prevMapSpanning=0
	f i=1:1 s s=$o(map(s)) q:'$zl(s)  d mapfixed(i,s)
	s s=""
	f i=1:1 s s=$o(map(s)) q:'$zl(s)  d mapvariable(i,s)
	; write padding if not 8-byte (on 64-bit) or 4-byte (on 32-bit platform) aligned after variable maps section
	s tmplen=varmapoff#ptrsize
	i tmplen d
	. s tmplen=ptrsize-tmplen
	. s rec=rec_$tr($j("",tmplen)," ",ZERO)
; cregs
	f  s s=$o(cregs(s)) q:'$l(s)  d cregion
; csegs
	f  s s=$o(csegs(s)) q:'$l(s)  d csegment
; cgnams
	f  s s=$o(gnams(s)) q:'$l(s)  d cgblname(s)
; cinst
	i inst>0 d cinst
; template access method
	i accmeth'[("\"_tmpacc) d error1
	s rec=rec_$tr($j($l(tmpacc),3)," ",0)
	s rec=rec_tmpacc
; templates
	new x
	s x=tmpreg("STATS"),tmpreg("AUTODB")='x*2+tmpreg("AUTODB") k tmpreg("STATS")	;combine items for bit mask - restore below
	s tmpreg("LOCK_CRIT")='tmpreg("LOCK_CRIT")					; again an adjustment before and after
	f  s s=$o(tmpreg(s)) q:'$l(s)  s rec=rec_$tr($j($l(tmpreg(s)),3)," ",0) s rec=rec_tmpreg(s)
	s tmpreg("STATS")=x,tmpreg("AUTODB")=tmpreg("AUTODB")#2				; save/restore cheaper than checking in loop
	s tmpreg("LOCK_CRIT")='tmpreg("LOCK_CRIT")					; restore GDE representation
	f i=2:1:$l(accmeth,"\") s am=$p(accmeth,"\",i) s s="" d
	. f  s s=$o(tmpseg(am,s)) q:'$l(s)  s rec=rec_$tr($j($l(tmpseg(am,s)),3)," ",0),rec=rec_tmpseg(am,s)
	u tempfile
	f  s record=$ze(rec,1,512),rec=$ze(rec,513,MAXSTRLEN) q:'$zl(record)  w record,!
	u @useio
	o file:chset="M" c file:delete
	c tempfile:rename=file
	i debug,$$MAP2NAM^GDEMAP(.map),$$ALL^GDEVERIF
	q 1

;-----------------------------------------------------------------------------------------------------------------------------------

gblstatmap:
	n s,ysr,cnt,xrefstatsreg,xrefcnt,ynam,blksiz
	s s=""
	s maxs=$o(regs(s),-1) 	; to ensure STDNULLCOLL for the statsDB mapping, last lower-case reg gets all of %YGS
				; It cannot be the first lower-case region as that would cause problems for the runtime
				; (since the map entries for %YGS and %YGS(FIRSTREG) would coalesce). Choosing the last
				; lower-case region avoids this coalesce issue and ensure there is a separate map entry
				; for just %YGS. This is needed by runtime (see "ygs_map_entry_changed" usage)
	s ysr=$zconvert(maxs,"L")
	; Map ^%Y* to a lower-case region
	s ynam="%Y*",ynam("NSUBS")=0,ynam("SUBS",0)=ynam,ynam("TYPE")="STAR" m nams(ynam)=ynam s nams(ynam)=ysr i $incr(nams)
	s blksiz=$$BLKSIZ($tr($j("",MAXREGLN)," ","x")) ; use maximum length region name for computation
	s s=maxs d  s s="" f  s s=$o(regs(s)) q:(maxs=s)  i (s?1U.e) s ysr=$zconvert(s,"L") d
	. d add2nams^GDEMAP("^%YGS("""_s_""")",ysr,"POINT")  i $incr(nams)
	. s xrefstatsreg(ysr)=s,xrefstatsreg(s)=ysr
	. m regs(ysr)=regs(s)				; copy & then partially override region settings for statsDB regions
	. s regs(ysr,"AUTODB")=1
	. s regs(ysr,"BEFORE_IMAGE")=0
	. s regs(ysr,"DYNAMIC_SEGMENT")=ysr
	. s regs(ysr,"JOURNAL")=0
	. s regs(ysr,"KEY_SIZE")=dflreg("KEY_SIZE")
	. s regs(ysr,"QDBRUNDOWN")=1	; have it always enabled on statsdbs as MUPIP SET can enable this only on basedb later
	. s regs(ysr,"STATS")=0
	. s regs(ysr,"STDNULLCOLL")=1
	. m segs(ysr)=segs(regs(s,"DYNAMIC_SEGMENT"))	; copy & then partially override segment settings for statsDB segments
	. s segs(ysr,"ACCESS_METHOD")="MM"
	. s segs(ysr,"ALLOCATION")=2050			; about enough for 2000 processes
	. s segs(ysr,"ASYNCIO")=0
	. s segs(ysr,"BLOCK_SIZE")=blksiz,regs(ysr,"RECORD_SIZE")=segs(ysr,"BLOCK_SIZE")-SIZEOF("blk_hdr")
	. s segs(ysr,"DEFER")=0		; setting defer=0 on an MM database implies NO flush timer on statsdb regions
	. s segs(ysr,"DEFER_ALLOCATE")=1
	. s segs(ysr,"ENCRYPTION_FLAG")=0
	. s segs(ysr,"EXTENSION_COUNT")=2050		; and a 2000 more at a time
	. ; Corresponding unique .gst file name for statsdb is determined at runtime when basedb is first opened.
	. ; For now just keep the name as the basedb name + ".gst"
	. s segs(ysr,"FILE_NAME")=segs(ysr,"FILE_NAME")_".gst"
	. s segs(ysr,"LOCK_SPACE")=defseg("LOCK_SPACE")
	. s segs(ysr,"RESERVED_BYTES")=0
	. i $i(nams),$i(regs),$i(segs)			; increment the counts
	; Determine gd_region.statsDB_reg_index for runtime
	k sreg
	s cnt=0,s=""
	for  set s=$o(regs(s))  quit:s=""  s xrefcnt(s)=cnt i $incr(cnt)
	for  set s=$o(xrefstatsreg(s))  quit:s=""  s sreg(s,"STATSDB_REG_INDEX")=xrefcnt(xrefstatsreg(s))
	q

BLKSIZ(regnm)
	; Calculate minimum block size for ^%YGS(regnm)
	new bhdsz,helpgld,maxksz,maxkpad,maxpid,minksz,minkpad,minreq,rhdsz,statsz,tmp
	set tmp=$piece($zversion," ",3),maxpid=$select("AIX"=tmp:2**26-2,"Linux"=tmp:2**22-1,1:2**16)
	set maxksz=$zlength($zcollate("%YGS("""_regnm_""","""_maxpid_""")",0))	; max key size for statistics record
	set minksz=$zlength($zcollate("%YGS("""_regnm_""",1)",0))		; min key size for statistics record
	set maxkpad=maxksz#8,maxkpad=$select(tmp:8-tmp,1:0)			; padding to align statistics of largest record
	set minkpad=minksz#8,minkpad=$select(tmp:8-tmp,1:0)			; padding to align statistics of smallest record
	set tmp=maxksz+maxkpad set:minksz+minkpad>tmp tmp=minksz+minkpad
	set minreq=SIZEOF("blk_hdr")+SIZEOF("rec_hdr")+tmp+SIZEOF("gvstats")
	quit $select(minreq#512:minreq\512+1*512,1:minreq)			; actual block size must round up to multiple of 512

fdatum:
	s x=segs(s,"ACCESS_METHOD")
	s filetype=$s((x="BG")!(x="MM"):"GDS",x="USER":"USER",1:"ERROR")
	i filetype="ERROR" d error1
	q
mapfixed:(i,key)
	n tmpmaplen,tmpnamelen,reg
	d writerec
	s tmpmaplen=varmapslen(i)
	s rec=rec_$$num2bin(4,varmapoff)		; gvkey.offset
	i (gtm64=TRUE) s rec=rec_$$num2bin(4,0) 	; add padding
	s reg=map(key)
	s rec=rec_$$num2bin(4,cregs(reg,"offset"))	; reg.offset
	i (gtm64=TRUE) s rec=rec_$$num2bin(4,0)		; add padding
	s tmpnamelen=vargblnamelen(i)
	s rec=rec_$$num2bin(4,tmpnamelen)		; gvname_len
	s rec=rec_$$num2bin(4,tmpmaplen+1)		; gvkey_len
	s varmapoff=varmapoff+tmpmaplen+2
	s curMapSpanning=(tmpnamelen'=tmpmaplen)
	i (curMapSpanning!prevMapSpanning) s isSpanned(reg)=1
	s prevMapSpanning=curMapSpanning
	q
mapvariable:(i,key)
	d writerec
	s rec=rec_key_ZERO_ZERO
	q
cregion:
	d writerec
	s rec=rec_$$num2bin(2,$l(s))
	s rec=rec_s_$tr($j("",MAXREGLN-$l(s))," ",ZERO)
	s rec=rec_$$num2bin(2,regs(s,"KEY_SIZE"))
	s rec=rec_$$num2bin(4,regs(s,"RECORD_SIZE"))
	s rec=rec_$$num2bin(4,csegs(regs(s,"DYNAMIC_SEGMENT"),"offset"))
	i (gtm64=TRUE) s rec=rec_$$num2bin(4,0) ; padding
	s rec=rec_$$num2bin(ptrsize,0)
	s rec=rec_ZERO						; OPEN state
	s rec=rec_ZERO						; LOCK_WRITE
	s rec=rec_$c(regs(s,"NULL_SUBSCRIPTS"))
	s rec=rec_$c(regs(s,"JOURNAL"))
	s rec=rec_$$num2bin(4,regs(s,"ALLOCATION"))
	s rec=rec_$$num2bin(4,regs(s,"EXTENSION"))
	s rec=rec_$$num2bin(4,regs(s,"AUTOSWITCHLIMIT"))
	s rec=rec_$$num2bin(4,regs(s,"ALIGNSIZE"))
	s rec=rec_$$num2bin(4,regs(s,"EPOCH_INTERVAL"))
	s rec=rec_$$num2bin(4,regs(s,"SYNC_IO"))
	s rec=rec_$$num2bin(4,regs(s,"YIELD_LIMIT"))
	s rec=rec_$$num2bin(2,regs(s,"BUFFER_SIZE"))
	s rec=rec_$c(regs(s,"BEFORE_IMAGE"))
	s rec=rec_$tr($j("",4)," ",ZERO)							;filler
	s rec=rec_$$num2bin(1,regs(s,"COLLATION_DEFAULT"))
	s rec=rec_$$num2bin(1,regs(s,"STDNULLCOLL"))
	s rec=rec_$$num2bin(1,regs(s,"INST_FREEZE_ON_ERROR"))
	s rec=rec_$$num2bin(1,regs(s,"QDBRUNDOWN"))
	s rec=rec_$$num2bin(1,$zl(regs(s,"FILE_NAME")))
	s rec=rec_regs(s,"FILE_NAME")_$tr($j("",SIZEOF("file_spec")-$zl(regs(s,"FILE_NAME")))," ",ZERO)
	s rec=rec_$tr($j("",8)," ",ZERO)							; reserved
	s rec=rec_$$num2bin(4,isSpanned(s))							; is_spanned
	n maxregindex s maxregindex=$get(sreg(s,"STATSDB_REG_INDEX"),TWO(32)-1)
	s rec=rec_$$num2bin(4,maxregindex)							; statsDB_reg_index
	s rec=rec_$$num2bin(1,regs(s,"EPOCHTAPER"))						; epoch tapering
	s rec=rec_$$num2bin(1,((s?1L.E)*4)+(('regs(s,"STATS"))*2)+regs(s,"AUTODB"))		; type of reserved DB
	s rec=rec_$$num2bin(1,'regs(s,"LOCK_CRIT"))						; LOCK crit with DB (1) or not (0)
	s rec=rec_filler45byte									; runtime filler
	s rec=rec_$tr($j("",SIZEOF("gd_region_padding"))," ",ZERO)				; padding
	q
csegment:
	d writerec
	s ref=$zl(rec)
	s am=segs(s,"ACCESS_METHOD")
	s rec=rec_$$num2bin(2,$l(s))
	s rec=rec_s_$tr($j("",MAXSEGLN-$l(s))," ",ZERO)
	s rec=rec_$$num2bin(2,$zl(segs(s,"FILE_NAME")))
	s rec=rec_segs(s,"FILE_NAME")_$tr($j("",SIZEOF("file_spec")-$zl(segs(s,"FILE_NAME")))," ",ZERO)
	s rec=rec_$$num2bin(2,segs(s,"BLOCK_SIZE"))
	s rec=rec_$$num2bin(2,segs(s,"EXTENSION_COUNT"))
	s rec=rec_$$num2bin(4,segs(s,"ALLOCATION"))
	i (gtm64=TRUE) s rec=rec_$tr($j("",12)," ",ZERO)						;reserved for clb + padding
	e  s rec=rec_$tr($j("",4)," ",ZERO)								;reserved for clb
	s rec=rec_".DAT"
	s rec=rec_$c(+segs(s,"DEFER"))
	s rec=rec_ZERO											;DYNAMIC segment
	s rec=rec_$$num2bin(1,segs(s,"BUCKET_SIZE"))
	s rec=rec_$$num2bin(1,segs(s,"WINDOW_SIZE"))
	s rec=rec_$$num2bin(4,segs(s,"LOCK_SPACE"))
	s rec=rec_$$num2bin(4,segs(s,"GLOBAL_BUFFER_COUNT"))
	s rec=rec_$$num2bin(4,segs(s,"RESERVED_BYTES"))
	s rec=rec_$$num2bin(4,segs(s,"MUTEX_SLOTS"))
	s rec=rec_$$num2bin(4,segs(s,"DEFER_ALLOCATE"))
	s x=$s(am="BG":1,am="MM":2,am="USER":4,1:-1)
	i x=-1 d error1
	s rec=rec_$$num2bin(4,x)
	s rec=rec_$$num2bin(ptrsize,0)		; file_cntl ptr
	s rec=rec_$$num2bin(ptrsize,0)		; repl_list ptr
	; Only for platforms that support encryption, we write this value. Others it will
	; always be 0 (ie encryption is off)
	i (encsupportedplat=TRUE) s rec=rec_$$num2bin(4,segs(s,"ENCRYPTION_FLAG"))
	e  s rec=rec_$$num2bin(4,0)
	s rec=rec_$$num2bin(4,segs(s,"ASYNCIO"))
	s rec=rec_filler16byte			; runtime filler
	q
cgblname:(s)
	n len,coll,ver
	d writerec
	s len=$zl(s)
	s rec=rec_s_$tr($j("",SIZEOF("mident")-len)," ",ZERO)
	s coll=gnams(s,"COLLATION")
	s rec=rec_$$num2bin(4,coll)
	s ver=$view("YCOLLATE",coll)
	s rec=rec_$$num2bin(4,ver)
	q
cinst:
	n s,len,n
	d writerec
	s s=inst("FILE_NAME")
	s len=$zl(s)
	s rec=rec_s_$tr($j("",SIZEOF("gd_inst_info")-len)," ",ZERO)
	q

;-----------------------------------------------------------------------------------------------------------------------------------

num2bin:(l,n)
	i (gtm64=TRUE) q $s(l=1:$$num2tiny(+n),l=2:$$num2shrt(+n),l=4:$$num2int(+n),l=8:$$num2long(+n),1:$$num2error)
	q $s(l=1:$$num2tiny(+n),l=2:$$num2shrt(+n),l=4:$$num2int(+n),1:$$num2error)
	;
num2tiny:(num)
	i (num<0)!(num'<256) d error1
	q $zch(num)
	;
num2shrt:(num)
	i (num<0)!(num'<TWO(16)) d error1
	i endian=TRUE q $zch(num\256,num#256)
	q $zch(num#256,num\256)
	;
num2int:(num)
	i (num<0)!(num'<TWO(32)) d error1
	i endian=TRUE q $zch(num\TWO(24),num\TWO(16)#256,num\TWO(8)#256,num#256)
	q $zch(num#256,num\TWO(8)#256,num\TWO(16)#256,num\TWO(24))
	;
num2long:(num)
	n t8,t16,t24,t32,t40,t48,t56
	s t8=TWO(8),t16=TWO(16),t24=TWO(24),t32=TWO(32),t40=TWO(40),t48=TWO(48),t56=TWO(56)
	i (num<0)!(num'<TWO(64)) d error1
	i endian=TRUE q $zch(num\t56,num\t48#256,num\t40#256,num\t32#256,num\t24#256,num\t16#256,num\t8#256,num#256)
	q $zch(num#256,num\t8#256,num\t16#256,num\t24#256,num\t32#256,num\t40#256,num\t48#256,num\t56)
	;
num2error:()
	d error1
	q 0

;----------------------------------------------------------------------------------------------------------------------------------

fatal:(msgno)
	q msgno\8*8+4
	;
error1:
	s $et="d ABORT^GDE"
	c tempfile:delete
	zm $$fatal(gdeerr("VERIFY")):"FAILED"
	;
writerec:
	n len
	s len=$zl(rec)
	i len<512 q
	s len=len\512*512
	s record=$ze(rec,1,len),rec=$ze(rec,len+1,MAXSTRLEN)
	; At this point, "rec" is guaranteed to be less than 512 bytes.
	u tempfile w record,! u @useio
	q
	;
writeerr
	u @useio
	c tempfile:delete
	zm gdeerr("WRITEERROR"):gdeputzs
	q 0
