;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2010-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
; 								;
; 	This source code contains the intellectual property	;
; 	of its copyright holder(s), and is made available	;
; 	under a license.  If you do not know the terms of	;
; 	the license, please stop and do not read further.	;
;	    	     	    	     	    	 		;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Part of gengtmdeftypes.
;
; Routine to process gtmtypeslist.txt file and generate GTMDefinedTypesInit.m from it.
;
; Since we want to map only GT.M's structures, an earlier phase has processed the raw
; GT.M include files to extract the typedefs that GT.M itself does. We read those in
; so we know what structures to generate maps for. This second phase generates expansions
; for ALL the structures. We separate the ones we care about from the ones we don't
; by comparing the names to our list from phase 1. This secondary selection is so we don't
; create maps for the thousands of system dependent includes that would bloat into near
; unusability.
;
	Set TRUE=1,FALSE=0
	Set debug=FALSE
	Set debugtoken=FALSE
	Set KeepFiles=FALSE
	Set gtmsdver="1.2.0"	; Set version id
	Set $ETrap="Goto ErrorTrap^scantypedefs"
	Set TAB=$ZChar(9)
	Set entrylvl=$ZLevel
	Set gtmver=$ZCmdline
	;
	; Set version flags
	;
	If ("V52000"]gtmver) Set PreV52000=TRUE
	Else  Set PreV52000=FALSE
	If ("V52001"]gtmver) Set PreV52001=TRUE
	Else  Set PreV52001=FALSE
	If ("V53000"]gtmver) Set PreV53000=TRUE
	Else  Set PreV53000=FALSE
	If ("V53001"]gtmver) Set PreV53001=TRUE
	Else  Set PreV53001=FALSE
	If ("V53001A"]gtmver) Set PreV53001A=TRUE
	Else  Set PreV53001A=FALSE
	If ("V53002"]gtmver) Set PreV53002=TRUE
	Else  Set PreV53002=FALSE
	If ("V53003"]gtmver) Set PreV53003=TRUE
	Else  Set PreV53003=FALSE
	If ("V53004"]gtmver) Set PreV53004=TRUE
	Else  Set PreV53004=FALSE
	If ("V54000"]gtmver) Set PreV54000=TRUE
	Else  Set PreV54000=FALSE
	If ("V54001"]gtmver) Set PreV54001=TRUE
	Else  Set PreV54001=FALSE
	If ("V54002"]gtmver) Set PreV54002=TRUE
	Else  Set PreV54002=FALSE
	;
	Set gtmhdw=$ZPiece($ZVersion," ",4)
	Set ExtraCcLdParms="",ExtraCcParms=""
	Set:(PreV53001&("x86_64"=gtmhdw)) (ExtraCcParms,ExtraCcLdParms)="-m32"
	Do:("RS6000"=gtmhdw)
	. Set:(PreV53001) ExtraCcParms=ExtraCcParms_" -q32",ExtraCcLdParms=ExtraCcLdParms_" -b32"
	. Set:('PreV53001) ExtraCcParms=ExtraCcParms_" -q64",ExtraCcLdParms=ExtraCcLdParms_" -b64"
	Do:("SPARC"=gtmhdw)
	. Set:(PreV53001) ExtraCcParms=ExtraCcParms_" -m32",ExtraCcLdParms=ExtraCcLdParms_" -m32"
	. Set:('PreV53001) ExtraCcParms=ExtraCcParms_" -m64",ExtraCcLdParms=ExtraCcLdParms_" -m64"
	Do:($ZVersion["Linux")
	. Set ExtraCcParms=ExtraCcParms_$$iquote()
	;
	; Define basic token types we will use in our parse
	;
	Set TKEOF=1		; End of file
	Set TKLBRACE=2		; Left brace "{"
	Set TKRBRACE=3		; Right brace "}"
	Set TKLBRACKET=4	; Left bracket "["
	Set TKRBRACKET=5	; Right bracket "]"
	Set TKCOLON=6		; Colon ":"
	Set TKSEMI=7		; Semi-colon ";"
	Set TKLPAREN=8		; Left paren "("
	Set TKRPAREN=9		; Right paren ")"
	Set TKASTERISK=10	; Asterisk "*" (multiply)
	Set TKPLUS=11		; Plus sign "+"
	Set TKMINUS=12		; Minus sign "-"
	Set TKSLASH=13		; Slash "/" (divide)
	Set TKCOMMA=14		; Comma ","
	Set TKDOT=15		; Period "."
	Set TKTYPEDEF=16	; Keyword "typedef"
	Set TKUNION=17		; Keyword "union"
	Set TKSTRUCT=18		; Keyword "struct"
	Set TKENUM=19		; Keyword "enum"
	Set TKSIZEOF=20		; Keyword "sizeof"
	Set TKVOLATILE=21	; Keyword "volatile"
	Set TKCONST=22		; Keyword "const"
	Set TKBASETYPE=23	; basic type (int, char, etc)
	Set TKGTMTYPE=24	; Defined GTM type (in types())
	Set TKINTEGER=25	; An integer
	Set TKOTHERTYPE=26	; Other type we don't care about
	;
	; "reverse lookups" for the token parser
	;
	Set CharScn("{")=TKLBRACE
	Set CharScn("}")=TKRBRACE
	Set CharScn("[")=TKLBRACKET
	Set CharScn("]")=TKRBRACKET
	Set CharScn(":")=TKCOLON
	Set CharScn(";")=TKSEMI
	Set CharScn("(")=TKLPAREN
	Set CharScn(")")=TKRPAREN
	Set CharScn("*")=TKASTERISK
	Set CharScn("+")=TKPLUS
	Set CharScn("-")=TKMINUS
	Set CharScn("/")=TKSLASH
	Set CharScn(",")=TKCOMMA
	Set CharScn(".")=TKDOT
	;
	Set Keywd("typedef")=TKTYPEDEF
	Set Keywd("union")=TKUNION
	Set Keywd("struct")=TKSTRUCT
	Set Keywd("enum")=TKENUM
	Set Keywd("sizeof")=TKSIZEOF
	Set Keywd("volatile")=TKVOLATILE
	Set Keywd("const")=TKCONST
	;
	; Define the token types we can see as a typedef target type
	;
	Set TypdfTypes(TKOTHERTYPE)=""
	Set TypdfTypes(TKGTMTYPE)=""
	Set TypdfTypes(TKBASETYPE)=""	; e.g. "addr" is a valid var name if a bit overloaded
	;
	; Type transformations. Others added dynamically
	;
	Set TransType("*fnptr")="addr"
	;
	; Define basic types we know about
	;
	Do DefBasicType("addr")
	Do DefBasicType("boolean_t")
	Do DefBasicType("char")
	Do DefBasicType("double")
	Do DefBasicType("float")
	Do DefBasicType("int")
	Do DefBasicType("int2")
	Do DefBasicType("int64_t")
	Do DefBasicType("intptr_t")
	Do DefBasicType("long")
	Do DefBasicType("short")
	Do DefBasicType("size_t")
	Do DefBasicType("ssize_t")
	Do DefBasicType("uint2")
	Do DefBasicType("uint64_t")
	Do DefBasicType("uintptr_t")
	Do DefBasicType("unsigned")
	Do DefBasicType("unsigned-char")
	Do DefBasicType("unsigned-int")
	Do DefBasicType("unsigned-long")
	Do DefBasicType("unsigned-short")
	Do DefBasicType("void")
	;
	; Create parse check aid (easy check for "struct" or "union")
	;
	Set sutype("struct")=1
	Set sutype("union")=1
	;
	Do:("SPARC"=gtmhdw)
	. Set types("boolean_t","type")="int"
	; todo - sparc and AIX have boolean_t as a basic type
	;
	; Define the token types we can experience while ignoring a function pointer's parameter.
	;
	Merge ParmTypes=TypdfTypes	; The GTMTYPEs
	Set ParmTypes(TKBASETYPE)=""
	Set ParmTypes(TKCOMMA)=""
	Set ParmTypes(TKASTERISK)=""
	Set ParmTypes(TKCONST)=""
	;
	; Determine if we are doing a pro build or a dbg build. Note that $gtm_dist is set to a verison
	; V5.2-000 or later (use of $Z-nonICU function).
	;
	Set gtmexe=$ZTrnlnm("gtm_exe")
	Set gtmver=$ZPiece(gtmexe,"/",4)
	Set gtmtyp=$ZPiece(gtmexe,"/",5)
	Do:(("pro"'=gtmtyp)&("dbg"'=gtmtyp)) Error("INVEXE","F","Invalid $gtm_exe value - verison not pro or dbg")
	;
	; Read in the type exclude list (types we ignore or otherwise don't need)
	;
	Set infile="gtmexcludetypelist.txt"
	Open infile:Readonly
	Use infile
	For i=1:1 Quit:$ZEof  Do
	. Read line
	. Quit:$ZEof
	. Quit:("#"=$ZExtract(line,1,1))	; Ignore lines starting with "#"
	. Set excludestructs(line)=1
	Close infile
	;
	; Version/platform dependent excludes
	;
	Do:(("x86"=gtmhdw)!("HP-PA"=gtmhdw)!(PreV53001&(("x86_64"=gtmhdw)!("RS6000"=gtmhdw)))!(PreV53002&("SPARC"=gtmhdw)))
	. Set excludestructs("int8")=1		; Not defined on 32 bit platforms
	. Set excludestructs("uint8")=1
	. Set excludestructs("gtm_int8")=1
	. Set excludestructs("gtm_uint8")=1
	Do:(("HP-PA"=gtmhdw)!("AXP"=gtmhdw))	; Eliminate encryption structures
	. Set excludestructs("gtmcrypt_init_t")=1
	. Set excludestructs("gtmcrypt_close_t")=1
	. Set excludestructs("gtmcrypt_hash_gen_t")=1
	. Set excludestructs("gtmcrypt_encode_t")=1
	. Set excludestructs("gtmcrypt_decode_t")=1
	. Set excludestructs("gtmcrypt_getkey_by_name_t")=1
	. Set excludestructs("gtmcrypt_getkey_by_hash_t")=1
	. Set excludestructs("gtmcrypt_strerror_t")=1
	. Set excludestructs("gtmcrypt_key_t")=1
	. Set excludestructs("muext_hash_hdr")=1
	. Set excludestructs("muext_hash_hdr_ptr_t")=1
	. Set excludestructs("db_key_map")=1
	Do:("HP-PA"=gtmhdw)	; Eliminate trigger structures
	. Set excludestructs("gvtr_subs_star_t")=1
	. Set excludestructs("gvtr_subs_point_t")=1
	. Set excludestructs("gvtr_subs_range_t")=1
	. Set excludestructs("gvtr_subs_pattern_t")=1
	. Set excludestructs("gvtr_subs_t")=1
	. Set excludestructs("gv_trigger_t")=1
	. Set excludestructs("gvtr_invoke_parms_t")=1
	. Set excludestructs("gtm_trigger_parms")=1
	. Set excludestructs("gvtr_piece_t")=1
	Do:("SPARC"=gtmhdw)
	. Set excludestructs("zcall_server")=1
	. Set excludestructs("zcall_entry")=1
	;
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;
	; Read in the structure list excluding those in the exclude list
	;
	; Structure and union names have potentially two names that we need to remember and deal with:
	;
	; 1. An optional "struct|union name" such as "struct gd_region_struct" that has uses in
	;    other structures,
	; 2. The structure/union typedef name at the end of the typedef.
	;
	; All typedef structures have #2 so that is the key used in our types and structs
	; arrays.
	;
	; Arrays that get setup here:
	;
	; Initially setup rawtypes(<gtmtype>) array to record records from the gtmtypelist.txt file. Note that
	; duplicate types can found here because we don't do pre-processor #ifdef resolutions in the initial scans
	; so types with multiple possible configurations can show up more than once. If we find a duplicate type,
	; the type is instead put into the alttypes() array. This gets resolved in a later step where the preprocessor
	; gets involved and we can make a definitive choice of which type to use.
	;
	; rawtypes() array:
	;
	;   rawtypes(<gtmtype>)=<fields 2 and 3 from the gtmtypelist.txt record>
	;
	; alttypes() array:
	;
	;   alttypes(<gtmtype>,0)=<count of duplicate defs for this gtmtype>
	;   alttypes(<gtmtype>,<n>)=<fields 2 and 3 from the gtmtypelist.txt record>
	;
	; Once all gtmtypelist.txt records are read in, the following arrays are created from rawtypes() array
	; records and rawtypes() is removed:
	;
	; types() array:
	;
	;   types(<gtmtype>)=<type>
	;
	;   When <type> is "struct", the following additional entries are created:
	;
	;   types(<gtmtype>),"struct")=<n>			- where <n> is the structs array index
	;   structnames(<struct name of type#1 above>)=<n>	- where <n> is the structs array index
	;   structs(<n>) (see structs array below)
	;
	;   When <type> is "union", the following additional entries are created:
	;
	;   types(<gtmtype>),"union")=<n>			- where <n> is the union array index
	;   unionnames(<struct name of type#1 above>)=<n>	- where <n> is the union array inde
	;   union(<n>) (see union array below)
	;
	Set infile="gtmtypelist.txt"
	Open infile:Readonly
	Use infile
	Set (stridx,typidx,unnidx)=0
	For  Quit:$ZEof  Do
	. Read line
	. Quit:$ZEof
	. Set typename=$ZPiece(line," ",1)
	. Do:(0=$Data(excludestructs(typename)))
	. . If (0<$Data(alttypes(typename))) Do
	. . . ;
	. . . ; typename already exists in alttypes, add new record there too
	. . . ;
	. . . Set atinx=$Increment(alttypes(typename,0))
	. . . Set alttypes(typename,atinx)=$ZPiece(line," ",2,3)
	. . Else  If 0<$Data(rawtypes(typename)) Do
	. . . ;
	. . . ; typename is already in rawtypes, migrate prev record out of rawtypes into alttypes
	. . . ; plus move new record to alttypes.
	. . . ;
	. . . Set atinx=$Increment(alttypes(typename,0))
	. . . Set alttypes(typename,atinx)=rawtypes(typename)
	. . . Kill rawtypes(typename)
	. . . Set atinx=$Increment(alttypes(typename,0))
	. . . Set alttypes(typename,atinx)=$ZPiece(line," ",2,3)
	. . Else  Do
	. . . Set rawtypes(typename)=$ZPiece(line," ",2,3)
	Close infile
	;
	; Now more fully process the rawtypes into types, structs, and unions
	;
	Set typename=""
	For  Set typename=$Order(rawtypes(typename)) Quit:typename=""  Do
	. Set deftype=$ZPiece(rawtypes(typename)," ",1)
	. If ("struct"=deftype)!("union"=deftype) Do
	. . Set types(typename)=deftype
	. . Set types(typename,"topname")=$ZPiece(rawtypes(typename)," ",2)
	. Else  If ("enum"=deftype) Set TransType(typename)="int" Quit  ; Enums become integers later (and thus base types)
	. Else  Do
	. . ;
	. . ; We have a simple(r) typedef. Verify 3rd token is same as first with
	. . ; few exceptions:
	. . ;
	. . ;   1. If 3rd token is NULL, ignore it
	. . ;   2. If 3rd token is "*", ignore it
	. . ;   3. If 2nd token is "volatile", ignore it so 3rd token becomes 2nd token
	. . ;
	. . Set type2=$ZPiece(rawtypes(typename)," ",2)
	. . Set:("volatile"=deftype) deftype=type2,type2=""
	. . Do:((""'=type2)&(typename'=type2)&("*"'=type2))
	. . . Do DoWrite("Unable to handle the following record: "_typename_" "_rawtypes(typename)_" - Aborting")
	. . . Halt
	. . Set types(typename)=deftype
	Kill rawtypes
	;
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;
	; Begin the next phase of processing. Create a C file with every include in it from $gtm_inc plus the appropriate
	; system includes to make it all work. The order of most of these includes is fairly important so we take a two-pronged
	; approach. We have a list of includes we know we want and the order they should be in. Any additional includes we
	; find that are not included in this list gets added to the end. This file is run through the pre-processor, scrubbed with
	; an awk strip-mining script to pull the typedefs we want back out of it then we will process that file further.
	;
	; First, the includes we know we want. Note: If the first char of the include name is "#", then the line is assumed
	; to be a pre-processor statement so is added to the output array without further adornment.
	;
	Do AddInclude("mdef.h",TRUE)
	Do AddInclude("<stddef.h>")
	Do AddInclude("gtm_fcntl.h")
	Do AddInclude("gtm_inet.h")
	Do AddInclude("gtm_iconv.h")
	Do AddInclude("gtm_socket.h")
	Do AddInclude("gtm_unistd.h")
	Do AddInclude("gtm_limits.h")
	Do AddInclude("<signal.h>")
	Do AddInclude("<sys/time.h>")
	Do AddInclude("<sys/un.h>")
	Do AddInclude("cache.h")
	Do:'PreV53004 AddInclude("hashtab_addr.h")
	Do AddInclude("hashtab_int4.h")
	Do AddInclude("hashtab_int8.h")
	Do AddInclude("hashtab_mname.h")
	Do:'PreV53004 AddInclude("hashtab_str.h")
	Do AddInclude("hashtab_objcode.h")
	Do AddInclude("error.h")
	Do AddInclude("rtnhdr.h")
	Do AddInclude("gdsroot.h")
	Do AddInclude("gdskill.h")
	Do AddInclude("ccp.h")
	Do AddInclude("gtm_facility.h")
	Do AddInclude("fileinfo.h")
	Do AddInclude("gdsbt.h")
	Do AddInclude("gdsfhead.h")
	Do AddInclude("filestruct.h")
	Do AddInclude("gdscc.h")
	Do AddInclude("comline.h")
	Do AddInclude("compiler.h")
	Do AddInclude("cmd_qlf.h")
	Do AddInclude("io.h")
	Do AddInclude("iosp.h")
	Do AddInclude("jnl.h")
	Do AddInclude("lv_val.h")
	Do:PreV54002 AddInclude("sbs_blk.h")
	Do:'PreV54002 AddInclude("tree.h")
	Do AddInclude("mdq.h")
	Do AddInclude("mprof.h")
	Do AddInclude("mv_stent.h")
	Do AddInclude("stack_frame.h")
	Do AddInclude("stp_parms.h")
	Do AddInclude("stringpool.h")
	Do AddInclude("buddy_list.h")
	Do AddInclude("tp.h")
	Do AddInclude("tp_frame.h")
	Do AddInclude("mlkdef.h")
	Do AddInclude("zshow.h")
	Do AddInclude("zwrite.h")
	Do AddInclude("zbreak.h")
	Do AddInclude("mmseg.h")
	Do AddInclude("gtmsiginfo.h")
	Do AddInclude("gtmimagename.h")
	Do AddInclude("iotcpdef.h")
	Do AddInclude("gt_timer.h")
	Do AddInclude("iosocketdef.h")
	Do AddInclude("ctrlc_handler_dummy.h")
	Do AddInclude("unw_prof_frame_dummy.h")
	Do AddInclude("op.h")
	Do AddInclude("gtmsecshr.h")
	Do AddInclude("error_trap.h")
	Do AddInclude("patcode.h")
	Do AddInclude("source_file.h")
	Do AddInclude("mupipbckup.h")
	Do AddInclude("dpgbldir.h")
	Do AddInclude("mmemory.h")
	Do AddInclude("have_crit.h")
	Do:'PreV53004 AddInclude("alias.h")
	Do AddInclude("repl_msg.h")
	Do AddInclude("gtmsource.h")
	Do AddInclude("gtmrecv.h")
	Do AddInclude("subscript.h")
	Do AddInclude("lvname_info.h")
	Do AddInclude("gvname_info.h")
	Do AddInclude("op_merge.h")
	Do AddInclude("cli.h")
	Do AddInclude("invocation_mode.h")
	Do AddInclude("fgncal.h")
	Do AddInclude("parse_file.h")
	Do AddInclude("repl_sem.h")
	Do:'PreV53003 AddInclude("gtm_zlib.h")
	Do AddInclude("gdsblk.h")
	Do AddInclude("jnl_typedef.h")
	Do AddInclude("gds_blk_upgrade.h")
	Do AddInclude("cws_insert.h")
	Do AddInclude("#ifdef UTF8_SUPPORTED")
	Do AddInclude("gtm_icu_api.h")
	Do AddInclude("gtm_utf8.h")
	Do AddInclude("#endif")
	Do AddInclude("muextr.h")
	Do AddInclude("#ifdef GTM_CRYPT")
	Do AddInclude("gtmcrypt.h")
	Do AddInclude("#endif")
	Do AddInclude("#ifdef GTM_TRIGGER")
	Do AddInclude("gv_trigger.h")
	Do AddInclude("gtm_trigger.h")
	Do AddInclude("#endif")
	Do AddInclude("gdsblkops.h")
	Do AddInclude("cmidef.h")
	Do AddInclude("cmmdef.h")
	Do AddInclude("gtcmlkdef.h")
	Do AddInclude("v15_gdsroot.h")
	Do AddInclude("v15_gdsbt.h")
	Do AddInclude("v15_gdsfhead.h")
	Do AddInclude("gtm_statvfs.h")
	Do AddInclude("startup.h")
	Do AddInclude("job.h")
	Do AddInclude("zroutines.h")
	Do AddInclude("iottdef.h")
	Do AddInclude("iomtdef.h")
	Do AddInclude("read_db_files_from_gld.h")
	Do AddInclude("nametabtyp.h")
	Do AddInclude("tp_unwind.h")
	Do AddInclude("toktyp.h")
	Do AddInclude("gtm_tls.h")	; This is needed to make sure gtm_repl.h is included *AFTER* gtm_tls.h
	;
	; List of #includes to ignore (because they get pulled in by other includes and/or are plug-in related or just cause errors)
	;
	Do ExclInclude("ccpact_tab.h")
	Do ExclInclude("cdb_sc_table.h")
	Do ExclInclude("emit_code_sp.h")
	Do ExclInclude("errorsp.h")
	Do ExclInclude("gdsdbver_sp.h")
	Do ExclInclude("gtcmtr_protos.h")
	Do ExclInclude("gtm_build_transport_string.h")
	Do ExclInclude("gtm_icu.h")
	Do ExclInclude("gtm_malloc_src.h")
	Do ExclInclude("gtm_rpc.h")
	Do ExclInclude("gtm_term.h")
	Do ExclInclude("gtm_threadgbl.h")
	Do ExclInclude("gtm_threadgbl_defs.h")
	Do ExclInclude("gtm_threadgbl_deftypes.h")
	Do ExclInclude("gtm_threadgbl_init.h")
	Do ExclInclude("gtmcrypt_dbk_ref.h")
	Do ExclInclude("ydbcrypt_interface.h")
	Do ExclInclude("gtmcrypt_pk_ref.h")
	Do ExclInclude("gtmcrypt_ref.h")
	Do ExclInclude("gtmcrypt_sym_ref.h")
	Do ExclInclude("gtmcrypt_util.h")
	Do ExclInclude("gtm_tls_impl.h")
	Do ExclInclude("gtmcrypt_funclist.h")
	Do ExclInclude("gtm_tls_funclist.h")
	Do ExclInclude("gtmimagetable.h")
	Do ExclInclude("gtmxc_types.h")
	Do ExclInclude("gv_trig_cmd_table.h")
	Do ExclInclude("gv_trigger_protos.h")
	Do ExclInclude("gvcst_blk_search.h")
	Do ExclInclude("gvcst_protos.h")
	Do ExclInclude("hashtab_implementation.h")
	Do ExclInclude("i386_mod_16.h")
	Do ExclInclude("i386_mod_32.h")
	Do ExclInclude("i386_ops.h")
	Do ExclInclude("i386_ops_2b.h")
	Do ExclInclude("i386_ops_g1.h")
	Do ExclInclude("i386_ops_g2.h")
	Do ExclInclude("i386_ops_g3.h")
	Do ExclInclude("i386_ops_g4.h")
	Do ExclInclude("i386_ops_g5.h")
	Do ExclInclude("i386_ops_g6.h")
	Do ExclInclude("i386_ops_g7.h")
	Do ExclInclude("i386_ops_g8.h")
	Do ExclInclude("i386_reg16.h")
	Do ExclInclude("i386_reg32.h")
	Do ExclInclude("i386_reg64.h")
	Do ExclInclude("i386_reg8.h")
	Do ExclInclude("i386_ss.h")
	Do ExclInclude("ia64_inst_struct.h")
	Do ExclInclude("incr_link_sp.h")
	Do ExclInclude("indir.h")
	Do ExclInclude("io_dev_dispatch.h")
	Do ExclInclude("iop.h")
	Do ExclInclude("jnl_rec_table.h")
	Do ExclInclude("jnlsp.h")
	Do ExclInclude("jobparams.h")
	Do ExclInclude("jobparamstrs.h")
	Do ExclInclude("jobsp.h")
	Do ExclInclude("lockhist.h")
	Do ExclInclude("make_mode_sp.h")
	Do ExclInclude("main_pragma.h")
	Do ExclInclude("mdefsa.h")
	Do ExclInclude("mdefsp.h")
	Do ExclInclude("merrors_ansi.h")
	Do ExclInclude("mu_dwngrd_header.h")
	Do ExclInclude("muext_rec_table.h")
	Do ExclInclude("mupip.h")
	Do ExclInclude("mupip_io_dev_dispatch.h")
	Do ExclInclude("op_fnextract.h")
	Do ExclInclude("opcode_def.h")
	Do ExclInclude("option.h")
	Do ExclInclude("stp_gcol_src.h")
	Do ExclInclude("tab_bg_trc_rec.h")
	Do ExclInclude("tab_db_csh_acct_rec.h")
	Do ExclInclude("tab_gvstats_rec.h")
	Do ExclInclude("tab_jpl_trc_rec.h")
	Do ExclInclude("tab_probecrit_rec.h")
	Do ExclInclude("tab_ystats_rec.h")
	Do ExclInclude("timersp.h")
	Do ExclInclude("trace_table_types.h")
	Do ExclInclude("trigger_compare_protos.h")
	Do ExclInclude("trigger_delete_protos.h")
	Do ExclInclude("trigger_fill_xecute_buffer.h")
	Do ExclInclude("trigger_parse_protos.h")
	Do ExclInclude("trigger_select_protos.h")
	Do ExclInclude("trigger_subs_def.h")
	Do ExclInclude("trigger_trgfile_protos.h")
	Do ExclInclude("trigger_update_protos.h")
	Do ExclInclude("urxsp.h")
	Do ExclInclude("v010_jnl.h")
	Do ExclInclude("v3_gdsfhead.h")
	Do ExclInclude("v07_cdb_sc.h")
	Do ExclInclude("v07_copy.h")
	Do ExclInclude("v07_fileinfo.h")
	Do ExclInclude("v07_filestruct.h")
	Do ExclInclude("v07_gdsbt.h")
	Do ExclInclude("v07_gdsfhead.h")
	Do ExclInclude("v07_gdsroot.h")
	Do ExclInclude("v07_gtm_facility.h")
	Do ExclInclude("v07_jnl.h")
	Do ExclInclude("v07_jnl_rec_table.h")
	Do ExclInclude("v07_jnlsp.h")
	Do ExclInclude("v07_mdef.h")
	Do ExclInclude("v07_mdefsp.h")
	Do ExclInclude("v12_jnl.h")
	Do ExclInclude("v12_jnl_rec_table.h")
	Do ExclInclude("v12_jnlsp.h")
	Do ExclInclude("v15_filestruct.h")
	Do ExclInclude("v15_tab_bg_trc_rec_fixed.h")
	Do ExclInclude("v15_tab_bg_trc_rec_variable.h")
	Do ExclInclude("v15_tab_db_csh_acct_rec.h")
	Do ExclInclude("viewtab.h")
	Do ExclInclude("xfer.h")
	Do ExclInclude("zbreaksp.h")
	Do ExclInclude("zcall.h")
	Do ExclInclude("zroutinessp.h")
	Do ExclInclude("zsockettab.h")
	;
	; Version dependent excludes (depends on which versions are shbin/non-shbin for a given release)
	;
	Do:((gtmhdw="x86")!(PreV53001&(gtmhdw="x86_64"))!(PreV53002&(gtmhdw="SPARC")))
	. Do ExclInclude("make_mode.h")
	Do:("HP-PA"=gtmhdw)
	. Do ExclInclude("trigger.h")
	;
	; Write our preferred includes to output file
	;
	Set cfile="tmpCFile.c"
	Open cfile:New
	Use cfile
	For i=1:1:inclstmt(0) Do
	. Write inclstmt(i),!
	. Write:("#include ""gtmsecshr.h"""=inclstmt(i)) "#undef CLIENT",!	; definition causes problems on Solaris
	;
	; Make sure all header files added.
	;
	Do CommandToPipe("/bin/ls -1 $gtm_inc/*.h",.results)
	For i=1:1:results(0) Do
	. Set incname=$ZPiece(results(i),"/",6)
	. Quit:(""=incname)					; Sometimes get blank lines at end.
	. If 0=$Data(exclincl(incname)),0=$Data(inclref(incname)) Do
	. . Write:("mu_reorg.h"=incname) "#undef DEFAULT",!	; On HPPA, someone defines DEFAULT as 2 which causes this to fail
	. . Write "#include """_incname_"""",!
	. . Write:("gtm_mtio.h"=incname) "#undef NOFLUSH",!	; Causes problems on AIX since is defined in sys/ioctl.h
	Close cfile
	;
	; Run it through the pre-processor to expand pre-processor statements selecting conditional
	; typedefs, etc. Disable all warnings which may occur in older sources and newer compilers
	;
	set cmdToPipe="$gt_cc_"_gtmtyp_" -I$gtm_inc -w -E "_ExtraCcParms_" "_cfile_" > "_cfile_"exp"
	Do CommandToPipe(cmdToPipe,.results)
	Do:(0'=results(0))
	. Use $P
	. Do DoWrite("Error messages from pre-processor compile: "_cmdToPipe)
	. For i=1:1:results(0) Write results(i),!
	. Do Error("BADPREPCOMP","E","Pre-process compile failed - aborting")
	;
	; Run the preprocessor output through the stripmine scrubber. This performs the following:
	;
	;   1. Extracts all typedefs.
	;   2. Puts spaces around token chars we need to scan (parens, brackets, etc).
	;   3. Eliminates leading white space.
	;   4. Translates tabs to spaces and eliminates multiple spaces allowing easy $ZPiece parsing
	;
	Do CommandToPipe("awk -f stripmine.awk "_cfile_"exp > "_cfile_"exp.smine",.results)
	Do:(0'=results(0))
	. Use $P
	. Do DoWrite("Error messages from awk processing:")
	. For i=1:1:results(0) Write results(i),!
	. Do Error("BADPREPCOMP","E","Awk stripmine operation failed - aborting")
	;
	; Remove tmpCFile.*
	;
	Do:'KeepFiles
	. Open cfile:Write
	. Close cfile:Delete
	. Open cfile_"exp":Write
	. Close cfile_"exp":Delete
	;
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;
	; Read in the expanded stripmined file. Should contain nothing but typedefs in it now to
	; reduce our need for parser sophistication.
	;
	Set infile=cfile_"exp.smine"
	Open infile:Readonly
	Use infile
	Set scstate=0		; Initial scan state (nothing pending)
	Set (inbuf,token,dirtoken,tokenval,dirtokenval)=""
	Set inlines=0,tokcnt=0
	Do GetToken(FALSE)	; Prime scanner pump
	Set tokcnt=0		; Reset counter after pump prime
	Do GetToken(FALSE)
	;
	; High level typedef loop - At start of loop, we only ever expect to see typedef statements.
	;
	For  Quit:(TKEOF=token)  Do
	. ;
	. ; Expect a typedef token here
	. ;
	. Do:(TKTYPEDEF'=token) Error("ASSERTFAIL","F","Expected token: TKTYPEDEF")
	. Kill typdf
	. Do GetToken(FALSE)
	. ;
	. ; Possibilities here are:
	. ;
	. ;   1. struct
	. ;   2. union
	. ;   3. enum (find end of definition and ignore it)
	. ;   4. volatile (ignored)
	. ;   5. const (ignored)
	. ;   6. some known type
	. ;
	. ; First get rid of the ignorable conditions
	. ;
	. Set isstructuniondef=FALSE,createdisuidx=0
	. Do:(TKCONST=token) GetToken(FALSE)
	. Do:(TKVOLATILE=token) GetToken(FALSE)
	. If (TKENUM=token) Do IgnoreTypedef(0) Quit	; Ignore and restart loop for this one
	. ;
	. ; Now we have a type we need to do something with
	. ;
	. If ((TKBASETYPE=token)!(TKOTHERTYPE=token)!(TKGTMTYPE=token)) Do
	. . Set typdf("type")=tokenval
	. . Do GetToken(FALSE)
	. . ;
	. . ; Possible to have an ignorable const or volatile here too
	. . ;
	. . Do:(TKCONST=token) GetToken(FALSE)
	. . Do:(TKVOLATILE=token) GetToken(FALSE)
	. Else  If ((TKSTRUCT=token)!(TKUNION=token)) Do	; Struct and union processing is identical
	. . Set typdf("type")=tokenval
	. . Do GetToken(FALSE)
	. . ;
	. . ; Possibilities here are:
	. . ;
	. . ;   1. struct name
	. . ;   2. left brace to start structure definition
	. . ;
	. . If ((TKOTHERTYPE=token)!(TKGTMTYPE=token)) Do	; name of struct or union
	. . . Set typdf("isuname")=tokenval
	. . . Do GetToken(FALSE)
	. . . ;
	. . . ; Syntactic possibilities here having already parsed "struct" or "union" are:
	. . . ;
	. . . ;   1. "structunionname type ;"
	. . . ;	  2. "structunionname * type ;"
	. . . ;   3. Note either of the above could have a "," and one or more types instead of the semi-colon
	. . . ;   4. "structunionname {"
	. . . ;   5. "{"
	. . . ;
	. . . ; For possibilities 1-3, record the structunionname and fall into end-of-entry processing. For 4-5, record the
	. . . ; structunion name (if #4) and parse the defined struct or union within the braces.
	. . . ;
	. . . Do:(TKLBRACE=token)
	. . . . Set createdisuidx=$$ParseUnionStructDef(.typdf)
	. . . . Do GetToken(FALSE)
	. . . . Set isstructuniondef=TRUE
	. . Else  Do				; We should be defining a new structure here
	. . . Do:(TKLBRACE'=token) Error("ASSERTFAIL","F","Expecting token TKLBRACE")
	. . . Set createdisuidx=$$ParseUnionStructDef(.typdf)
	. . . Do GetToken(FALSE)
	. . . Set isstructuniondef=TRUE
	. Else  Do Error("ASSERTFAIL","F","Unexpected token encounterd: "_tokenval)
	. ;
	. ; At this point we expect the next token to be one or more types that the previous type is to be typedef'd as.
	. ; It is possible for any of these (potentially comma delimited) type(s) to be preceeded by an asterisk in which
	. ; case the given type is morphed into an address type instead. Note if the preceeding type was a struct or union
	. ; definition, there cannot be an address.
	. ;
	. Do:((0=$Data(TypdfTypes(token)))&(TKASTERISK'=token)&(TKLPAREN'=token))
	. . Do Error("ASSERTFAIL","F","Expecting typedef target type")
	. Do:((TKASTERISK=token)&isstructuniondef) Error("ASSERTFAIL","F","Address field for structure def")
	. Do:(TKASTERISK=token)
	. . Set typdf("origtype")=typdf("type"),typdf("type")="addr"
	. . Do GetToken(FALSE)
	. . For  Quit:(TKASTERISK'=token)  Do GetToken(FALSE)	; eat potential "**" or more
	. If (TKLPAREN=token) Do		; Have function pointer definition
	. . Do GetToken(FALSE)
	. . Do:(TKASTERISK'=token) Error("ASSERTFAIL","F","Expecting TKASTERISK in fuction pointer definition")
	. . Do GetToken(FALSE)
	. . Do:(TKVOLATILE=token) GetToken(FALSE)
	. . Do:(0=$Data(TypdfTypes(token))) Error("ASSERTFAIL","F","Expecting function pointer type")
	. . Set typdeftype=tokenval
	. . Do GetToken(FALSE)
	. . Do:(TKRPAREN'=token) Error("ASSERTFAIL","F","Expecting TKRPAREN in fuction pointer definition")
	. . Do GetToken(FALSE)
	. . Do:(TKLPAREN'=token) Error("ASSERTFAIL","F","Expecting TKLPAREN in fuction pointer definition")
	. . Do GetToken(FALSE)
	. . For  Quit:(TKRPAREN=token)  Do	; Have to eat the parms until find a closing paren
	. . . Do:(0=$Data(ParmTypes(token))) Error("ASSERTFAIL","F","Unexpected token in function pointer parse")
	. . . Do GetToken(FALSE)
	. Else  Set typdeftype=tokenval
	. ;
	. ; If this type is known in types, then we need to remember it. Else we don't..
	. ;
	. Set keeptype=TRUE		; Start with this assumption although we may change our mind if we
	. 				; discover this type is not one of ours
	. Do:(0=$Data(types(typdeftype)))
	. . Set keeptype=FALSE		; If not found, change assumption to FALSE unless we find it
	. . Do:(0'=$Data(alttypes(typdeftype,0)))
	. . . Set maxloop=alttypes(typdeftype,0)		; Var could disappear inside loop so cache maxval
	. . . For i=1:1:maxloop Quit:keeptype  Do
	. . . . Do:(typdf("type")=$ZPiece(alttypes(typdeftype,i)," ",1))
	. . . . . Set keeptype=TRUE
	. . . . . Set types(typdeftype)=$ZPiece(alttypes(typdeftype,i)," ",1)
	. . . . . Do:(("struct"=types(typdeftype))!("union"=types(typdeftype)))
	. . . . . . Set types(typdeftype,"topname")=$ZPiece(alttypes(typdeftype,i)," ",2)
	. . . . . ;Do DoWrite("Converted "_typdeftype_" from alttype to type")
	. . . . . ;Do dbgzwrite("types(typdeftype),alttypes(typdeftype,*),typdeftype","**** Converted type")
	. . . . . Kill alttypes(typdeftype)
	. . Quit:(keeptype)
	. . ;
	. . ; We don't recognize this type. Put out a temporary message that we are ignoring it and continue
	. . ;
	. . Do DoWrite("Did not recognize type for this typedef: "_typdeftype_" - ignoring type and all defs of that type")
	. . Set keeptype=FALSE;
	. . For  Quit:(TKSEMI=token)  Do       	   	; Get rid of everything up to the semi-colon
	. . . For  Quit:(TKASTERISK'=token)  Do GetToken(FALSE)
	. . . If (TKCOLON=token) Do  Quit	; Do under before quitting
	. . . . Do GetToken(FALSE)
	. . . . Do:(TKINTEGER'=token) Error("ASSERTFAIL","F","Non-numeric bit length")
	. . . . Do GetToken(FALSE)
	. . . . Set typdf("nofield")=TRUE
	. . . Do:((TKOTHERTYPE'=token)&(TKRPAREN'=token)) Error("ASSERTFAIL","F","Unexpected token while ignoring types")
	. . . Do GetToken(FALSE)
	. . . Do:(TKCOMMA=token) GetToken(FALSE)
	. Do:(keeptype)
	. . ;
	. . ; Ok, we want to keep this type.
	. . ;
	. . Set typdf("newtype")=typdeftype
	. . Merge types(typdeftype)=typdf		; Add to sum of knowledge for this type
	. . Do GetToken(FALSE)
	. . For  Quit:(TKCOMMA'=token)  Do		; Could be a list of types equated to this definition
	. . . Do GetToken(FALSE)
	. . . Do:(0=$Data(TypdfTypes(token))) Error("ASSERTFAIL","F","Expecting typedef target type")
	. . . ;
	. . . ; We have a comma delimited type that needs a copy of the typdf entry just created.
	. . . ;
	. . . If (TKASTERISK=token) Do
	. . . . Do:(0=$Data(typdf("origtype")))		; Avoid if prev type already an address
	. . . . . Set typdf("origtype")=typdf("type")
	. . . . . Set typdf("type")="addr"
	. . . . For  Quit:(TKASTERISK'=token)  Do GetToken(FALSE)
	. . . . Do:(0=$Data(TypdfTypes(token))) Error("ASSERTFAIL","F","Expecting typedef target type")
	. . . Else  If (0<$Data(typdf("origtype"))) Do	; Previous type was *'d, this one isn't = unconvert
	. . . . Set typdf("type")=typdf("origtype")
	. . . . Kill typdf("origtype")
	. . . Set typdf("newtype")=tokenval		; Set typedef target type as comma delimited type
	. . . Do GetToken(FALSE)
	. . . Merge types(typdf("newtype"))=typdf	; Add to sum of knowledge for this type
	. Do:(TKSEMI'=token) Error("ASSERTFAIL","F","Expecting end of statement TKSEMI token")
	. Do GetToken(TRUE)
	Do:'KeepFiles
	. Close infile
	. Open infile:Write
	. Close infile:Delete
	Do DoWrite($ZDate($Horolog,"24:60:SS")_" Finished typedef parse - lines: "_inlines_"  tokens: "_tokcnt)
	;
	; Pre-process the types looking for:
	;
	;   1. Types that can be "promoted" to other simple types.
	;   2. If new type is a basic type, set basic flag appropriately.
	;   3. For simple struct references (e.g. type is "mstr", pull in the reference if it is known so it
	;      all fields get expanded appropriately when the structure is sniffed for offset/length info.
	;
	; First up, promote all the simple types first so they can be properly resolved in structures when
	; we do them next.
	;
	Set type=""
	For  Set type=$Order(types(type)) Quit:""=type  Do
	. Set idxcnt=$Get(types(type,"idxcnt"),0)
	. Do:((0=idxcnt)&(0=$Data(types(type,"topname"))))
	. . ; Process type define with no structures or unions (simple typedef)
	. . set errordetail="Type "_type_" is not excluded but neither is its definition found - if is for a plug-in,"
	. . set errordetail=errordetail_" add to excluded types in gtmexcludetypelist.txt"
	. . set errordetail=errordetail_" or if system dependent exclude - add to excludestructs() in this routine"
	. . set errordetail=errordetail_" or add a call to ExclInclude() also in this routine"
	. . Do:(0=($Data(types(type,"type"))#2)) Error("TYPENOTFND","E",errordetail)
	. . Set types(type,"type")=$$ResolveType(types(type,"type"))
	;
	; Now promote all the simple types in the isuidx entries
	;
	Set type=""
	For  Set type=$Order(types(type)) Quit:""=type  Do
	. For isuidx=1:1:$Get(types(type,"idxcnt"),0) Do
	. . For fldidx=1:1:types(type,"isuidx",isuidx,0) Do
	. . . Set types(type,"isuidx",isuidx,fldidx,"type")=$$ResolveType(types(type,"isuidx",isuidx,fldidx,"type"))
	;
	; Delete the simple types, they have no more useful information
	;
	Set type=""
	For  Set type=$Order(types(type)) Quit:""=type  Do
	. Kill:(0=$Data(types(type,"isuidx",1,0))) types(type)
	;
	; Go through the remaining types. For isuidx types that are not simple, see if a type exists exists that we can use
	; to redefine this type. We should already be resolved down to the lowest level simple type possible and all simple
	; types have now been removed so if we find one, it is a struct or union. If found, substitute its definition in for
	; the current one expanding the isuidx entires as necessary.
	;
	Set type=""
	For  Set type=$Order(types(type)) Quit:""=type  Do
	. For isuidx=1:1:types(type,"idxcnt") Do
	. . For fldidx=1:1:types(type,"isuidx",isuidx,0) Do
	. . . Set basetypeData=$Data(basetype(types(type,"isuidx",isuidx,fldidx,"type")))
	. . . Set typesData=$Data(types(types(type,"isuidx",isuidx,fldidx,"type")))
	. . . Do:((0=basetypeData)&(0<typesData))
	. . . . ;
	. . . . ; Here if not a base type and type has a structure definition
	. . . . ;
	. . . . Set origidxcnt=types(type,"idxcnt")	; Use to recompute suptr in copied structs
	. . . . Set subtyp=types(type,"isuidx",isuidx,fldidx,"type")
	. . . . For isuidx2=1:1:$Get(types(subtyp,"idxcnt"),0) Do			; If type is defined, add it to this entry
	. . . . . Set:(0=$Data(types(type,"origidxcnt"))) types(type,"origidxcnt")=types(type,"idxcnt")
	. . . . . Set newisuidx=$Increment(types(type,"idxcnt"))
	. . . . . Merge types(type,"isuidx",newisuidx)=types(subtyp,"isuidx",isuidx2)	; Copy one set of entries over
	. . . . . For fldidx2=1:1:types(type,"isuidx",newisuidx,0) Do			; Check if any suptr entries need updating
	. . . . . . Do:(0<$Data(types(type,"isuidx",newisuidx,fldidx2,"suptr")))
	. . . . . . . Set types(type,"isuidx",newisuidx,fldidx2,"suptr")=types(type,"isuidx",newisuidx,fldidx2,"suptr")+origidxcnt
	. . . . Set types(type,"isuidx",isuidx,fldidx,"suptr")=origidxcnt+1		; Point to copied structure
	;
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;
	; Start of next phase - write another C file with output statements to yield the types we want to parse.
	;
	Set cfile="tmpCFile2.c"
	Open cfile:New
	Use cfile
	Set lincnt=0
	For i=1:1:inclstmt(0) Do
	. Write inclstmt(i),!
	. Set lincnt=lincnt+1
	. If ("#include ""gtmsecshr.h"""=inclstmt(i)) Do
	. . Write "#undef CLIENT",!	; definition causes problems on Solaris
	. . Set lincnt=lincnt+1
	;
	; Make sure all header files added.
	;
	Do CommandToPipe("/bin/ls -1 $gtm_inc/*.h",.results)
	For i=1:1:results(0) Do
	. Set incname=$ZPiece(results(i),"/",6)
	. If 0=$Data(exclincl(incname)),0=$Data(inclref(incname)) Do
	. . If ("mu_reorg.h"=incname) Do
	. . . Write "#undef DEFAULT",!				; On HPPA, someone defines DEFAULT as 2 which causes this to fail
	. . . Set lincnt=lincnt+1
	. . Write "#include """_incname_"""",!
	. . Set lincnt=lincnt+1
	. . If ("gtm_mtio.h"=incname) Do
	. . . Write "#undef NOFLUSH",!				; Causes problems on AIX since is defined in sys/ioctl.h
	. . . Set lincnt=lincnt+1
	Kill results,exclincl,inclref,excludestructs,inclstmt	; No longer needed and we are about to use a metric gob or storage
	;
	; Define the macros we want
	;
	Write !
	Write "#ifndef SIZEOF",!
	Write "#  define SIZEOF(x) sizeof(x)",!
	Write "#endif",!
	Write "#define PRINT_OFFSET(FieldDesignation, Struct, Member, Dimension)"
	Write " printf(""field ""FieldDesignation""%d|%d|%d\n"", (int)offsetof(Struct, Member),"
	Write " (int)sizeof(temp_##Struct.Member), (Dimension))",!
	Write "#define PRINT_FLEXBASE(FieldDesignation, Struct, Member, Dimension)"
	Write " printf(""field ""FieldDesignation""%d|%d|%d\n"", (int)offsetof(Struct, Member), (int)0, (int)0)",!
	Write "#define PRINT_SUSIZE(Type) printf(""susize ""#Type""|%d\n"", (int)SIZEOF(Type))",!
	Write !
	Write "int main(void)",!
	Write "{",!
	Set lincnt=lincnt+9
	;
	; Go through types first to generate the "temp" instantiations for the structures/unions
	;
	Set type=""
	For  Set type=$Order(types(type)) Quit:(type="")  Do
	. Quit:(0=$Data(types(type,"isuidx",1,0)))		; Bypass is not a structure
	. Write TAB,types(type,"newtype")," temp_",types(type,"newtype"),";",!
	. Set lincnt=lincnt+1
	Write !
	Set lincnt=lincnt+1
	Set type=""
	For  Set type=$Order(types(type)) Quit:(type="")  Do
	. Quit:(0=$Data(types(type,"isuidx",1,0)))		; Bypass is not a structure
	. Write TAB,"PRINT_SUSIZE(",types(type,"newtype"),");",!
	. Set lincnt=lincnt+1
	. For fldidx=1:1:types(type,"isuidx",1,0) Do	; Loop through the lvl 1 fields
	. . Quit:(0<$Data(types(type,"isuidx",1,fldidx,"nofield")))    ; Ignore bit and void fields
	. . Set fieldname=types(type,"isuidx",1,fldidx,"newtype")
	. . If (0<$Data(types(type,"isuidx",1,fldidx,"suptr"))) Do
	. . . ;
	. . . ; Have a pointer to a secondary structure. Output the high(er) level structure first, then its fields
	. . . ;
	. . . Do ExtractFieldInfo(type,1,fldidx,types(type,"newtype"),fieldname)
	. . . If (0<$Data(types(type,"isuidx",1,fldidx,"dim"))) Do
	. . . . Do ProcessNestedStructUnion(type,types(type,"isuidx",1,fldidx,"suptr"),fldidx,types(type,"newtype"),fieldname_"[0]")
	. . . Else  Do
	. . . . Do ProcessNestedStructUnion(type,types(type,"isuidx",1,fldidx,"suptr"),fldidx,types(type,"newtype"),fieldname)
	. . Else  Do ExtractFieldInfo(type,1,fldidx,types(type,"newtype"),fieldname)
	Write TAB,"return 0;",!
	Write "}",!
	Set lincnt=lincnt+2
	Close cfile
	Do DoWrite($ZDate($Horolog,"24:60:SS")_" Finished writing C offset/size routine ("_lincnt_" lines written)")
	;
	; Need to build and execute this routine now.
	;
	Set outfile=$ZPiece(cfile,".",1)
	Set ccCmd="$gt_cc_"_gtmtyp_$Select(("OS390"'=$ZPiece($ZVersion," ",3)):" -O0",1:" -qNOOPT")_" -I$gtm_inc -o "_outfile_".o "
	Do CommandToPipe(ccCmd_ExtraCcLdParms_" "_ExtraCcParms_" "_outfile_".c",.compresults)
	Do:((0'=compresults(0))!(FALSE=$$StatFile(outfile_".o")))
	. Do dbgzwrite("compresults",$get(lastpipecmd))
	. Do dbgzwrite("compresults","Results from compile of "_outfile)
	. Do Error("COMPFAIL","F","Compile of offsets routine failed")
	Set ldCmd="$gt_ld_linker -o "_outfile_" $gt_ld_options_pro -L$gtm_obj $gt_ld_sysrtns ${gt_ld_syslibs:s/-lncurses//} "
	Do CommandToPipe(ldCmd_ExtraCcLdParms_" "_outfile_".o",.linkresults)
	Do:(FALSE=$$StatFile(outfile))
	. Set file=outfile_".link_output.txt"
	. Open file:New
	. Use file
	. For i=1:1:linkresults(0) Write linkresults(i),!
	. Close file
	. Use $P
	. Do Error("LINKFAIL","F","Link of "_outfile_" failed - see "_outfile_".link_output.txt")
	Do DoWrite($ZDate($Horolog,"24:60:SS")_" Finished compiling/linking C offset/size routine")
	;
	; Execute the program we just built under a pipe and process the output
	;
	Kill linkresults	; Not needed
	Set pipe="offpipe",lincnt=0
	Open pipe:(Shell="/bin/sh":Command="./"_outfile)::"PIPE"
	Use pipe
	Set pipeopen=TRUE
	For  Quit:$ZEof  Do
	. Read line
	. Quit:$ZEof
	. Set lincnt=lincnt+1
	. Set type=$ZPiece(line," ",1)
	. Set input=$ZPiece(line," ",2,999)
	. If "field"=type Do			; Get info on structure fields
	. . Set type=$ZPiece(input,"|",1)
	. . Set isuidx=$ZPiece(input,"|",2)
	. . Set fldidx=$ZPiece(input,"|",3)
	. . Set fldotyp=$ZPiece(input,"|",4)
	. . Set fldname=$ZPiece(input,"|",5)
	. . Set fldoff=$ZPiece(input,"|",6)
	. . Set fldlen=$ZPiece(input,"|",7)
	. . Set flddim=$ZPiece(input,"|",8)
	. . Do:(fldname'[types(type,"isuidx",isuidx,fldidx,"newtype")) Error("MISMATCH","F","Field names do not match")
	. . Set fullidx=$Increment(types(type,"fullexp",0))
	. . Set types(type,"fullexp",fullidx,"fldname")=fldname
	. . Set:(1<flddim) types(type,"fullexp",fullidx,"flddim")=flddim
	. . Set types(type,"fullexp",fullidx,"fldoff")=fldoff
	. . Set types(type,"fullexp",fullidx,"fldlen")=fldlen
	. . Set types(type,"fullexp",fullidx,"type")=fldotyp
	. . Set:debug types(type,"fullexp",fullidx,"basic")=(0<$Data(basetype(fldotyp)))	; Only need this when debugging
	. Else  If "susize"=type Do		; Define size for entire structure/union
	. . Set type=$ZPiece(input,"|",1)
	. . Set typlen=$ZPiece(input,"|",2)
	. . Set types(type,"typlen")=typlen
	. Else  Do Error("BADINPUT","F","Line from running routine does not start with a known record type token (field or typsz)")
	Close pipe
	Set pipeopen=FALSE
	Set endline=" Finished running and reading results of C offset/size routine ("_lincnt_" lines read)"
	Do DoWrite($ZDate($Horolog,"24:60:SS")_endline)
	;
	Do:'KeepFiles
	. Open cfile:Write
	. Close cfile:Delete
	. Set cfile=$ZPiece(cfile,".",1)	; Isolate filename
	. Open cfile_".o":Write
	. Close cfile_".o":Delete
	. Open cfile:Write
	. Close cfile:Delete
	;
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;
	; Start of last phase - write out the GTM structure definition file
	;
	Set sdfile="GTMDefinedTypesInit.m"
	Open sdfile:New
	For f=sdfile do Prolog(f)
	Set lincnt=32,(fldcnt,gvstats,strctcnt)=0
	Use sdfile
	;
	Write TAB,"Set gtmsdver=""",gtmsdver,"""",!
	Write TAB,"Set gtmsdtyp=""",gtmtyp,"""",!
	Write TAB,";",!
	Set type=""
	For  Set type=$Order(types(type)) Quit:""=type  Do
	. Quit:(0=$Data(types(type,"fullexp")))		; Ignore any types with no displayable fields (e.g. ModR_M)
	. Write TAB,";",!
	. Write TAB,"Set gtmtypes(""",type,""")=""",types(type),"""",!
	. Write TAB,"Set gtmtypes(""",type,""",0)=",types(type,"fullexp",0),!
	. Write TAB,"Set gtmtypes(""",type,""",""len"")=",types(type,"typlen"),!
	. Set lincnt=lincnt+4
	. Set strctcnt=strctcnt+1
	. Do:(0'=$Data(types(type,"isuname")))
	. . Set:("struct"=types(type)) gtmstructs(types(type,"isuname"))=type
	. . Set:("union"=types(type)) gtmunions(types(type,"isuname"))=type
	. For fldidx=1:1:types(type,"fullexp",0) Do
	. . Write TAB,"Set gtmtypes(""",type,""",",fldidx,",""name"")=""",types(type,"fullexp",fldidx,"fldname"),"""",!
	. . Write TAB,"Set gtmtypes(""",type,""",",fldidx,",""off"")=",types(type,"fullexp",fldidx,"fldoff"),!
	. . Write TAB,"Set gtmtypes(""",type,""",",fldidx,",""len"")=",types(type,"fullexp",fldidx,"fldlen"),!
	. . Write TAB,"Set gtmtypes(""",type,""",",fldidx,",""type"")=""",types(type,"fullexp",fldidx,"type"),"""",!
	. . If "gvstats_rec_t"=type,$Increment(gvstats,types(type,"fullexp",fldidx,"fldlen"))
	. . Set fld=$ZPiece(types(type,"fullexp",fldidx,"fldname"),".",2,99)	; Eliminate structure (type) header
	. . 									; isolating just the field name
	. . Write TAB,"Set gtmtypfldindx(""",type,""",""",fld,""")=",fldidx,!
	. . Set lincnt=lincnt+5
	. . Set fldcnt=fldcnt+1
	. . Do:(0'=$Data(types(type,"fullexp",fldidx,"flddim")))
	. . . Write TAB,"Set gtmtypes(""",type,""",",fldidx,",""dim"")=",types(type,"fullexp",fldidx,"flddim"),!
	. . . Set lincnt=lincnt+1
	;
	Write TAB,";",!
	Write TAB,"; Structure cross reference (struct topname key to retrieve type",!
	Write TAB,";",!
	Set lincnt=lincnt+3
	Set type=""
	For  Set type=$Order(gtmstructs(type)) Quit:""=type  Do
	. Write TAB,"Set gtmstructs(""",type,""")=""",gtmstructs(type),"""",! Set lincnt=lincnt+1
	Write TAB,";",!
	Write TAB,"; Union cross reference (union topname key to retrieve type",!
	Write TAB,";",!
	Set lincnt=lincnt+3
	Set type=""
	For  Set type=$Order(gtmunions(type)) Quit:""=type  Do
	. Write TAB,"Set gtmunions(""",type,""")=""",gtmunions(type),"""",! Set lincnt=lincnt+1
	;
	Write TAB,";",!
	Write TAB,"Quit",!
	Set lincnt=lincnt+2
	Close sdfile
	Use $Principal
	Set endline=" Finished writing GTMDefinedTypesInit.m output file ("_lincnt_" lines written for "_strctcnt
	Set endline=endline_" structures with "_fldcnt_" fields)"
	Do DoWrite($ZDate($Horolog,"24:60:SS")_endline)

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;
	; All done, close it down..
	;
Done	Close:pipeopen pipe
	Use $P
	Do:debug
	. Set dumpfile="scantypedefs-DEBUG-zshowdump.txt"
	. Open dumpfile:New
	. Use dumpfile
	. ZShow "*"
	. Close dumpfile
	. Use $P
	Quit
;
Prolog(file)
copyrightbegin	;
	set cfile=$ztrnlnm("gtm_tools")_"/copyright.txt"
	; If $gtm_tools/copyright.txt does not exist (possible when building older versions), proceed without erroring out
	set cfileexception="set ecode="""" goto copyrightend^scantypedefs"
	set xxxx="2010"
	set yyyy=$zdate($H,"YYYY")
	open cfile:(readonly:exception=cfileexception)
	use file write ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;",!
	for i=1:1 use cfile read line quit:$zeof  do
	. if (1<$zl(line,"XXXX")) do
	. . set str=$zpiece(line,"XXXX",1)_xxxx_$zpiece(line,"XXXX",2)
	. . set str=$zpiece(str,"YYYY",1)_yyyy_$zpiece(str,"YYYY",2)
	. else  do
	. . set str=line
	. use file w ";"_str_";",!
	close cfile
copyrightend	;
	use file
	write ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;",!!
	Write ";",!
	Write "; Generated by scantypedefs.m at ",$ZDate($Horolog,"24:60:SS")," on ",$ZDate($Horolog,"YEAR-MM-DD"),!
	Write "; Build done with GT.M version: ",$ZVersion,!
	Write ";",!
	Write "; Environment variables during run:",!
	Write ";   $gtm_dist: ",$ZTrnlnm("gtm_dist"),!
	Write ";   $gtm_exe: ",$ZTrnlnm("gtm_exe"),!
	Write ";   $gtm_src: ",$ZTrnlnm("gtm_src"),!
	Write ";   $gtm_inc: ",$ZTrnlnm("gtm_inc"),!
	Write ";",!
	Write "; Note this file should not be manually invoked",!
	Write ";",!
	Write TAB,"Write ""GTM-E-GTMSDFILE This routine ("",$TEXT(+0),"") should not be manually invoked"",!",!
	Write TAB,"Quit",!
	Write ";",!
	If sdfile=file
	Write "; Entry point used by ",$select(sdfile=file:"gtmpcat and others",1:"gdeinit.m")," to define GTM structure fields",!
	Write ";",!
	Write "Init",!
	Write TAB,"; GT.M structure and field definitions",!
	Quit
;
; Routine to parse a union or struct definition putting the pieces in the passed in array. If/when
; we recurse, a new level is created at the insert point.
;
ParseUnionStructDef(typdf)
	New newidx,fldidx,bracecnt,newfldidx,isstructuniondef,createdisuidx,typdeftype
	Set newidx=$Increment(typdf("idxcnt"))
	Do GetToken(FALSE)		; Get past first bracket
	Set bracecnt=1			; Will be done (with this level) when this goes back to zero
	For  Quit:(0=bracecnt)  Do
	. ;
	. ; Processing somewhat mirrors the above parse but without "typedef" preceeding it.
	. ;
	. Set isstructuniondef=FALSE,createdisuidx=0	; Imbedded type not (currently) a struct or a union definition
	. Do:(TKCONST=token) GetToken(FALSE)	  	; First check for ignorable tokens
	. Do:(TKVOLATILE=token) GetToken(FALSE)
	. If (TKRBRACE=token) Set bracecnt=bracecnt-1 Quit	; Caller takes care of advancing the token
	. If ((TKBASETYPE=token)!(TKOTHERTYPE=token)!(TKGTMTYPE=token)) Do
	. . Set fldidx=$Increment(typdf("isuidx",newidx,0))
	. . Set typdf("isuidx",newidx,fldidx,"type")=tokenval
	. . Do GetToken(FALSE)
	. . ;
	. . Do:(TKCONST=token) GetToken(FALSE)		; More ignorable possibilities
	. . Do:(TKVOLATILE=token) GetToken(FALSE)
	. . ;
	. . ; We have a base type. Next token could be either the type
	. . ; we are typedef'd to or could be "*" in which case our type morphs
	. . ; to an address.
	. . ;
	. . Do:(TKASTERISK=token)
	. . . Set typdf("isuidx",newidx,fldidx,"origtype")=typdf("isuidx",newidx,fldidx,"type")
	. . . Set typdf("isuidx",newidx,fldidx,"type")="addr"
	. . . For  Quit:(TKASTERISK'=token)  Do GetToken(FALSE)
	. Else  If ((TKSTRUCT=token)!(TKUNION=token)) Do	; Struct and union processing is identical
	. . Set fldidx=$Increment(typdf("isuidx",newidx,0))
	. . Set typdf("isuidx",newidx,fldidx,"type")=tokenval
	. . Do GetToken(FALSE)
	. . ;
	. . ; Syntactic possibilities here having already parsed "struct" or "union" are:
	. . ;
	. . ;   1. "structunionname type ;"
	. . ;	2. "structunionname * type ;"
	. . ;   3. Note either of the above could have a "," and one or more types instead of the semi-colon
	. . ;   4. "structunionname {"
	. . ;   5. "{"
	. . ;
	. . ; For possibilities 1-3, record the structunionname and fall into end-of-entry processing. For 4-5, record the
	. . ; structunion name (if #4) and parse the defined struct or union within the braces.
	. . ;
	. . If ((TKOTHERTYPE=token)!(TKGTMTYPE=token)) Do		; name of struct or union
	. . . Set typdf("isuidx",newidx,fldidx,"isuname")=tokenval
	. . . Do GetToken(FALSE)
	. . . ;
	. . . ; Of all the possibilities at this point, the only one we care about is if we have an opening
	. . . ; brace or not. If yes, off we go to process the struct. If not, fall out to end of item processing.
	. . . ;
	. . . Do:(TKLBRACE=token)
	. . . . Set createdisuidx=$$ParseUnionStructDef(.typdf)
	. . . . Do GetToken(FALSE)
	. . . . Set isstructuniondef=TRUE
	. . Else  Do				; We should be defining a new structure here
	. . . Do:(TKLBRACE'=token) Error("ASSERTFAIL","F","Expecting token TKLBRACE")
	. . . Set createdisuidx=$$ParseUnionStructDef(.typdf)
	. . . Set isstructuniondef=TRUE
	. . . Do GetToken(FALSE)
	. Else  If (TKENUM=token) Do		; Simple enum type - ignore name and consider it an int
	. . Do GetToken(FALSE)
	. . Set fldidx=$Increment(typdf("isuidx",newidx,0))
	. . Set typdf("isuidx",newidx,fldidx,"type")="int"
	. . Do GetToken(FALSE)			; Eat enum-type - fall into end type processing
	. Else  Do Error("ASSERTFAIL","F","Unexpected token during struct/union parse:"_tokenval)
	. ;
	. ; At this point we expect the next token to be one or more types that the previous type is to be typedef'd as.
	. ; It is possible for any of these (potentially comma delimited) type(s) to be preceeded by an asterisk in which
	. ; case the given type is morphed into an address type instead. Note if the preceeding type was a struct or union
	. ; definition, there cannot be an address.
	. ;
	. Do:((0=$Data(TypdfTypes(token)))&(TKASTERISK'=token)&(TKLPAREN'=token))
	. . Do Error("ASSERTFAIL","F","Expecting typedef target type")
	. ;
	. ; We always record internal types - only thrown away when/if outer typedef is tossed. If this new type was
	. ; a union or struct, we need to record a pointer to the just created type (whatever typdf("idxcnt") is).
	. ;
	. If isstructuniondef Do
	. . Do:(0=createdisuidx) Error("ASSERTFAIL","F","Created index is 0 after structure create")
	. . Set typdf("isuidx",newidx,fldidx,"suptr")=createdisuidx	; -> created struct/union def
	. Else  If (TKASTERISK=token) Do
	. . Set typdf("isuidx",newidx,fldidx,"origtype")=typdf("isuidx",newidx,fldidx,"type")
	. . set typdf("isuidx",newidx,fldidx,"type")="addr"
	. . For  Quit:(TKASTERISK'=token)  Do GetToken(FALSE)
	. If (TKLPAREN=token) Do		; Have function pointer definition
	. . Do GetToken(FALSE)
	. . Do:(TKASTERISK'=token) Error("ASSERTFAIL","F","Expecting TKASTERISK in fuction pointer definition")
	. . Do GetToken(FALSE)
	. . Do:(0=$Data(TypdfTypes(token))) Error("ASSERTFAIL","F","Expecting function pointer type")
	. . Set typdeftype=tokenval
	. . Do GetToken(FALSE)
	. . Do:(TKRPAREN'=token) Error("ASSERTFAIL","F","Expecting TKRPAREN in fuction pointer definition")
	. . Do GetToken(FALSE)
	. . Do:(TKLPAREN'=token) Error("ASSERTFAIL","F","Expecting TKLPAREN in fuction pointer definition")
	. . Do GetToken(FALSE)
	. . For  Quit:(TKRPAREN=token)  Do	; Have to eat the parms until find a closing paren
	. . . Do:(0=$Data(ParmTypes(token))) Error("ASSERTFAIL","F","Unexpected token in function pointer parse")
	. . . Do GetToken(FALSE)
	. . Set typdf("isuidx",newidx,fldidx,"type")="addr"	; function pointers are address so use that as type
	. . 							; instead of return val type from func
	. Else  Set typdeftype=tokenval
	. Set typdf("isuidx",newidx,fldidx,"newtype")=typdeftype
	. Do GetToken(FALSE)
	. Do:(TKLBRACKET=token)	; Dimension specification (for field)
	. . Set expr=""
	. . Do GetToken(FALSE)
	. . For  Quit:(TKRBRACKET=token)  Do			; Capture tokens until "]"
	. . . Set expr=expr_tokenval
	. . . Do GetToken(FALSE)
	. . Set typdf("isuidx",newidx,fldidx,"dim")=expr
	. . Do GetToken(FALSE)
	. Do:(TKCOLON=token)	; Bit field definition
	. . Do GetToken(FALSE)
	. . Do:(TKINTEGER'=token) Error("ASSERTFAIL","F","Non-numeric bit length")
	. . Do GetToken(FALSE)
	. . Set typdf("isuidx",newidx,fldidx,"nofield")=TRUE	; This is a bit field so cannot be processed later - marked
	. For  Quit:(TKCOMMA'=token)  Do	; Could be a list of comma-delimited types equated to this definition
	. . Do GetToken(FALSE)
	. . Do:((0=$Data(TypdfTypes(token)))&(TKASTERISK'=token)) Error("ASSERTFAIL","F","Expecting typedef target type")
	. . ;
	. . ; We have a comma delimited type that needs a copy of the typdf entry just created.
	. . ;
	. . Set newfldidx=$Increment(typdf("isuidx",newidx,0))
	. . Merge typdf("isuidx",newidx,newfldidx)=typdf("isuidx",newidx,fldidx)
	. . Kill typdf("isuidx",newidx,newfldidx,"dim")		; This copy doesn't (yet) have a dimension
	. . If (TKASTERISK=token) Do
	. . . Do:(0=$Data(typdf("isuidx",newidx,newfldidx,"origtype")))		; Avoid if prev type already an address
	. . . . Set typdf("isuidx",newidx,newfldidx,"origtype")=typdf("isuidx",newidx,newfldidx,"type")
	. . . . Set typdf("isuidx",newidx,newfldidx,"type")="addr"
	. . . For  Quit:(TKASTERISK'=token)  Do GetToken(FALSE)
	. . . Do:(0=$Data(TypdfTypes(token))) Error("ASSERTFAIL","F","Expecting typedef target type")
	. . Else  If (0<$Data(typdf("isuidx",newidx,newfldidx,"origtype"))) Do	; Previous type was *'d, this one isn't = unconvert
	. . . Set typdf("isuidx",newidx,newfldidx,"type")=typdf("isuidx",newidx,newfldidx,"origtype")
	. . . Kill typdf("isuidx",newidx,newfldidx,"origtype")
	. . Set typdf("isuidx",newidx,newfldidx,"newtype")=tokenval
	. . Do GetToken(FALSE)
	. . If (TKLBRACKET=token) Do	; Dimension specification (for field)
	. . . Set expr=""
	. . . Do GetToken(FALSE)
	. . . For  Quit:(TKRBRACKET=token)  Do			; Capture tokens until "]"
	. . . . Set expr=expr_tokenval
	. . . . Do GetToken(FALSE)
	. . . Set typdf("isuidx",newidx,newfldidx,"dim")=expr
	. . . Do GetToken(FALSE)
	. . If (TKCOLON=token) Do;	; Have another bit field
	. . . Do GetToken(FALSE)
	. . . Do:(TKINTEGER'=token) Error("ASSERTFAIL","F","Non-numeric bit length")
	. . . Do GetToken(FALSE)
	. . . Set typdf("isuidx",newidx,newfldidx,"nofield")=TRUE    ; This is a bit field so cannot be processed later - marked
	. Do:(TKSEMI'=token) Error("ASSERTFAIL","F","Expecting end of statement TKSEMI token")
	. Do GetToken(FALSE)
	;
	; Our parsing is complete after the closing brace for this level. The caller (either us or the main level)
	; knows how to deal with the defined type(s) at that level. Return the level we just created.
	;
	Quit newidx

;
; Routine to generate nested queries for nested structures or unions
;
ProcessNestedStructUnion(type,isuidx,pfldidx,structname,fieldnameTD)
	New fieldname,fldidx
	For fldidx=1:1:types(type,"isuidx",isuidx,0) Do	; Loop thru the fields in this structure
	. Quit:(0<$Data(types(type,"isuidx",isuidx,fldidx,"nofield")))	    ; ignore bitfield or void fields
	. Set fieldname=types(type,"isuidx",isuidx,fldidx,"newtype")
	. If (0<$Data(types(type,"isuidx",isuidx,fldidx,"suptr"))) Do
	. . ;
	. . ; Have a pointer to a secondary structure. Output the high(er) level structure first, then its fields
	. . ;
	. . Do ExtractFieldInfo(type,isuidx,fldidx,structname,fieldnameTD_"."_fieldname)
	. . If (0<$Data(types(type,"isuidx",isuidx,fldidx,"dim"))) Do
	. . . Set fldTDparm=fieldnameTD_"."_fieldname_"[0]"
	. . Else  Do
	. . . Set fldTDparm=fieldnameTD_"."_fieldname
	. . Do ProcessNestedStructUnion(type,types(type,"isuidx",isuidx,fldidx,"suptr"),fldidx,structname,fldTDparm)
	. Else  Do ExtractFieldInfo(type,isuidx,fldidx,structname,fieldnameTD_"."_fieldname)
	Quit

;
; Routine to generate the query line into the C program for a given element
;
ExtractFieldInfo(type,isuidx,fldidx,structname,fieldname)
	New fldsgn,dim,fldtyp
	Set fldsgn=type_"|"_isuidx_"|"_fldidx_"|"_types(type,"isuidx",isuidx,fldidx,"type")_"|"_structname_"."_fieldname_"|"
	Set fldtyp=types(type,"isuidx",isuidx,fldidx,"type")
	; Flexible arrays have no dimension, so dim is just "". Structures without dimension are given an invalid value to let
	; us process them differently
	Set dim=$Get(types(type,"isuidx",isuidx,fldidx,"dim"),-1)
	Do:'$length(dim) 	; Flexible array definition
	. Write TAB,"PRINT_FLEXBASE(""",fldsgn,""", ",structname,", ",fieldname,", (int)(",dim,"));",!
	Do:$length(dim)		; Dimension defined
	. ; Dimension for dimension-less definitions and [unsigned] char types forced to 1
	. Set:(dim<0)!(("char"=fldtyp)!("unsigned-char"=fldtyp)) dim=1
	. Write TAB,"PRINT_OFFSET(""",fldsgn,""", ",structname,", ",fieldname,", (int)(",dim,"));",!
	Set lincnt=lincnt+1
	Quit

;
; Routine to resolve a given type into its most basic known type (e.g. TID->INTPTR_T->intptr_t)
;
ResolveType(type)
	Quit:(0<$Data(basetype(type))) type		; Fast return if it is a basic type
	Quit:(0<$Data(TransType(type))) TransType(type)	; Not quite as fast return if it already transformed to lowest type
	;
	; Type has not yet been transformed to its lowest type so do that now if we can
	;
	If ((0<$Data(types(type)))&(0=$Data(sutype(types(type,"type"))))) Do  Quit TransType(type) ; simple (not function pointer)
	. Do:(type=types(type)) Error("ASSERTFAIL","F","types(type) same as type which can never be")
	. Set TransType(type)=$$ResolveType(types(type,"type"))
	;
	; Else we can't resolve it further at this stage - maybe next stage..
	Quit type

;
; Find end of typedef as we are going to ignore the rest of this one
;
IgnoreTypedef(bracecnt)
	New seensemi
	Set seensemi=FALSE
	For  Quit:(seensemi&(0=bracecnt))  Do
	. Do GetToken(FALSE)
	. If (TKEOF=token) Do Error("ASSERTFAIL","F","Hit EOF while scanning for end of typedef (IgnoreTypedef)")
	. Else  If ((0=bracecnt)&(TKSEMI=token)) Set seensemi=TRUE
	. Else  If (TKLBRACE=token) Set bracecnt=bracecnt+1
	. Else  If (TKRBRACE=token) Set bracecnt=bracecnt-1
	Do GetToken(FALSE)
	Quit

;
; Add an include to the structures. Creates two indexes:
;
; inclstmt(0)=<n>		    - max count
; inclstmt(<n>)=<include statement> - could be #ifdef/#endif
; inclref(<include name>)=<n>	    - index into inclstmt
;
AddInclude(incl,writeit)
	New incidx,addquotes,preproc
	If "<"=$ZExtract(incl,1) Set writeit=TRUE,addquotes=FALSE,preproc=FALSE
	Else  If "#"=$ZExtract(incl,1) Set writeit=TRUE,addquotes=FALSE,preproc=TRUE
	Else  Set addquotes=TRUE,preproc=FALSE
	Set writeit=$Get(writeit,FALSE)
	Set:('writeit) writeit=$$StatFile("$gtm_inc/"_incl)
	Do:(writeit)
	. Set incidx=$Increment(inclstmt(0))
	. Set inclref(incl)=incidx
	. Set:(addquotes) incl=""""_incl_""""
	. Set inclstmt(incidx)=$Select(preproc:"",TRUE:"#include ")_incl
	Quit

;
; Add an excluded include to the structures
;
; exclincl(<include name>)=""
;
ExclInclude(incl)
	Set exclincl(incl)=""
	Quit

;
; Routine to tokenize the input stripmined file. This is a basic tokenizer with the following
; dependencies (which are all part of the stripmine.awk script):
;
;   1. The input file has had spaces put around the tokens it recognizes so parsing can be done
;      quickly and efficiently using $ZPiece().
;   2. White space has been reduced to single spaces, again for parsing efficiency (no tabs or
;      multiple consecutive white space.
;   3. We only have to parse typedef statements, not the larger C universe again streamlining
;      our parsing.
;   4. First token on a line has no preceeding white space.
;
GetToken(EofOk)
	New done
	Set done=FALSE
	If TKEOF=dirtoken Do
	. ;
	. ; Simple case where we are out of input
	. ;
	. Set token=TKEOF
	. Set tokenval=""
	. Set done=TRUE
	Do:(done&'EofOk) Error("ASSERTFAIL","F","End of file detected in an inappropriate place")
	Quit:done
	;
	; Else we need the next token
	;
	Set prevtoken=token
	Set token=dirtoken
	Set tokenval=dirtokenval
	Set:(TKEOF'=token) tokcnt=tokcnt+1
	Do:((TKEOF=token)&'EofOk) Error("ASSERTFAIL","F","End of file detected in an inappropriate place")
	Do:(debugtoken) dbgzwrite("token,tokenval","Called from "_$Stack($Stack(-1)-1,"PLACE"))
	;
	; Scan to create next director token/val
	;
	Set dirtokenval=""
	For  Quit:((""'=inbuf)!done)  Do
	. If $ZEof Do			; Oops, at EOF with nothing read
	. . Set dirtoken=TKEOF
	. . Set done=TRUE
	. Else  Do			; Read a new line - still might detect EOF but might get lucky too!
	. . Read inbuf
	. . Set lastreadline=inbuf	; Save original line to parse for debuggingg
	. . Set:'$ZEof inlines=inlines+1
	Quit:done			; Processing already complete - bypass parse scan
	;
	; Compute the director token
	;
	For  Quit:(""'=dirtokenval)  Do
	. Set dirtokenval=$ZPiece(inbuf," ",1)
	. Set inbuf=$ZExtract(inbuf,$ZLength(dirtokenval)+2,999999)	; Remaining buffer
	;
	; Decide on the director token type and return
	;
	If (0<$Data(CharScn(dirtokenval))) Set dirtoken=CharScn(dirtokenval) Quit
	If (0<$Data(Keywd(dirtokenval))) Set dirtoken=Keywd(dirtokenval) Quit
	If (0<$Data(types(dirtokenval))) Set dirtoken=TKGTMTYPE Quit
	If (0<$Data(basetype(dirtokenval))) Set dirtoken=TKBASETYPE Quit
	If (dirtokenval?1.N) Set dirtoken=TKINTEGER Quit
	Set dirtoken=TKOTHERTYPE
	Quit

;
; Routine to define basic types (no further redefs of these types).
;
;    - basetype(type) - indicates this is a basic type
;
DefBasicType(type)
	Set type=$Translate(type," ","-")	; So types match existing defined types (no imbedded spaces)
	Set basetype(type)=1
	Quit

;
; Build the include string needed to help older GT.M versions find include files in new locations
;  https://superuser.com/questions/981780/how-does-gcc-find-the-following-header-file
;
iquote()
	new cmd,i,include,line,pipe,start
	set pipe="pipe",cmd="cpp -v /dev/null -o /dev/null",start=0
	open pipe:(command=cmd)::pipe
	use pipe
	for i=1:1  read line(i) quit:($zeof)!(line(i)="End of search list.")  do
	.	set:('start)&(line(i)?1"#include "5E1" search starts here:") start=1
	.	quit:'start
	.	if line(i)?1" /".E set include=$get(include)_" -I"_line(i)
	close pipe
	quit:$quit include
	write include,!
	quit

;
; Routine to execute a command in a pipe and return the executed lines in an array
;
CommandToPipe(cmd,results)
	New pipecnt,pipe,saveIO
	Set lastpipecmd=cmd
	Kill results
	Set pipe="CmdPipe"
	Set saveIO=$IO
	Open pipe:(Shell="/usr/local/bin/tcsh":Command=cmd)::"PIPE"
	Use pipe
	Set pipecnt=1
	For  Read results(pipecnt) Quit:$ZEof  Set pipecnt=pipecnt+1
	Close pipe
	Set results(0)=pipecnt-1
	Kill results(pipecnt)
	Use saveIO
	Quit

;
; Routine to see if a file exists
;
StatFile(fname)
	Open fname:(Readonly:Exception="Set $ECode="""" Quit FALSE")
	Close fname
	Quit TRUE

;
; Output error message - generate dump for fatal errors..
;
Error(msgid,severity,text)
	New zshowdmps
	Use $P
	Write !,"SCANTYPEDEFS-",severity,"-",msgid," ",text," from ",$Stack($Stack(-1)-1,"PLACE"),!!
	Do ;:("F"=severity)
	. Set zshowdmps=$Increment(ZShowDumpsCreated)
	. Set dumpfile="scantypedefs-fail.zshowdmp-"_$ZDate($Horolog,"YEARMMDD-2460SS")_"-"_zshowdmps_".txt"
	. Open dumpfile:New
	. Use dumpfile
	. ZShow "*"
	. Close dumpfile
	Break:debug
	Halt
;
; Write to $P saving and restoring $IO
;
DoWrite(text)
	New saveio
	Set saveio=$IO
	Use $P
	Write text,!
	Use saveio
	Quit

;
; Routine to enable debugging
;
dbgzwrite(zwrarg,sfx)
	New saveio
	Set saveio=$IO
	Use $P
	Write "DbgZwrite at ",$Stack($Stack-1,"PLACE"),":----------- ",$Select(""'=$Get(sfx,""):"("_sfx_")",TRUE:"")_":",!
	ZWrite @zwrarg
	Use saveio
	Quit

;
; Error trap to record info about what the problem might be..
;
ErrorTrap
	Use $P
	Write !!,$ZDate($Horolog,"24:60:SS")," Error Trap has been signaled!",!!!
	Do Error("ETRAPINVOK","F",$ZStatus)
	Write !,"SCANTYPEDEFS-F-UNABLECONT Unable to continue after error - halting",!!
	Halt	; We should never return from Error but if we do..
