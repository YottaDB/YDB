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
verify:	;implement the verb: VERIFY, also invoked from show and GDEGET
ALL()	;external
	n verified,gqual s verified=1
	s gqual="NAME" d ALLNAM
	s gqual="REGION" d ALLREG,usereg
	s gqual="SEGMENT" d ALLSEG,useseg
	d ALLTEM
	zm gdeerr("VERIFY"):$s(verified:"OK",1:"FAILED") w !
	q verified

;-----------------------------------------------------------------------------------------------------------------------------------
; called from GDEPARSE.M

ALLNAM
	n NAME s NAME=""
	f  s NAME=$o(nams(NAME)) q:'$l(NAME)  d name1
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
	i '$d(nams(NAME)) k verified zm $$info(gdeerr("OBJNOTFND")):"Name":$s(NAME'="#":NAME,1:"Local Locks") q
name1:	i '$d(regs(nams(NAME))) s verified=0 zm gdeerr("MAPBAD"):"Region":nams(NAME):"Name":$s(NAME'="#":NAME,1:"Local Locks")
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
	i "MM"=am,1=squals("ENCRYPTION_FLAG") s verified=0 zm $$info(gdeerr("CRYPTNOMM")):s,gdeerr("SEGIS"):am:SEGMENT
	s x=$$SQUALS(am,.squals)
	q
usereg:	n REGION,NAME s REGION=""
	f  s REGION=$o(regs(REGION)) q:'$l(REGION)  d usereg1
	q
usereg1:	s NAME=""
	f  s NAME=$o(nams(NAME)) q:$g(nams(NAME))=REGION!'$l(NAME)
	i '$l(NAME) s verified=0 zm gdeerr("MAPBAD"):"A":"NAME":"REGION":REGION
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
key2blk:	;bs:block size, y:supportable max key size, f:size of reserved bytes, ks:key size
	s y=bs-f-SIZEOF("blk_hdr")-len("min_val")-SIZEOF("rec_hdr")-len("bstar_rec")-len("hide_subs")
	i ks>y s verified=0 zm gdeerr("KEYSIZIS"):ks,gdeerr("KEYFORBLK"):bs:y,gdeerr("REGIS"):REGION
	q
buf2blk:	i REGION="TEMPLATE","USER"[am,am'=tmpacc q
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
	i $d(alloc),$d(asl),alloc>asl s verified=0 zm gdeerr("VALTOOBIG"):alloc:asl_" (AUTOSWITCHLIMIT)":"ALLOCATION" q
	i $d(ext),$d(alloc),$d(asl),alloc'=asl,ext+alloc>asl d
	. s rquals("ALLOCATION")=asl
	. zm gdeerr("JNLALLOCGROW"):alloc:asl:"region":REGION
	q
prefix(str1,str2)  ;check whether str1 is a prefix of str2
	n len1,len2
	s len1=$l(str1),len2=$l(str2)
	q:(len1>len2)!'len1 0
	i ($e(str2,1,len1)=str1) q 1
	q 0

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
   f  s REGION=$o(regs(REGION)) q:'$l(REGION)  i regs(REGION,"DYNAMIC_SEGMENT")=SEGMENT s bs=regs(REGION,"RECORD_SIZE") d segreg
	q verified
segreg:
	i am'="USER" d
	. s bs="BLOCK_SIZE",bs=$s($d(squals(bs)):squals(bs),$d(segs(SEGMENT,bs)):segs(SEGMENT,bs),1:tmpseg(am,bs))
	. s f="RESERVED_BYTES",f=$s($d(squals(f)):squals(f),$d(segs(SEGMENT,f)):segs(SEGMENT,f),1:tmpseg(am,f))
	. s x="RECORD_SIZE",x=$s($d(regs(REGION,x)):regs(REGION,x),1:tmpreg(x))
	. s ks="KEY_SIZE",ks=$s($d(regs(REGION,ks)):regs(REGION,ks),1:tmpreg(ks))
	. d key2blk ;GTM-6941
	i '$s(SEGMENT="TEMPLATE":tmpreg("JOURNAL"),1:regs(REGION,"JOURNAL")) q
	s x=$s(SEGMENT="TEMPLATE":tmpreg("BUFFER_SIZE"),1:regs(REGION,"BUFFER_SIZE")) d buf2blk
	i nommbi,$s(SEGMENT="TEMPLATE":tmpreg("BEFORE_IMAGE"),1:regs(REGION,"BEFORE_IMAGE")) d mmbichk
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

;-----------------------------------------------------------------------------------------------------------------------------------
; called from GDEADD.M, GDECHANG.M and GDETEMPL.M, [GTM-7184]
NQUALS(rquals)
	n nullsub s nullsub=rquals("NULL_SUBSCRIPTS")
	i ($$prefix(nullsub,"NEVER")!$$prefix(nullsub,"FALSE")) s rquals("NULL_SUBSCRIPTS")=0
	e  d
	. i ($$prefix(nullsub,"ALWAYS")!$$prefix(nullsub,"TRUE")) s rquals("NULL_SUBSCRIPTS")=1
	. e  d
	. . i ($$prefix(nullsub,"EXISTING")) s rquals("NULL_SUBSCRIPTS")=2
	q

