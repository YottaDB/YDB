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
rename:	;implement the verb: RENAME
NAME(old,new)
	i old=new q
	i old="*" zm gdeerr("LVSTARALON")
	i '$d(nams(old)) d error1
	i $d(nams(new)) d error2
	; check if changing a name (with ranges) poses issues with overlap amongst other existing name ranges
	d namerangeoverlapcheck^GDEPARSE(.new,nams(old),old)
	s update=1
	m nams(new)=new
	s nams(new)=nams(old)
	k nams(old)
	i $d(namrangeoverlap) d namcoalesce^GDEMAP
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
GBLNAME(old,new)
	i old=new q
	i '$d(gnams(old)) d error1
	; check if changing (i.e. deleting) collation for "old" GBLNAME poses issues with existing names & ranges
	i $d(gnams(old,"COLLATION")) d
	. d gblnameeditchecks^GDEPARSE(old,0)
	. i $d(namrangeoverlap) d namcoalesce^GDEMAP
	i $d(gnams(new)) d error2
	; check if changing (i.e. adding) collation for "new" GBLNAME poses issues with existing names & ranges
	d gblnameeditchecks^GDEPARSE(new,+$get(gnams(old,"COLLATION")))
	i $d(namrangeoverlap) d namcoalesce^GDEMAP
	s update=1
	m gnams(new)=gnams(old)
	k gnams(old)
	q
error1:
	zm gdeerr("OBJNOTFND"):"Old "_$tr(gqual,upper,lower):old
	q
error2:
	zm gdeerr("OBJDUP"):"New "_$tr(gqual,upper,lower):new
	q
