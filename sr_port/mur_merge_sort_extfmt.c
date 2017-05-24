/****************************************************************
 *								*
 * Copyright (c) 2015-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtm_multi_proc.h"
#include "do_shmat.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "ipcrmid.h"
#include "interlock.h"
#include "gtm_rename.h"

GBLREF 	mur_gbls_t		murgbl;
GBLREF	mur_shm_hdr_t		*mur_shm_hdr;	/* Pointer to mur_forward-specific header in shared memory */
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	readonly char 		*ext_file_type[];

STATICDEF jext_heap_elem_t	*heap_array, *htmp_array;
STATICDEF int			heap_size;

error_def(ERR_FILECREATE);
error_def(ERR_SYSCALL);

/* ELEM1 and ELEM2 are of type "jnlext_multi_t *".
 * Returns zero     if ELEM1 < ELEM2
 * Returns non-zero if ELEM1 >= ELEM2
 */
#define	ELEM1_IS_GREATER(ELEM1, ELEM2)							\
	((ELEM1->time > ELEM2->time)							\
		|| ((ELEM1->time == ELEM2->time)					\
			&& ((ELEM1->token_seq.token > ELEM2->token_seq.token)		\
				|| (ELEM1->token_seq.token == ELEM2->token_seq.token)	\
					&& (ELEM1->update_num >= ELEM2->update_num))))

/* Adds an element into heap "heap_array". Updates "heap_size".
 * If element cannot be added right away (due to multi-region TP resolution) it will add element into a hashtable.
 */
void	mur_add_elem(jext_heap_elem_t *elem, boolean_t resolved)
{
	int			child, parent, rctl_index, num_more_reg;
	jext_heap_elem_t	*h0, *h1, *h2, tmp;
	jnlext_multi_t		*j1, *j2;
	token_num		token;
	reg_ctl_list		*rctl, *rctl_next;
	ht_ent_int8		*tabent;
	forw_multi_struct	*forw_multi, *cur_forw_multi, *prev_forw_multi;
	boolean_t		deleted;

	/* If this element corresponds to a multi-region TP transaction that needs to wait for other partners then
	 * add this to a wait-queue first. When all regions are resolved, this will then be moved over to the heap.
	 */
	j1 = elem->jext_rec;
#	ifdef MUR_DEBUG
	fprintf(stderr, "token = %llu : Region = %s : Resolved = %d\n",
		(long long)j1->token_seq.token, mur_ctl[elem->rctl_index].gd->rname, resolved);
#	endif
	if (!resolved && (num_more_reg = j1->num_more_reg))	/* caution: assignment */
	{	/* Reuse the essential components of the "forw_multi" structure used in forward phase */
		if (NULL == htmp_array)
			htmp_array = (jext_heap_elem_t *)malloc(murgbl.reg_total * SIZEOF(jext_heap_elem_t));
		rctl_index = elem->rctl_index;
		htmp_array[rctl_index] = *elem;
		rctl = &mur_ctl[rctl_index];
		token = j1->token_seq.token;
		MUR_FORW_TOKEN_LOOKUP(forw_multi, token, j1->time);
		if (NULL != forw_multi)
		{
			assert(forw_multi->time == j1->time);
			rctl_next = forw_multi->first_tp_rctl;
			assert(NULL != rctl_next);
			forw_multi->first_tp_rctl = rctl;
			rctl->next_tp_rctl = rctl_next;
			forw_multi->num_reg_seen_forward++;
			if (forw_multi->num_reg_seen_forward == num_more_reg)
			{	/* This multi-region TP token is completely resolved. Now insert into heap. */
				for ( ; NULL != rctl; rctl = rctl->next_tp_rctl)
				{
					rctl_index = rctl - &mur_ctl[0];
					elem = &htmp_array[rctl_index];
					mur_add_elem(elem, RESOLVED_TRUE);
				}
				tabent = forw_multi->u.tabent;
				assert(NULL != tabent);
				assert(tabent == lookup_hashtab_int8(&murgbl.forw_token_table, (gtm_uint64_t *)&token));
				if ((tabent->value == forw_multi) && (NULL == forw_multi->next))
				{	/* forw_multi is the ONLY element in the linked list so it is safe to delete the
					 * hashtable entry itself.
					 */
					deleted = delete_hashtab_int8(&murgbl.forw_token_table, &forw_multi->token);
					assert(deleted);
				} else
				{	/* delete "forw_multi" from the singly linked list */
					for (prev_forw_multi = NULL, cur_forw_multi = tabent->value;
						(NULL != cur_forw_multi);
							prev_forw_multi = cur_forw_multi, cur_forw_multi = cur_forw_multi->next)
					{
						if (cur_forw_multi == forw_multi)
						{
							assert(prev_forw_multi != forw_multi);
							if (NULL == prev_forw_multi)
								tabent->value = cur_forw_multi->next;
							else
								prev_forw_multi->next = cur_forw_multi->next;
							break;
						}
					}
					assert(NULL != cur_forw_multi);
				}
			}
		} else
		{
			forw_multi = (forw_multi_struct *)get_new_free_element(murgbl.forw_multi_list);
			forw_multi->token = token;
			forw_multi->time = j1->time;
			forw_multi->first_tp_rctl = rctl;
			rctl->next_tp_rctl = NULL;
			forw_multi->num_reg_seen_forward = 0;
			if (!add_hashtab_int8(&murgbl.forw_token_table, &token, forw_multi, &tabent))
			{	/* More than one TP transaction has the same token. This is possible in case of
				 * non-replication but we expect the rec_time to be different between the colliding
				 * transactions. In replication, we use jnl_seqno which should be unique. Assert that.
				 */
				assert(!mur_options.rollback);
				assert(NULL != tabent->value);
				forw_multi->next = (forw_multi_struct *)tabent->value;
				tabent->value = (char *)forw_multi;
			} else
				forw_multi->next = NULL;
			forw_multi->u.tabent = tabent;
		}
		return;
	}
	child = heap_size + 1;
	heap_size = child;
	assert(child <= murgbl.reg_total);
	assert(NULL != heap_array);
	h0 = &heap_array[0];
	h0[child] = *elem;
	do
	{
		parent = child / 2;
		if (!parent)	/* reached root of heap */
			break;
		h1 = h0 + parent;
		h2 = h0 + child;
		j1 = h1->jext_rec;
		j2 = h2->jext_rec;
		if (!ELEM1_IS_GREATER(j1, j2))
			break;	/* no need to go any higher in heap */
		/* Swap h1 and h2 and continue up the heap */
		tmp = *h1;
		*h1 = *h2;
		*h2 = tmp;
		child = parent;
	} while(TRUE);
	return;
}

