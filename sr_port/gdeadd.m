;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2012 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
add:	;implement the verb: ADD
NAME
	i $d(nams(NAME)) zm gdeerr("OBJDUP"):"Name":NAME
	i '$d(lquals("REGION")) zm gdeerr("QUALREQD"):"Region"
	s update=1,nams=nams+1
	s nams(NAME)=lquals("REGION")
	q
REGION
	i $d(regs(REGION)) zm gdeerr("OBJDUP"):"Region":REGION
	i '$d(lquals("DYNAMIC_SEGMENT")) zm gdeerr("QUALREQD"):"Dynamic_segment"
	i $d(lquals("JOURNAL")),lquals("JOURNAL"),'$d(lquals("BEFORE_IMAGE")) zm gdeerr("QUALREQD"):"Before_image"
	i $d(lquals("NULL_SUBSCRIPTS")) d NQUALS^GDEVERIF(.lquals)
	i '$$RQUALS^GDEVERIF(.lquals) zm gdeerr("OBJNOTADD"):"Region":REGION
	s update=1,s="",regs=regs+1
	f  s s=$o(tmpreg(s)) q:'$l(s)  s regs(REGION,s)=tmpreg(s)
	f  s s=$o(lquals(s)) q:'$l(s)  s regs(REGION,s)=lquals(s)
	i $d(segs),$d(regs(REGION,"DYNAMIC_SEGMENT")),$d(segs(regs(REGION,"DYNAMIC_SEGMENT"),"ACCESS_METHOD")) d
	. i "MM"=segs(regs(REGION,"DYNAMIC_SEGMENT"),"ACCESS_METHOD"),'$d(lquals("BEFORE_IMAGE")) s regs(REGION,"BEFORE_IMAGE")=0
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
