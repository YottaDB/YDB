;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2007 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
tttscan	;parse ttt.txt
	n (work,infile,opc,opx,vxi,vxx,ttt,opcdcnt)
	k work,ttt
	s ttt=opcdcnt
	s eof=0,eofcd=$c(26),eol=$c(13)
	s linnum=0,instr="",ptr=1,dir="" d scan,scan
	d proc
	c infile
	q
proc	d block q:tok=eofcd
	g proc:'z
proc1	i tok'?1"OC_"1.AN q:tok=eofcd  d scan g proc1
	g proc
block	i tok=eol d scan g block
	d head q:z
	s work(indx)=ttt
	d body i z s em="invalid template body" d err
	s ttt(ttt)="VXT_END",ttt=ttt+1
	q
head	i tok'?1"OC_"1.AN.1"_".AN s z=1,em="invalid opcode name" d err,scan q
	i '$d(opc(tok)) s em="triple op code not defined in opcode.h",z=1 d err,scan q
	s indx=opc(tok)
	s opcode=tok,case=""
	d scan
	i tok=":" d scan s z=0 q
	i tok'="-" s em="dash or colon expected here" d err s z=1 q
	d scan
	i tok="BYTE" s indx=indx+.1
	e  i tok="WORD" s indx=indx+.2
	e  i tok="LONG" s indx=indx+.3
	e  i tok="MVAL" s indx=indx+.1
	e  i tok="MSTR" s indx=indx+.2
	e  i tok="MFLT" s indx=indx+.3
	e  i tok="MINT" s indx=indx+.4
	e  i tok="BOOL" s indx=indx+.5
	e  i tok="MVADDR" s indx=indx+.6
	e  s em="byte, word or long expected here" s z=1 d err q
	d scan
	i tok'=":" s em="colon expected here" d err s z=1 q
	d scan
	s z=0
	q
body	i dir=":"!((dir="-")&(tok?1"OC_"1.AN)) s z=0 q
	i $d(vxi(tok)) s ttt(ttt)=vxi(tok,1),ttt=ttt+1
	e  i tok="irepab" s ttt(ttt)="VXT_IREPAB",ttt=ttt+1
	e  i tok="irepl" s ttt(ttt)="VXT_IREPL",ttt=ttt+1
	e  i $c(26)=tok&(tok=dir) s z=0 q
	e  s em="vax opcode not defined" d err s z=1 q
	d scan
bod1	d arg i tok="," d scan g bod1
	i tok=eol!(tok=eofcd) d scan g body
	s em="comma or end of line expected" d err s z=1
	q
arg	i tok="val" s x="VAL" g argval
	i tok="addr" s x="ADDR" g argval
	i tok="vnum" s x="VNUM" g argval
	i tok="jmp" s x="JMP" g argval
	i tok="#" g arglit
	i tok="xfer" g argxfer
	i tok="G",dir="^" d scan,scan s x="GREF" g argval
	i tok'="@" s gotat=0
	e  s gotat=1 d scan
	s displ=""
	i tok="(" g argpara
	i tok="-" g argpush
	i dir="(" s displ=tok d scan g argpara
	d argreg i z q
	i gotat s mode=6
	e  s mode=5
	d argstsh
	s z=0
	q
argval	d scan i tok'="." s em="period (.) expected" d err s z=1 q
	s ttt(ttt)="VXT_"_x,ttt=ttt+1 d scan s ttt(ttt)=tok,ttt=ttt+1
	d scan
	s z=0
	q
arglit	s ttt(ttt)="VXT_LIT",ttt=ttt+1 d scan s ttt(ttt)=tok,ttt=ttt+1
	d scan
	s z=0
	q
argxfer	d scan i tok'="." s em="period (.) expected" d err s z=1 q
	s ttt(ttt)="VXT_XFER",ttt=ttt+1 d scan s ttt(ttt)="SIZEOF(char *) * (short int)"_tok,ttt=ttt+1
	d scan
	s z=0
	q
argpara	d scan,argreg i z s em="register expected" d err q
	i tok'=")" s em="right paranthesis expected" d err s z=1 q
	d scan
	i tok'="+" s mode=$s(gotat:11,1:6)
	e  d scan s mode=$s(gotat:9,1:8)
	i displ'="" s mode=$s(mode=6:10,1:11)
	i mode=11,displ="" s displ=0
	d argstsh
	i displ'="" s ttt(ttt)="VXT_DISP",ttt=ttt+1,ttt(ttt)=displ,ttt=ttt+1
	s z=0
	q
argpush	d scan i tok'="(" s em="left paranthesis expected" d err s z=1 q
	i gotat s em="illegal addressing mode" d err s z=1 q
	d scan,argreg i z s em="register expected" d err s z=1 q
	i tok'=")" s em="right paranthesis expected" d err s z=1 q
	d scan
	s mode=7
	d argstsh
	s z=0
	q
argreg	n x
	i tok?1"r"1.2n s x=+$e(tok,2,3) g argreg1:x<16 s z=1 q
	i tok="pc" s x=15
	e  i tok="sp" s x=14
	e  i tok="fp" s x=13
	e  i tok="ap" s x=12
	e  s z=1 q
argreg1	d scan s z=0,regnum=x
	q
argstsh	s ttt(ttt)="VXT_REG",ttt=ttt+1
	s ttt(ttt)="0x"_$s(mode>9:$c(mode+55),1:mode)_$s(regnum>9:$c(regnum+55),1:regnum)
	s ttt=ttt+1
	q
err	i erflag q
	u "" w "%TTTGEN-F ",em," at line ",oldlin," character ",oldptr,!
	s erflag=1
	q
scan	i eof g sceof
	s oldlin=linnum,oldptr=ptr
	i $e(instr,ptr)'="" g sc1
	u infile r instr g sceof:$zeof s erflag=0,linnum=linnum+1,ptr=1,t=eol u "" w $j(linnum,6),"  ",instr,! g scfin
sc1	i $c(9)_" "[$e(instr,ptr) s ptr=ptr+1 g scan
	i $e(instr,ptr)?1P s t=$e(instr,ptr),ptr=ptr+1 g scfin:t'=";" s ptr=9999 g scan
	i $e(instr,ptr)'?1AN&($e(instr,ptr)'="_")&($e(instr,ptr)'="$") g scerr
	n x s x=ptr f ptr=ptr+1:1 q:$e(instr,ptr)'?1AN&($e(instr,ptr)'="_")&($e(instr,ptr)'="$")
	s t=$e(instr,x,ptr-1)
scfin	s tok=dir,dir=t
	q
scerr	b
	q
sceof	s eof=1,t=eofcd
	g scfin
