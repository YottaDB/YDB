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
#include "io.h"
#include <descrip.h>
#include "gtm_caseconv.h"

#define MAX_DRIVER_NAME_LEN 8
#define PROLOG "GTM$DRIVER$"

struct fgn_driver_list_struct
{
	unsigned char name[MAX_DRIVER_NAME_LEN];
	struct fgn_driver_list_struct *next;
	dev_dispatch_struct disp_table;
};

static struct fgn_driver_list_struct *fgn_driver_root = 0;

dev_dispatch_struct *io_get_fgn_driver(mstr *s)
{
	struct fgn_driver_list_struct *ptr;
	unsigned char in_name[MAX_DRIVER_NAME_LEN], *cp;
	unsigned char image_name[MAX_DRIVER_NAME_LEN + SIZEOF(PROLOG)];
	int n, nmax;
	uint4 status;
	struct dsc$descriptor_s image_name_desc;
	int4 (*callback)();

	nmax = s->len;
	if (nmax > MAX_DRIVER_NAME_LEN)
		nmax = MAX_DRIVER_NAME_LEN;
	lower_to_upper(in_name, s->addr, nmax);
	for (n = nmax, cp = &in_name[nmax] ; n < MAX_DRIVER_NAME_LEN ; n++)
		*cp++ = 0;
	for (ptr = fgn_driver_root ; ptr ; ptr = ptr->next)
	{
		if (memcmp(in_name, ptr->name, SIZEOF(ptr->name)) == 0)
			return &(ptr->disp_table);
	}
	memcpy(image_name, PROLOG, SIZEOF(PROLOG) - 1);
	memcpy(&image_name[SIZEOF(PROLOG) - 1], in_name, nmax);
	image_name_desc.dsc$w_length = nmax + SIZEOF(PROLOG) - 1;
	assert(image_name_desc.dsc$w_length < SIZEOF(image_name));
	image_name_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	image_name_desc.dsc$b_class = DSC$K_CLASS_S;
	image_name_desc.dsc$a_pointer = image_name;
	status = lib$find_image_symbol(&image_name_desc, & image_name_desc, &callback);
	if ((status & 1) == 0)
	{
		rts_error(VARLSTCNT(1) status);
		return 0;
	}
	ptr = malloc(SIZEOF(*ptr));
	/* NOTE: SHOULD PROTECT THIS WITH CONDITION HANDLER */
	status = (*callback)(&(ptr->disp_table));
	if ((status & 1) == 0)
	{
		free(ptr);
		rts_error(VARLSTCNT(1) status);
		return 0;
	}
	memcpy(ptr->name, in_name, SIZEOF(ptr->name));
	ptr->next = fgn_driver_root;
	fgn_driver_root = ptr;
	return &ptr->disp_table;
}
