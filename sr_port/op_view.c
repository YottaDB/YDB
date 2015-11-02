/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
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
#include "dpgbldir.h"	/* for get_next_gdr() prototype */
#include "ast.h"
#include "wcs_flu.h"
#include "stringpool.h"
#include "gtmdbglvl.h"
#include "gtm_malloc.h"
#include "alias.h"
#include "fullbool.h"
#ifdef GTM_TRIGGER
#include "rtnhdr.h"		/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif

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
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	int4			zdir_form;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		badchar_inhibit;
GBLREF	int			gv_fillfactor;
GBLREF	symval			*curr_symval;
GBLREF	uint4			gtmDebugLevel;
GBLREF	boolean_t		lvmon_enabled;
GBLREF	spdesc			stringpool;

error_def(ERR_ACTRANGE);
error_def(ERR_COLLATIONUNDEF);
error_def(ERR_COLLDATAEXISTS);
error_def(ERR_INVZDIRFORM);
error_def(ERR_ISOLATIONSTSCHN);
error_def(ERR_JNLFLUSH);
error_def(ERR_PATLOAD);
error_def(ERR_PATTABNOTFND);
error_def(ERR_REQDVIEWPARM);
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

/* if changing noisolation status within TP and already referenced the global, then error */
#define SET_GVNH_NOISOLATION_STATUS(gvnh, status)							\
{													\
	GBLREF	uint4			dollar_tlevel;							\
													\
	if (!dollar_tlevel || gvnh->read_local_tn != local_tn || status == gvnh->noisolation)		\
		gvnh->noisolation = status;								\
	else												\
		rts_error(VARLSTCNT(6) ERR_ISOLATIONSTSCHN, 4, gvnh->gvname.var_name.len, 		\
			gvnh->gvname.var_name.addr, gvnh->noisolation, status);				\
}

