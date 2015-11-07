/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* disk_block_available(int fd, uint4 *free_blocks)
 *      parameter:
 *              fd:     file descriptor of the file that is located
 *                      on the disk being examined
 *		free_blocks:
 *			address to an uint4 where the number of
 *			free blocks will be returned to
 *      return:
 *		the status of the system service sys$getdviw()
 *		if status is normal, then free_blocks is the correct value
 *		otherwise, free_blocks is undetermined
 */
#include "mdef.h"

#include <dvidef.h>
#include <ssdef.h>
#include <efndef.h>


#include "vmsdtype.h"
#include "disk_block_available.h"

int4 disk_block_available(int fd, uint4 *free_blocks)
{
	unsigned short		retlen;
        struct
        {
                item_list_3	item;
                int4		terminator;
        } item_list;
	short			iosb[4];
	int4			status;

	item_list.item.buffer_length         = 4;
	item_list.item.item_code             = DVI$_FREEBLOCKS;
	item_list.item.buffer_address        = free_blocks;
	item_list.item.return_length_address = &retlen;
	item_list.terminator 		     = 0;
	status = sys$getdviw(EFN$C_ENF, fd, NULL, &item_list, iosb, NULL, 0, 0);
	if (SS$_NORMAL == status)
		status = iosb[0];

	return status;
}
