/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_CTL_H
#define _REPL_CTL_H

enum
{
	JNL_FILE_UNREAD,
	JNL_FILE_OPEN,
	JNL_FILE_CLOSED,
	JNL_FILE_EMPTY
}; /* jnl_file_state */

typedef enum
{
	TR_NOT_FOUND,
	TR_FIND_ERR,
	TR_WILL_NOT_BE_FOUND,
	TR_FIND_WOULD_BLOCK,
	TR_FOUND
} tr_search_state_t;

typedef enum
{
	TR_LINEAR_SEARCH,
	TR_BINARY_SEARCH
} tr_search_method_t;

typedef struct {
	unsigned char	*recbuff; 	/* The journal record pointer */
	int		reclen;		/* The journal record length */
	uint4		recaddr;	/* On-disk journal record offset */
	uint4		readaddr;	/* Next on-disk read offset */
	uint4		buffremaining;	/* Remaining buffer space */
	unsigned char	*base;		/* Buffer */
} repl_buff_desc;

#define REPL_BLKSIZE(x)		((x)->fc->jfh->alignsize)

typedef struct {
	uint4		eof_addr;	/* On-disk last byte offset */
	jnl_file_header	*jfh;
	int		fd;
	gd_id		id;
} repl_file_control_t;

enum {
	REPL_MAINBUFF =	0,
	REPL_SCRATCHBUFF,
	REPL_NUMBUFF
};

typedef struct {
	int			buffindex;
	repl_buff_desc		buff[2]; /* Main buffer, and scratch */
	repl_file_control_t	*fc;
	struct repl_ctl_struct	*backctl; /* Back pointer to ctl element to
					   * which this buffer belongs */
} repl_buff_t;

/*
 * repl_ctl_element maintains information for reading from the journal
 * files. A list of these structures consists sub-lists, each sub-list
 * corresponding to a region. Each sub-list contains info about the various
 * generations of the journal file of particular region, the earliest
 * generation coming first while searching from the beginning of the list
 */
typedef struct repl_ctl_struct
{
	gd_region		*reg;
	repl_buff_t		*repl_buff;
	seq_num			min_seqno;	/* least JNL_SEQNO in this file */
	seq_num			max_seqno;	/* largest JNL_SEQNO in this file */
	uint4			min_seqno_dskaddr;
	uint4			max_seqno_dskaddr;
	seq_num			seqno;		/* Next read positioned at first
						 * jnl rec with JNL_SEQNO seqno */
	trans_num		tn; 		/* tn corresponding to seqno */
	int4			file_state; 	/* enum jnl_file_state */
	boolean_t		lookback;
	boolean_t		first_read_done;
	boolean_t		fh_read_done;
	boolean_t		read_complete;
	int4			jnl_fn_len;
	char			jnl_fn[JNL_NAME_SIZE];
	struct repl_ctl_struct	*prev;
	struct repl_ctl_struct	*next;
} repl_ctl_element;

typedef struct {
	seq_num		seqno;		/* the last sequence number seen in a block before linear search returns */
	seq_num		prev_seqno;	/* the one previous to the last one */
} tr_search_status_t;

#define JNL_BLK_DSKADDR(addr, blksize) \
	((ROUND_DOWN((addr), (blksize)) > JNL_FILE_FIRST_RECORD) ? \
	 	ROUND_DOWN((addr), (blksize)) : JNL_FILE_FIRST_RECORD)

int repl_ctl_close(repl_ctl_element *ctl);
int repl_ctl_create(repl_ctl_element **ctl, gd_region *reg, int jnl_fn_len,
	char *jnl_fn, boolean_t init);

#endif
