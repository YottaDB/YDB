;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2010 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
loadvx	;load op codes
	n (vxi,vxx,loadh)
	k vxi,vxx
	s lnr=0
	s file=loadh("vxi.h") o file:read u file
loop	r x i $zeof g fini
	s rec=x,lnr=lnr+1
	d proc
	g loop
fini	c file
	q
proc	i $e(x,"1")'="#" q
pr0     s x=$tr(x,$c(9)," ")
	i x'?1"#define"1." "1"VXI_"1.AN1." "1"(0x"1.3AN1")".E s ec=1 g err
	s y=$f(x,"VXI_"),x=$e(x,y,999)
	s cd=$p(x," ",1),cdx="VXI_"_cd
	f i=1:1:$l(cd) i $e(cd,i)?1U s cd=$e(cd,1,i-1)_$c($a(cd,i)+32)_$e(cd,i+1,999)
	s val=$p($p($p(x,"(",2),")",1),"0x",2)
	d hex2dec
	i $d(vxx(val)) s ec=2 g err
	i $d(vxi(cd)) s ec=3 g err
	s vxi(cd)=val,vxi(cd,1)=cdx,vxx(val)=cd
	q
err	u "" w rec,!,"error code=",ec,"   line=",lnr,!
	u file
	q
hex2dec	n n,x,i
	s n=0,i=1
h2d1	s x=$e(val,i) i x="" s val=n q
	i x?1U s x=$a(x)-55
	e  i x?1L s x=$a(x)-87
	e  i x'?1N b
	s n=n*16+x,i=i+1
	g h2d1
