;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2001-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
change:	;implement the verb: CHANGE
NAME
	i '$d(nams(NAME)) zm gdeerr("OBJNOTFND"):"Name":$$namedisp^GDESHOW(NAME,0)
	i '$d(lquals("REGION")) zm gdeerr("QUALREQD"):"Region"
	; check if changing the mapping of a name (with ranges) poses issues with overlap amongst other existing name ranges
	d namerangeoverlapcheck^GDEPARSE(.NAME,lquals("REGION"))
	s update=1
	m nams(NAME)=NAME
	s nams(NAME)=lquals("REGION")
	i $d(namrangeoverlap) d namcoalesce^GDEMAP
	q
REGION
	i '$d(regs(REGION)) zm gdeerr("OBJNOTFND"):"Region":REGION
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
	i ("MM"=am)!("USER"[am) d
	. s s="" f  s s=$o(regs(s)) q:'$l(s)  d
	. . i "MM"=am,(regs(s,"DYNAMIC_SEGMENT")=SEGMENT),'$d(lquals("BEFORE_IMAGE")) s regs(s,"BEFORE_IMAGE")=0
	. . i "USER"[am,(regs(s,"DYNAMIC_SEGMENT")=SEGMENT),'$d(lquals("JOURNAL")) s regs(s,"JOURNAL")=0
	q
GBLNAME
	i '$d(gnams(GBLNAME)) zm gdeerr("OBJNOTFND"):"Global Name":GBLNAME
	i '$d(lquals("COLLATION")) zm gdeerr("QUALREQD"):"Collation"
	i gnams(GBLNAME,"COLLATION")=lquals("COLLATION") zm gdeerr("OBJNOTCHG"):"gblname":GBLNAME
	; check if changing collation for GBLNAME poses issues with existing names & ranges
	d gblnameeditchecks^GDEPARSE(GBLNAME,lquals("COLLATION"))
	i $d(namrangeoverlap) d namcoalesce^GDEMAP
	s update=1
	s gnams(GBLNAME,"COLLATION")=lquals("COLLATION")
	q
INSTANCE
	i '$d(lquals("FILE_NAME")) zm gdeerr("QUALREQD"):"File Name"
	i $zl(lquals("FILE_NAME"))=0 d:$d(inst("FILE_NAME"))  q
	. kill inst
	. s update=1,inst=0
	i $d(inst("FILE_NAME")),inst("FILE_NAME")=lquals("FILE_NAME") d
	. i inst("FILE_NAME")'=$g(inst("envvar")) zm gdeerr("OBJNOTCHG"):"instance":""
	s update=1
	s inst=1,inst("FILE_NAME")=lquals("FILE_NAME")
	q
