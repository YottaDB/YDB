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
template:	;implement the verb: TEMPLATE
REGION
	i $d(lquals("JOURNAL")),lquals("JOURNAL"),'tmpreg("JOURNAL"),'$d(lquals("BEFORE_IMAGE")) d
	. zm gdeerr("QUALREQD"):"Before_image"
	i $d(lquals("NULL_SUBSCRIPTS")) d NQUALS^GDEVERIF(.lquals)
	i '$$TRQUALS^GDEVERIF(.lquals) zm gdeerr("OBJNOTCHG"):"region":"template"
	s update=1,s=""
	f  s s=$o(lquals(s)) q:'$l(s)  s tmpreg(s)=lquals(s) i s="ALLOCATION" s tmpreg("EXTENSION")=lquals(s)\10
	q
SEGMENT
	i $d(lquals("ACCESS_METHOD")) s am=lquals("ACCESS_METHOD")
	e  s am=tmpacc
	i '$$TSQUALS^GDEVERIF(am,.lquals) zm gdeerr("OBJNOTCHG"):"segment":"template"
	s update=1,s="",tmpacc=am
	f  s s=$o(lquals(s)) q:'$l(s)  s tmpseg(tmpacc,s)=lquals(s)
	q
