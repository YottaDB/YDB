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
gdemsgin:	;message initialization - equate global symbols to numeric values
GDEMSGIN
	s gdeerr("BLKSIZ512")=150503435
	s gdeerr("EXECOM")=150503443
	s gdeerr("FILENOTFND")=150503451
	s gdeerr("GDCREATE")=150503459
	s gdeerr("GDECHECK")=150503467
	s gdeerr("GDUNKNFMT")=150503475
	s gdeerr("GDUPDATE")=150503483
	s gdeerr("GDUSEDEFS")=150503491
	s gdeerr("ILLCHAR")=150503498
	s gdeerr("INPINTEG")=150503506
	s gdeerr("KEYSIZIS")=150503523
	s gdeerr("KEYTOOBIG")=150503515
	s gdeerr("KEYFORBLK")=150503891
	s gdeerr("KEYWRDAMB")=150503530
	s gdeerr("KEYWRDBAD")=150503538
	s gdeerr("LOADGD")=150503547
	s gdeerr("LOGOFF")=150503555
	s gdeerr("LOGON")=150503563
	s gdeerr("LVSTARALON")=150503570
	s gdeerr("MAPBAD")=150503579
	s gdeerr("MAPDUP")=150503587
	s gdeerr("NAMSTARTBAD")=150503594
	s gdeerr("NOACTION")=150503603
	s gdeerr("RPAREN")=150503610
	s gdeerr("NOEXIT")=150503619
	s gdeerr("NOLOG")=150503627
	s gdeerr("NONEGATE")=150503642
	s gdeerr("NOVALUE")=150503634
	s gdeerr("OBJDUP")=150503650
	s gdeerr("OBJNOTADD")=150503658
	s gdeerr("OBJNOTCHG")=150503666
	s gdeerr("OBJNOTFND")=150503674
	s gdeerr("OBJREQD")=150503682
	s gdeerr("PREFIXBAD")=150503690
	s gdeerr("QUALBAD")=150503698
	s gdeerr("QUALDUP")=150503706
	s gdeerr("QUALREQD")=150503714
	s gdeerr("RECSIZIS")=150503731
	s gdeerr("RECTOOBIG")=150503723
	s gdeerr("REGIS")=150503739
	s gdeerr("SEGIS")=150503747
	s gdeerr("VALTOOBIG")=150503755
	s gdeerr("VALTOOLONG")=150503762
	s gdeerr("VALTOOSMALL")=150503771
	s gdeerr("VALUEBAD")=150503778
	s gdeerr("VALUEREQD")=150503786
	s gdeerr("VERIFY")=150503795
	s gdeerr("BUFSIZIS")=150503803
	s gdeerr("BUFTOOSMALL")=150503811
	s gdeerr("MMNOBEFORIMG")=150503819
	s gdeerr("NOJNL")=150503827
	s gdeerr("GDREADERR")=150503835
	s gdeerr("GDNOTSET")=150503843
	s gdeerr("INVGBLDIR")=150503851
	s gdeerr("WRITEERROR")=150503859
	s gdeerr("NONASCII")=150503866
	s gdeerr("CRYPTNOMM")=150503874
	s gdeerr("JNLALLOCGROW")=150503883
	q
