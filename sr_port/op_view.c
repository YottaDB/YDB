/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#ifdef DEBUG
# include "gtm_syslog.h"	/* Needed for white box case in VTK_STORDUMP */
#endif
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "io.h"
#include "util.h"
#include "have_crit.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbgtr.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmd_qlf.h"
#include "collseq.h"
#include "error.h"
#include "iosp.h"
#include "jnl.h"
#include "lv_val.h"
#include "view.h"
#include "send_msg.h"
#include "op.h"
#include "change_reg.h"
#include "patcode.h"
#include "mprof.h"
#include "cmidef.h"
#include "gvcmz.h"
#include "testpt.h"
#include "mvalconv.h"
#include "dpgbldir.h"		/* For get_next_gdr() prototype */
#include "ast.h"
#include "wcs_flu.h"
#include "stringpool.h"
#include "gtmdbglvl.h"
#include "gtm_malloc.h"
#include "alias.h"
#include "fullbool.h"
#include "anticipatory_freeze.h"
#include "gvnh_spanreg.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"
#ifdef GTM_TRIGGER
# include "rtnhdr.h"		/* For rtn_tabent in gv_trigger.h */
# include "gv_trigger.h"
# include "gtm_trigger.h"
#endif
#include "wbox_test_init.h"
#include "mutex.h"
#include "gtmlink.h"
#ifdef DEBUG
# include "gtmsecshr.h"
#endif
#ifdef AUTORELINK__SUPPORTED
# include "relinkctl.h"
#endif
#include "gtmimagename.h"
#include "cache.h"
#include "set_gbuff_limit.h"	/* Needed for set_gbuff_limit() */
#include "interlock.h"
#include "wcs_backoff.h"
#include "wcs_wt.h"
#include "localvarmonitor.h"
#include "is_file_identical.h"	/* Needed for JNLPOOL_INIT_IF_NEEDED */

STATICFNDCL void lvmon_release(void);
STATICFNDCL void view_dbop(unsigned char keycode, viewparm *parmblkptr, mval *thirdarg);

GBLREF	volatile int4 		db_fsync_in_prog;
GBLREF	boolean_t		certify_all_blocks;
GBLREF	bool			undef_inhibit, jobpid;
GBLREF	bool			view_debug1, view_debug2, view_debug3, view_debug4;
GBLREF	bool			zdefactive;
GBLREF	unsigned short		zdefbufsiz;
GBLREF	int4			break_message_mask;
GBLREF	command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target, *gv_target_list;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	symval			*curr_symval;
GBLREF	trans_num		local_tn;	/* Transaction number for THIS PROCESS */
GBLREF	int4			zdir_form;
GBLREF	boolean_t		gvdupsetnoop;	/* If TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		badchar_inhibit;
GBLREF	int			gv_fillfactor;
GBLREF	symval			*curr_symval;
GBLREF	uint4			gtmDebugLevel;
GBLREF	boolean_t		lvmon_enabled;
GBLREF	spdesc			stringpool;
GBLREF	boolean_t		is_updproc;
GBLREF	uint4			process_id;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		dmterm_default;
GBLREF mstr			extnam_str;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;

error_def(ERR_ACTRANGE);
error_def(ERR_COLLATIONUNDEF);
error_def(ERR_COLLDATAEXISTS);
error_def(ERR_DBFSYNCERR);
error_def(ERR_GBLNOMAPTOREG);
error_def(ERR_INVZDIRFORM);
error_def(ERR_ISOLATIONSTSCHN);
error_def(ERR_JNLFLUSH);
error_def(ERR_TPNOSTATSHARE);
error_def(ERR_PATLOAD);
error_def(ERR_PATTABNOTFND);
error_def(ERR_REQDVIEWPARM);
error_def(ERR_SEFCTNEEDSFULLB);
error_def(ERR_TEXT);
error_def(ERR_TRACEON);
error_def(ERR_VIEWCMD);
error_def(ERR_YDIRTSZ);
error_def(ERR_ZDEFACTIVE);

#define MAX_YDIRTSTR 32
#define ZDEFMIN 1024
#define ZDEFDEF 32767
#define ZDEFMAX 65535

#define WRITE_LITERAL(x) (outval.str.len = SIZEOF(x) - 1, outval.str.addr = (x), op_write(&outval))

/* If changing noisolation status within TP and already referenced the global, then error */
#define SET_GVNH_NOISOLATION_STATUS(gvnh, status)								\
{														\
	GBLREF	uint4			dollar_tlevel;								\
														\
	if (!dollar_tlevel || gvnh->read_local_tn != local_tn || status == gvnh->noisolation)			\
		gvnh->noisolation = status;									\
	else													\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ISOLATIONSTSCHN, 4, gvnh->gvname.var_name.len, 	\
			gvnh->gvname.var_name.addr, gvnh->noisolation, status);					\
}

