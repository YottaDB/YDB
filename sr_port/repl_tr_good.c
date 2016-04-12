/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "repl_tr_good.h"

boolean_t repl_tr_good(uchar_ptr_t tr, int tr_len, seq_num seqno)
{ /* verify that the transaction corresponds to seqno and is well formed */

	boolean_t		first_rec;
	jnl_tm_t		time;
	enum jnl_record_type	rectype;
	int			reclen;
	jnl_record		*jrec;

	for (first_rec = TRUE, time = 0, jrec = (jnl_record *)tr;
	     tr_len > 0;
	     tr_len -= reclen, jrec = (jnl_record *)((uchar_ptr_t)jrec + reclen))
	{
		rectype = (enum jnl_record_type)jrec->prefix.jrec_type;
		reclen  = jrec->prefix.forwptr;
		if (  !IS_VALID_RECTYPE(jrec)
		    || !IS_REPLICATED(rectype)
		    || (reclen < MIN_JNLREC_SIZE || reclen > tr_len)
		    || !IS_VALID_LINKS(jrec)
		    || !IS_VALID_SUFFIX(jrec))
		{
			assert(FALSE);
			return FALSE; /* malformed record */
		}
		if (first_rec)
		{
			if (IS_FENCED(rectype))
			{
				if (!IS_TUPD(rectype) && !IS_FUPD(rectype))
				{
					assert(FALSE);
					return FALSE;
				}
				if (reclen >= tr_len) /* incomplete transaction */
				{
					assert(FALSE);
					return FALSE;
				}
			} else
			{
				if (!IS_SET_KILL_ZKILL_ZTRIG(rectype) && JRT_NULL != rectype)
				{
					assert(FALSE);
					return FALSE;
				}
				if (reclen != tr_len) /* should have been the only record in the transcation */
				{
					assert(FALSE);
					return FALSE;
				}
			}
			first_rec = FALSE;
			time = jrec->prefix.time;
		} else
		{
			if (!IS_FENCED(rectype)) /* records following first must be part of a transaction */
			{
				assert(FALSE);
				return FALSE;
			}
			if (!IS_COM(rectype) && reclen == tr_len) /* incomplete */
			{
				assert(FALSE);
				return FALSE;
			}
			if (time != jrec->prefix.time)
			{
				assert(FALSE);
				return FALSE;
			}
		}
		if (seqno != GET_JNL_SEQNO(jrec))
		{
			assert(FALSE);
			return FALSE;
		}
		/* For TP, if we knew the global mapping, we could check that
		 * a. records belonging to a region have the same TN and pini_addr
		 * b. number of participants is correct
		 */
	}
	return (!first_rec);
}
