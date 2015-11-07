;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2006, 2013 Fidelity Information Services, Inc	;
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
	n varmapslen,vargblnamelen,tmplen,ptrsize,varmapoff,gnamcnt,gblnamelen,filler16byte,filler12byte
	d CREATEGLDMAP^GDEMAP
	s ptrsize=4
	s s="",gdeputzs="",varmapslen=0,mapcnt=0,hasSpanGbls=0
	f  s s=$o(map(s)),tmplen=$l(s) q:'tmplen  d
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
	s rec=rec_$tr($j("",(3*ptrsize))," ",ZERO)		; reserved
	s rec=rec_$$num2bin(ptrsize,filesize)			; end
	s rec=rec_$$num2bin(4,hasSpanGbls)			; has_span_gbls
	s rec=rec_filler12byte					; for runtime filler
	s rec=hdrlab_$$num2bin(4,$l(hdrlab)+4+filesize)_rec
	i create zm gdeerr("GDCREATE"):file
	e  s:$ZVersion["VMS" $p(file,";",2)=$p(file,";",2)+1  zm gdeerr("GDUPDATE"):file
	s gdexcept="s gdeputzs=$zs  zgoto "_$zl_":writeerr^GDEPUT"
	i $ZVersion["VMS"  s tempfile=$p(file,";",1)_"inprogress"
	e  s tempfile=file_"inprogress"
	s chset=$SELECT($ZV["OS390":"ISO8859-1",1:"")
	o tempfile:(rewind:noreadonly:newversion:recordsize=512:fixed:blocksize=512:exception=gdexcept:ochset=chset)
; maps
	s s="",curMapSpanning=0,prevMapSpanning=0
	f i=1:1 s s=$o(map(s)) q:'$l(s)  d mapfixed(i,s)
	f i=1:1 s s=$o(map(s)) q:'$l(s)  d mapvariable(i,s)
	; write padding if not 4-byte aligned after variable maps section
	s tmplen=varmapoff#ptrsize
	i tmplen s tmplen=ptrsize-tmplen,rec=rec_$tr($j("",tmplen)," ",ZERO)
; cregs
	f  s s=$o(cregs(s)) q:'$l(s)  d cregion
; csegs
	f  s s=$o(csegs(s)) q:'$l(s)  d csegment
; cgnams
	f  s s=$o(gnams(s)) q:'$l(s)  d cgblname(s)
; template access method
	i accmeth'[("\"_tmpacc) d error1
	s rec=rec_$tr($j($l(tmpacc),3)," ",0)
	s rec=rec_tmpacc
; templates
	f  s s=$o(tmpreg(s)) q:'$l(s)  s rec=rec_$tr($j($l(tmpreg(s)),3)," ",0) s rec=rec_tmpreg(s)
	f i=2:1:$l(accmeth,"\") s am=$p(accmeth,"\",i) s s="" d
	. f  s s=$o(tmpseg(am,s)) q:'$l(s)  s rec=rec_$tr($j($l(tmpseg(am,s)),3)," ",0),rec=rec_tmpseg(am,s)
	u tempfile
	f  s record=$e(rec,1,512),rec=$e(rec,513,MAXSTRLEN) q:'$l(record)  w record,!
	u @useio
	i $ZV'["VMS" o file c file:delete
	c tempfile:rename=file
	q 1

;-----------------------------------------------------------------------------------------------------------------------------------

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
	s rec=rec_$$num2bin(4,0)
	s rec=rec_ZERO										; OPEN state
	s rec=rec_ZERO										; LOCK_WRITE
	s rec=rec_$c(regs(s,"NULL_SUBSCRIPTS"))
	s rec=rec_$c(regs(s,"JOURNAL"))
	s rec=rec_$$num2bin(4,regs(s,"ALLOCATION"))
	s rec=rec_$$num2bin(2,regs(s,"EXTENSION"))
	s rec=rec_$$num2bin(2,regs(s,"BUFFER_SIZE"))
	s rec=rec_$c(regs(s,"BEFORE_IMAGE"))
	s rec=rec_$tr($j("",4)," ",ZERO)							;filler
	s rec=rec_$$num2bin(1,regs(s,"COLLATION_DEFAULT"))
	s rec=rec_$$num2bin(1,regs(s,"STDNULLCOLL"))
	s rec=rec_$$num2bin(1,$l(regs(s,"FILE_NAME")))
	s rec=rec_regs(s,"FILE_NAME")_$tr($j("",SIZEOF("file_spec")-$l(regs(s,"FILE_NAME")))," ",ZERO)
	s rec=rec_$tr($j("",8)," ",ZERO)							; reserved
	s rec=rec_$$num2bin(4,isSpanned(s))							; is_spanned
	s rec=rec_filler12byte									; runtime filler
	q
csegment:
	d writerec
	s ref=$l(rec)
	s am=segs(s,"ACCESS_METHOD")
	s rec=rec_$$num2bin(2,$l(s))
	s rec=rec_s_$tr($j("",MAXSEGLN-$l(s))," ",ZERO)
	s rec=rec_$$num2bin(2,$l(segs(s,"FILE_NAME")))
	s rec=rec_segs(s,"FILE_NAME")_$tr($j("",SIZEOF("file_spec")-$l(segs(s,"FILE_NAME")))," ",ZERO)
	s rec=rec_$$num2bin(2,segs(s,"BLOCK_SIZE"))
	s rec=rec_$$num2bin(2,segs(s,"EXTENSION_COUNT"))
	s rec=rec_$$num2bin(4,segs(s,"ALLOCATION"))
	s rec=rec_$tr($j("",4)," ",ZERO)								;reserved for clb
	s rec=rec_".DAT"
	s rec=rec_$c(+segs(s,"DEFER"))
	s rec=rec_ZERO											;DYNAMIC segment
	s rec=rec_$$num2bin(1,segs(s,"BUCKET_SIZE"))
	s rec=rec_$$num2bin(1,segs(s,"WINDOW_SIZE"))
	s rec=rec_$$num2bin(4,segs(s,"LOCK_SPACE"))
	s rec=rec_$$num2bin(4,segs(s,"GLOBAL_BUFFER_COUNT"))
	s rec=rec_$$num2bin(4,segs(s,"RESERVED_BYTES"))
	s rec=rec_$$num2bin(4,segs(s,"MUTEX_SLOTS"))
	s x=$s(am="BG":1,am="MM":2,am="USER":4,1:-1)
	i x=-1 d error1
	s rec=rec_$$num2bin(4,x)
	s rec=rec_$$num2bin(4,0)		; file_cntl ptr
	s rec=rec_$$num2bin(4,0)		; repl_list ptr
	s rec=rec_filler16byte			; runtime filler
	q
cgblname:(s)
	n len,coll,ver
	d writerec
	s len=$zl(s)
	s rec=rec_s_$tr($j("",SIZEOF("mident")-len)," ",ZERO)
	s rec=rec_$$num2bin(4,gnams(s,"COLLATION"))
	s coll=gnams(s,"COLLATION")
	s rec=rec_$$num2bin(4,coll)
	s ver=$view("YCOLLATE",coll)
	s rec=rec_$$num2bin(4,ver)
	q

;-----------------------------------------------------------------------------------------------------------------------------------

num2bin:(l,n)
	q $s(l=1:$$num2tiny(+n),l=2:$$num2shrt(+n),l=4:$$num2long(+n),1:$$num2error)
	;
num2tiny:(num)
	i (num<0)!(num'<256) d error1
	q $c(num)
	;
num2shrt:(num)
	i (num<0)!(num'<TWO(16)) d error1
	i endian=TRUE q $c(num\256,num#256)
	q $c(num#256,num\256)
	;
num2long:(num)
	i (num<0)!(num'<TWO(32)) d error1
	i endian=TRUE q $c(num/16777216,num/65536#256,num/256#256,num#256)
	q $c(num#256,num/256#256,num/65536#256,num/16777216)

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
	s len=$l(rec)
	i len<512 q
	s len=len\512*512
	s record=$e(rec,1,len),rec=$e(rec,len+1,MAXSTRLEN)
	; At this point, "rec" is guaranteed to be less than 512 bytes.
	u tempfile w record,! u @useio
	q
writeerr
	u @useio
	c tempfile:delete
	zm gdeerr("WRITEERROR"):gdeputzs
	q 0
