/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_ctype.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "error.h"
#include "repl_sem.h"
#include "gtmimagename.h"
#include "hashtab_str.h"
#include "eintr_wrappers.h"
#include "gtmmsg.h"
#include "anticipatory_freeze.h"
#ifdef DEBUG
#include "dpgbldir.h"
#include "is_proc_alive.h"
#endif

#define MAX_TAG_LEN				128	/* Maximum size of an error mnemonic */
#define MAX_READ_SZ				1024	/* Mnemonic + flags shouldn't exceed this limit */
#define COMMENT_DELIMITER			';'
#define NEWLINE					0x0A
#define EOL_REACHED				(char *)(-1)
#define EOF_REACHED				(char *)(-2)

#define EXHAUST_CURRENT_LINE(BUFF, HANDLE, FGETS_RC)				\
{										\
	assert(NEWLINE != BUFF[STRLEN(BUFF) - 1]);				\
	while (TRUE)								\
	{									\
		FGETS_FILE(BUFF, MAX_READ_SZ, HANDLE, FGETS_RC);		\
		if ((NULL == FGETS_RC) || NEWLINE == BUFF[STRLEN(BUFF) - 1])	\
			break;							\
	}									\
}

error_def(ERR_ASSERT);
error_def(ERR_CUSTERRNOTFND);
error_def(ERR_CUSTERRSYNTAX);
error_def(ERR_CUSTOMFILOPERR);
error_def(ERR_DSKSPCAVAILABLE);
error_def(ERR_ENOSPCQIODEFER);
error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_TEXT);
error_def(ERR_INSTFRZDEFER);

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
#ifdef DEBUG
GBLREF	uint4			process_id;
GBLREF	volatile boolean_t	timer_in_handler;
#endif

/* Typically prototypes are included in the header file. But, in this case the static function - get_mnemonic_offset - has the
 * hash_table_str as one of the function parameters which means all the files which includes anticipatory_freeze.h needs to include
 * hashtab_str.h and since there are lot of such C files, we chose to define static function prototypes in the C file itself.
 */
STATICFNDCL char	*scan_space(FILE *handle, char *buff, char *buffptr, char *buff_top);
STATICFNDCL int		get_mnemonic_offset(hash_table_str **err_hashtab, char *mnemonic_buf, int mnemonic_len);

/* Scan through whitespace in the current buffer (read more if required) */
STATICFNDEF char 	*scan_space(FILE *handle, char *buff, char *buffptr, char *buff_top)
{
	char		*fgets_rc;

	while (TRUE)
	{
		for (; (buffptr < buff_top) && (ISSPACE_ASCII(*buffptr)); buffptr++)
			;
		if (buffptr < buff_top)
			return buffptr;	/* first non-whitespace character */
		if (NEWLINE == *(buffptr - 1))
			return EOL_REACHED;
		/* current buffer is exhausted and we haven't seen a newline; read more */
		FGETS_FILE(buff, MAX_READ_SZ, handle, fgets_rc);
		if (NULL == fgets_rc)
			break;
		buffptr = buff;
		buff_top = buffptr + STRLEN(buff);
	}
	return EOF_REACHED;
}