/* Removes an element from heap "heap_array". Updates "heap_size" */
jext_heap_elem_t mur_remove_elem(void)
{
	int			parent, lchild, rchild, child;
	jext_heap_elem_t	ret, *h0, *h1, *h2, tmp;
	jnlext_multi_t		*j1, *j2;

	assert(NULL != heap_array);
	h0 = &heap_array[0];
	ret = h0[1];
	parent = heap_size;
	assert(parent <= murgbl.reg_total);
	h0[1] = h0[parent];
	heap_size = parent - 1;
	parent = 1;
	do
	{
		lchild = parent * 2;
		if (lchild > heap_size)
			break;	/* No children exist inside heap bounds. No more swapping needed. Can stop here */
		rchild = lchild + 1;
		if (rchild <= heap_size)
		{	/* Compare parent with lchild and rchild as both exist */
			h1 = h0 + lchild;
			h2 = h0 + rchild;
			j1 = h1->jext_rec;
			j2 = h2->jext_rec;
			if (!ELEM1_IS_GREATER(j1, j2))
			{	/* lchild is lesser than rchild. Compare parent with lchild */
				h2 = h0 + parent;
				j2 = h2->jext_rec;
				if (!ELEM1_IS_GREATER(j2, j1))
				{	/* parent is lesser than lchild. Heap property is satisfied. Can stop here */
					break;
				} else
				{	/* Swap lchild with parent and descend down the heap */
					tmp = *h1;
					*h1 = *h2;
					*h2 = tmp;
					parent = lchild;
				}
			} else
			{	/* rchild is greater than lchild. Compare parent with rchild */
				h1 = h0 + parent;
				j1 = h1->jext_rec;
				if (!ELEM1_IS_GREATER(j1, j2))
				{	/* parent is lesser than rchild. Heap property is satisfied. Can stop here */
					break;
				} else
				{	/* Swap rchild with parent and descend down the heap */
					tmp = *h1;
					*h1 = *h2;
					*h2 = tmp;
					parent = rchild;
				}
			}
		} else
		{	/* Compare parent with lchild (the only child that exists) */
			h1 = h0 + parent;
			h2 = h0 + lchild;
			j1 = h1->jext_rec;
			j2 = h2->jext_rec;
			if (!ELEM1_IS_GREATER(j1, j2))
			{	/* parent is lesser than lchild. Heap property is satisfied. Can stop here */
				break;
			} else
			{	/* Swap lchild with parent and descend down the heap */
				tmp = *h1;
				*h1 = *h2;
				*h2 = tmp;
				parent = lchild;
			}
		}
	} while(TRUE);
	return ret;
}