void	op_view(int numarg, mval *keyword, ...)
{
	boolean_t		dbgdmpenabled, old_bool, was_crit, was_skip_gtm_putmsg;
	char			*chptr;
	collseq			*new_lcl_collseq;
	gd_addr			*addr_ptr;
	gd_region		*reg, *r_top, *save_reg;
	gvnh_reg_t		*gvnh_reg;
	gvnh_spanreg_t		*gvspan;
	gv_namehead		*gvnh;
	hash_table_mname	*table;
	ht_ent_mname		*tabent, *table_base_orig, *topent;
	int			acnt, clrlen, lcnt, lct, icnt, ncol, nct, size, status, table_size_orig;
	int4			testvalue, tmpzdefbufsiz;
	jnl_buffer_ptr_t	jb;
	lv_blk			*lvbp;
	lv_val			*lv, *lvp, *lvp_top;
	lvmon_var		*lvmon_var_p, *lvmon_vars_base;
	mstr			tmpstr;
	mval			*arg, *nextarg, outval;
	noisolation_element	*gvnh_entry;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	symval			*cstab, *lvlsymtab;
	uint4			jnl_status, dummy_errno;
	va_list			var;
	viewparm		parmblk, parmblk2;
	viewtab_entry		*vtp;
#	ifdef AUTORELINK_SUPPORTED
	open_relinkctl_sgm	*linkctl;
	relinkrec_t		*linkrec;
#	endif
	static readonly char msg1[] = "Caution: Database Block Certification Has Been ";
	static readonly char lv_msg1[] =
		"Caution: GT.M reserved local variable string pointer duplicate check diagnostic has been";
	static readonly char msg2[] = "Disabled";
	static readonly char msg3[] = "Enabled";
	static readonly char lower[] = "LOWER";
	static readonly char upper[] = "UPPER";
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, keyword);
	jnl_status = 0;
	assertpro(1 <= numarg);
	MV_FORCE_STR(keyword);
	numarg--;	/* Remove keyword from count */
	if (0 < numarg)
	{
		arg = va_arg(var, mval *);
		MV_FORCE_STR(arg);
	} else
		arg = (mval *)NULL;
	vtp = viewkeys(&keyword->str);
	view_arg_convert(vtp, (int)vtp->parm, arg, &parmblk, IS_DOLLAR_VIEW_FALSE);
	switch(vtp->keycode)
	{
#		ifdef UNICODE_SUPPORTED
		case VTK_BADCHAR:
			badchar_inhibit = FALSE;
			break;
		case VTK_NOBADCHAR:
			badchar_inhibit = TRUE;
			break;
#		endif
		case VTK_BREAKMSG:
			break_message_mask = MV_FORCE_INT(parmblk.value);
			break;
		case VTK_DEBUG1:
			outval.mvtype = MV_STR;
			view_debug1 = (0 != MV_FORCE_INT(parmblk.value));
			break;
		case VTK_DEBUG2:
			outval.mvtype = MV_STR;
			view_debug2 = (0 != MV_FORCE_INT(parmblk.value));
			break;
		case VTK_DEBUG3:
			outval.mvtype = MV_STR;
			view_debug3 = (0 != MV_FORCE_INT(parmblk.value));
			break;
		case VTK_DEBUG4:
			outval.mvtype = MV_STR;
			view_debug4 = (0 != MV_FORCE_INT(parmblk.value));
			break;
		case VTK_DBFLUSH:
		case VTK_DBSYNC:
		case VTK_EPOCH:
		case VTK_FLUSH:
		case VTK_GVSRESET:
		case VTK_JNLFLUSH:
		case VTK_POOLLIMIT:
			view_dbop(vtp->keycode, &parmblk, (numarg > 1) ? va_arg(var, mval *) : (mval *)NULL);
			break;
		case VTK_DMTERM:
			dmterm_default = TRUE;
			break;
		case VTK_NODMTERM:
			dmterm_default = FALSE;
			break;
		case VTK_FULLBOOL:
		case VTK_FULLBOOLWARN:
		case VTK_NOFULLBOOL:

			if ((VTK_NOFULLBOOL == vtp->keycode) && (OLD_SE != TREF(side_effect_handling)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SEFCTNEEDSFULLB);
			old_bool = TREF(gtm_fullbool);
			TREF(gtm_fullbool) = (VTK_FULLBOOL == vtp->keycode) ? FULL_BOOL
				: (VTK_FULLBOOLWARN == vtp->keycode) ? FULL_BOOL_WARN : GTM_BOOL;
			if (old_bool != TREF(gtm_fullbool))
				cache_table_rebuild();
			break;
		case VTK_GDSCERT:
			outval.mvtype = MV_STR;
			op_wteol(1);
			WRITE_LITERAL(msg1);
			certify_all_blocks = (0 != MV_FORCE_INT(parmblk.value));
			if (certify_all_blocks)
				WRITE_LITERAL(msg3);
			else
				WRITE_LITERAL(msg2);
			op_wteol(1);
			break;
		case VTK_GVDUPSETNOOP:
			gvdupsetnoop = (0 != MV_FORCE_INT(parmblk.value));
			break;
		case VTK_LVNULLSUBS:
			TREF(lv_null_subs) = LVNULLSUBS_OK;
			break;
		case VTK_NOLVNULLSUBS:
			TREF(lv_null_subs) = LVNULLSUBS_NO;
			break;
		case VTK_NEVERLVNULLSUBS:
			TREF(lv_null_subs) = LVNULLSUBS_NEVER;
			break;
		case VTK_JNLERROR:
			/* In case of update process, don't let this variable be user-controlled. We always want to error out
			 * if we are about to invoke "jnl_file_lost". This way we will force the operator to fix whatever
			 * caused the jnl_file_lost invocation (e.g. permissions, disk space issues etc.) and restart
			 * the receiver server so replication can resume from wherever we last left. On the other hand,
			 * automatically turning off journaling (and hence replication) (like is a choice for GT.M)
			 * would make resumption of replication much more effort for the user.
			 */
			assert(!is_updproc || (JNL_FILE_LOST_ERRORS == TREF(error_on_jnl_file_lost)));
			if (!is_updproc)
			{
				TREF(error_on_jnl_file_lost) = MV_FORCE_INT(parmblk.value);
				if (MAX_JNL_FILE_LOST_OPT < TREF(error_on_jnl_file_lost))
					TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_TURN_OFF;
				parmblk.gv_ptr = NULL;
				view_dbop(vtp->keycode, &parmblk, (mval *)NULL);
			}
			break;
		case VTK_JNLWAIT:
			/* Go through all regions that could have possibly been open across all global directories */
			if (!dollar_tlevel)
			{	/* Only if we're not in a TP transaction */
				for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
					for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
						jnl_wait(reg);
			}
			break;
		case VTK_JOBPID:
			outval.mvtype = MV_STR;
			jobpid = (0 != MV_FORCE_INT(parmblk.value));
			break;
		case VTK_LABELS:
			if ((SIZEOF(upper) - 1) == parmblk.value->str.len)
			{
				if (!memcmp(upper, parmblk.value->str.addr, SIZEOF(upper) - 1))
					glb_cmd_qlf.qlf &= ~CQ_LOWER_LABELS;
				else  if (!memcmp(lower, parmblk.value->str.addr, SIZEOF(lower) - 1))
					glb_cmd_qlf.qlf |= CQ_LOWER_LABELS;
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
			}
			break;
		case VTK_NOISOLATION:
			if (NOISOLATION_NULL == parmblk.ni_list.type)
			{
				for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
					SET_GVNH_NOISOLATION_STATUS(gvnh, FALSE);
				parmblk.ni_list.type = NOISOLATION_PLUS;
			}
			assert((NOISOLATION_PLUS == parmblk.ni_list.type) || (NOISOLATION_MINUS == parmblk.ni_list.type));
			if (NOISOLATION_PLUS == parmblk.ni_list.type)
			{
				for (gvnh_entry = parmblk.ni_list.gvnh_list; gvnh_entry; gvnh_entry = gvnh_entry->next)
					SET_GVNH_NOISOLATION_STATUS(gvnh_entry->gvnh, TRUE);
			} else
			{
				for (gvnh_entry = parmblk.ni_list.gvnh_list; gvnh_entry; gvnh_entry = gvnh_entry->next)
					SET_GVNH_NOISOLATION_STATUS(gvnh_entry->gvnh, FALSE);
			}
			break;
		case VTK_PATCODE:
			if (arg)
				tmpstr = parmblk.value->str;
			else
				tmpstr.len = 0;
			status = setpattab(&tmpstr);
			if (0 == status)
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PATTABNOTFND, 2, parmblk.value->str.len,
						parmblk.value->str.addr);
			}
			break;
		case VTK_RESETGVSTATS:
			for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
			{
				for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
				{
					if (!reg->open)
						continue;
					switch(REG_ACC_METH(reg))
					{
						case dba_mm:
						case dba_bg:
							csa = &FILE_INFO(reg)->s_addrs;
							memset((char *)csa->gvstats_rec_p, 0, SIZEOF(gvstats_rec_t));
							break;
						case dba_cm:
						case dba_usr:
							break;
						default:
							assert(FALSE && REG_ACC_METH(reg));
							break;
					}
				}
			}
			break;
		case VTK_YCHKCOLL:
			if (arg)
				lct = MV_FORCE_INT(parmblk.value);
			else
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REQDVIEWPARM);
			}
			if (0 == lct)
				break;	/* 0 value of collation is always good */
			if ((lct < MIN_COLLTYPE) || (lct > MAX_COLLTYPE))
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ACTRANGE, 1, lct);
			}
			was_skip_gtm_putmsg = TREF(skip_gtm_putmsg);
			TREF(skip_gtm_putmsg) = TRUE;	/* To avoid ready_collseq from doing gtm_putmsg in case of errors.
							 * not doing so will cause GDECHECK errors in caller (GDE).
							 */
			new_lcl_collseq = ready_collseq(lct);
			TREF(skip_gtm_putmsg) = was_skip_gtm_putmsg;
			if (0 == new_lcl_collseq)
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
			}
			break;
		case VTK_YDIRTVAL:
			/* This is an internal use only call to VIEW which is used to modify directory
			 * tree entries.  It places a string in in the static array view_ydirt_str.  A subsequent
			 * view to YDIRTREE causes the value to be put in the directory tree.
			 * For example VIEW "YDIRTVAL":$C(55,0,0,0),"YDIRTREE":XYZ would cause global ^XYZ
			 * to have a root block of 0x00000055, on a little-endian computer.
			 * NOTE: it is easy to corrupt a database with these calls.
			 * The use of the "Y" sentinal character indicates that the VIEW keyword is interal
			 * FIS use only, and that FIS will not provide upward compatibility to the user.
			 * These keywords should NOT be used outside of FIS software
			 */
			if (NULL == TREF(view_ydirt_str))
				TREF(view_ydirt_str) = (char *)malloc(MAX_YDIRTSTR + 1);
			TREF(view_ydirt_str_len) = parmblk.value->str.len;
			if (TREF(view_ydirt_str_len) > MAX_YDIRTSTR)
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_YDIRTSZ);
			}
			if (TREF(view_ydirt_str_len) > 0)
				memcpy(TREF(view_ydirt_str), parmblk.value->str.addr, TREF(view_ydirt_str_len));
			break;
		case VTK_YDIRTREE:
			/* See comment under YDIRTVAL above
			 * Restriction: This program will update an existing entry in a directory tree but
			 * will fail if the entry is not present.  So make sure that the global exists before
			 * a YDIRTREE update is performed
			 */
			size = extnam_str.len;		/* internal use of op_gvname should not disturb extended reference */
			op_gvname(VARLSTCNT(1) parmblk.value);
			extnam_str.len = size;
			arg = (numarg > 1) ? va_arg(var, mval *) : NULL;
			if (NULL != arg)
			{
				view_arg_convert(vtp, VTP_DBREGION, arg, &parmblk2, IS_DOLLAR_VIEW_FALSE);
				reg = parmblk2.gv_ptr;
				/* Determine if "reg" is mapped to by global name. If not issue error */
				gvnh_reg = TREF(gd_targ_gvnh_reg);	/* Set up by op_gvname */
				gvspan = (NULL == gvnh_reg) ? NULL : gvnh_reg->gvspan;
				if (((NULL != gvspan) && !gvnh_spanreg_ismapped(gvnh_reg, gd_header, reg))
					|| ((NULL == gvspan) && (reg != gv_cur_region)))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GBLNOMAPTOREG, 4,
						parmblk.value->str.len, parmblk.value->str.addr, REG_LEN_STR(reg));
				}
				if (NULL != gvspan)
					GV_BIND_SUBSREG(gd_header, reg, gvnh_reg);
			}
			assert(INVALID_GV_TARGET == reset_gv_target);
			reset_gv_target = gv_target;
			gv_target = cs_addrs->dir_tree;		/* Trick the put program into using the directory tree */
			outval.mvtype = MV_STR;
			outval.str.len = TREF(view_ydirt_str_len);
			outval.str.addr = (char *)TREF(view_ydirt_str);
			op_gvput(&outval);
			RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
			gv_target->root = 0;
			/* Now that root is set to 0, the next access to this global will go through gvcst_root_search which will
			 * re-initialize "nct", "act" and "ver" accordingly and also initialize "collseq" ONLY IF "act" is non-zero.
			 * Since we do not know if "act" will be 0 or not in gvcst_root_search, let us reset "collseq" to NULL here.
			 */
			gv_target->collseq = NULL;
			break;
		case VTK_YLCT:
			if (arg)
				lct = MV_FORCE_INT(parmblk.value);
			else
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REQDVIEWPARM);
			}
			nextarg = NULL;
			ncol = -1;
			if (numarg > 1)
			{
				nextarg = va_arg(var, mval *);
				ncol = MV_FORCE_INT(nextarg);
			}
			nextarg = NULL;
			nct = -1;
			if (numarg > 2)
			{
				nextarg = va_arg(var, mval *);
				nct = MV_FORCE_INT(nextarg);
			}
			if ((-1 == lct) && (-1 == ncol) && (-1 == nct))
				break;
			/* lct = -1 indicates user wants to change only ncol, not lct */
			if (-1 != lct)
			{
				if ((lct < MIN_COLLTYPE) || (lct > MAX_COLLTYPE))
				{
					va_end(var);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ACTRANGE, 1, lct);
				}
			}
			/* At this point, verify that there is no local data with subscripts */
			for (cstab = curr_symval; cstab; cstab = cstab->last_tab)
			{
				assert(cstab->h_symtab.top == cstab->h_symtab.base + cstab->h_symtab.size);
				for (tabent = cstab->h_symtab.base, topent = cstab->h_symtab.top; tabent < topent; tabent++)
				{
					if (HTENT_VALID_MNAME(tabent, lv_val, lv))
					{
						assert(LV_IS_BASE_VAR(lv));
						if (lv && LV_HAS_CHILD(lv))
						{
							va_end(var);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_COLLDATAEXISTS);
						}
					}
				}
			}
			if (-1 != lct)
			{
				if (0 != lct)
				{
					new_lcl_collseq = ready_collseq(lct);
					if (0 == new_lcl_collseq)
					{
						va_end(var);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
					}
					TREF(local_collseq) = new_lcl_collseq;
				} else
				{
					TREF(local_collseq) = 0;
					if (NULL != TREF(lcl_coll_xform_buff))
					{
						assert(0 < TREF(max_lcl_coll_xform_bufsiz));
						free(TREF(lcl_coll_xform_buff));
						TREF(lcl_coll_xform_buff) = NULL;
						TREF(max_lcl_coll_xform_bufsiz) = 0;
					}
				}
			}
			if (-1 != ncol)
				TREF(local_collseq_stdnull) = (ncol ? TRUE: FALSE);
			if (-1 != nct)
				TREF(local_coll_nums_as_strings) = (nct ? TRUE: FALSE);
			break;
		case VTK_PATLOAD:
			if (!load_pattern_table(parmblk.value->str.len, parmblk.value->str.addr))
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PATLOAD, 2, parmblk.value->str.len,
						parmblk.value->str.addr);
			}
			break;
		case VTK_NOUNDEF:
			undef_inhibit = TRUE;
			break;
		case VTK_UNDEF:
			undef_inhibit = FALSE;
			break;
		case VTK_TESTPOINT:
			testvalue = MV_FORCE_INT(parmblk.value);
			set_testpoint(testvalue);
			break;
		case VTK_ZDEFER:
			if (zdefactive)
			{
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZDEFACTIVE);
			}
			zdefactive = TRUE;
			tmpzdefbufsiz = MV_FORCE_INT(parmblk.value);
			if (1 == tmpzdefbufsiz)
				tmpzdefbufsiz = ZDEFDEF;
			else if (tmpzdefbufsiz < ZDEFMIN)
				tmpzdefbufsiz = ZDEFMIN;
			else if (tmpzdefbufsiz > ZDEFMAX)
				tmpzdefbufsiz = ZDEFMAX;
			zdefbufsiz = (unsigned short)tmpzdefbufsiz;
			break;
		case VTK_ZFLUSH:
			gvcmz_zflush();
			break;
		case VTK_TRACE :
			testvalue = MV_FORCE_INT(parmblk.value);
			if (testvalue)
			{
				if (2 > numarg)
				{
					va_end(var);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TRACEON);
				}
				arg = va_arg(var, mval *);
				MV_FORCE_STR(arg);
				turn_tracing_on(arg, FALSE, TRUE);
			} else
			{
				if (2 == numarg)
				{
					arg = va_arg(var, mval *);
					MV_FORCE_STR(arg);
					turn_tracing_off(arg);
				} else
					turn_tracing_off(NULL);
			}
			break;
		case VTK_ZDIR_FORM:			/* Vestigial remnant from VMS - to be removed in the future */
			break;
		case VTK_FILLFACTOR:
			testvalue = MV_FORCE_INT(parmblk.value);
			if (MAX_FILLFACTOR < testvalue)
				testvalue = MAX_FILLFACTOR;
			else if (MIN_FILLFACTOR > testvalue)
				testvalue = MIN_FILLFACTOR;
			gv_fillfactor = testvalue;
			break;
		case VTK_STPGCOL:
			INVOKE_STP_GCOL(INTCAST(stringpool.top - stringpool.free) + 1);/* Computation to avoid assert in stp_gcol */
			break;
		case VTK_LVGCOL:
			als_lvval_gc();
			break;
		case VTK_LVREHASH:
			/* This doesn't actually expand or contract the local variable hash table but does cause it to be
			 * rebuilt. Then we need to do the same sort of cleanup that add_hashtab_mname_symval does.
			 */
			/* Step 1: remember table we started with */
			table = &curr_symval->h_symtab;
			table_base_orig = table->base;
			table_size_orig = table->size;
			/* Step 2 - rebuild the local variable hash table */
			/* We'll do the base release once we do the reparations */
			DEFER_BASE_REL_HASHTAB(table, TRUE);
			expand_hashtab_mname(&curr_symval->h_symtab, curr_symval->h_symtab.size);
			/* Step 3 - repair the l_symtab entries on the stack from the rebuilt hash table */
			if (table_base_orig != curr_symval->h_symtab.base)
			{	/* Only needed if expansion was successful */
				als_lsymtab_repair(table, table_base_orig, table_size_orig);
				FREE_BASE_HASHTAB(table, table_base_orig);
			}
			DEFER_BASE_REL_HASHTAB(table, FALSE);
			break;
		case VTK_STORDUMP:
