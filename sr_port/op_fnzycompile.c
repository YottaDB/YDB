/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2023 Fidelity National Information        *
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *                                                              *
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.                                         *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/
/*sparm*/
#include "mdef.h"

#include "op.h"
#include "stringpool.h"
#include "error.h"
#include "mvalconv.h"

#include "compiler.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mdq.h"
#include "cgp.h"
#include "mmemory.h"
#include "stp_parms.h"
#include "list_file.h"
#include "source_file.h"
#include "lb_init.h"
#include "reinit_compilation_externs.h"
#include "comp_esc.h"
#include "resolve_blocks.h"
#include "hashtab_str.h"
#include "rtn_src_chksum.h"
#include "gtmmsg.h"
#include "iosp.h"       /* for SS_NORMAL */
#include "start_fetches.h"


error_def(ERR_UNKNOWNSYSERR);
error_def(ERR_INDRMAXLEN);

GBLREF int                      source_column;
GBLREF boolean_t                mstr_native_align, save_mstr_native_align;
GBLREF char                     cg_phase;       // фаза кодогенератора
GBLREF command_qualifier        cmd_qlf;        // надо обнулить, что бы в "тихом" режиме работал
GBLREF int                      mlmax;
GBLREF mline                    mline_root;
GBLREF spdesc                   indr_stringpool, rts_stringpool, stringpool;
GBLREF src_line_struct          src_head;
GBLREF triple                   t_orig;

LITREF  mval                    literal_null;


void op_fnzycompile(mval *string, mval *ret)
{
    int             errknt, errpos;
    uint4           line_count;
    mlabel          *null_lab;
    mident          null_mident;
    size_t          len, lent;
    char            *out;

    int             rc = 0;             // error position
    command_qualifier   cmd_qlf_save;   

    MV_FORCE_STR(string); 
    
    if (MAX_SRCLINE < (unsigned)string->str.len)
            RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
 
   if (string->str.len == 0) {
        *ret = literal_null;
        return;
    }

    DCL_THREADGBL_ACCESS;

    SETUP_THREADGBL_ACCESS;

    if (rts_stringpool.base == stringpool.base)
    {
        rts_stringpool = stringpool;
        if (!indr_stringpool.base)
        {
            stp_init(STP_INITSIZE);
            indr_stringpool = stringpool;
        } else
            stringpool = indr_stringpool;
    }

    cmd_qlf_save = cmd_qlf;
    cmd_qlf.qlf = 0;

    run_time = FALSE;
    TREF(compile_time) = TRUE;
    TREF(transform) = FALSE;
    TREF(dollar_zcstatus) = SS_NORMAL;
    reinit_compilation_externs();
    memset(&null_mident, 0, SIZEOF(null_mident));
    ESTABLISH(compiler_ch);
    COMPILE_HASHTAB_CLEANUP;

    memcpy((TREF(source_buffer)).addr,string->str.addr,string->str.len);
    (TREF(source_buffer)).len = string->str.len + 1;
    *((TREF(source_buffer)).addr + string->str.len) = *((TREF(source_buffer)).addr + string->str.len + 1) = '\0';

    /* To check trings from nested blocks, we will mask the level deep */
    out = (TREF(source_buffer)).addr;
    while(*out != '\0') {
        if (*out == '.') *out = ' ';
        if ((*out != '\t') && (*out != ' ')) break;
        out++;
    }
       
    COMPILE_HASHTAB_CLEANUP;

    save_mstr_native_align = mstr_native_align;
    mstr_native_align = FALSE;
    
    TREF(source_error_found) = errknt = 0;

    cg_phase = CGP_NOSTATE;

    dqinit(&src_head, que);
    tripinit();

    null_lab = get_mladdr(&null_mident);
    null_lab->ml = &mline_root;
    mlmax++;


    (TREF(fetch_control)).curr_fetch_trip =
    (TREF(fetch_control)).curr_fetch_opr = newtriple(OC_LINEFETCH);
    (TREF(fetch_control)).curr_fetch_count = 0;

    TREF(code_generated) = FALSE;
    TREF(source_line) = line_count = 1;
    TREF(source_error_found) = 0;

    lb_init();
    if (!line(&line_count))
    {
        errknt++;
    }

        cg_phase = CGP_RESOLVE;
        newtriple(OC_LINESTART);

        // always provide a default QUIT
        newtriple(OC_RET);
        mline_root.externalentry = t_orig.exorder.fl;

        assert(indr_stringpool.base == stringpool.base);
        INVOKE_STP_GCOL(0);

        start_fetches(OC_NOOP);
        resolve_blocks();

        errknt = resolve_ref(errknt);


    // where the parser stopped
    rc = TREF(last_source_column);

    COMPILE_HASHTAB_CLEANUP;
    reinit_compilation_externs();

    run_time = TRUE;
    TREF(compile_time) = FALSE;
    TREF(transform) = TRUE;

    assert(indr_stringpool.base == stringpool.base);

    if (indr_stringpool.base == stringpool.base)
    {
        indr_stringpool = stringpool;
        stringpool = rts_stringpool;
    }

    mstr_native_align = save_mstr_native_align;

    (TREF(source_buffer)).len = 0;
    cmd_qlf = cmd_qlf_save;

    REVERT;
    
    errknt = TREF(source_error_found);
    errpos = TREF(last_source_column);

    if ( !errknt )
    {
        *ret = literal_null;
        return;   
    }
        const err_ctl *ec = err_check(errknt);
        const err_msg *em = NULL;

    if( NULL != ec )
        {
            GET_MSG_INFO(errknt, ec, em);
    
            len = strlen( em->msg );

            lent = strlen( em->tag );

            len += (lent + MAX_NUM_SIZE + 9);
    
            ENSURE_STP_FREE_SPACE(len);

    
            lent = snprintf((char*)stringpool.free, len, "%i,%%YDB-E-%s,%s", errpos, em->tag, em->msg);

            ret->mvtype = MV_STR;
            ret->str.addr = (char *)stringpool.free;
            ret->str.len = lent;
            stringpool.free += lent;
            return;
        
        } else 

            rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_UNKNOWNSYSERR, 1, errknt);
}