STATICFNDEF int		get_mnemonic_offset(hash_table_str **err_hashtab, char *mnemonic_buf, int mnemonic_len)
{
	const err_msg			*msg_beg, *msg_top;
	hash_table_str			*tmp_err_hashtab;
	ht_ent_str			*err_htent;
	stringkey			key;
	err_msg				*msg_info;
	boolean_t			added;
	DEBUG_ONLY(int			idx;)

	msg_beg = merrors_ctl.fst_msg;
	msg_top = msg_beg + merrors_ctl.msg_cnt;
	assert('\0' == mnemonic_buf[mnemonic_len]);
	if (NULL == (tmp_err_hashtab = *err_hashtab))
	{	/* create and populate hash-table for future lookups */
		tmp_err_hashtab = (hash_table_str *)malloc(SIZEOF(hash_table_str));
		DEBUG_ONLY(tmp_err_hashtab->base = NULL);
		init_hashtab_str(tmp_err_hashtab, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
		assert(tmp_err_hashtab->base);
		for (msg_info = (err_msg *)msg_beg; msg_info < msg_top; msg_info++)
		{
			key.str.addr = msg_info->tag;
			key.str.len = STRLEN(msg_info->tag);
			COMPUTE_HASH_STR(&key);
			added = add_hashtab_str(tmp_err_hashtab, &key, msg_info, &err_htent);
			assert(added);
			assert(err_htent->value);
			assert(msg_info->tag == ((err_msg *)(err_htent->value))->tag);
		}
		*err_hashtab = tmp_err_hashtab;
	}
	assert(NULL != tmp_err_hashtab);
	/* lookup for the mnemonic */
	key.str.addr = mnemonic_buf;
	key.str.len = mnemonic_len;
	COMPUTE_HASH_STR(&key);
	if (NULL == (err_htent = lookup_hashtab_str(tmp_err_hashtab, &key)))
		return -1;
	msg_info = (err_msg *)(err_htent->value);
	assert(msg_info >= msg_beg && msg_info < msg_top);
	return msg_info - msg_beg;
}

/* Determine whether a given msg_id qualifies for an anticipatory freeze or not */
boolean_t	is_anticipatory_freeze_needed(sgmnt_addrs *csa, int msg_id)
{
	const err_ctl		*ctl;
	int			idx;

	assert(NULL != jnlpool.jnlpool_ctl);
	/* Certain error messages should NOT trigger a freeze even if they are so configured in the custom errors file as they might
	 * result in instance freezes that can be set indefinitely. Currently, we know of at least 3 such messages:
	 * 1. ENOSPCQIODEFER and INSTFRZDEFER : To ensure we don't set anticipatory freeze if we don't/can't hold crit
	 *					(due to possible deadlock)
	 * 2. DSKSPCAVAILABLE : To ensure we don't set anticipatory freeze if the disk space becomes available after an initial
	 *			lack of space.
	 * These messages have csa == NULL so they are guarranteed to not trigger a freeze.
	 */

	assert(((ERR_ENOSPCQIODEFER != msg_id) && (ERR_DSKSPCAVAILABLE != msg_id) && (ERR_INSTFRZDEFER != msg_id))
	       || (NULL == csa));
	if (!csa || !csa->nl || !csa->hdr || !csa->hdr->freeze_on_fail)
		return FALSE;
	ctl = err_check(msg_id);
	if (NULL != ctl)
	{
		GET_MSG_IDX(msg_id, ctl, idx);
		assert(idx < ARRAYSIZE(jnlpool_ctl->merrors_array));
		if (jnlpool_ctl->merrors_array[idx] & AFREEZE_MASK)
			return TRUE;
	}
	return FALSE;
}

/* set the anticipatory freeze in the journal pool */
void		set_anticipatory_freeze(sgmnt_addrs *csa, int msg_id)
{
	boolean_t			was_crit;
	sgmnt_addrs			*repl_csa;
	const err_msg			*msginfo;
#	ifdef DEBUG
	qw_off_t			write_addr;
	uint4				write;
#	endif

	assert(is_anticipatory_freeze_needed(csa, msg_id));
	DEBUG_ONLY(
		write_addr = jnlpool_ctl->write_addr;
		write = jnlpool_ctl->write;
	)
	assert(write == write_addr % jnlpool_ctl->jnlpool_size);
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	assert(NULL != repl_csa);
	was_crit = repl_csa->now_crit;
	if (!was_crit)
	{
		if (csa->now_crit)
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
		else if (FALSE == grab_lock(jnlpool.jnlpool_dummy_reg, FALSE, GRAB_LOCK_ONLY))
		{
			MSGID_TO_ERRMSG(msg_id, msginfo);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INSTFRZDEFER, 4, LEN_AND_STR(msginfo->tag),
				     REG_LEN_STR(csa->region));
			return;
		}
	}
	/* Now that we hold necessary locks, set the freeze and the comment field */
	jnlpool.jnlpool_ctl->freeze = TRUE;
	GENERATE_INST_FROZEN_COMMENT(jnlpool.jnlpool_ctl->freeze_comment, SIZEOF(jnlpool.jnlpool_ctl->freeze_comment), msg_id);
	/* TODO : Do we need a SHM_WRITE_MEMORY_BARRIER ? */
	if (!was_crit)
		rel_lock(jnlpool.jnlpool_dummy_reg);
}