#			ifdef DEBUG
			if (gtm_white_box_test_case_enabled
				&& (WBTEST_HOLD_CRIT_TILL_LCKALERT == gtm_white_box_test_case_number))
			{	/* Hold crit for a long enough interval to generate lock alert which then does a continue_proc */
				grab_crit(gv_cur_region);
				icnt = TREF(continue_proc_cnt);
				DBGGSSHR((LOGFLAGS, "op_view: Pid %d, initial icnt: %d\n", process_id, icnt));
				for (lcnt = 0; (MUTEXLCKALERT_INTERVAL * 12) > lcnt; lcnt++)
				{	/* Poll for icnt to be increased - check every quarter second */
					SHORT_SLEEP(250);	/* Quarter second nap */
					if (TREF(continue_proc_cnt) > icnt)
						break;
					DBGGSSHR((LOGFLAGS, "op_view: loop: %d - continue_proc_cnt: %d\n", lcnt,
						  TREF(continue_proc_cnt)));
				}
				DBGGSSHR((LOGFLAGS, "op_view: pid %d, lcnt: %d, icnt: %d, continue_proc_cnt: %d\n",
					  process_id, lcnt, icnt, TREF(continue_proc_cnt)));
				rel_crit(gv_cur_region);
			} else		/* If we do the white box test, avoid the rest */
