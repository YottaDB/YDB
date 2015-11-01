;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GSEL	;GT.M %GSEL utility - global select into a local array
	;invoke ^%GSEL to create %ZG - a local array of existing globals, interactively
	;
	n add,beg,cnt,end,g,gd,gdf,k,out,pat,stp,i,c,g1
	i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%GSEL" u $p:(ctrap=$c(3):exc="zg "_$zl_":LOOP^%GSEL")
	d init,main
	u $p:(ctrap="":exc="")
	q
GD	n add,beg,cnt,end,g,gd,gdf,k,out,pat,stp
	s cnt=0,(out,gd,gdf)=1
	d main
	i gdf s %ZG="*" d setup,it w !,"Total of ",cnt," global",$s(cnt=1:".",1:"s."),!
	q
CALL	n add,beg,cnt,end,g,gd,gdf,k,out,pat,stp,i,c,g1
	s (cnt,gd)=0
	i $d(%ZG)>1 s g="" f  s g=$o(%ZG(g)) q:'$l(g)  s cnt=cnt+1
	i $g(%ZG)'?.N s out=0 d setup,it s %ZG=cnt q
	s out=1
	d main
	q
init	k %ZG
	s (cnt,gd)=0,out=1
	q
main	f  d inter q:'$l(%ZG)
	s %ZG=cnt
	q
inter	r !,"Global ^",%ZG,! q:'$l(%ZG)
	i $e(%ZG)="?",$l(%ZG)=1 d help q
	i (%ZG="?D")!(%ZG="?d") d cur q
	d setup,it
	w !,$s(gd:"T",1:"Current t"),"otal of ",cnt," global",$s(cnt=1:".",1:"s."),!
	q
setup	i gd s add=1,cnt=0,g=%ZG k %ZG s %ZG=g
	e  i "'-"[$e(%ZG) s add=0,g=$e(%ZG,2,999)
	e  s add=1,g=%ZG
	s g=$tr(g,"? !""#$&'()+'-./;<=>@[]\^_`{}|~","%")
	s g=$tr(g,$c(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,127))
	s g1="" f i=1:1:$l(g) s c=$e(g,i) if $a(c)<128 s g1=g1_c ; Filter out "all" non-ascii characters (be it M or UTF-8)
	s g=g1
	s end=$p(g,":",2),beg=$p(g,":")
	i end=beg s end=""
	q
it	s gdf=0
	i end'?."*",end']beg q
	s g=beg d pat
	i pat["""" d start f  d search q:'$l(g)  d save
	i pat["""",'$l(end) q
	s beg=stp
	i '$l(g) s g=stp
	s pat=".E",stp="^"_$e(end)_$tr($e(end,2,9999),"%","z") d start f  d search q:'$l(g)  d save
	s g=end d pat
	i pat["""" s:beg]g g=beg d start f  d search q:'$l(g)  d save
	q
pat
	n tmpstp
	i $e(g)="%" s g="!"_$e(g,2,9999)
	s pat=g
	f  q:$l(g,"%")<2  s g=$p(g,"%",1)_"#"_$p(g,"%",2,999),pat=$p(pat,"%",1)_"""1E1"""_$p(pat,"%",2,999)
	f  q:$l(g,"*")<2  s g=$p(g,"*",1)_"$"_$p(g,"*",2,999),pat=$p(pat,"*",1)_""".E1"""_$p(pat,"*",2,999)
	i $e(g)="!" s g="%"_$e(g,2,9999),pat="%"_$e(pat,2,9999)
	i pat["""" s pat="1""^"_pat_""""
	s tmpstp="z",$p(tmpstp,"z",30)="z"
	s g="^"_$p($p(g,"#"),"$"),stp=g_$e(tmpstp,$l(g)-1,31)
	q
start	i g="^" s g="^%"
	i g?@pat,$d(@g) d save
	q
search	f  s g=$o(@g) s:g]stp g="" q:g?@pat!'$l(g)
	q
save	i add,'$d(%ZG(g)) s %ZG(g)="",cnt=cnt+1 d prt:out
	i 'add,$d(%ZG(g)) k %ZG(g) s cnt=cnt-1 d prt:out
	q
prt	w:$x>70 ! w g,?$x\10+1*10
	q
help	;
	w !,?2,"<RET>",?25,"to leave",!,?2,"""*""",?25,"for all"
	w !,?2,"global",?25,"for 1 global"
	w !,?2,"global1:global2",?25,"for a range"
	w !,?2,"""*"" as a wildcard",?25,"permitting any number of characters"
	w !,?2,"""%"" as a wildcard",?25,"for a single character in positions other than the first"
	w !,?2,"""?"" as a wildcard",?25,"for a single character in positions other than the first"
	i gd q
	w !,?2,"""'"" as the 1st character",!,?25,"to remove globals from the list"
	w !,?2,"?D",?25,"for the currently selected globals",!
	q
cur	w ! s g=""
	f  s g=$o(%ZG(g)) q:'$l(g)  w:$x>70 ! w g,?($x\10+1*10)
	q
ERR	u $p w !,$p($ZS,",",2,999),!
	u $p:(ctrap="":exc="")
	s $ec=""
	q
LOOP	d main
	u $p:(ctrap="":exc="")
	q
