;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2006-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
verify:	;implement the verb: VERIFY, also invoked from show and GDEGET
ALL()	;external
	n verified,gqual s verified=1
	s gqual="NAME" d ALLNAM
	s gqual="GBLNAME" d ALLGBL
	s gqual="REGION" d ALLREG,usereg
	s gqual="SEGMENT" d ALLSEG,useseg
	d ALLTEM
	s:('verified)&('$zstatus) $zstatus=gdeerr("VERIFY")
	zm gdeerr("VERIFY"):$s(verified:"OK",1:"FAILED") w !
	q verified

;-----------------------------------------------------------------------------------------------------------------------------------
; called from GDEPARSE.M

ALLNAM
	n NAME,hassubs s NAME="",hassubs=0
	f  s NAME=$o(nams(NAME)) q:'$zl(NAME)  d name1  i +$g(nams(NAME,"NSUBS")) s hassubs=1
	; if using subscripted names, check that all regions where a globals spans has STDNULLCOLL set to TRUE
	i hassubs d
	. n map,currMap,nextMap,nextMapHasSubs,reg,gblname,mapreg
	. d NAM2MAP^GDEMAP
	. s currMap="",nextMap="",nextMapHasSubs=0
	. f  s currMap=$o(map(currMap),-1) q:currMap="#)"  d
	. . s hassubs=$zf(currMap,ZERO)
	. . ; Check if current map entry has subscripts. If so this map entry should have STDNULLCOLL set.
	. . ; Also check if next map entry had subscripts. If so this map entry should have STDNULLCOLL set
	. . ; 	That is because a portion of the global in the next map entry lies in the current map entry region.
	. . i (hassubs!nextMapHasSubs) d
	. . . ; check if region has STDNULLCOLL defined to true
	. . . s reg=map(currMap)
	. . . i '+$g(regs(reg,"STDNULLCOLL")) d
	. . . . s verified=0
	. . . . i nextMapHasSubs d
	. . . . . s gblname=$ze(nextMap,1,nextMapHasSubs-2)
	. . . . . i '$d(mapreg(reg,gblname)) zm gdeerr("STDNULLCOLLREQ"):reg:"^"_gblname s mapreg(reg,gblname)=""
	. . . . i hassubs d
	. . . . . s gblname=$ze(currMap,1,hassubs-2)
	. . . . . i '$d(mapreg(reg,gblname)) zm gdeerr("STDNULLCOLLREQ"):reg:"^"_gblname s mapreg(reg,gblname)=""
	. . s nextMapHasSubs=hassubs,nextMap=currMap
	q
ALLGBL
	n GBLNAME s GBLNAME=""
	f  s GBLNAME=$o(gnams(GBLNAME)) q:""=GBLNAME  d gblname1
	q
ALLREG
	n REGION s REGION=""
	f  s REGION=$o(regs(REGION)) q:'$l(REGION)  d region1
	q
ALLSEG
	n SEGMENT s SEGMENT=""
	f  s SEGMENT=$o(segs(SEGMENT)) q:'$l(SEGMENT)  d seg1
; No duplicate region->segment mappings
	n refdyns s s=""
	f  s s=$o(regs(s)) q:'$l(s)  d:$d(refdyns(regs(s,"DYNAMIC_SEGMENT"))) dupseg s refdyns(regs(s,"DYNAMIC_SEGMENT"),s)=""
; No duplicate segment->file mappings
	n reffils
	f  s s=$o(segs(s)) q:'$l(s)  d:$d(reffils(segs(s,"FILE_NAME"))) dupfile s reffils(segs(s,"FILE_NAME"),s)=""
	q