#			endif
			if (gtmDebugLevel)
			{	/* gtmdbglvl must be non-zero to have hope of printing a storage dump */
				dbgdmpenabled = (GDL_SmDump & gtmDebugLevel);
				gtmDebugLevel |= GDL_SmDump;		/* Turn on indicator to force print */
				printMallocDump();
				if (!dbgdmpenabled)
					gtmDebugLevel &= (~GDL_SmDump);	/* Shut indicator back off */
			}
			break;
		case VTK_LOGTPRESTART:
			/* The TPRESTART logging frequency can be specified through environment variable or
			 * through VIEW command. The default value for logging frequency is 1 if value is
			 * not specified in the view command and env variable is not defined. There are 4
			 * possible combinations which will determine the final value of logging frequency
			 * depending upon whether value is specified in the VIEW command or env variables
			 * is defined. These combinations are summarized in the following table.
			 *
			 *	value in	environment	Final
			 *	VIEW command 	variable	value
			 *	X		X		1
			 *	X		A		A
			 *	B		X		B
			 *	B		C		B
			 */
			if (!numarg)
			{
				if (!TREF(tprestart_syslog_delta))
					TREF(tprestart_syslog_delta) = 1;
			} else
			{
				TREF(tprestart_syslog_delta) = MV_FORCE_INT(parmblk.value);
				if (0 > TREF(tprestart_syslog_delta))
					TREF(tprestart_syslog_delta) = 0;
			}
			TREF(tp_restart_count) = 0;
			break;
		case VTK_NOLOGTPRESTART:
			TREF(tprestart_syslog_delta) = 0;
			break;
		case VTK_LOGNONTP:
			if (!numarg)
			{
				if (!TREF(nontprestart_log_delta))
					TREF(nontprestart_log_delta) = 1;
			} else
			{
				TREF(nontprestart_log_delta) = MV_FORCE_INT(parmblk.value);
				if (0 > TREF(nontprestart_log_delta))
					TREF(nontprestart_log_delta) = 0;
			}
			TREF(nontprestart_count) = 0;
			break;
		case VTK_NOLOGNONTP:
			TREF(nontprestart_log_delta) = 0;
			break;
		case VTK_LINK:
			init_relink_allowed(&parmblk.value->str);
			break;
