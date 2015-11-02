/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef REPL_SORT_TR_BUFF_H
#define REPL_SORT_TR_BUFF_H

#ifdef DEBUG
# define DBG_VERIFY_TR_BUFF_SORTED(tr_buff, tr_bufflen)								\
{														\
	uchar_ptr_t		tb;										\
	jnl_record		*rec;										\
	int			tlen;										\
	uint4			reclen, prev_updnum = 0;							\
	enum jnl_record_type	rectype;									\
	jrec_prefix		*prefix;									\
	boolean_t		already_sorted = TRUE;								\
														\
	tb = tr_buff;												\
	tlen = tr_bufflen;											\
	while(JREC_PREFIX_SIZE <= tlen)										\
	{													\
		prefix = (jrec_prefix *)tb;									\
		rectype = (enum jnl_record_type)prefix->jrec_type;	\
		reclen = prefix->forwptr;									\
		rec = (jnl_record *)(tb);									\
		if (JRT_TCOM != rectype)									\
		{												\
			already_sorted = (already_sorted && (prev_updnum <= rec->jrec_set_kill.update_num));	\
			prev_updnum = rec->jrec_set_kill.update_num;						\
			assert(already_sorted);									\
		}												\
		tb += reclen;											\
		tlen -= reclen;											\
	}													\
}
#else
# define DBG_VERIFY_TR_BUFF_SORTED(tr_buff, tr_bufflen)
#endif

void repl_sort_tr_buff(uchar_ptr_t tr_buff, uint4 tr_bufflen);

typedef struct
{
	long		working_offset,
			end;
} reg_jrec_info_t;

#endif
