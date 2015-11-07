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
gdeput:	;output the result of the session to the global directory file
GDEPUT()
	n rec,gds,cregs,csegs,cregcnt,csegcnt,maxrecsize,mapcnt,map
	d PUTMAKE^GDEMAP
	s s="",gdeputzs=""
	f mapcnt=0:1 s s=$o(map(s)) q:'$l(s)  s cregs(map(s))=""
	s maxrecsize=0
	f cregcnt=0:1 s s=$o(cregs(s)) q:'$l(s)  d
	. s csegs(regs(s,"DYNAMIC_SEGMENT"))=s i maxrecsize<regs(s,"RECORD_SIZE") s maxrecsize=regs(s,"RECORD_SIZE")
	f csegcnt=0:1 s s=$o(csegs(s)) q:'$l(s)  d fdatum
	i cregcnt'=csegcnt d error1
	s x=SIZEOF("gd_contents")+(mapcnt*SIZEOF("gd_map")),s=""
	f i=0:1 s s=$o(cregs(s)) q:'$l(s)  s cregs(s,"offset")=i*SIZEOF("gd_region")+x
	s x=x+(cregcnt*SIZEOF("gd_region"))
	f i=0:1 s s=$o(csegs(s)) q:'$l(s)  s csegs(s,"offset")=i*SIZEOF("gd_segment")+x
	s x=x+(csegcnt*SIZEOF("gd_segment"))
	s rec=""
; contents
	s rec=rec_$c(0,0,0,0)									; not used
	s rec=rec_$$num2bin(4,maxrecsize)							; max rec size
	s filesize=SIZEOF("gd_contents")
	s rec=rec_$$num2bin(2,mapcnt)_$$num2bin(2,cregcnt)_$$num2bin(2,csegcnt)_$$num2bin(2,0)	; maps,regs,segs,filler
	s rec=rec_$$num2bin(4,filesize)								; mapptr
	s filesize=filesize+(mapcnt*SIZEOF("gd_map"))
	s rec=rec_$$num2bin(4,filesize)								; regionptr
	s filesize=filesize+(cregcnt*SIZEOF("gd_region"))
	s rec=rec_$$num2bin(4,filesize)								; segmentptr
	s filesize=filesize+(csegcnt*SIZEOF("gd_segment")),base=filesize
	s rec=rec_$tr($j("",12)," ",ZERO)							; reserved
	s rec=rec_$$num2bin(4,filesize)								; end
	s rec=hdrlab_$$num2bin(4,$l(hdrlab)+4+filesize)_rec
	i create zm gdeerr("GDCREATE"):file
	e  s:$ZVersion["VMS" $p(file,";",2)=$p(file,";",2)+1  zm gdeerr("GDUPDATE"):file
	s gdexcept="s gdeputzs=$zs  zgoto "_$zl_":writeerr^GDEPUT"
	i $ZVersion["VMS"  s tempfile=$p(file,";",1)_"inprogress"
	e  s tempfile=file_"inprogress"
	s chset=$SELECT($ZV["OS390":"ISO8859-1",1:"")
	o tempfile:(rewind:noreadonly:newversion:recordsize=512:fixed:blocksize=512:exception=gdexcept:ochset=chset)
; maps
	s s=""
	f  s s=$o(map(s)) q:'$l(s)  d map
; cregs
	f  s s=$o(cregs(s)) q:'$l(s)  d cregion
; csegs
	f  s s=$o(csegs(s)) q:'$l(s)  d csegment
; template access method
	i accmeth'[("\"_tmpacc) d error1
	s rec=rec_$tr($j($l(tmpacc),3)," ",0)
	s rec=rec_tmpacc
; templates
	f  s s=$o(tmpreg(s)) q:'$l(s)  s rec=rec_$tr($j($l(tmpreg(s)),3)," ",0) s rec=rec_tmpreg(s)
	f i=2:1:$l(accmeth,"\") s am=$p(accmeth,"\",i) s s="" d
	. f  s s=$o(tmpseg(am,s)) q:'$l(s)  s rec=rec_$tr($j($l(tmpseg(am,s)),3)," ",0),rec=rec_tmpseg(am,s)
	u tempfile
	f  s record=$e(rec,1,512),rec=$e(rec,513,9999) q:'$l(record)  w record,!
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
map:
	d writerec
	i $l(s)'=SIZEOF("mident") d error1
	s rec=rec_s_$$num2bin(4,cregs(map(s),"offset"))
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
	s x=$s(am="BG":1,am="MM":2,am="USER":4,1:-1)
	i x=-1 d error1
	s rec=rec_$$num2bin(4,x)
	s rec=rec_$$num2bin(4,0)		; file_cntl ptr
	s rec=rec_$$num2bin(4,0)		; repl_list ptr
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
	e  q $c(num#256,num\256)
	;
num2long:(num)
	i (num<0)!(num'<TWO(32)) d error1
	i endian=TRUE q $c(num/16777216,num/65536#256,num/256#256,num#256)
	e  q $c(num#256,num/256#256,num/65536#256,num/16777216)

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
	i $l(rec)<512 q
	s record=$e(rec,1,512),rec=$e(rec,513,9999)
	u tempfile w record,! u @useio
	q
	;
writeerr
	u @useio
	c tempfile:delete
	zm gdeerr("WRITEERROR"):gdeputzs
	q 0
