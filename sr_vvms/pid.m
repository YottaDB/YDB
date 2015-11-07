;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1990, 2002 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
PID	; operations on VMS pids
	;
DELPRC(p);
	n id,oldpriv,st,$zt
	s ($zt,zt)="d error",oldpriv=$zsetprv("GROUP,WORLD"),id=""
	f  s id=$o(p(id)) q:id=""  s st=$zc("DELPRC",id) w !,"Deleting process ",p(id)
	i $zsetprv(oldpriv)
	q
FORCEX(p);
	n id,oldpriv,st,$zt
	s ($zt,zt)="d error",oldpriv=$zsetprv("GROUP,WORLD"),id=""
	f  s id=$o(p(id)) q:id=""  s st=$zc("FORCEX",id) w !,"Stopping process ",p(id)
	i $zsetprv(oldpriv)
	q
GETIMG(img,p);
	q:'$l($g(img))
	n id,oldpriv,$zt
	k p s p=0,img=$tr(img,"abcdefghijklmnopqrstuvwxyz","ABCDEFGHIJKLMNOPQRSTUVWXYZ")
	s ($zt,zt)="d error",oldpriv=$zsetprv("GROUP,WORLD"),id=$zpid(0)
	d  f  s id=$zpid(1) q:id=""  d
	. i $zparse($zgetjpi(id,"IMAGNAME"),"NAME")=img s p(id)=$$FUNC^%DH(id,8),p=p+1
	i $zsetprv(oldpriv)
	q
SHOW(p);
	n id s id=""
	f  s id=$o(p(id)) q:id=""  w !,p(id)
	q
SHOWIMG	n image,pid
	r !,"Image name: ",image d GETIMG(image,.pid)
	i 'pid w !,"No processes found running that image" q
	w !,"The following processes are running image ",image,":"
	d SHOW(.pid)
	q
STOPIMG	n image,pid,wait
	r !,"Image name: ",image d GETIMG(image,.pid)
	i 'pid w !,"No processes found running that image" q
	r !,"Pause in seconds between FORCEX and DELPRC: ",wait
	d STPIMG(image,wait)
	q
STPIMG(img,wt)
	q:'$l($g(img))
	n t,pid s wt=$g(wt) s:wt<1 wt=20
	d GETIMG(img,.pid)
	i 'pid w !,"No processes stopped" q
	d FORCEX(.pid)
	f t=1:1:wt h 1 d GETIMG(img,.pid) q:'pid
	i pid d DELPRC(.pid):pid
	q
error	w !
	i $d(id),id w "Unable to access process with pid: ",$$FUNC^%DH(id)
	e  w $p($zs,",",2,99),!
	i $zs["GETIMG" s id=0
	s $zt=zt
	q