/* It returns 0 for normal exit. A non-zero value for an error */
int mur_merge_sort_extfmt(void)
{
	FILE			*fp, *fp_out, **fp_array;
	boolean_t		extr_file_created, single_reg, skip_sort;
	boolean_t		is_dummy_gbldir;
	char			*buff, extr_fn[MAX_FN_LEN + 1], *fn, *fn_out;
	char			rename_fn[MAX_FN_LEN + 1];
	int			rename_fn_len, fn_len;
	char			*ptr;
	char			errstr[1024];
	enum broken_type	recstat;
	fi_type			*file_info;
	gd_region		*reg;
	int			*max_index, *next_index, index, nxtindex, size;
	int			buffsize, extr_fn_len;
	int			shmid, rc, save_errno;
	int			tmplen, i;
	int4			*size_ptr;
	jext_heap_elem_t	*heap, htmp;
	jnlext_multi_t		*jext_rec, *jext_start;
	jnlext_multi_t		*shm_ptr;
	reg_ctl_list		*rctl_start, *rctl, *rctl_top;
	shm_reg_ctl_t		*shm_rctl;
	size_t			jm_size, ret_size;
	uint4			status;
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */

	single_reg = (1 == murgbl.reg_total);
	if (single_reg)
		return 0;	/* No need to do any merge sort since there is only ONE region */
	rctl_start = mur_ctl;
	rctl_top = rctl_start + murgbl.reg_total;
	mp_hdr = multi_proc_shm_hdr;	/* Usable only if "multi_proc_in_use" is TRUE */
	/* If forward phase was interrupted (e.g. external signal) and the parallel processes were asked to terminate
	 * incompletely, skip the sort phase. But do cleanup.
	 */
	skip_sort = (multi_proc_in_use && IS_FORCED_MULTI_PROC_EXIT(mp_hdr));
	save_errno = 0;	/* At end of this function, save_errno points to the last (of one or more) error encountered.
			 * It does not matter the exact value since all the caller mupip_recover cares is a non-zero or zero.
			 */
	if (multi_proc_in_use)
	{
		assert(!single_reg);
		/* Note down individual extract file information first in a region loop.
		 * Attach to shmids in a separate loop that way if we encounter an error in shmat
		 * midway in the region loop, we can at cleanup the extract files of ALL regions before returning.
		 */
		for (rctl = rctl_start, shm_rctl = mur_shm_hdr->shm_rctl_start; rctl < rctl_top; rctl++, shm_rctl++)
		{
			for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
			{
				extr_file_created = shm_rctl->extr_file_created[recstat];
				rctl->extr_file_created[recstat] = extr_file_created;
				if (!extr_file_created)
					continue;
				file_info = (void *)malloc(SIZEOF(fi_type));
				/* Figure out the length of the file */
				fn_len = mur_shm_hdr->extr_fn_len[recstat];
				fn_len++;	/* for the '_' */
				reg = rctl->gd;
				is_dummy_gbldir = reg->owning_gd->is_dummy_gbldir;
				if (!is_dummy_gbldir)
				{
					assert(reg->rname_len);
					fn_len += reg->rname_len;
				} else
				{	/* maximum # of regions is limited by MULTI_PROC_MAX_PROCS (since that is the limit
					 * that "gtm_multi_proc" can handle. Use the byte-length of MULTI_PROC_MAX_PROCS-1.
					 */
					assert(!memcmp(reg->rname, "DEFAULT", reg->rname_len));
					assert(1000 == MULTI_PROC_MAX_PROCS);
					fn_len += 3;	/* 999 is maximum valid value and has 3 decimal digits */
				}
				fn_len += 1;	/* for terminating null byte = '\0' */
				file_info->fn = malloc(fn_len);
				memcpy(file_info->fn, mur_shm_hdr->extr_fn[recstat].fn, mur_shm_hdr->extr_fn_len[recstat]);
				file_info->fn_len = mur_shm_hdr->extr_fn_len[recstat];
				rctl->file_info[recstat] = file_info;
				/* Now adjust the file name to be region-specific. Add a region-name suffix.
				 * If no region-name is found, add region #.
				 */
				tmplen = file_info->fn_len;
				ptr = &file_info->fn[tmplen];
				*ptr++ = '_'; tmplen++;
				if (!is_dummy_gbldir)
				{
					memcpy(ptr, reg->rname, reg->rname_len);
					tmplen += reg->rname_len;
				} else
					tmplen += SPRINTF(ptr, "%d", rctl - &mur_ctl[0]);
				file_info->fn_len = tmplen;
				file_info->fn[tmplen] = '\0';
				assert(tmplen + 1 <= fn_len);	/* assert allocation is enough and no overflows */
			}
		}
		for (rctl = rctl_start, shm_rctl = mur_shm_hdr->shm_rctl_start; rctl < rctl_top; rctl++, shm_rctl++)
		{
			shmid = shm_rctl->jnlext_shmid;
			if (INVALID_SHMID != shmid)
			{
				shm_ptr = (jnlext_multi_t *)do_shmat(shmid, 0, 0);
				if (-1 == (sm_long_t)shm_ptr)
				{
					save_errno = errno;
					SNPRINTF(errstr, SIZEOF(errstr),
						"shmat() : shmid=%d shmsize=0x%llx", shmid, shm_rctl->jnlext_shm_size);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
								ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
					goto cleanup;
				}
			} else
				shm_ptr = NULL;
			for (size_ptr = &shm_rctl->jnlext_list_size[0], recstat = 0;
								recstat < TOT_EXTR_TYPES;
									recstat++, size_ptr++)
			{
				rctl->jnlext_multi_list_size[recstat] = *size_ptr;
				rctl->jnlext_shm_list[recstat] = shm_ptr;
				if (NULL != shm_ptr)
					shm_ptr += *size_ptr;
			}
		}
	}
	if (!skip_sort)
	{
#		ifdef MUR_DEBUG
		for (rctl = rctl_start; rctl < rctl_top; rctl++)
		{
			for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
			{
				jext_start = (jnlext_multi_t *)rctl->jnlext_shm_list[recstat];
				for (i = 0; i < rctl->jnlext_multi_list_size[recstat]; i++)
				{
					if (0 == i)
						fprintf(stderr, "Extract file name : %s\n",
							((fi_type *)rctl->file_info[recstat])->fn);
					jext_rec = (rctl->this_pid_is_owner)
							? (jnlext_multi_t *)find_element(rctl->jnlext_multi_list[recstat], i)
							: (jnlext_multi_t *)&jext_start[i];
					fprintf(stderr, "%s : list size = %d : time = %d : token_seq = %lld : update_num = %u : "
						"num_reg = %d : size = %lld\n",
						rctl->gd->rname, rctl->jnlext_multi_list_size[recstat], jext_rec->time,
						(long long int)jext_rec->token_seq.jnl_seqno,
						jext_rec->update_num, jext_rec->num_more_reg,
						(long long int)jext_rec->size);
				}
			}
		}
#		endif
		max_index = NULL;
		for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
		{
			extr_file_created = FALSE;
			for (rctl = rctl_start; rctl < rctl_top; rctl++)
				if (rctl->extr_file_created[recstat])
					extr_file_created = TRUE;
			if (!extr_file_created)
				continue;	/* If no files of this type were created, move on to the next type */
			extr_fn_len = 0;
			if (NULL == max_index)
			{
				max_index = (int4 *)malloc(murgbl.reg_total * SIZEOF(int4));
				next_index = (int4 *)malloc(murgbl.reg_total * SIZEOF(int4));
				fp_array = (FILE **)malloc(murgbl.reg_total * SIZEOF(FILE *));
				heap_array = (jext_heap_elem_t *)malloc((murgbl.reg_total + 1) * SIZEOF(jext_heap_elem_t));
				buff = NULL;
				buffsize = 0;
			}
			fp_out = NULL;
			/* Assert that no elements are there in the heap or the wait-queue */
			assert(!heap_size);
			assert(!murgbl.forw_token_table.count);
			for (index = 0, rctl = rctl_start; rctl < rctl_top; rctl++, index++)
			{
				size = rctl->jnlext_multi_list_size[recstat];
				max_index[index] = size;
				if (size)
				{	/* Add first element in list to heap */
					htmp.rctl_index = index;
					htmp.jext_rec = (rctl->this_pid_is_owner)
							? (jnlext_multi_t *)find_element(rctl->jnlext_multi_list[recstat], 0)
							: (jnlext_multi_t *)&rctl->jnlext_shm_list[recstat][0];
					mur_add_elem(&htmp, RESOLVED_FALSE);
					next_index[index] = 1;
					fn = ((fi_type *)rctl->file_info[recstat])->fn;
					Fopen(fp, fn, "r");
					if (NULL == fp)
					{
						assert(FALSE);
						save_errno = errno;
						SNPRINTF(errstr, SIZEOF(errstr),
							"fopen() : %s", fn);
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
									ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
						goto cleanup;
					}
					fp_array[index] = fp;
					if (0 == extr_fn_len)
					{	/* Create the merged extract file. Remove the region-name prefix that was added */
						fn_out = &extr_fn[0];
						if (multi_proc_in_use)
						{	/* Copy file name length from shared memory. Note that shared memory
							 * contains the name of the extract file minus the region-name suffix
							 * whereas "fn" contains it with the region-name suffix. To create the
							 * merged extract file, we dont want the region suffix. So use "fn" but
							 * only upto the length indicated by the shared memory "extr_fn_len" field.
							 */
							extr_fn_len = mur_shm_hdr->extr_fn_len[recstat];
						} else
							extr_fn_len = rctl->extr_fn_len_orig[recstat];
						memcpy(fn_out, fn, extr_fn_len);
						extr_fn[extr_fn_len] = '\0';
						/* Argument journal -extract=-stdout ? */
						if (!mur_options.extr_fn_is_stdout[recstat])
						{
							rename_fn_len = ARRAYSIZE(rename_fn);
							if (RENAME_FAILED == rename_file_if_exists(fn_out, extr_fn_len,
											rename_fn, &rename_fn_len, &status))
							{
								assert(FALSE);
								save_errno = status;
								SNPRINTF(errstr, SIZEOF(errstr),
									"rename_file_if_exists() : %s", fn_out);
								gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
									ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
								goto cleanup;
							}
							Fopen(fp_out, fn_out, "w");
							if (NULL == fp_out)
							{
								assert(FALSE);
								save_errno = errno;
								SNPRINTF(errstr, SIZEOF(errstr),
									"fopen() : %s", fn_out);
								gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
									ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
								goto cleanup;
							}
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_FILECREATE, 4,
								LEN_AND_STR(ext_file_type[recstat]), extr_fn_len, extr_fn);
						} else
							fp_out = stdout;
						assert(NULL != fp_out);
						mur_write_header_extfmt(NULL, fp_out, fn_out, recstat);
					}
				} else
				{
					next_index[index] = 0;
					fp_array[index] = NULL;
				}
			}
			/* It is possible "heap_size" is 0 at this point. See "INCTN" record comment in mur_forward.c for how. */
			while (heap_size)
			{
				htmp = mur_remove_elem();
				index = htmp.rctl_index;
				assert(index < murgbl.reg_total);
				jext_rec = htmp.jext_rec;
				jm_size = jext_rec->size;
				assert(jm_size);
				if (jm_size >= buffsize)
				{
					if (NULL != buff)
						free(buff);
					buffsize = jm_size * 2;
					buff = malloc(buffsize);
				}
				fp = fp_array[index];
				GTM_FREAD(buff, 1, jm_size, fp, ret_size, save_errno);
				if (ret_size < jm_size)
				{
					assert(FALSE);
					rctl = &rctl_start[index];
					fn = ((fi_type *)rctl->file_info[recstat])->fn;
					SNPRINTF(errstr, SIZEOF(errstr), "fread() : %s : Expected = %lld : Actual = %lld",
													fn, jm_size, ret_size);
					if (save_errno)
					{	/* ERROR encountered during GTM_FREAD */
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
									ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
						goto cleanup;
					} else
					{	/* EOF reached during GTM_FREAD */
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7)
									ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM);
						goto cleanup;
					}
				}
				buff[jm_size] = '\0';
				GTM_FWRITE(buff, 1, jm_size, fp_out, ret_size, save_errno);
				if (ret_size < jm_size)
				{
					assert(FALSE);
					assert(save_errno);
					SNPRINTF(errstr, SIZEOF(errstr), "fwrite() : %s : Expected = %lld : Actual = %lld",
												fn_out, jm_size, ret_size);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
								ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
					goto cleanup;
				}
				nxtindex = next_index[index];
				if (nxtindex < max_index[index])
				{	/* Add one more element from "index"th region's list */
					rctl = &rctl_start[index];
					htmp.rctl_index = index;
					htmp.jext_rec = (rctl->this_pid_is_owner)
							? (jnlext_multi_t *)find_element(rctl->jnlext_multi_list[recstat], nxtindex)
							: (jnlext_multi_t *)&rctl->jnlext_shm_list[recstat][nxtindex];
					mur_add_elem(&htmp, RESOLVED_FALSE);
					next_index[index]++;
				}
			}
			/* Now that the heap is empty, assert that no elements in the wait-queue either */
			assert(!murgbl.forw_token_table.count);
			for (index = 0, rctl = rctl_start; rctl < rctl_top; rctl++, index++)
			{
				fp = fp_array[index];
				if (NULL != fp)
				{
					FCLOSE(fp, rc);
					assert(0 == rc);
					fp_array[index] = NULL;
				}
			}
			if ((NULL != fp_out) && (stdout != fp_out))
			{
				FCLOSE(fp_out, rc);
				assert(0 == rc);
			}
		}
	}