#		ifdef DEBUG_ALIAS
		case VTK_LVMONOUT:
			als_lvmon_output();
			break;
		case VTK_LVMONSTART:
			lvmon_enabled = TRUE;	/* Enable lv_val monitoring */
			/* Clear any existing marks on all lv_vals */
			for (lvlsymtab = curr_symval; lvlsymtab; lvlsymtab = lvlsymtab->last_tab)
			{
				for (lvbp = curr_symval->lv_first_block; lvbp; lvbp = lvbp->next)
				{
					for (lvp = (lv_val *)LV_BLK_GET_BASE(lvbp), lvp_top = LV_BLK_GET_FREE(lvbp, lvp);
							lvp < lvp_top; lvp++)
					{
						assert(LV_IS_BASE_VAR(lv));
						lvp->lvmon_mark = FALSE;
					}
				}
			}
			break;
		case VTK_LVMONSTOP:
			als_lvmon_output();
			lvmon_enabled = FALSE;
			break;
#		endif
#		ifdef DEBUG
		case VTK_LVDMP:		/* Write partial lv_val into to output device */
			outval.mvtype = MV_STR;
			lv = (lv_val *)parmblk.value;
			util_out_print("", RESET);	/* Reset the buffer */
			util_out_print("LV: !AD  addr: 0x!XJ  mvtype: 0x!4XW  sign: !UB  exp: !UL  m[0]: !UL [0x!XL]  "
					"m[1]: !UL [0x!XL]  str.len: !UL  str.addr: 0x!XJ", SPRINT, arg->str.len, arg->str.addr,
					lv, lv->v.mvtype, lv->v.sgn, lv->v.e, lv->v.m[0], lv->v.m[0], lv->v.m[1], lv->v.m[1],
					lv->v.str.len, lv->v.str.addr);
			outval.str.addr = TREF(util_outptr);
			outval.str.len = STRLEN(TREF(util_outptr));
			op_write(&outval);
			op_wteol(1);
			break;
