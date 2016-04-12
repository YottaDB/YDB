;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%H	;GT.M %H utility - date and time conversions to and from $H format
	;invoke with %DT in $H day format at %CDS to set %DAT mm/dd/yyyy
	;invoke with %TM in $H time format at %CTS to set %TIM hh:mm:ss
	;invoke with %DT in mm/dd/yy[yy] format at %CDN to set %DAT to $H form
	;invoke with %TM in hh:mm:ss format at %CTN to set %TIM to $H format
	;the labels without the % are corresponding functions
	q
%CDS	S %DAT=$$CDS(%DT)
	q
CDS(dt)
	i dt'<0,dt<94658 q +$zd(dt,"MM")_"/"_+$zd(dt,"DD")_$zd(dt,"/YEAR")
	q ""
	;
%CTS	s %TIM=$$CTS(%TM)
	q
CTS(tm)
	i tm'<0,tm<86400 q $zd(","_tm,"24:60:SS")
	q ""
	;
%CDN	s %DAT=$$CDN(%DT)
	q
CDN(dt)
	n cc,dat,dd,mm,yy,dh,zd
	s mm=+dt,dd=$p(dt,"/",2),yy=$p(dt,"/",3),dat=""
	i mm<1 q ""
	i mm>13 q ""
	i dd<1 q ""
	s zd=$ZDATEFORM
	i $l(yy)<3 d
	. s dh=$H
	. s yy=yy+(100*$S('zd:19,(zd>1840)&($L(zd)=4):($E(zd,1,2)+$S($E(zd,3,4)'>yy:0,1:1)),1:$E($ZDATE(dh,"YEAR"),1,2)))
	;                  20th           rolling                                current century
	i dd>$s(+mm'=2:$e(303232332323,mm)+28,yy#4:28,yy#100:29,yy#400:28,1:29) q ""
	s dat=yy-1841,mm=mm-1,cc=1
	i dat<0 s dd=dd-1,cc=-1
	s dat=dat\4*1461+(dat#4-$s(dat'<0:0,1:4)*365)+(mm*30)+$e(10112234455,mm)+dd-(yy-1800\100-(yy-1600\400))
	i yy#4,mm>1 s dat=dat-cc
	i yy#100=0,mm<2,yy#400 s dat=dat+cc
	q dat
	;
%CTN	s %TIM=$$CTN(%TM)
	q
CTN(tm)
	n h,m,s
	s h=+tm,m=$p(tm,":",2),s=$p(tm,":",3)
	i h'<0,h<24,m'<0,m<60,s'<0,s<60 q h*60+m*60+s
	q ""
