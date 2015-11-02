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
gdeinit: ;set up local variables and arrays
GDEINIT
	i $ZVER'["VMS" view "BADCHAR"
	s renpref=""
	s log=0,logfile="GDELOG.LOG",BOL=""
	s ZERO=$c(0),ONE=$c(1),TRUE=ONE,FALSE=ZERO,TAB=$c(9)
	s endian("VAX","VMS")=FALSE,glo("VMS")=1024
	s endian("AXP","VMS")=FALSE,endian("AXP","OSF1")=FALSE,glo("VMS")=1024,glo("OSF1")=1024
	s endian("x86","SCO")=FALSE,endian("x86","UWIN")=FALSE,endian("x86","Linux")=FALSE,endian("x86","CYGWIN")=FALSE
	s endian("x86_64","Linux")=FALSE
	s glo("SCO")=384,glo("UWIN")=1024,glo("Linux")=1024,glo("CYGWIN")=1024
	s endian("SEQUOIA_SERIES_400","VAX")=TRUE,glo("VAX")=1024
	s endian("HP-PA","HP-UX")=TRUE,glo("HP-UX")=1024
	s endian("IA64","HP-UX")=TRUE,glo("HP-UX")=1024
	s endian("IA64","Linux")=FALSE,glo("Linux")=1024
	s endian("SPARC","SUN/OS_V4.x")=TRUE,endian("SPARC","Solaris")=TRUE,glo("SUN/OS_V4.x")=800,glo("Solaris")=1024
	s endian("MIPS","A25")=TRUE,glo("A25")=1024
	s endian("B30","NONSTOP-UX")=TRUE,glo("NONSTOP-UX")=1024
	s endian("B32","NONSTOP-UX")=TRUE,glo("NONSTOP-UX")=1024
	s endian("MC-680x0","SYS_V/68_R3V6")=TRUE,endian("MC-680x0","TOPIX")=TRUE,glo("SYS_V/68_R3V6")=1024,glo("TOPIX")=1024
	s endian("RS6000","AIX")=TRUE,glo("AIX")=1024
	s endian("S390","OS390")=TRUE,endian("S390X","Linux")=TRUE,glo("OS390")=1024
	; The following line is for support of AIX V3.2.5 only and can (and should)
	; be removed (along with this comment) as soon as we drop support for
	; AIX V3.2.5.  This change is needed to correspond to the change in
	; release_name.h.  C9801-000344
	s glo("AIX325")=glo("AIX")
	s HEX(0)=1
	s gtm64=$p($zver," ",4)
	i "/IA64/RS6000/SPARC/x86_64/x86/S390/S390X"[("/"_gtm64) s encsupportedplat=TRUE,gtm64=$s("x86"=gtm64:FALSE,1:TRUE)
	e  s (encsupportedplat,gtm64)=FALSE
	i (gtm64=TRUE) f x=1:1:16 s HEX(x)=HEX(x-1)*16 i x#2=0 s TWO(x*4)=HEX(x)
	e  f x=1:1:8 s HEX(x)=HEX(x-1)*16 i x#2=0 s TWO(x*4)=HEX(x)
	f i=25:1:30 s TWO(i)=TWO(i-1)*2
	s TWO(31)=TWO(32)*.5
	s lower="abcdefghijklmnopqrstuvwxyz",upper="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	s endian=endian($p($zver," ",4),$p($zver," ",3))
	s ver=$p($zver," ",3)
	s defglo=glo(ver)
	s comline=$zcmdline
	s nullsubs="\NEVER\FALSE\ALWAYS\TRUE\EXISTING"
	s nommbi=1              ; this is used in gdeverif and should be removed along with the code when support is added
	d UNIX:ver'="VMS"
	d VMS:ver="VMS"
	d syntabi
;
	i (gtm64=FALSE) d
	. s SIZEOF("am_offset")=324
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=44
	. s SIZEOF("gd_map")=36
	. if ver'="VMS" d
	. . s SIZEOF("gd_region")=356
	. . s SIZEOF("gd_region_padding")=0			; not used on VMS
	. . s SIZEOF("gd_segment")=340
	. e  d
	. . s SIZEOF("gd_region")=332
	. . s SIZEOF("gd_segment")=336
	e  d
	. s SIZEOF("am_offset")=332
	. s SIZEOF("file_spec")=256
	. s SIZEOF("gd_header")=16
	. s SIZEOF("gd_contents")=80
	. s SIZEOF("gd_map")=40
	. s SIZEOF("gd_region")=368
	. s SIZEOF("gd_region_padding")=4
	. s SIZEOF("gd_segment")=360
	s SIZEOF("mident")=32
	s SIZEOF("blk_hdr")=16
	i ver'="VMS" d
	. s SIZEOF("rec_hdr")=4	;GTM-6941
	e  d
	. s SIZEOF("rec_hdr")=3
	s SIZEOF("dsk_blk")=512
	i ver'="VMS" d
	. s SIZEOF("max_str")=1048576
	e  d
	. s SIZEOF("max_str")=32767
	s SIZEOF("reg_jnl_deq")=4				; not used on VMS
	s MAXNAMLN=SIZEOF("mident")-1,MAXREGLN=32,MAXSEGLN=32	; maximum name length allowed is 31 characters
	s PARNAMLN=31,PARREGLN=31,PARSEGLN=31
;
; tokens are used for error reporting only
	s tokens("TKIDENT")="identifier"
	s tokens("TKNUMLIT")="number"
	s tokens("TKEOL")="end-of-line"
	s tokens("""")="TKSTRLIT",tokens("TKSTRLIT")="string literal"
	s tokens("@")="TKAMPER",tokens("TKAMPER")="ampersand"
	s tokens("*")="TKASTER",tokens("TKASTER")="asterisk"
	s tokens(":")="TKCOLON",tokens("TKCOLON")="colon"
	s tokens(",")="TKCOMMA",tokens("TKCOMMA")="comma"
	s tokens("$")="TKDOLLAR",tokens("TKDOLLAR")="dollar sign"
	s tokens("=")="TKEQUAL",tokens("TKEQUAL")="equal sign"
	s tokens("<")="TKLANGLE",tokens("TKLANGLE")="left angle bracket"
	s tokens("[")="TKLBRACK",tokens("TKLBRACK")="left bracket"
	s tokens("(")="TKLPAREN",tokens("TKLPAREN")="left parenthesis"
	s tokens("-")="TKDASH",tokens("TKDASH")="dash"
	s tokens("%")="TKPCT",tokens("TKPCT")="percent sign"
	s tokens(".")="TKPERIOD",tokens("TKPERIOD")="period"
	s tokens(")")="TKRPAREN",tokens("TKRPAREN")="right parenthesis"
	s tokens("]")="TKRBRACK",tokens("TKRBRACK")="right bracket"
	s tokens(">")="TKRANGLE",tokens("TKRANGLE")="right angle bracket"
	s tokens(";")="TKSCOLON",tokens("TKSCOLON")="semicolon"
	s tokens("/")="TKSLASH",tokens("TKSLASH")="slash"
	s tokens("_")="TKUSCORE",tokens("TKUSCORE")="underscore"
	s tokens("!")="TKEXCLAM",tokens("TKEXCLAM")="exclamation point"
	s tokens("TKOTHER")="other"
; maximums and mimimums
; region
	s minreg("ALLOCATION")=$s(ver'="VMS":200,1:10)
	s minreg("BEFORE_IMAGE")=0,minreg("COLLATION_DEFAULT")=0,minreg("STDNULLCOLL")=0
	s minreg("EXTENSION")=0
	i ver'="VMS" d
	. s minreg("AUTOSWITCHLIMIT")=16384
	. s minreg("ALIGNSIZE")=4096			; geq RECORD_SIZE
	. s minreg("EPOCH_INTERVAL")=1
	. s minreg("SYNC_IO")=0
	. s minreg("YIELD_LIMIT")=0
	. s minreg("INST_FREEZE_ON_ERROR")=0
	. s minreg("BUFFER_SIZE")=2307
	. s minreg("QDBRUNDOWN")=0
	. s minreg("RECORD_SIZE")=0
	e  d
	. s minreg("RECORD_SIZE")=SIZEOF("rec_hdr")+4
	s minreg("JOURNAL")=0,minreg("KEY_SIZE")=3,minreg("NULL_SUBSCRIPTS")=0
	s maxreg("ALLOCATION")=TWO(24),maxreg("BEFORE_IMAGE")=1
	s maxreg("COLLATION_DEFAULT")=255,maxreg("STDNULLCOLL")=1
	i ver="VMS" do
	. s maxreg("EXTENSION")=HEX(4)-1
	. s maxreg("BUFFER_SIZE")=2000
	e  d
	. s maxreg("EXTENSION")=1073741823
	. s maxreg("AUTOSWITCHLIMIT")=8388607
	. s maxreg("ALIGNSIZE")=4194304
	. s maxreg("EPOCH_INTERVAL")=32767
	. s maxreg("SYNC_IO")=1
	. s maxreg("YIELD_LIMIT")=2048
	. s maxreg("INST_FREEZE_ON_ERROR")=1
	. s maxreg("BUFFER_SIZE")=32768
	. s maxreg("QDBRUNDOWN")=1
	s maxreg("JOURNAL")=1,maxreg("KEY_SIZE")=1019,maxreg("NULL_SUBSCRIPTS")=2
	s maxreg("RECORD_SIZE")=SIZEOF("max_str")
; segments
; bg
	s minseg("BG","ALLOCATION")=10,minseg("BG","BLOCK_SIZE")=SIZEOF("dsk_blk"),minseg("BG","EXTENSION_COUNT")=0
	s minseg("BG","GLOBAL_BUFFER_COUNT")=64,minseg("BG","LOCK_SPACE")=10,minseg("BG","RESERVED_BYTES")=0
	s maxseg("BG","ALLOCATION")=TWO(27),(maxseg("BG","BLOCK_SIZE"),maxseg("BG","RESERVED_BYTES"))=HEX(4)-SIZEOF("dsk_blk")
	i ver'="VMS" s maxseg("BG","ALLOCATION")=TWO(30)-TWO(25) ; supports 992M blocks for UNIX only
	s maxseg("BG","EXTENSION_COUNT")=HEX(4)-1,maxseg("BG","LOCK_SPACE")=65536
	i (gtm64=TRUE) s maxseg("BG","GLOBAL_BUFFER_COUNT")=2147483647 ; 2G-1
	e  s maxseg("BG","GLOBAL_BUFFER_COUNT")=65536
; mm
	s minseg("MM","ALLOCATION")=10,minseg("MM","BLOCK_SIZE")=SIZEOF("dsk_blk"),minseg("MM","DEFER")=0
	s minseg("MM","LOCK_SPACE")=10,minseg("MM","EXTENSION_COUNT")=0,minseg("MM","RESERVED_BYTES")=0
	s maxseg("MM","ALLOCATION")=TWO(27),(maxseg("MM","BLOCK_SIZE"),maxseg("BG","RESERVED_BYTES"))=HEX(4)-SIZEOF("dsk_blk")
	i ver'="VMS" s maxseg("MM","ALLOCATION")=TWO(30)-TWO(25) ; supports 992M blocks for UNIX only
	s maxseg("MM","DEFER")=86400,maxseg("MM","LOCK_SPACE")=1000,maxseg("MM","EXTENSION_COUNT")=HEX(4)-1
	q

;-----------------------------------------------------------------------------------------------------------------------------------

; gde command language syntax table
syntabi:
	s syntab("ADD","NAME")=""
	s syntab("ADD","NAME","REGION")="REQUIRED"
	s syntab("ADD","NAME","REGION","TYPE")="TREGION"
	s syntab("ADD","REGION")=""
	s syntab("ADD","REGION","COLLATION_DEFAULT")="REQUIRED"
	s syntab("ADD","REGION","COLLATION_DEFAULT","TYPE")="TNUMBER"
	s syntab("ADD","REGION","STDNULLCOLL")="NEGATABLE"
	s syntab("ADD","REGION","DYNAMIC_SEGMENT")="REQUIRED"
	s syntab("ADD","REGION","DYNAMIC_SEGMENT","TYPE")="TSEGMENT"
	i ver'="VMS" s syntab("ADD","REGION","INST_FREEZE_ON_ERROR")="NEGATABLE"
	s syntab("ADD","REGION","JOURNAL")="NEGATABLE,REQUIRED,LIST"
	s syntab("ADD","REGION","JOURNAL","ALLOCATION")="REQUIRED"
	s syntab("ADD","REGION","JOURNAL","ALLOCATION","TYPE")="TNUMBER"
	s syntab("ADD","REGION","JOURNAL","AUTOSWITCHLIMIT")="REQUIRED"
	s syntab("ADD","REGION","JOURNAL","AUTOSWITCHLIMIT","TYPE")="TNUMBER"
	s syntab("ADD","REGION","JOURNAL","BUFFER_SIZE")="REQUIRED"
	s syntab("ADD","REGION","JOURNAL","BUFFER_SIZE","TYPE")="TNUMBER"
	s syntab("ADD","REGION","JOURNAL","BEFORE_IMAGE")="NEGATABLE"
	s syntab("ADD","REGION","JOURNAL","EXTENSION")="REQUIRED"
	s syntab("ADD","REGION","JOURNAL","EXTENSION","TYPE")="TNUMBER"
	s syntab("ADD","REGION","JOURNAL","FILE_NAME")="REQUIRED"
	s syntab("ADD","REGION","JOURNAL","FILE_NAME","TYPE")="TFSPEC"
	;s syntab("ADD","REGION","JOURNAL","STOP_ENABLED")="NEGATABLE"
	s syntab("ADD","REGION","KEY_SIZE")="REQUIRED"
	s syntab("ADD","REGION","KEY_SIZE","TYPE")="TNUMBER"
	s syntab("ADD","REGION","NULL_SUBSCRIPTS")="NEGATABLE,REQUIRED"
	s syntab("ADD","REGION","NULL_SUBSCRIPTS","TYPE")="TNULLSUB"
	s syntab("ADD","REGION","NULL_SUBSCRIPTS","TYPE","VALUES")=nullsubs
	i ver'="VMS" s syntab("ADD","REGION","QDBRUNDOWN")="NEGATABLE"
	s syntab("ADD","REGION","RECORD_SIZE")="REQUIRED"
	s syntab("ADD","REGION","RECORD_SIZE","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT")=""
	s syntab("ADD","SEGMENT","ACCESS_METHOD")="REQUIRED"
	s syntab("ADD","SEGMENT","ACCESS_METHOD","TYPE")="TACCMETH"
	s syntab("ADD","SEGMENT","ACCESS_METHOD","TYPE","VALUES")=accmeth
	s syntab("ADD","SEGMENT","ALLOCATION")="REQUIRED"
	s syntab("ADD","SEGMENT","ALLOCATION","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","BLOCK_SIZE")="REQUIRED"
	s syntab("ADD","SEGMENT","BLOCK_SIZE","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","BUCKET_SIZE")="REQUIRED"
	s syntab("ADD","SEGMENT","BUCKET_SIZE","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","DEFER")="NEGATABLE"
 	i ver'="VMS" s syntab("ADD","SEGMENT","ENCRYPTION_FLAG")="NEGATABLE"
	s syntab("ADD","SEGMENT","EXTENSION_COUNT")="REQUIRED"
	s syntab("ADD","SEGMENT","EXTENSION_COUNT","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","FILE_NAME")="REQUIRED"
	s syntab("ADD","SEGMENT","FILE_NAME","TYPE")="TFSPEC"
	s syntab("ADD","SEGMENT","GLOBAL_BUFFER_COUNT")="REQUIRED"
	s syntab("ADD","SEGMENT","GLOBAL_BUFFER_COUNT","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","LOCK_SPACE")="REQUIRED"
	s syntab("ADD","SEGMENT","LOCK_SPACE","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","RESERVED_BYTES")="REQUIRED"
	s syntab("ADD","SEGMENT","RESERVED_BYTES","TYPE")="TNUMBER"
	s syntab("ADD","SEGMENT","WINDOW_SIZE")="REQUIRED"
	s syntab("ADD","SEGMENT","WINDOW_SIZE","TYPE")="TNUMBER"
	s syntab("CHANGE","NAME")=""
	s syntab("CHANGE","NAME","REGION")="REQUIRED"
	s syntab("CHANGE","NAME","REGION","TYPE")="TREGION"
	s syntab("CHANGE","REGION")=""
	s syntab("CHANGE","REGION","COLLATION_DEFAULT")="REQUIRED"
	s syntab("CHANGE","REGION","COLLATION_DEFAULT","TYPE")="TNUMBER"
	s syntab("CHANGE","REGION","STDNULLCOLL")="NEGATABLE"
	s syntab("CHANGE","REGION","DYNAMIC_SEGMENT")="REQUIRED"
	s syntab("CHANGE","REGION","DYNAMIC_SEGMENT","TYPE")="TSEGMENT"
	i ver'="VMS" s syntab("CHANGE","REGION","INST_FREEZE_ON_ERROR")="NEGATABLE"
	s syntab("CHANGE","REGION","JOURNAL")="NEGATABLE,REQUIRED,LIST"
	s syntab("CHANGE","REGION","JOURNAL","ALLOCATION")="REQUIRED"
	s syntab("CHANGE","REGION","JOURNAL","ALLOCATION","TYPE")="TNUMBER"
	s syntab("CHANGE","REGION","JOURNAL","AUTOSWITCHLIMIT")="REQUIRED"
	s syntab("CHANGE","REGION","JOURNAL","AUTOSWITCHLIMIT","TYPE")="TNUMBER"
	s syntab("CHANGE","REGION","JOURNAL","BUFFER_SIZE")="REQUIRED"
	s syntab("CHANGE","REGION","JOURNAL","BUFFER_SIZE","TYPE")="TNUMBER"
	s syntab("CHANGE","REGION","JOURNAL","BEFORE_IMAGE")="NEGATABLE"
	s syntab("CHANGE","REGION","JOURNAL","EXTENSION")="REQUIRED"
	s syntab("CHANGE","REGION","JOURNAL","EXTENSION","TYPE")="TNUMBER"
	s syntab("CHANGE","REGION","JOURNAL","FILE_NAME")="REQUIRED"
	s syntab("CHANGE","REGION","JOURNAL","FILE_NAME","TYPE")="TFSPEC"
	;s syntab("CHANGE","REGION","JOURNAL","STOP_ENABLED")="NEGATABLE"
	s syntab("CHANGE","REGION","KEY_SIZE")="REQUIRED"
	s syntab("CHANGE","REGION","KEY_SIZE","TYPE")="TNUMBER"
	s syntab("CHANGE","REGION","NULL_SUBSCRIPTS")="NEGATABLE,REQUIRED"
	s syntab("CHANGE","REGION","NULL_SUBSCRIPTS","TYPE")="TNULLSUB"
	s syntab("CHANGE","REGION","NULL_SUBSCRIPTS","TYPE","VALUES")=nullsubs
	i ver'="VMS" s syntab("CHANGE","REGION","QDBRUNDOWN")="NEGATABLE"
	s syntab("CHANGE","REGION","RECORD_SIZE")="REQUIRED"
	s syntab("CHANGE","REGION","RECORD_SIZE","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT")=""
	s syntab("CHANGE","SEGMENT","ACCESS_METHOD")="REQUIRED"
	s syntab("CHANGE","SEGMENT","ACCESS_METHOD","TYPE")="TACCMETH"
	s syntab("CHANGE","SEGMENT","ACCESS_METHOD","TYPE","VALUES")=accmeth
	s syntab("CHANGE","SEGMENT","ALLOCATION")="REQUIRED"
	s syntab("CHANGE","SEGMENT","ALLOCATION","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","BLOCK_SIZE")="REQUIRED"
	s syntab("CHANGE","SEGMENT","BLOCK_SIZE","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","BUCKET_SIZE")="REQUIRED"
	s syntab("CHANGE","SEGMENT","BUCKET_SIZE","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","DEFER")="NEGATABLE"
	i ver'="VMS" s syntab("CHANGE","SEGMENT","ENCRYPTION_FLAG")="NEGATABLE"
	s syntab("CHANGE","SEGMENT","EXTENSION_COUNT")="REQUIRED"
	s syntab("CHANGE","SEGMENT","EXTENSION_COUNT","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","FILE_NAME")="REQUIRED"
	s syntab("CHANGE","SEGMENT","FILE_NAME","TYPE")="TFSPEC"
	s syntab("CHANGE","SEGMENT","GLOBAL_BUFFER_COUNT")="REQUIRED"
	s syntab("CHANGE","SEGMENT","GLOBAL_BUFFER_COUNT","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","LOCK_SPACE")="REQUIRED"
	s syntab("CHANGE","SEGMENT","LOCK_SPACE","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","RESERVED_BYTES")="REQUIRED"
	s syntab("CHANGE","SEGMENT","RESERVED_BYTES","TYPE")="TNUMBER"
	s syntab("CHANGE","SEGMENT","WINDOW_SIZE")="REQUIRED"
	s syntab("CHANGE","SEGMENT","WINDOW_SIZE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION")=""
	s syntab("TEMPLATE","REGION","COLLATION_DEFAULT")="REQUIRED"
	s syntab("TEMPLATE","REGION","COLLATION_DEFAULT","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION","STDNULLCOLL")="NEGATABLE"
	s syntab("TEMPLATE","REGION","DYNAMIC_SEGMENT")="REQUIRED"
	s syntab("TEMPLATE","REGION","DYNAMIC_SEGMENT","TYPE")="TSEGMENT"
	i ver'="VMS" s syntab("TEMPLATE","REGION","INST_FREEZE_ON_ERROR")="NEGATABLE"
	s syntab("TEMPLATE","REGION","JOURNAL")="NEGATABLE,REQUIRED,LIST"
	s syntab("TEMPLATE","REGION","JOURNAL","ALLOCATION")="REQUIRED"
	s syntab("TEMPLATE","REGION","JOURNAL","ALLOCATION","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION","JOURNAL","AUTOSWITCHLIMIT")="REQUIRED"
	s syntab("TEMPLATE","REGION","JOURNAL","AUTOSWITCHLIMIT","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION","JOURNAL","BUFFER_SIZE")="REQUIRED"
	s syntab("TEMPLATE","REGION","JOURNAL","BUFFER_SIZE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION","JOURNAL","BEFORE_IMAGE")="NEGATABLE"
	s syntab("TEMPLATE","REGION","JOURNAL","EXTENSION")="REQUIRED"
	s syntab("TEMPLATE","REGION","JOURNAL","EXTENSION","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION","JOURNAL","FILE_NAME")="REQUIRED"
	s syntab("TEMPLATE","REGION","JOURNAL","FILE_NAME","TYPE")="TFSPEC"
	;s syntab("TEMPLATE","REGION","JOURNAL","STOP_ENABLED")="NEGATABLE"
	s syntab("TEMPLATE","REGION","KEY_SIZE")="REQUIRED"
	s syntab("TEMPLATE","REGION","KEY_SIZE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","REGION","NULL_SUBSCRIPTS")="NEGATABLE,REQUIRED"
	s syntab("TEMPLATE","REGION","NULL_SUBSCRIPTS","TYPE")="TNULLSUB"
	s syntab("TEMPLATE","REGION","NULL_SUBSCRIPTS","TYPE","VALUES")=nullsubs
	i ver'="VMS" s syntab("TEMPLATE","REGION","QDBRUNDOWN")="NEGATABLE"
	s syntab("TEMPLATE","REGION","RECORD_SIZE")="REQUIRED"
	s syntab("TEMPLATE","REGION","RECORD_SIZE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT")=""
	s syntab("TEMPLATE","SEGMENT","ACCESS_METHOD")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","ACCESS_METHOD","TYPE")="TACCMETH"
	s syntab("TEMPLATE","SEGMENT","ACCESS_METHOD","TYPE","VALUES")=accmeth
	s syntab("TEMPLATE","SEGMENT","ALLOCATION")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","ALLOCATION","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","BLOCK_SIZE")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","BLOCK_SIZE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","BUCKET_SIZE")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","BUCKET_SIZE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","DEFER")="NEGATABLE"
	i ver'="VMS" s syntab("TEMPLATE","SEGMENT","ENCRYPTION_FLAG")="NEGATABLE"
	s syntab("TEMPLATE","SEGMENT","EXTENSION_COUNT")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","EXTENSION_COUNT","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","FILE_NAME")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","FILE_NAME","TYPE")="TFSPEC"
	s syntab("TEMPLATE","SEGMENT","GLOBAL_BUFFER_COUNT")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","GLOBAL_BUFFER_COUNT","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","LOCK_SPACE")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","LOCK_SPACE","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","RESERVED_BYTES")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","RESERVED_BYTES","TYPE")="TNUMBER"
	s syntab("TEMPLATE","SEGMENT","WINDOW_SIZE")="REQUIRED"
	s syntab("TEMPLATE","SEGMENT","WINDOW_SIZE","TYPE")="TNUMBER"
	s syntab("DELETE","NAME")=""
	s syntab("DELETE","REGION")=""
	s syntab("DELETE","SEGMENT")=""
	s syntab("EXIT")=""
	s syntab("HELP")=""
	s syntab("LOCKS","REGION")="REQUIRED"
	s syntab("LOCKS","REGION","TYPE")="TREGION"
	s syntab("LOG","OFF")=""
	s syntab("LOG","ON")="OPTIONAL"
	s syntab("LOG","ON","TYPE")="TFSPEC"
	s syntab("SETGD","FILE")="REQUIRED"
	s syntab("SETGD","FILE","TYPE")="TFSPEC"
	s syntab("SETGD","QUIT")=""
	s syntab("QUIT")=""
	s syntab("RENAME","NAME")=""
	s syntab("RENAME","REGION")=""
	s syntab("RENAME","SEGMENT")=""
	s syntab("SHOW")=""
	s syntab("SHOW","ALL")=""
	s syntab("SHOW","TEMPLATE")=""
	s syntab("SHOW","MAP")=""
	s syntab("SHOW","MAP","REGION")="REQUIRED"
	s syntab("SHOW","MAP","REGION","TYPE")="TREGION"
	s syntab("SHOW","NAME")=""
	s syntab("SHOW","REGION")=""
	s syntab("SHOW","SEGMENT")=""
	s syntab("SHOW","COMMANDS")=""
	s syntab("SHOW","COMMANDS","FILE")="OPTIONAL"
	s syntab("SHOW","COMMANDS","FILE","TYPE")="TFSPEC"
	s syntab("SPAWN")=""
	s syntab("VERIFY","ALL")=""
	s syntab("VERIFY","MAP")=""
	s syntab("VERIFY","NAME")=""
	s syntab("VERIFY","REGION")=""
	s syntab("VERIFY","SEGMENT")=""
	s syntab("VERIFY","TEMPLATE")=""
	q
VMS
	s endian=FALSE
	s hdrlab="GTCGBLDIR009"		; must be concurrently maintained in gbldirnam.h!!!
	s tfile="GTM$GBLDIR"
	s accmeth="\BG\MM\USER"
	s helpfile="GTM$HELP:GDE.HLB"
	s defdb="MUMPS"
	s defgld="MUMPS.GLD",defgldext=".GLD"
	s defreg="$DEFAULT"
	s defseg="$DEFAULT"
	s dbfilpar=".1AN.1""-"".1""_"".1"":"".1""$"".1""["".1""]"".1""<"".1"">"".1""."".1"";"""
	s filexfm="$tr(filespec,lower,upper)"
	s sep="TKSLASH"
	q

UNIX:
	s hdrlab="GTCGBDUNX008"         ; must be concurrently maintained in gbldirnam.h!!!
	i (gtm64=TRUE) s hdrlab="GTCGBDUNX108" ; the high order digit is a 64-bit flag
	s tfile="$gtmgbldir"
	s accmeth="\BG\MM"
	s helpfile="$gtm_dist/gdehelp.gld"
	s defdb="mumps.dat"
	s defgld="mumps.gld",defgldext="*.gld"
	s defreg="DEFAULT"
	s defseg="DEFAULT"
	s dbfilpar="1E"
	s filexfm="filespec"
	s sep="TKDASH"
	q