/* initialize jnlpool_ctl->merrors_array to set up the list of errors that should trigger anticipatory freeze errors */
boolean_t		init_anticipatory_freeze_errors()
{
	int				idx, save_errno, status, mnemonic_len, offset, line_no;
	FILE				*handle;
	char				*fgets_rc;
	char				buff[MAX_READ_SZ], mnemonic_buf[MAX_TAG_LEN];
	char				*buffptr, *buff_top, *errptr, *errptr_top;
	mstr				custom_err_file;
	hash_table_str			*err_hashtab = NULL;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* TODO : Currently, we process errors that belong to merrors[] as those are the ones related to database/journal. Need
	 * to check if cmerrors/cmierrors also need to be included in this list or not.
	 */
	assert(IS_MUPIP_IMAGE); 				/* is_src_server is not initialized at this point */
	assert(jnlpool_ctl && !jnlpool_ctl->pool_initialized);	/* should be invoked BEFORE the journal pool is fully-initialized */
	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);		/* should hold journal pool access control semaphore */
	/* Now, read the custom errors file and populate the journal pool */
	custom_err_file = TREF(gtm_custom_errors);
	handle = Fopen(custom_err_file.addr, "r");
	if (NULL == handle)
	{
		save_errno = errno;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CUSTOMFILOPERR, 4, LEN_AND_LIT("fopen"), custom_err_file.len,
				custom_err_file.addr, save_errno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CUSTOMFILOPERR, 4, LEN_AND_LIT("fopen"), custom_err_file.len,
				custom_err_file.addr, save_errno);
		return FALSE;
	}
	line_no = 0;
	/* The code below parses a custom errors file in the following format.
	 *
	 * file		::= line*
	 * line		::= mnemonic SPACE* comment? EOL |
	 * 		    comment EOL
	 * mnemonic	::= ALNUM+
	 * comment	::= COMMENT_DELIMITER ANY*
	 *
	 * SPACE		::= any ASCII white space character
	 * COMMENT_DELIMITER	::= ';'
	 * ANY			::= any ASCII character except end of line
	 * EOL			::= ASCII end of line character
	 * ALNUM		::= any ASCII alphanumeric character
	 *
	 * NOTES:
	 *	"*" denotes zero-or-more of the previous item
	 *	"?" denotes zero-or-one of the previous item
	 *	"+" denotes one-or-more of the previous item
	 *	"|" denotes multiple alternatives
	 * 	The mnemonic must match an entry in the GT.M error message list.
	 *	Anything between the COMMENT_DELIMITER and EOL is ignored.
	 *	Each iteration of the loop parses one line.
	 */
	while (TRUE)
	{
		FGETS_FILE(buff, MAX_READ_SZ, handle, fgets_rc);
		line_no++;
		if (NULL == fgets_rc)
			break;
		buffptr = buff;
		buff_top = buffptr + STRLEN(buff);
		errptr = &mnemonic_buf[0];
		errptr_top = errptr + MAX_TAG_LEN;
		/* The first character has to be alpha-numeric or a comment */
		if (!ISALNUM_ASCII(*buffptr) && (COMMENT_DELIMITER != *buffptr))
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_CUSTERRSYNTAX, 3, custom_err_file.len, custom_err_file.addr,
					line_no, ERR_TEXT, 2,
					LEN_AND_LIT("First character should be comment (;) or alpha numeric"));
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_CUSTERRSYNTAX, 3, custom_err_file.len, custom_err_file.addr,
					line_no, ERR_TEXT, 2,
					LEN_AND_LIT("First character should be comment (;) or alpha numeric"));
			return FALSE;
		}
		while (ISALNUM_ASCII(*buffptr))
		{
			*errptr++ = *buffptr++;
			if (errptr > errptr_top)
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_CUSTERRSYNTAX, 3, custom_err_file.len,
						custom_err_file.addr, line_no, ERR_TEXT, 2, LEN_AND_LIT("Mnemonic too long"));
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_CUSTERRSYNTAX, 3, custom_err_file.len,
						custom_err_file.addr, line_no, ERR_TEXT, 2, LEN_AND_LIT("Mnemonic too long"));
				return FALSE;
			}
			assert(buffptr < buff_top); /* errptr > errptr_top should fail before this */
		}
		*errptr = '\0';
		if (0 < (mnemonic_len = (errptr - &mnemonic_buf[0])))
		{	/* Non-empty error mnemonic found; look it up */
			if (-1 == (offset = get_mnemonic_offset(&err_hashtab, mnemonic_buf, mnemonic_len)))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CUSTERRNOTFND, 2, mnemonic_len, mnemonic_buf);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CUSTERRNOTFND, 2, mnemonic_len, mnemonic_buf);
				return FALSE;
			}
			jnlpool_ctl->merrors_array[offset] |= AFREEZE_MASK; /* duplicate entries are not considered an error */
		}
		assert(ISSPACE_ASCII(*buffptr) || (COMMENT_DELIMITER == *buffptr));
		if (EOL_REACHED == (buffptr = scan_space(handle, buff, buffptr, buff_top)))
			continue;
		else if (EOF_REACHED == buffptr)
			break;
		assert(buffptr < buff_top);
		if (COMMENT_DELIMITER != *buffptr)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_CUSTERRSYNTAX, 3, custom_err_file.len, custom_err_file.addr,
					line_no, ERR_TEXT, 2, LEN_AND_LIT("Unexpected character found after mnemonic"));
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_CUSTERRSYNTAX, 3, custom_err_file.len, custom_err_file.addr,
					line_no, ERR_TEXT, 2, LEN_AND_LIT("Unexpected character found after mnemonic"));
			return FALSE;
		}
		/* Need to ignore the rest of the current buffer and exhaust the current line */
		if (NEWLINE != *(buff_top - 1))
			EXHAUST_CURRENT_LINE(buff, handle, fgets_rc);
	}
	if (err_hashtab)
	{
		free_hashtab_str(err_hashtab);
		free(err_hashtab);
	}
	if (!feof(handle))
	{
		save_errno = errno;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CUSTOMFILOPERR, 4, LEN_AND_LIT("fgets"), custom_err_file.len,
				custom_err_file.addr, save_errno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CUSTOMFILOPERR, 4, LEN_AND_LIT("fgets"), custom_err_file.len,
				custom_err_file.addr, save_errno);
		return FALSE;
	}
	FCLOSE(handle, status);
	if (SS_NORMAL != status)
	{
		save_errno = errno;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CUSTOMFILOPERR, 4, LEN_AND_LIT("fclose"), custom_err_file.len,
				custom_err_file.addr, save_errno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CUSTOMFILOPERR, 4, LEN_AND_LIT("fclose"), custom_err_file.len,
				custom_err_file.addr, save_errno);
		return FALSE;
	}
	jnlpool_ctl->instfreeze_environ_inited = TRUE;
	return TRUE;
}

#ifdef DEBUG
void clear_fake_enospc_if_master_dead(void)
{
	gd_addr				*addr_ptr;
	gd_region			*r_top, *r_local;
	sgmnt_addrs			*csa;

	if((jnlpool_ctl->jnlpool_creator_pid != process_id) && !is_proc_alive(jnlpool_ctl->jnlpool_creator_pid, 0))
	{
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
			{
				if ((dba_bg != r_local->dyn.addr->acc_meth) && (dba_mm != r_local->dyn.addr->acc_meth))
					continue;
				csa = REG2CSA(r_local);
				if ((NULL != csa) && (NULL != csa->nl))
					if (csa->nl->fake_db_enospc || csa->nl->fake_jnl_enospc)
					{
						csa->nl->fake_db_enospc = FALSE;
						csa->nl->fake_jnl_enospc = FALSE;
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT,
							     2, LEN_AND_LIT("Resetting fake_db_enospc and fake_jnl_enospc because "
									    "fake ENOSPC master is dead"));
					}
			}
		}
	}
}
#endif
