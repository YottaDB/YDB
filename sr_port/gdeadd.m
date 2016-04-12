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
add:	;implement the verb: ADD
NAME
	i $d(nams(NAME)) zm gdeerr("OBJDUP"):"Name":$$namedisp^GDESHOW(NAME,0)
	i '$d(lquals("REGION")) zm gdeerr("QUALREQD"):"Region"
	; check if changing (i.e. adding) a name (with ranges) poses issues with overlap amongst other existing name ranges
	d namerangeoverlapcheck^GDEPARSE(.NAME,lquals("REGION"))
	s update=1,nams=nams+1
	m nams(NAME)=NAME
	s nams(NAME)=lquals("REGION")
	i $d(namrangeoverlap) d namcoalesce^GDEMAP
	q
REGION
	i $d(regs(REGION)) zm gdeerr("OBJDUP"):"Region":REGION
	i '$d(lquals("DYNAMIC_SEGMENT")) zm gdeerr("QUALREQD"):"Dynamic_segment"
	i '$$RQUALS^GDEVERIF(.lquals) zm gdeerr("OBJNOTADD"):"Region":REGION
	s update=1,s="",regs=regs+1
	f  s s=$o(tmpreg(s)) q:'$l(s)  s regs(REGION,s)=tmpreg(s)
	f  s s=$o(lquals(s)) q:'$l(s)  s regs(REGION,s)=lquals(s)
	i $d(segs),$d(regs(REGION,"DYNAMIC_SEGMENT")),$d(segs(regs(REGION,"DYNAMIC_SEGMENT"),"ACCESS_METHOD")) d
	. i "MM"=segs(regs(REGION,"DYNAMIC_SEGMENT"),"ACCESS_METHOD"),'$d(lquals("BEFORE_IMAGE")) s regs(REGION,"BEFORE_IMAGE")=0
	. i "USER"[segs(regs(REGION,"DYNAMIC_SEGMENT"),"ACCESS_METHOD"),'$d(lquals("JOURNAL")) s regs(REGION,"JOURNAL")=0
	q
SEGMENT
	i $d(segs(SEGMENT)) zm gdeerr("OBJDUP"):"Segment":SEGMENT
	i '$d(lquals("FILE_NAME")) zm gdeerr("QUALREQD"):"File"
	s am=$s($d(lquals("ACCESS_METHOD")):lquals("ACCESS_METHOD"),1:tmpacc)
	i '$$SQUALS^GDEVERIF(am,.lquals) zm gdeerr("OBJNOTADD"):"Segment":SEGMENT
	s update=1,s="",segs=segs+1
	s segs(SEGMENT,"ACCESS_METHOD")=am
	i "MM"=am s s="" f  s s=$o(regs(s)) q:'$l(s)  d
	. i regs(s,"DYNAMIC_SEGMENT")=SEGMENT,'$d(lquals("BEFORE_IMAGE")) s regs(s,"BEFORE_IMAGE")=0
	d seg
	f  s s=$o(lquals(s)) q:'$l(s)  s segs(SEGMENT,s)=lquals(s)
	q
seg:	s segs(SEGMENT,"FILE_NAME")=lquals("FILE_NAME")
	f  s s=$o(tmpseg(am,s)) q:'$l(s)  s segs(SEGMENT,s)=tmpseg(am,s)
	q
GBLNAME
	i $d(gnams(GBLNAME)) zm gdeerr("OBJDUP"):"Global Name":GBLNAME
	i '$d(lquals("COLLATION")) zm gdeerr("QUALREQD"):"Collation"
	; check if changing (i.e. adding) collation for GBLNAME poses issues with existing names & ranges
	d gblnameeditchecks^GDEPARSE(GBLNAME,lquals("COLLATION"))
	i $d(namrangeoverlap) d namcoalesce^GDEMAP
	s update=1,gnams=gnams+1
	s gnams(GBLNAME,"COLLATION")=lquals("COLLATION")
	q
