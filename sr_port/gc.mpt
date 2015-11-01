;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1989, 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GC	;GT.M %GC utility - global copy
	;
	n %GI,%GO,%SC,$et s %ZL=$zl,$et="zg "_$ZL_":ERR^%GC"
	u 0:(ctrap=$c(3):exc="zg "_$ZL_":RESTART^%GC")
	w !,"Global copy",!
RESTART	r !,"Show copied nodes <Yes>? ",%SC s %SC=($tr(%SC,"yes","YES")=$e("YES",1,$l(%SC)))
	f  r !,"From global ^",%GI q:%GI=""  d COPY
	u 0:(ctrap="":exc="")
	q
COPY	n c,ix
	i $e(%GI)="?" s ix=%GI d help q
	i $e(%GI)'="^" s %GI="^"_%GI
	i '$d(@%GI) w !,"Global "_%GI_" does not exist." q
	f  r !,"To global   ^",%GO q:$e(%GO)'="?"  s ix=%GO d help
	i %GO="" q
	i $e(%GO)'="^" s %GO="^"_%GO
	i $d(@%GO) w !,"Global "_%GO_" already exists." q
	s c=0
	i '$d(%SC) n %SC s %SC=0
	i $d(@%GI)'[0 s @%GO=@%GI s c=c+1 i %SC w !,%GO,"=",@%GI
	f  s %GI=$q(@%GI) q:%GI=""  s c=c+1,ix="("_$p(%GI,"(",2,999),@(%GO_ix)=@%GI i %SC w !,%GO_ix,"=",@%GI
	w !,"Total ",c," nodes copied.",!
	q
help	i $l(ix)=2,"Dd"[$e(ix,2) d ^%GD u 0:flush q
	W !!,"This routine copies a node and all its descendents"
	w !,"from one global variable to another"
	w !,"""From global"" requests the source for the copy,"
	w !,"""To global"" requests the destination"
	w !,"Use standard MUMPS gvn syntax to specify the node names"
	w !,"?D invokes %GD to get a global directory"
	w !,"<RETURN> drops you back to the prior prompt or out of %GC"
	w !
	q
ERR	w !,$p($zs,",",2,99),!
	u 0:(ctrap="":exc="")
	s $ec=""
	q