void	op_view(UNIX_ONLY_COMMA(int numarg) mval *keyword, ...)
{
	int4			testvalue, tmpzdefbufsiz;
	uint4			jnl_status, dummy_errno;
	int			status;
	gd_region		*reg, *r_top, *save_reg;
	gv_namehead		*gvnh;
	mval			*arg, *nextarg, outval;
	mstr			tmpstr;
	va_list			var;
	viewparm		parmblk;
	viewtab_entry		*vtp;
	gd_addr			*addr_ptr;
	noisolation_element	*gvnh_entry;
	int			lct, ncol;
	collseq			*new_lcl_collseq;
	ht_ent_mname		*tabent, *topent;
	lv_val			*lv;
	symval			*cstab;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_buffer_ptr_t	jb;
	int			table_size_orig;
	ht_ent_mname		*table_base_orig;
	hash_table_mname	*table;
	boolean_t		dbgdmpenabled, was_crit;
	symval			*lvlsymtab;
	lv_blk			*lvbp;
	lv_val			*lvp, *lvp_top;
	VMS_ONLY(int		numarg;)

	static readonly char msg1[] = "Caution: Database Block Certification Has Been ";
	static readonly char msg2[] = "Disabled";
	static readonly char msg3[] = "Enabled";
	static readonly char lv_msg1[] =
		"Caution: GT.M reserved local variable string pointer duplicate check diagnostic has been";
	static readonly char upper[] = "UPPER";
	static readonly char lower[] = "LOWER";
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, keyword);
	VMS_ONLY(va_count(numarg));
		jnl_status = 0;
	if (numarg < 1)
		GTMASSERT;
	MV_FORCE_STR(keyword);
	numarg--;	/* remove keyword from count */
	if (0 < numarg)
	{
		arg = va_arg(var, mval *);
		MV_FORCE_STR(arg);
	} else
		arg = (mval *)NULL;
	vtp = viewkeys(&keyword->str);
	view_arg_convert(vtp, arg, &parmblk);
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
		case VTK_FLUSH:
			if (NULL == gd_header)		/* open gbldir */
				gvinit();
			save_reg = gv_cur_region;
			if (NULL == parmblk.gv_ptr)
			{	/* flush all regions */
				reg = gd_header->regions;
				r_top = reg + gd_header->n_regions - 1;
			} else	/* flush selected region */
				r_top = reg = parmblk.gv_ptr;
			for (;  reg <= r_top;  reg++)
			{
				if (!reg->open)
					gv_init_reg(reg);
				if (!reg->read_only)
				{
					gv_cur_region = reg;
					change_reg(); /* for jnl_ensure_open */
					ENSURE_JNL_OPEN(cs_addrs, gv_cur_region);
					wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
				}
			}
			gv_cur_region = save_reg;
			change_reg();
			break;
		case VTK_FULLBOOL:
			TREF(gtm_fullbool) = FULL_BOOL;
			break;
		case VTK_FULLBOOLWARN:
			TREF(gtm_fullbool) = FULL_BOOL_WARN;
			break;
		case VTK_NOFULLBOOL:
			TREF(gtm_fullbool) = GTM_BOOL;
			break;
		case VTK_GDSCERT0:
			outval.mvtype = MV_STR;
			op_wteol(1);
			WRITE_LITERAL(msg1);
			certify_all_blocks = FALSE;
			WRITE_LITERAL(msg2);
			op_wteol(1);
			break;
		case VTK_GDSCERT1:
			outval.mvtype = MV_STR;
			op_wteol(1);
			WRITE_LITERAL(msg1);
			certify_all_blocks = TRUE;
			WRITE_LITERAL(msg3);
			op_wteol(1);
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
		case VTK_LVDUPCHECK:
			/* This feature is not needed any more. This is a noop now */
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
#		ifndef VMS
		case VTK_JNLERROR:
			TREF(error_on_jnl_file_lost) = MV_FORCE_INT(parmblk.value);
			if (MAX_JNL_FILE_LOST_OPT < TREF(error_on_jnl_file_lost))
				TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_TURN_OFF;
			if (NULL == gd_header)		/* open gbldir */
				gvinit();
			save_reg = gv_cur_region;
			/* change all regions */
			reg = gd_header->regions;
			r_top = reg + gd_header->n_regions - 1;
			for (;  reg <= r_top;  reg++)
			{
				if (!reg->open)
					gv_init_reg(reg);
				if (!reg->read_only)
				{
					gv_cur_region = reg;
					change_reg();
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
			}
			break;
#		endif
		case VTK_JNLFLUSH:
			if (NULL == gd_header)		/* open gbldir */
				gvinit();
			save_reg = gv_cur_region;
			if (NULL == parmblk.gv_ptr)
			{	/* flush all journal files */
				reg = gd_header->regions;
				r_top = reg + gd_header->n_regions - 1;
			} else	/* flush journal for selected region */
				r_top = reg = parmblk.gv_ptr;
			for (;  reg <= r_top;  reg++)
			{
				if (!reg->open)
					gv_init_reg(reg);
				gv_cur_region = reg;
				change_reg();
				csa = cs_addrs;
				csd = csa->hdr;
				if (JNL_ENABLED(csd))
				{
					was_crit = csa->now_crit;
					if (!was_crit)
						grab_crit(reg);
					if (JNL_ENABLED(csd))
					{
						jnl_status = jnl_ensure_open();
						if (0 == jnl_status)
						{
							jb = csa->jnl->jnl_buff;
							if (SS_NORMAL == (jnl_status = jnl_flush(reg)))
							{
								assert(jb->dskaddr == jb->freeaddr);
								UNIX_ONLY(jnl_fsync(reg, jb->dskaddr));
								UNIX_ONLY(assert(jb->freeaddr == jb->fsync_dskaddr));
							} else
							{
								send_msg(VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
									ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error with journal flush during op_view"),
									jnl_status);
								assert(FALSE);
							}
						} else
							send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
					}
					if (!was_crit)
						rel_crit(reg);
				}
			}
			gv_cur_region = save_reg;
			change_reg();
			break;
		case VTK_JNLWAIT:
			/* go through all regions that could have possibly been open across all global directories */
			for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
				for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions;  reg < r_top;  reg++)
					jnl_wait(reg);
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
			assert(NOISOLATION_PLUS == parmblk.ni_list.type || NOISOLATION_MINUS == parmblk.ni_list.type);
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
				rts_error(VARLSTCNT(4) ERR_PATTABNOTFND, 2, parmblk.value->str.len, parmblk.value->str.addr);
			}
			break;
		case VTK_RESETGVSTATS:
			for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
			{
				for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
				{
					if (!reg->open)
						continue;
					switch(reg->dyn.addr->acc_meth)
					{
						case dba_mm:
						case dba_bg:
							csa = &FILE_INFO(reg)->s_addrs;
							memset(&csa->gvstats_rec, 0, SIZEOF(gvstats_rec_t));
							break;
						case dba_cm:
						case dba_usr:
							break;
						default:
							GTMASSERT;
							break;
					}
				}
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
				rts_error(VARLSTCNT(1) ERR_YDIRTSZ);
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
			op_gvname(VARLSTCNT(1) parmblk.value);
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
				rts_error(VARLSTCNT(1) ERR_REQDVIEWPARM);
			}
			nextarg = NULL;
			ncol = -1;
			if (numarg > 1)
			{
				nextarg = va_arg(var, mval *);
				ncol = MV_FORCE_INT(nextarg);
			}
			if ((-1 == lct) && (-1 == ncol))
				break;
			/* lct = -1 indicates user wants to change only ncol, not lct */
			if (-1 != lct)
			{
				if (lct < MIN_COLLTYPE || lct > MAX_COLLTYPE)
				{
					va_end(var);
					rts_error(VARLSTCNT(3) ERR_ACTRANGE, 1, lct);
				}
			}
			/* at this point, verify that there is no local data with subscripts */
			for (cstab = curr_symval;  cstab;  cstab = cstab->last_tab)
			{
				assert(cstab->h_symtab.top == cstab->h_symtab.base + cstab->h_symtab.size);
				for (tabent = cstab->h_symtab.base, topent = cstab->h_symtab.top;
				     tabent < topent; tabent++)
				{
					if (HTENT_VALID_MNAME(tabent, lv_val, lv))
					{
						assert(LV_IS_BASE_VAR(lv));
						if (lv && LV_HAS_CHILD(lv))
						{
							va_end(var);
							rts_error(VARLSTCNT(1) ERR_COLLDATAEXISTS);
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
						rts_error(VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
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
			break;
		case VTK_PATLOAD:
			if (!load_pattern_table(parmblk.value->str.len, parmblk.value->str.addr))
			{
				va_end(var);
				rts_error(VARLSTCNT(4) ERR_PATLOAD, 2, parmblk.value->str.len, parmblk.value->str.addr);
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
				rts_error(VARLSTCNT(1) ERR_ZDEFACTIVE);
			}
			zdefactive = TRUE;
			tmpzdefbufsiz = MV_FORCE_INT(parmblk.value);
			if (1 == tmpzdefbufsiz)
				tmpzdefbufsiz = ZDEFDEF;
			else  if (tmpzdefbufsiz < ZDEFMIN)
				tmpzdefbufsiz = ZDEFMIN;
			else  if (tmpzdefbufsiz > ZDEFMAX)
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
					rts_error(VARLSTCNT(1) ERR_TRACEON);
				}
				arg = va_arg(var, mval *);
				MV_FORCE_STR(arg);
				turn_tracing_on(arg);
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
		case VTK_ZDIR_FORM:
			VMS_ONLY(
				if (NULL != arg)
				{
					testvalue = MV_FORCE_INT(parmblk.value);
					if (!IS_VALID_ZDIR_FORM(testvalue))
					{
						va_end(var);
						rts_error(VARLSTCNT(3) ERR_INVZDIRFORM, 1, testvalue);
					}
				} else
					testvalue = ZDIR_FORM_FULLPATH;
				zdir_form = testvalue;
			)
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
			INVOKE_STP_GCOL(INTCAST(stringpool.top - stringpool.free) + 1);/* computation to avoid assert in stp_gcol */
			break;
		case VTK_LVGCOL:
			als_lvval_gc();
			break;
		case VTK_LVREHASH:
			/* This doesn't actually expand or contract the local variable hash table but does cause it to be
			 * rebuilt. Then we need to do the same sort of cleanup that add_hashtab_mname_symval does. */
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
			if (gtmDebugLevel)
			{	/* gtmdbglvl must be non-zero to have hope of printing a storage dump */
				dbgdmpenabled = (GDL_SmDump & gtmDebugLevel);
				gtmDebugLevel |= GDL_SmDump;		/* Turn on indicator to force print */
				printMallocDump();
				if (!dbgdmpenabled)
					gtmDebugLevel &= (~GDL_SmDump);	/* Shut indicator back off */
			}
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
		default:
			va_end(var);
			rts_error(VARLSTCNT(1) ERR_VIEWCMD);
	}
	va_end(var);
	return;
}