cleanup:
	/* Delete region-specific extract files that were created */
	for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
	{
		for (index = 0, rctl = rctl_start; rctl < rctl_top; rctl++, index++)
		{
			if (!rctl->extr_file_created[recstat])
				continue;
			fn = ((fi_type *)rctl->file_info[recstat])->fn;
			assert('\0' != fn[0]);
			MUR_JNLEXT_UNLINK(fn); /* Note: "fn" is cleared inside this macro so a later call to
						* MUR_JNLEXT_UNLINK "mur_close_file_extfmt" will not try the UNLINK again.
						*/
			/* Do not clear "rctl->extr_file_created[recstat]" here as this is used later in
			 * "mur_close_file_extfmt.c" to issue ERR_FILENOTCREATE message if appropriate.
			 */
		}
	}
	if (multi_proc_in_use)
	{	/* Cleanup shmids used to communicate between parent and child processes */
		shm_rctl = mur_shm_hdr->shm_rctl_start;
		for (rctl = rctl_start; rctl < rctl_top; rctl++, shm_rctl++)
		{
			shmid = shm_rctl->jnlext_shmid;
			if (INVALID_SHMID == shmid)
				continue;
			rc = shm_rmid(shmid);
			if (0 != rc)
			{
				assert(FALSE);
				save_errno = errno;
				SNPRINTF(errstr, SIZEOF(errstr), "shm_rmid() : shmid=%d", shmid);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
							ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
			}
		}
	}
	return save_errno;
}
