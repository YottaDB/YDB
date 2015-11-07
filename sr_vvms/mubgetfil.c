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

/*
 * mubgetfil.c
 *
 * Description: parse the file spec and determines whether it is backup to file, exec or tcpip.
 *		if it is to file, then determines whether it is a directory and sets "directory"
 *		and "is_directory" correspondingly.
 *
 * Input:       char *name		-- specifies the file spec
 *		ushort len		-- specifies the file spec
 * Output:      backup_reg_list *list	-- parsing result will be put to the backup_to and backup_file fields of list
 *		directory		-- if backup to file and it is a directory, this will be set
 *		is_directory		-- if backup to file, this will be set to reflect whether it is a directory.
 */

#include "mdef.h"

#include "gtm_string.h"

#include <rms.h>
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mupipbckup.h"
#include "gtm_caseconv.h"

GBLDEF mstr directory;
GBLDEF bool is_directory;

bool mubgetfil(backup_reg_list *list, char *name, unsigned short len)
{
	mstr 		*mubchkfs(), *temp, file;
	uint4 		status;
	char            tcp[5];

        if (0 == len)
                return FALSE;

        if ('|' == *name)
        {
                len -= 1;
                list->backup_to = backup_to_exec;
                list->backup_file.len = len;
                list->backup_file.addr = (char *)malloc(len + 1);
                memcpy(list->backup_file.addr, name + 1, len);
                return TRUE;
        }

        if (len > 5)
        {
                lower_to_upper(tcp, name, 5);
                if (0 == memcmp(tcp, "TCP:/", 5))
                {
                        list->backup_to = backup_to_tcp;
                        len -= 5;
                        name += 5;
                        while ('/' == *name)
                        {
                                len--;
                                name++;
                        }
                        list->backup_file.len = len;
                        list->backup_file.addr = (char *)malloc(len + 1);
                        memcpy(list->backup_file.addr, name, len);
                        *(list->backup_file.addr + len) = 0;
                        return TRUE;
                }
        }

	file.addr = name;
	file.len = len;
	if (NULL == (temp = mubchkfs(&file))) /* mubchkfs is responsible for error message if NULL, and allocate space otherwise */
		return FALSE;

	if (']' == *(temp->addr + temp->len - 1))
	{
		is_directory = TRUE;
		directory = *temp;
		mubexpfilnam(list);
	}
	else
	{
		is_directory = FALSE;
		list->backup_file = *temp;
	}

	return TRUE;
}
