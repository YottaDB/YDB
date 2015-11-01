load(test,host,pid,testnum)
	s zdstr="YY/MM/DD-24:60:SS"
	s ^test(testnum,test,1,"host")=host
	s ^test(testnum,test,2,"pid")=pid
	s ^test(testnum,test,3,"wait_starttime")=$zdate($h,zdstr)
	s ^test(testnum,test,"current_status")="------- WAITING -----------"
	f loadi=1:1 q:$get(^cpucur(testnum,test,pid,host))'=""  d
	.	s ^test(testnum,test,4,"wait_loopcnt")=loadi
	.	l ^master
	.		s cpumax=$$^getnear("^cpumax"_"("""_test_""","""_host_""")")
	.		s iomax=$$^getnear("^iomax"_"("""_test_""","""_host_""")")
	.		s cpucur=$$^getnear("^cpucur")
	.		s iocur=$$^getnear("^iocur")
	.		s cpu=$$^getnear("^cpu"_"("""_test_""","""_host_""")")
	.		s io=$$^getnear("^io"_"("""_test_""","""_host_""")")
	.		i (cpu+cpucur'>cpumax)&(io+iocur'>iomax)  d
	.		.	s ^readytorun=1
	.		.	i (testnum=^waittestnum)  s ^waittestnum=^waittestnum+1
	.		.	e  i (testnum>^waittestnum)  q
	.		.	s ^cpucur=cpucur+cpu
	.		.	s ^iocur=iocur+io
	.		.	s ^cpucur(testnum,test,pid,host)=cpu
	.		.	s ^iocur(testnum,test,pid,host)=io
	.		.	s ^readytorun=0
	.		i (testnum=^waittestnum)&(^readytorun=1) s ^waittestnum=^waittestnum+1
	.	l
	.	h 1
	s ^test(testnum,test,5,"run_starttime")=$zdate($h,zdstr);
	s ^test(testnum,test,"current_status")="------- RUNNING -----------"
	q
