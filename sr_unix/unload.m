;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2002 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
unload(test,host,pid,testnum)
	s zdstr="YY/MM/DD-24:60:SS"
	s ^test(testnum,test,"current_status")="------- FINISHING -----------"
	l ^master
		s cpucur=$$^getnear("^cpucur")
		s iocur=$$^getnear("^iocur")
		s ^cpucur=cpucur-^cpucur(testnum,test,pid,host)
		s ^iocur=iocur-^iocur(testnum,test,pid,host)
		k ^cpucur(pid)
		k ^iocur(pid)
	l
	h 1
	s ^test(testnum,test,6,"termination_time")=$zdate($h,zdstr);
	s ^test(testnum,test,"current_status")="------- TERMINATED -----------"
	q