#		endif
		case VTK_STATSHARE:
			if (!TREF(statshare_opted_in))
			{	/* Don't both to opt-in if already opted-in - it just confuses things */
				if (0 < dollar_tlevel)
				{	/* Can't do this in TP */
					va_end(var);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TPNOSTATSHARE);
				}
				gvcst_statshare_optin();
			}
			break;
		case VTK_NOSTATSHARE:
			if (TREF(statshare_opted_in))
			{	/* Don't both to opt-out if already opted-out - it just confuses things */
				if (0 < dollar_tlevel)
				{	/* Can't do this in TP */
					va_end(var);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TPNOSTATSHARE);
				}
				gvcst_statshare_optout();
			}
			break;
		case VTK_LVMON:
			if (0 < numarg)
			{	/* A variable list was supplied - Need a new table but first free any existing old table first */
				if (NULL != TREF(lvmon_vars_anchor))
					lvmon_release();			/* Table exists. Free allocated var names/values */
				TREF(lvmon_vars_count) = numarg;		/* Elements in this table */
				size = numarg * SIZEOF(lvmon_var);		/* Byte size of new table */
				lvmon_vars_base = TREF(lvmon_vars_anchor) = malloc(size);	/* Allocate new table */
				memset(lvmon_vars_base, 0, size);		/* Clear new table */
				for (acnt = numarg, lvmon_var_p = lvmon_vars_base;  0 < acnt; acnt--, lvmon_var_p++)
				{	/* Load up new table from args given */
					MV_FORCE_STR(arg);
					lvmon_var_p->lvmv.var_name.len = arg->str.len;
					lvmon_var_p->lvmv.var_name.addr = malloc(arg->str.len);
					memcpy(lvmon_var_p->lvmv.var_name.addr, arg->str.addr, arg->str.len);
					COMPUTE_HASH_MNAME(&lvmon_var_p->lvmv);	/* Set hash value for var name */
					if (1 < acnt)				/* If another var to fetch, do so */
						arg = va_arg(var, mval *);
				}
				TREF(lvmon_active) = TRUE;			/* We are active now */
			} else
			{
				if (NULL != TREF(lvmon_vars_anchor))
					lvmon_release();			/* No vars specified - free what have if any */
				TREF(lvmon_active) = FALSE;			/* No monitoring active now */
			}
			break;
		default:
			va_end(var);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VIEWCMD);
	}
	va_end(var);
	return;
}

