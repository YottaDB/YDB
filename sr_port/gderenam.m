;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rename:	;implement the verb: RENAME
NAME(old,new)
	i old=new q
	i old="*" zm gdeerr("LVSTARALON")
	i '$d(nams(old)) d error1
	i $d(nams(new)) d error2
	s update=1,nams(new)=nams(old),s=""
	f  s s=$o(nams(old,s)) q:'$l(s)  s nams(new,s)=nams(old,s)
	k nams(old)
	q
REGION(old,new)
	i old=new q
	i '$d(regs(old)) d error1
	i $d(regs(new)) d error2
	s update=1,s=""
	f  s s=$o(regs(old,s)) q:'$l(s)  s regs(new,s)=regs(old,s)
	k regs(old)
	f  s s=$o(nams(s)) q:'$l(s)  i nams(s)=old s nams(s)=new
	q
SEGMENT(old,new)
	i old=new q
	i '$d(segs(old)) d error1
	i $d(segs(new)) d error2
	n lquals s update=1,am=segs(old,"ACCESS_METHOD"),s=""
	f  s s=$o(segs(old,s)) q:'$l(s)  s segs(new,s)=segs(old,s),lquals(s)=segs(old,s)
	i '$$SQUALS^GDEVERIF(am,.lquals) k segs(new) zm gdeerr("OBJNOTCHG"):"segment":old
	s segs(new,"ACCESS_METHOD")=am k segs(old)
	f  s s=$o(regs(s)) q:'$l(s)  i regs(s,"DYNAMIC_SEGMENT")=old s regs(s,"DYNAMIC_SEGMENT")=new
	q
error1:
	zm gdeerr("OBJNOTFND"):"Old "_$tr(gqual,upper,lower):old
	q
error2:
	zm gdeerr("OBJDUP"):"New "_$tr(gqual,upper,lower):new
	q