NAME
	i '$d(nams(NAME)) k verified d  q
	. zm $$info(gdeerr("OBJNOTFND")):"Name":$s(NAME'="#":$$namedisp^GDESHOW(NAME,0),1:"Local Locks")
name1:	i '$d(regs(nams(NAME))) d
	. s verified=0
	. zm gdeerr("MAPBAD"):"Region":nams(NAME):"Name":$s(NAME'="#":$$namedisp^GDESHOW(NAME,0),1:"Local Locks")
	q
GBLNAME
	i '$d(gnams(GBLNAME)) k verified zm $$info(gdeerr("OBJNOTFND")):"Global Name":GBLNAME q
gblname1:
	n s,sval,errissued s s=""
	f  s s=$o(gnams(GBLNAME,s)) q:""=s  s sval=gnams(GBLNAME,s) d
	. s errissued=0
	. i $d(mingnam(s)),mingnam(s)>sval s errissued=1 zm gdeerr("VALTOOSMALL"):sval:mingnam(s):s
	. i $d(maxgnam(s)),maxgnam(s)<sval s errissued=1 zm gdeerr("VALTOOBIG"):sval:maxgnam(s):s
	. i errissued s verified=0 zm gdeerr("GBLNAMEIS"):GBLNAME
	. i (s="COLLATION") d
	. . i $d(gnams(GBLNAME,"COLLVER")) d
	. . . d chkcoll^GDEPARSE(sval,GBLNAME,gnams(GBLNAME,"COLLVER"))
	. . e  d chkcoll^GDEPARSE(sval,GBLNAME)
	; now that all gblnames and names have been read, do some checks between them
	; ASSERT : i $d(namrangeoverlap)  zsh "*"  zg 0
	d gblnameeditchecks^GDEPARSE("*",0)	; check all name specifications are good given the gblname collation settings
	; ASSERT : i $d(namrangeoverlap)  zsh "*"  zg 0
	q
REGION
	i '$d(regs(REGION)) k verified zm $$info(gdeerr("OBJNOTFND")):"Region":REGION q
region1:	i '$d(segs(regs(REGION,"DYNAMIC_SEGMENT"))) s verified=0
	i  zm gdeerr("MAPBAD"):"Dynamic segment":regs(REGION,"DYNAMIC_SEGMENT"):"Region":REGION q
	n rquals s s=""
	f  s s=$o(regs(REGION,s)) q:'$l(s)  s rquals(s)=regs(REGION,s)
	f  s s=$o(minreg(s)) q:'$l(s)  i '$d(rquals(s)) s verified=0 zm $$info(gdeerr("QUALREQD")):s,gdeerr("REGIS"):REGION
	f  s s=$o(maxreg(s)) q:'$l(s)  i '$d(rquals(s)) s verified=0 zm $$info(gdeerr("QUALREQD")):s,gdeerr("REGIS"):REGION
	s x=$$RQUALS(.rquals)
	q
SEGMENT
	i '$d(segs(SEGMENT)) k verified zm $$info(gdeerr("OBJNOTFND")):"Segment":SEGMENT q
seg1:	i '$d(segs(SEGMENT,"ACCESS_METHOD")) s verified=0 zm $$info(gdeerr("QUALREQD")):"Access method",gdeerr("SEGIS"):"":SEGMENT q
	s am=segs(SEGMENT,"ACCESS_METHOD")
	n squals s s=""
	f  s s=$o(segs(SEGMENT,s)) q:'$l(s)  s squals(s)=segs(SEGMENT,s)
	f  s s=$o(minseg(am,s)) q:'$l(s)  i '$d(squals(s)) s verified=0 zm $$info(gdeerr("QUALREQD")):s,gdeerr("SEGIS"):am:SEGMENT
	f  s s=$o(maxseg(am,s)) q:'$l(s)  i '$d(squals(s)) s verified=0 zm $$info(gdeerr("QUALREQD")):s,gdeerr("SEGIS"):am:SEGMENT
	i "MM"=am  do
	. i 1=squals("ENCRYPTION_FLAG") s verified=0 zm $$info(gdeerr("GDECRYPTNOMM")):SEGMENT
	. i 1=squals("ASYNCIO")         s verified=0 zm $$info(gdeerr("GDEASYNCIONOMM")):SEGMENT
	s x=$$SQUALS(am,.squals)
	q
usereg:	n REGION,NAME s REGION=""
	f  s REGION=$o(regs(REGION)) q:'$l(REGION)  d usereg1
	q
usereg1:	s NAME=""
	f  s NAME=$o(nams(NAME)) q:$g(nams(NAME))=REGION!'$zl(NAME)
	i '$zl(NAME) s verified=0 zm gdeerr("MAPBAD"):"A":"NAME":"REGION":REGION
	q
useseg:	n SEGMENT,REGION s SEGMENT=""
	f  s SEGMENT=$o(segs(SEGMENT)) q:'$l(SEGMENT)  d useseg1
	q
useseg1:	s REGION=""
	f  s REGION=$o(regs(REGION)) q:$g(regs(REGION,"DYNAMIC_SEGMENT"))=SEGMENT!'$l(REGION)
	i '$l(REGION) s verified=0 zm gdeerr("MAPBAD"):"A":"REGION":"SEGMENT":SEGMENT
	q
;-----------------------------------------------------------------------------------------------------------------------------------
; routine services

info:(mesno)
	q mesno\8*8+3
	;
dupseg:	s verified=0
	zm gdeerr("MAPDUP"):"Regions":$o(refdyns(regs(s,"DYNAMIC_SEGMENT"),"")):s:"Dynamic segment":regs(s,"DYNAMIC_SEGMENT")
	q
dupfile:	s verified=0
	zm gdeerr("MAPDUP"):"Dynamic segments":$o(reffils(segs(s,"FILE_NAME"),"")):s:"File":segs(s,"FILE_NAME")
	q
ALLTEM
	s x=$$TRQUALS(.tmpreg)
	; The change is for TR C9E02-002518, any template command updates only active segment
	; so verify only that segment with template region, not all segments
	d tmpseg
	q
tmpseg:	n squals s s=""
	f  s s=$o(tmpseg(am,s)) q:'$l(s)  s squals(s)=tmpseg(am,s)
	s x=$$TSQUALS(am,.squals)
	q
regelm:	if s'="DYNAMIC_SEGMENT",'$data(tmpreg(s)) zmessage $$info(gdeerr("QUALBAD")):s
	else  if $data(minreg(s)),minreg(s)>rquals(s) zmessage gdeerr("VALTOOSMALL"):rquals(s):minreg(s):s
	else  if $data(maxreg(s)),maxreg(s)<rquals(s) zmessage gdeerr("VALTOOBIG"):rquals(s):maxreg(s):s
	if  set verified=0
	quit
segelm:	if s'="FILE_NAME",'$length(tmpseg(am,s)) zmessage $$info(gdeerr("QUALBAD")):s
	else  if $data(minseg(am,s)),minseg(am,s)>squals(s) zmessage gdeerr("VALTOOSMALL"):squals(s):minseg(am,s):s
	else  if $data(maxseg(am,s)),maxseg(am,s)<squals(s) zmessage gdeerr("VALTOOBIG"):squals(s):maxseg(am,s):s
	if  set verified=0
	quit
key2blk:
	; the computation below allows for at least 1 max-key record in a data OR index block.
	; since an index block always contains a *-key, we need to account for that too.
	; bs:block size, y:supportable max key size, f:size of reserved bytes, ks:key size
	set len("block_id")=4	;size of block_id
	set len("bstar_rec")=8	;size for bstar record
	if REGION="TEMPLATE" quit  ; do not do keysize/blksize check for TEMPLATE region as this is not a real region
	set y=bs-f-SIZEOF("blk_hdr")-SIZEOF("rec_hdr")-len("block_id")-len("bstar_rec")
	if ks>y set verified=0 zmessage gdeerr("KEYSIZIS"):ks,gdeerr("KEYFORBLK"):bs:f:y,gdeerr("REGIS"):REGION
	quit
buf2blk:	i REGION="TEMPLATE" q
	i "USER"[am s verified=0 zm gdeerr("NOJNL"):am,gdeerr("REGIS"):REGION,gdeerr("SEGIS"):am:SEGMENT
	q
mmbichk:	i REGION="TEMPLATE",am="MM",tmpacc'="MM" q
	i am="MM" s verified=0 zm gdeerr("MMNOBEFORIMG"),gdeerr("REGIS"):REGION,gdeerr("SEGIS"):am:SEGMENT
	q
allocchk(rquals)
	n ext,alloc,asl,qn
	s qn="EXTENSION",ext=$s($d(rquals(qn)):rquals(qn),$d(regs(REGION,qn)):regs(REGION,qn),1:tmpreg(qn))
	s qn="ALLOCATION",alloc=$s($d(rquals(qn)):rquals(qn),$d(regs(REGION,qn)):regs(REGION,qn),1:tmpreg(qn))
	s qn="AUTOSWITCHLIMIT",asl=$s($d(rquals(qn)):rquals(qn),$d(regs(REGION,qn)):regs(REGION,qn),1:tmpreg(qn))
	i alloc>asl s verified=0 zm gdeerr("VALTOOBIG"):alloc:asl_" (AUTOSWITCHLIMIT)":"ALLOCATION" q
	i alloc'=asl,ext+alloc>asl d
	. s rquals("ALLOCATION")=asl
	. zm gdeerr("JNLALLOCGROW"):alloc:asl:"region":REGION
	q

;-----------------------------------------------------------------------------------------------------------------------------------
; called from GDEADD.M and GDECHANG.M

RQUALS(rquals)
	if '$data(verified) new verified set verified=1
	set s=""
	for  set s=$order(rquals(s)) quit:'$length(s)  do regelm
	quit:'verified verified
	if $data(rquals("FILE_NAME")),$zlength(rquals("FILE_NAME"))>(SIZEOF("file_spec")-1) set verified=0
	if  zmessage $$info(gdeerr("VALTOOLONG")):rquals("FILE_NAME"):SIZEOF("file_spec")-1:"Journal filename",gdeerr("REGIS"):REGION
	set ks="KEY_SIZE",ks=$select($data(rquals(ks)):rquals(ks),$data(regs(REGION,ks)):regs(REGION,ks),1:tmpreg(ks))
	set x="RECORD_SIZE",x=$select($data(rquals(x)):rquals(x),$data(regs(REGION,x)):regs(REGION,x),1:tmpreg(x))
	do allocchk(.rquals)
	if REGION="TEMPLATE" set bs=tmpseg(tmpacc,"BLOCK_SIZE"),f=tmpseg(tmpacc,"RESERVED_BYTES")
	; note "else" used in two consecutive lines intentionally (instead of using a do block inside one else).
	; this is because we want the QUIT to quit out of RQUALS and the NEW of SEGMENT,am to happen at the RQUALS level.
	else  set s="DYNAMIC_SEGMENT",s=$select($data(rquals(s)):rquals(s),$data(regs(REGION,s)):regs(REGION,s),1:0)
	else  quit:'$data(segs(s)) verified new SEGMENT,am do
	. set SEGMENT=s,am=segs(s,"ACCESS_METHOD"),bs=$get(segs(s,"BLOCK_SIZE")),f=$get(segs(s,"RESERVED_BYTES"))
	do:((minreg("KEY_SIZE")'>ks)&(maxreg("KEY_SIZE")'<ks)&("USER"'=am)) key2blk ;GTM-6941
	set x="JOURNAL"
	quit:'$select('$data(rquals(x)):tmpreg(x),1:rquals(x)) verified
	set x="BUFFER_SIZE",x=$select($data(rquals(x)):rquals(x),$data(regs(REGION,x)):regs(REGION,x),1:tmpreg(x)) do buf2blk
	if nommbi set x="BEFORE_IMAGE" do:$select('$data(rquals(x)):tmpreg(x),1:rquals(x)) mmbichk
	quit verified
	;
SQUALS(am,squals)
	if '$data(verified) new verified set verified=1
	new s set s=""
	for  set s=$order(squals(s)) quit:'$length(s)  do:$length(squals(s)) segelm
	quit:'verified verified
	new bs set bs="BLOCK_SIZE"
	if $data(squals(bs)),((minseg(am,"BLOCK_SIZE")'>squals(bs))&(maxseg(am,"BLOCK_SIZE")'<squals(bs))) do
	. if squals(bs)#512 set x=squals(bs),squals(bs)=((x\512)+1)*512
	. if  zmessage gdeerr("BLKSIZ512"):x:squals(bs),gdeerr("SEGIS"):am:SEGMENT
	set s="WINDOW_SIZE"
	if SEGMENT="TEMPLATE" set x=tmpreg("RECORD_SIZE") do segreg quit verified
	new REGION set REGION=""
	for  set REGION=$order(regs(REGION)) quit:'$length(REGION)  do
	. if regs(REGION,"DYNAMIC_SEGMENT")=SEGMENT set bs=regs(REGION,"RECORD_SIZE") do segreg
	quit verified
segreg:
	if am'="USER" do
	. set bs="BLOCK_SIZE",bs=$select($data(squals(bs)):squals(bs),$data(segs(SEGMENT,bs)):segs(SEGMENT,bs),1:tmpseg(am,bs))
	. set f="RESERVED_BYTES",f=$select($data(squals(f)):squals(f),$data(segs(SEGMENT,f)):segs(SEGMENT,f),1:tmpseg(am,f))
	. set x="RECORD_SIZE",x=$select($data(regs(REGION,x)):regs(REGION,x),1:tmpreg(x))
	. set ks="KEY_SIZE",ks=$select($data(regs(REGION,ks)):regs(REGION,ks),1:tmpreg(ks))
	. do:((minseg(am,"BLOCK_SIZE")'>bs)&(maxseg(am,"BLOCK_SIZE")'<bs)) key2blk ;GTM-6941
	if '$select(SEGMENT="TEMPLATE":0,1:regs(REGION,"JOURNAL")) quit
	set x=$select(SEGMENT="TEMPLATE":tmpreg("BUFFER_SIZE"),1:regs(REGION,"BUFFER_SIZE")) do buf2blk
	if nommbi,$select(SEGMENT="TEMPLATE":0,1:regs(REGION,"BEFORE_IMAGE")) do mmbichk
	quit

;-----------------------------------------------------------------------------------------------------------------------------------
; called from GDETEMPL.M

TRQUALS(rquals)
	n REGION,SEGMENT,am s (REGION,SEGMENT)="TEMPLATE",am=tmpacc
	q $$RQUALS(.rquals)
	;
TSQUALS(am,squals)
	n REGION,SEGMENT s (REGION,SEGMENT)="TEMPLATE"
	q $$SQUALS(am,.squals)

