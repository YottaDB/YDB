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
regelm:	i s'="DYNAMIC_SEGMENT",'$d(tmpreg(s)) zm $$info(gdeerr("QUALBAD")):s
	e  i $d(minreg(s)),minreg(s)>rquals(s) zm gdeerr("VALTOOSMALL"):rquals(s):minreg(s):s
	e  i $d(maxreg(s)),maxreg(s)<rquals(s) zm gdeerr("VALTOOBIG"):rquals(s):maxreg(s):s
	i  s verified=0 zm gdeerr("REGIS"):REGION
	q
segelm:	i s'="FILE_NAME",'$l(tmpseg(am,s)) zm $$info(gdeerr("QUALBAD")):s
	e  i $d(minseg(am,s)),minseg(am,s)>squals(s) zm gdeerr("VALTOOSMALL"):squals(s):minseg(am,s):s
	e  i $d(maxseg(am,s)),maxseg(am,s)<squals(s) zm gdeerr("VALTOOBIG"):squals(s):maxseg(am,s):s
	i  s verified=0 zm gdeerr("SEGIS"):am:SEGMENT
	q
key2blk:
	; the computation below allows for at least 1 max-key record in a data OR index block.
	; since an index block always contains a *-key, we need to account for that too.
	; bs:block size, y:supportable max key size, f:size of reserved bytes, ks:key size
	i REGION="TEMPLATE" q  ; do not do keysize/blksize check for TEMPLATE region as this is not a real region
	s y=bs-f-SIZEOF("blk_hdr")-len("min_val")-SIZEOF("rec_hdr")-len("hide_subs")-len("bstar_rec")
	i ks>y s verified=0 zm gdeerr("KEYSIZIS"):ks,gdeerr("KEYFORBLK"):bs:f:y,gdeerr("REGIS"):REGION
	q
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
	i '$d(verified) n verified s verified=1
	s len("min_val")=4   ;size for value field in index block
	s len("bstar_rec")=8   ;size for bstar record
	s len("hide_subs")=8   ;size for hidden subscript
	s s=""
	f  s s=$o(rquals(s)) q:'$l(s)  d regelm
	i $d(rquals("FILE_NAME")),$zl(rquals("FILE_NAME"))>(SIZEOF("file_spec")-1) s verified=0
	i  zm $$info(gdeerr("VALTOOLONG")):rquals("FILE_NAME"):SIZEOF("file_spec")-1:"Journal filename",gdeerr("REGIS"):REGION
	s ks="KEY_SIZE",ks=$s($d(rquals(ks)):rquals(ks),$d(regs(REGION,ks)):regs(REGION,ks),1:tmpreg(ks))
	s x="RECORD_SIZE",x=$s($d(rquals(x)):rquals(x),$d(regs(REGION,x)):regs(REGION,x),1:tmpreg(x))
	d allocchk(.rquals)
	i REGION="TEMPLATE" s bs=tmpseg(tmpacc,"BLOCK_SIZE"),f=tmpseg(tmpacc,"RESERVED_BYTES")
	; note "else" used in two consecutive lines intentionally (instead of using a do block inside one else).
	; this is because we want the QUIT to quit out of RQUALS and the NEW of SEGMENT,am to happen at the RQUALS level.
	e  s s="DYNAMIC_SEGMENT",s=$s($d(rquals(s)):rquals(s),$d(regs(REGION,s)):regs(REGION,s),1:0)
	e  q:'$d(segs(s)) verified n SEGMENT,am d
	. s SEGMENT=s,am=segs(s,"ACCESS_METHOD"),bs=$g(segs(s,"BLOCK_SIZE")),f=$g(segs(s,"RESERVED_BYTES"))
	i minreg("KEY_SIZE")'>ks d
	. i am'="USER" d key2blk ;GTM-6941
	s x="JOURNAL"
	i '$s('$d(rquals(x)):tmpreg(x),1:rquals(x)) q verified
	s x="BUFFER_SIZE",x=$s($d(rquals(x)):rquals(x),$d(regs(REGION,x)):regs(REGION,x),1:tmpreg(x)) d buf2blk
	i nommbi s x="BEFORE_IMAGE" i $s('$d(rquals(x)):tmpreg(x),1:rquals(x)) d mmbichk
	q verified
	;
SQUALS(am,squals)
	i '$d(verified) n verified s verified=1
	s len("min_val")=4   ;size for value field in index block
	s len("bstar_rec")=8   ;size for bstar record
	s len("hide_subs")=8   ;size for hidden subscript
	n s s s=""
	f  s s=$o(squals(s)) q:'$l(s)  i $l(squals(s)) d segelm
	n bs s bs="BLOCK_SIZE"
	i $d(squals(bs)),squals(bs)#512 s x=squals(bs),squals(bs)=x\512+1*512
	i  zm gdeerr("BLKSIZ512"):x:squals(bs),gdeerr("SEGIS"):am:SEGMENT
	s s="WINDOW_SIZE"
	i SEGMENT="TEMPLATE" s x=tmpreg("RECORD_SIZE") d segreg q verified
	n REGION s REGION=""
	f  s REGION=$o(regs(REGION)) q:'$l(REGION)  d
	. i regs(REGION,"DYNAMIC_SEGMENT")=SEGMENT s bs=regs(REGION,"RECORD_SIZE") d segreg
	q verified
segreg:
	i am'="USER" d
	. s bs="BLOCK_SIZE",bs=$s($d(squals(bs)):squals(bs),$d(segs(SEGMENT,bs)):segs(SEGMENT,bs),1:tmpseg(am,bs))
	. s f="RESERVED_BYTES",f=$s($d(squals(f)):squals(f),$d(segs(SEGMENT,f)):segs(SEGMENT,f),1:tmpseg(am,f))
	. s x="RECORD_SIZE",x=$s($d(regs(REGION,x)):regs(REGION,x),1:tmpreg(x))
	. s ks="KEY_SIZE",ks=$s($d(regs(REGION,ks)):regs(REGION,ks),1:tmpreg(ks))
	. d key2blk ;GTM-6941
	i '$s(SEGMENT="TEMPLATE":0,1:regs(REGION,"JOURNAL")) q
	s x=$s(SEGMENT="TEMPLATE":tmpreg("BUFFER_SIZE"),1:regs(REGION,"BUFFER_SIZE")) d buf2blk
	i nommbi,$s(SEGMENT="TEMPLATE":0,1:regs(REGION,"BEFORE_IMAGE")) d mmbichk
	q

;-----------------------------------------------------------------------------------------------------------------------------------
; called from GDETEMPL.M

TRQUALS(rquals)
	n REGION,SEGMENT,am s (REGION,SEGMENT)="TEMPLATE",am=tmpacc
	q $$RQUALS(.rquals)
	;
TSQUALS(am,squals)
	n REGION,SEGMENT s (REGION,SEGMENT)="TEMPLATE"
	q $$SQUALS(am,.squals)

