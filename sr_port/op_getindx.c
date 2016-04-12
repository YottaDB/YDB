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

#include "lv_val.h"
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "undx.h"
#include "mvalconv.h"

#define IS_INTEGER 0

GBLREF bool		undef_inhibit;

LITREF mval		literal_null ;

error_def(ERR_UNDEF);
error_def(ERR_LVNULLSUBS);

lv_val	*op_getindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{
	VMS_ONLY(int		argcnt;)
	boolean_t		is_canonical;
	int			arg1;
	int                     length;
	mval			*key;
	mval			tmp_sbs;
	lvTree			*lvt;
	lvTreeNode		*lv, *parent;
	unsigned char		buff[512], *end;
	va_list			var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt));
	lv = (lvTreeNode *)start;
	arg1 = --argcnt;
	while (lv && (0 < argcnt--))
	{
		key = va_arg(var, mval *);
		lvt = LV_GET_CHILD(lv);
		if (NULL == lvt)
		{
			lv = NULL;
			break;
		}
		MV_FORCE_DEFINED(key);
		is_canonical = MV_IS_CANONICAL(key);
		if (!is_canonical)
		{
			assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
			assert(MV_IS_STRING(key));
			if ((0 == key->str.len) && (LVNULLSUBS_NEVER == TREF(lv_null_subs)))
			{
				va_end(var);
				rts_error(VARLSTCNT(1) ERR_LVNULLSUBS);
			}
			if (TREF(local_collseq))
			{
				ALLOC_XFORM_BUFF(key->str.len);
				tmp_sbs.mvtype = MV_STR;
				tmp_sbs.str.len = TREF(max_lcl_coll_xform_bufsiz);
				assert(NULL != TREF(lcl_coll_xform_buff));
				tmp_sbs.str.addr = TREF(lcl_coll_xform_buff);
				do_xform(TREF(local_collseq), XFORM, &key->str, &tmp_sbs.str, &length);
				tmp_sbs.str.len = length;
				s2pool(&(tmp_sbs.str));
				key = &tmp_sbs;
			}
			lv = lvAvlTreeLookupStr(lvt, key, &parent);
		} else
		{	/* Need to set canonical bit before calling tree search functions.
			 * But input mval could be read-only so cannot modify that even if temporarily.
			 * So take a copy of the mval and modify that instead.
			 */
			tmp_sbs = *key;
			key = &tmp_sbs;
			MV_FORCE_NUM(key);
			TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(key);	/* used by the lvAvlTreeLookup* functions below */
			if (MVTYPE_IS_INT(tmp_sbs.mvtype))
				lv = lvAvlTreeLookupInt(lvt, key, &parent);
			else
				lv = lvAvlTreeLookupNum(lvt, key, &parent);
		}
	}
	va_end(var);
	if (!lv || !LV_IS_VAL_DEFINED(lv))
	{
		if (undef_inhibit)
			lv = (lvTreeNode *)&literal_null;
		else
		{
			VAR_START(var, start);
			end = undx(start, var, arg1, buff, SIZEOF(buff));
			va_end(var);
			rts_error(VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
		}
	}
	return (lv_val *)lv;
}