/* Routine to locate the structure associated with local variable monitoring across given points in GTM code
 * and release all of:
 *   1. The malloc'd variable names.
 *   2. The malloc'd variable values.
 *   3. The array of lvmon_vars structures anchored at TREF(lvmon_var_anchor).
 */
STATICFNDEF void lvmon_release(void)
{
	int		acnt;
	lvmon_var	*lvmon_var_p, *lvmon_vars_base;
	lvmon_value_ent	*lvmon_val_ent_p;
	int		ecnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (acnt = TREF(lvmon_vars_count), lvmon_var_p = TREF(lvmon_vars_anchor);
	     0 < acnt;
	     acnt--, lvmon_var_p++)
	{
		assert(NULL != lvmon_var_p->lvmv.var_name.addr);
		if (NULL != lvmon_var_p->lvmv.var_name.addr)
		{
			free(lvmon_var_p->lvmv.var_name.addr);	/* Free var name */
			lvmon_var_p->lvmv.var_name.addr = NULL;
		}
		for (ecnt = MAX_LVMON_VALUES, lvmon_val_ent_p = &lvmon_var_p->values[0];
		     0 < ecnt;
		     ecnt--, lvmon_val_ent_p++)
		{
			assert(NULL != lvmon_val_ent_p->varvalue.addr);
			if (NULL != lvmon_val_ent_p->varvalue.addr)
			{
				free(lvmon_val_ent_p->varvalue.addr);
				lvmon_val_ent_p->varvalue.addr = NULL;
			}
		}
	}
	free(TREF(lvmon_vars_anchor));		/* Release the old table */
	TREF(lvmon_vars_anchor) = NULL;
}

