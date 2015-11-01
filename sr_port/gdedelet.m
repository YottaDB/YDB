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
delete:	;implement the verb: DELETE
NAME
	i '$d(nams(NAME)) zm gdeerr("OBJNOTFND"):"Name":NAME
	i NAME="*" zm gdeerr("LVSTARALON")
	s update=1 k nams(NAME) s nams=nams-1
	q
REGION
	i '$d(regs(REGION)) zm gdeerr("OBJNOTFND"):"Region":REGION
	s update=1 k regs(REGION) s regs=regs-1
	q
SEGMENT
	i '$d(segs(SEGMENT)) zm gdeerr("OBJNOTFND"):"Segment":SEGMENT
	s update=1 k segs(SEGMENT) s segs=segs-1
	q
