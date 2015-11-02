/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "copy.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "locklits.h"
#include "error.h"
#include "rc.h"
#include "gtcm.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"
#include "mlk_pvtblk_insert.h"
#include "lckclr.h"
#include "mlk_lock.h"
#include "mlk_bckout.h"

GBLREF UINTPTR_T	rc_auxown;		/* From server, address to use in auxown field, same as OMI */
GBLREF unsigned short	lks_this_cmd;
GBLREF mlk_pvtblk	*mlk_pvt_root;
GBLREF int		omi_pid;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

#define KEY_BUFF_SIZE 1024

int rc_prc_lock(rc_q_hdr *qhdr)
{
	rc_req_lock	*req;
	rc_sbkey	*sbk;
	short		i, len, pid_len;
	mval		lock, ext;
	mlk_pvtblk	**prior, *mp, *x;
	rc_lknam	*fl;
	unsigned char	action;
	bool		blocked;
	char		key_buff[KEY_BUFF_SIZE], *lkname;
	unsigned char	buff[12], *cp;
	short		subcnt;
	int		in_pid, locks_done, temp;

	ESTABLISH_RET(rc_dbms_ch, RC_SUCCESS);
	/*  Clean up dead locks in private list */
	for (prior = &mlk_pvt_root; *prior; )
	{	if (!(*prior)->granted || !(*prior)->nodptr || (*prior)->nodptr->owner != omi_pid)
			mlk_pvtblk_delete(prior);
		else
		{	(*prior)->trans = 0;
			prior = &(*prior)->next;
		}
	}

	req = (rc_req_lock *)qhdr;
	in_pid = ((qhdr->r.pid1.value << 16) | qhdr->r.pid2.value);
	cp = i2asc(buff,in_pid);
	pid_len = cp - buff;
	lks_this_cmd = 0;
	if (req->hdr.r.fmd.value & RC_MODE_CLEARLOCK)
	{	/*  Loop through all the locks, unlocking ones that belong to this client */
		for (prior = &mlk_pvt_root; *prior; )
		{	if ((*prior)->nodptr->auxowner == rc_auxown
					&& pid_len == (*prior)->value[(*prior)->total_length]
					&& !memcmp(&(*prior)->value[(*prior)->total_length+1],
					buff,pid_len))
 			{	mlk_unlock(*prior);
				mlk_pvtblk_delete(prior);
			}else
				prior = &(*prior)->next;
		}
		action = LOCKED;
	} else if (req->hdr.r.fmd.value & RC_MODE_DECRLOCK)
	{	sbk = &req->dlocks[0].sb_key;
		fl = req->dlocks;

		/*  Loop through all requested locks */
		for ( i = 0; i < req->nlocks.value; i++)
		{
			if ((char *) fl - (char *) req + sbk->len.value + 1 > req->hdr.r.len.value)
			{	/* we are beyond the end of this packet and still have
				 * lock requests to process.  Bad packet.
				 */
				qhdr->a.erc.value = RC_BADXBUF;
				REVERT;
#ifdef DEBUG
				gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"Bad Rq: Invalid nLockNames value.");
#endif
				return -1;
			}

			if ((temp = rc_frmt_lck(key_buff, KEY_BUFF_SIZE, (unsigned char *)sbk->key, sbk->len.value, &subcnt)) < 0)
			{
			        temp = -temp;
			        qhdr->a.erc.value = temp;
				REVERT;

#ifdef DEBUG
				if (temp == RC_KEYTOOLONG)
				       gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
				else
				       gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"Invalid lock request.");
#endif
				return -1;
			}
			temp += subcnt;

			/*  Loop through all owned locks looking for requested one */
			for (prior = &mlk_pvt_root; *prior; )
			{	if ((*prior)->nodptr->auxowner == rc_auxown
						&& temp == (*prior)->total_length
						&& !memcmp(&(*prior)->value[0],key_buff,temp)
						&& pid_len == (*prior)->value[(*prior)->total_length]
						&& !memcmp(&(*prior)->value[(*prior)->total_length+1],
						buff,pid_len))
	 			{	(*prior)->level -= 1;
					if (!(*prior)->level)
					{	mlk_unlock(*prior);
						mlk_pvtblk_delete(prior);
					}
				}else
					prior = &(*prior)->next;
			}
		fl = (rc_lknam*)((char *)fl + sbk->len.value - 1 + SIZEOF(rc_lknam));
		sbk = &fl->sb_key;

		}
		qhdr->a.erc.value = RC_SUCCESS;
		REVERT;
		return 0;
	} else
		action = INCREMENTAL;
	lock.mvtype = ext.mvtype = MV_STR;
	lkname = (char*)req->dlocks[0].sb_key.key;
	sbk = &req->dlocks[0].sb_key;
	fl = req->dlocks;
	for ( i = 0; i < req->nlocks.value; i++)
	{
		if ((char *) fl - (char *) req + sbk->len.value + 1 > req->hdr.r.len.value)
		{	/* we are beyond the end of this packet and still have
			 * lock requests to process.  Bad packet.
			 */
			qhdr->a.erc.value = RC_BADXBUF;
			REVERT;
#ifdef DEBUG
			gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"Bad Rq: Invalid nLockNames value.");
#endif
			return -1;
		}

		if ((qhdr->a.erc.value = rc_fnd_file(&fl->xdsid)) != RC_SUCCESS)
		    {
			REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"rc_fnd_file failed.");