STATICFNDEF void view_dbop(unsigned char keycode, viewparm *parmblkptr, mval *thirdarg)
{
	boolean_t		was_crit;
	gd_region		*reg, *r_top, *save_reg;
	jnlpool_addrs_ptr_t	save_jnlpool;
	int			icnt, lcnt, save_errno, status;
	int4			nbuffs;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			jnl_status, dummy_errno, wcsflu_parms;
	unix_db_info		*udi;

	if (NULL == gd_header)		/* Open gbldir */
		gvinit();
	save_reg = gv_cur_region;
	save_jnlpool = jnlpool;
	if (NULL == parmblkptr->gv_ptr)
	{	/* Operate on all regions */
		reg = gd_header->regions;
		r_top = reg + gd_header->n_regions - 1;
	} else	/* Operate on selected region */
		r_top = reg = parmblkptr->gv_ptr;
	for (; reg <= r_top; reg++)
	{
		if (IS_STATSDB_REG(reg))
			continue;	/* Skip statsdb regions for the VIEW command */
		if (!reg->open)
			gv_init_reg(reg, NULL);
		TP_CHANGE_REG(reg);
		/* note that the jnlpool needs to be initialized and validated for options which could write to the database
		 * so instance freeze can be honored.  No data is writtern just file header, etc. so OK on secondary
		 */
		switch(keycode)
		{
			case VTK_DBFLUSH:
				if (!reg->read_only)
				{
					JNLPOOL_INIT_IF_NEEDED(cs_addrs, cs_data, cs_addrs->nl, SCNDDBNOUPD_CHECK_FALSE);
					nbuffs = (NULL != thirdarg) ? MV_FORCE_INT(thirdarg) : cs_addrs->nl->wcs_active_lvl;
					JNL_ENSURE_OPEN_WCS_WTSTART(cs_addrs, reg, nbuffs, NULL, FALSE, dummy_errno);
				}
				break;
			case VTK_DBSYNC:
				if (!reg->read_only)
				{
					csa = cs_addrs;
					udi = FILE_INFO(reg);
					JNLPOOL_INIT_IF_NEEDED(cs_addrs, cs_data, cs_addrs->nl, SCNDDBNOUPD_CHECK_FALSE);
					DB_FSYNC(reg, udi, csa, db_fsync_in_prog, save_errno);
					if (0 != save_errno)
					{
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg),
								save_errno);
						save_errno = 0;
					}
				}
				break;
			case VTK_EPOCH:
			case VTK_FLUSH:
				if (!reg->read_only  && !FROZEN_CHILLED(cs_addrs))
				{
					JNLPOOL_INIT_IF_NEEDED(cs_addrs, cs_data, cs_addrs->nl, SCNDDBNOUPD_CHECK_FALSE);
					ENSURE_JNL_OPEN(cs_addrs, gv_cur_region);
					/* We should NOT invoke wcs_recover here because it's possible we are in the final retry
					 * of a TP transaction. In this case, we likely have pointers to non-dirty global buffers
					 * in our transaction histories. Doing cache recovery could dry clean most of these buffers.
					 * And that might cause us to get confused, attempt to restart, and incorrectly issue a
					 * TPFAIL error because we are already in the final retry. By passing the WCSFLU_IN_COMMIT
					 * bit, we instruct wcs_flu to avoid wcs_recover.
					 */
					wcsflu_parms = WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SPEEDUP_NOBEFORE;
					if (dollar_tlevel)
						wcsflu_parms |= WCSFLU_IN_COMMIT;
					wcs_flu(wcsflu_parms);
				}
				break;
			case VTK_GVSRESET:
				change_reg();
				if (!reg->read_only)
					CLRGVSTATS(cs_addrs);
				/* Reset process stats in either process-private storage or a shared statsDB record */
				memset((char *)cs_addrs->gvstats_rec_p, 0, SIZEOF(gvstats_rec_t));
				break;
			case VTK_JNLERROR:
				if (!reg->read_only)
				{
					csa = cs_addrs;
					csd = csa->hdr;
					if (JNL_ENABLED(csd))
					{
						was_crit = csa->now_crit;
						if (!was_crit)
							grab_crit(reg);
						if (JNL_ENABLED(csd))
							csa->jnl->error_reported = FALSE;
						if (!was_crit)
							rel_crit(reg);
					}
				}
				break;
			case VTK_JNLFLUSH:
				csa = cs_addrs;
				csd = csa->hdr;
				if (JNL_ENABLED(csd))
				{
					JNLPOOL_INIT_IF_NEEDED(cs_addrs, cs_data, cs_addrs->nl, SCNDDBNOUPD_CHECK_FALSE);
					was_crit = csa->now_crit;
					if (!was_crit)
						grab_crit(reg);
					if (JNL_ENABLED(csd))
					{
						jnl_status = jnl_ensure_open(reg, csa);
						if (0 == jnl_status)
						{
							jb = csa->jnl->jnl_buff;
							if (SS_NORMAL == (jnl_status = jnl_flush(reg)))
							{
								assert(jb->dskaddr == jb->freeaddr);
								assert(jb->freeaddr == jb->rsrv_freeaddr);
								jnl_fsync(reg, jb->dskaddr);
								assert(jb->freeaddr == jb->fsync_dskaddr);
							} else
							{
								send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2,
									JNL_LEN_STR(csd), ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error with journal flush during op_view"),
									jnl_status);
								assert(FALSE);
							}
						} else
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
									DB_LEN_STR(reg));
					}
					if (!was_crit)
						rel_crit(reg);
				}
				break;
			case VTK_POOLLIMIT:
				csa = cs_addrs;
				csd = csa->hdr;
				set_gbuff_limit(&csa, &csd, thirdarg);
				break;
		}
	}
	gv_cur_region = save_reg;
	change_reg();
	if (save_jnlpool && (save_jnlpool != jnlpool))
		jnlpool = save_jnlpool;
	return;
}
