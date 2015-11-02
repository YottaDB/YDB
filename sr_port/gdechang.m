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
change:	;implement the verb: CHANGE
NAME
	i '$d(nams(NAME)) zm gdeerr("OBJNOTFND"):"Name":NAME
	i '$d(lquals("REGION")) zm gdeerr("QUALREQD"):"Region"
	s update=1
	s nams(NAME)=lquals("REGION")
	q
REGION
	i '$d(regs(REGION)) zm gdeerr("OBJNOTFND"):"Region":REGION
	i $d(lquals("JOURNAL")),lquals("JOURNAL"),'regs(REGION,"JOURNAL"),'$d(lquals("BEFORE_IMAGE")) d
	. zm gdeerr("QUALREQD"):"Before_image"
	i $d(lquals("NULL_SUBSCRIPTS")) d NQUALS^GDEVERIF(.lquals)
	i '$$RQUALS^GDEVERIF(.lquals) zm gdeerr("OBJNOTCHG"):"region":REGION
	s update=1,s=""
	f  s s=$o(lquals(s)) q:'$l(s)  s regs(REGION,s)=lquals(s)
	q
SEGMENT
	i '$d(segs(SEGMENT)) zm gdeerr("OBJNOTFND"):"Segment":SEGMENT
	s am=$s($d(lquals("ACCESS_METHOD")):lquals("ACCESS_METHOD"),1:segs(SEGMENT,"ACCESS_METHOD"))
	i '$$SQUALS^GDEVERIF(am,.lquals) zm gdeerr("OBJNOTCHG"):"segment":SEGMENT
	s update=1,s=""
	s segs(SEGMENT,"ACCESS_METHOD")=am
	f  s s=$o(lquals(s)) q:'$l(s)  s segs(SEGMENT,s)=lquals(s)
	f  s s=$o(tmpseg(am,s)) q:'$l(s)  d
	. i '$l(tmpseg(am,s)) s segs(SEGMENT,s)="" q
	. i '$l(segs(SEGMENT,s)) s segs(SEGMENT,s)=tmpseg(am,s)
	i "MM"=am,"MM"'=tmpacc s s="" f  s s=$o(regs(s)) q:'$l(s)  d
	. i regs(s,"DYNAMIC_SEGMENT")=SEGMENT,'$d(lquals("BEFORE_IMAGE")) s regs(s,"BEFORE_IMAGE")=0
	q