#endif
			return -1;
		    }

		if ((temp = rc_frmt_lck(key_buff, KEY_BUFF_SIZE, (unsigned char *)sbk->key, sbk->len.value, &subcnt)) < 0)
		{
			temp = -temp;
			qhdr->a.erc.value = temp;
			REVERT;
#ifdef DEBUG
			if (temp == RC_KEYTOOLONG)
				gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
			else
				gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"Invalid lock request.");
#endif
			return -1;
		}
		temp += subcnt;
		mp = (mlk_pvtblk*)malloc(SIZEOF(mlk_pvtblk) + temp + pid_len + 1);
		memset(mp, 0, SIZEOF(mlk_pvtblk) - 1);
		mp->subscript_cnt = subcnt;
		mp->total_length = temp;
		memcpy(mp->value, key_buff, mp->total_length);
		mp->region = gv_cur_region;
		mp->ctlptr = (mlk_ctldata*)cs_addrs->lock_addrs[0];
		mp->value[mp->total_length] = pid_len;
		memcpy(&mp->value[mp->total_length + 1], buff, pid_len);
		if (!mlk_pvtblk_insert(mp))
		{	if (!(mp->value[mp->total_length] == mlk_pvt_root->value[mlk_pvt_root->total_length]
				&& !memcmp(&mp->value[mp->total_length + 1], &mlk_pvt_root->value[mp->total_length + 1],
				mp->total_length + 1)))
			{	free(mp);	/* Server owns lock on behalf of a different agent/client pair */
				REVERT;
				qhdr->a.erc.value = RC_LOCKCONFLICT;
				return 0;	/* Return lock not granted */
			}
			free(mp);
		} else if (mp != mlk_pvt_root)
		{	REVERT;
			qhdr->a.erc.value = RC_GLOBERRUNSPEC;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_GLOBERRUNSPEC.");
#endif
			return -1;/* error */

		}
		fl = (rc_lknam*)((char *)fl + sbk->len.value - 1 + SIZEOF(rc_lknam));
		sbk = &fl->sb_key;
	}
	blocked = FALSE;
	lckclr();
	for (x = mlk_pvt_root, locks_done = 0; locks_done < lks_this_cmd; x = x->next)
	{
	        locks_done++;
		/* If lock is obtained */
		if (!mlk_lock(x,rc_auxown,TRUE))
		{
			x->granted = TRUE;
			if (action == INCREMENTAL)
				x->level += x->translev;
			else
				x->level = 1;
		} else
		{
			blocked = TRUE;
			break;
		}
	}
	if (blocked)	/* need to back out all locks */
	{
		for (x = mlk_pvt_root, i = 0;
			i < locks_done ; x = x->next, i++)
		{
			mlk_bckout(x, action);
		}
		qhdr->a.erc.value = RC_LOCKCONFLICT;
	} else
		qhdr->a.erc.value = RC_SUCCESS;
	REVERT;
	return 0;
}

