/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gbldirnam.h"
#include "hashtab_mname.h"
#include "iosize.h"
#include "probe.h"
#include "dpgbldir.h"
#ifdef UNIX
#include "gtmio.h"
#elif defined(VMS)
#include <fab.h>
#else
#error unsupported platform
#endif
#include "dpgbldir_sysops.h"
#include "targ_alloc.h"
#include "gtm_logicals.h"

GBLREF	gd_addr		*gd_header;
GBLREF	gv_namehead	*gv_target_list;

LITREF	char gde_labels[GDE_LABEL_NUM][GDE_LABEL_SIZE];

STATICDEF gdr_name	*gdr_name_head;
STATICDEF gd_addr	*gd_addr_head;

error_def(ERR_GDINVALID);

/*+
Function:       ZGBLDIR

		This function searches the list of global directory names for
		the specified names.  If not found, it adds the new name to the
		list and calls GD_LOAD.  A pointer to the global directory
		structure is returned, and the name entry is pointed at it.
		The global directory pointer is then returned to the caller.

Syntax:         gd_addr *zgbldir(mval *v)

Prototype:      ?

Return:         *gd_addr -- a pointer to the global directory structure

Arguments:      mval *v	-- an mval that contains the name of the global
		directory to be accessed.  The name may require translation.

Side Effects:   NONE

Notes:          NONE
-*/
gd_addr *zgbldir(mval *v)
{
	gd_addr		*gd_ptr;
	gdr_name	*name;
	mstr		temp_mstr, *tran_name;

	for (name = gdr_name_head;  name;  name = (gdr_name *)name->link)
		if (v->str.len == name->name.len && !memcmp(v->str.addr, name->name.addr, v->str.len))
			return name->gd_ptr;
	if (!v->str.len)
	{
		temp_mstr.addr = GTM_GBLDIR;
		temp_mstr.len = SIZEOF(GTM_GBLDIR) - 1;
		tran_name = get_name(&temp_mstr);
	} else
		tran_name = get_name(&v->str);
	gd_ptr = gd_load(tran_name);
	name = (gdr_name *)malloc(SIZEOF(gdr_name));
	if (name->name.len = v->str.len)	/* Note embedded assignment */
	{
		name->name.addr = (char *)malloc(v->str.len);
		memcpy(name->name.addr, v->str.addr, v->str.len);
	}
	/* Store translated global directory name as well */
	assert(tran_name->len);
	name->exp_name = *tran_name;
	/* free up memory allocated for mstr field in get_name.
	 * memory allocated for addr field of the mstr is needed as it has been copied over to "name->exp_name" */
	free(tran_name);

	if (gdr_name_head)
		name->link = (gdr_name *)gdr_name_head;
	else
		name->link = 0;
	gdr_name_head = name;
	gdr_name_head->gd_ptr = gd_ptr;
	return gd_ptr;
}

/*+
Function:       GD_LOAD

Syntax:		gd_addr *gd_load(mstr *gd_name)

		Open a global directory file and verify that it is a valid GD.
		Determine if it has already been opened.  If not, setup and
		initialize the GT.M structures used to access the GD based on
		the information in the file, enter in the linked list of global
		directories and return a pointer to it.  If already opened, return
		a pointer to it.

Prototype:      ?

Return:         gd_addr *	(all errors are signalled)

Arguments:	gd_name is the name of the file to be opened

Side Effects:   None
Notes:          A) While checking may be done earlier for duplicate names,
		unique identification of files can require OS specific
		operations useable only after the file is open, so checks
		must be done within this function for duplicate files.
-*/
gd_addr *gd_load(mstr *v)
{
	void		*file_ptr;	/* This is a temporary structure as the file open and manipulations are currently stubs */
	header_struct	*header, temp_head;
	gd_addr		*table, *gd_addr_ptr;
	gd_binding	*map, *map_top;
	gd_region	*reg, *reg_top;
	uint4		t_offset, size;
	short		i;

	file_ptr = open_gd_file(v);

	for (gd_addr_ptr = gd_addr_head;  gd_addr_ptr;  gd_addr_ptr = gd_addr_ptr->link)
	{	/* if already open then return old structure */
		if (comp_gd_addr(gd_addr_ptr, file_ptr))
		{
			close_gd_file(file_ptr);
			return gd_addr_ptr;
		}
	}
	file_read(file_ptr, SIZEOF(header_struct), (uchar_ptr_t)&temp_head, 1);		/* Read in header and verify is valid GD */
	for (i = 0;  i < GDE_LABEL_NUM;  i++)
	{
		if (!memcmp(temp_head.label, gde_labels[i], GDE_LABEL_SIZE - 1))
			break;
	}
	if (GDE_LABEL_NUM == i)
	{
		close_gd_file(file_ptr);
		rts_error(VARLSTCNT(8) ERR_GDINVALID, 6, v->len, v->addr, LEN_AND_LIT(GDE_LABEL_LITERAL),
				SIZEOF(temp_head.label), temp_head.label);
	}
	size = LEGAL_IO_SIZE(temp_head.filesize);
	header = (header_struct *)malloc(size);
	file_read(file_ptr, size, (uchar_ptr_t)header, 1);			/* Read in body of file */
	table = (gd_addr *)((char *)header + SIZEOF(header_struct));
        table->local_locks = (struct gd_region_struct *)((UINTPTR_T)table->local_locks + (UINTPTR_T)table);
	table->maps = (struct gd_binding_struct *)((UINTPTR_T)table->maps + (UINTPTR_T)table);
	table->regions = (struct gd_region_struct *)((UINTPTR_T)table->regions + (UINTPTR_T)table);
	table->segments = (struct gd_segment_struct *)((UINTPTR_T)table->segments + (UINTPTR_T)table);
	table->end = (table->end + (UINTPTR_T)table);
	for (map = table->maps, map_top = map + table->n_maps;  map < map_top;  map++)
	{
		t_offset = map->reg.offset;
		map->reg.addr = (gd_region *)((char *)table + t_offset);
		assert(SIZEOF(map->name) == (MAX_MIDENT_LEN + 1));
		map->name[MAX_MIDENT_LEN] = '\0';	/* reset 32nd byte to 0 since only 31 bytes are used in map.
							 * this is necessary so "mid_len" can be invoked on this
							 * as it expects a null-terminated string.
							 */
	}
	for (reg = table->regions, reg_top = reg + table->n_regions;  reg < reg_top;  reg++)
	{
		t_offset = reg->dyn.offset;
		reg->dyn.addr = (gd_segment *)((char *)table + t_offset);
	}
	table->link = gd_addr_head;
	gd_addr_head = table;
	fill_gd_addr_id(gd_addr_head, file_ptr);
	close_gd_file(file_ptr);
	table->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
	init_hashtab_mname(table->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	return table;
}

/*+
Function:       GET_NEXT_GDR

		This function returns the next entry in the list of open
		global directories.  If the input parameter is zero, the
		first entry is returned, otherwise the next entry in the
		list is returned.  If the input parameter is not a member
		of the list, then zero will be returned.

Syntax:         gd_addr *get_next_gdr(gd_addr *prev)

Prototype:      ?

Return:         *gd_addr -- a pointer to the global directory structure

Arguments:      The previous global directory accessed;

Side Effects:   NONE

Notes:          NONE
-*/
gd_addr *get_next_gdr(gd_addr *prev)
{
	gd_addr	*ptr;

	if (!prev)
		return gd_addr_head;

	for (ptr = gd_addr_head;  ptr && ptr != prev;  ptr = ptr->link)
		if (!GTM_PROBE(SIZEOF(*ptr), ptr, READ)) /* Called from secshr, have to check access to memory */
			return NULL;
	if (ptr && GTM_PROBE(SIZEOF(*ptr), ptr, READ))
		return ptr->link;
	return NULL;
}

/* Maintain list of regions for GTCM_SERVER */
void cm_add_gdr_ptr(gd_region *greg)
{
	gd_addr	*ga;

	ga = (gd_addr *)malloc(SIZEOF(gd_addr));
	ga->end = 0;	/* signifies a GT.CM gd_addr */
	ga->regions = greg;
	ga->n_regions = 1;
	ga->link = gd_addr_head;
	gd_addr_head = ga;
	return;
}

void cm_del_gdr_ptr(gd_region *greg)
{
	gd_addr	*ga1, *ga2;

	for (ga1 = ga2 = gd_addr_head;  ga1;  ga1 = ga1->link)
	{
		if (ga1->regions == greg)
		{
			if (ga1 == gd_addr_head)
				gd_addr_head = ga1->link;
			else
				ga2->link = ga1->link;
			free(ga1);
			break;
		}
		ga2 = ga1;
	}
	return;
}

boolean_t get_first_gdr_name(gd_addr *current_gd_header, mstr *log_nam)
{
	gdr_name	*name;

	for (name = gdr_name_head;  name;  name = (gdr_name *)name->link)
	{
		if (name->gd_ptr == current_gd_header)
		{
			*log_nam = name->exp_name;
			return (TRUE);
		}
	}
	return FALSE;
}

void gd_rundown(void)		/* Wipe out the global directory structures */
{
	gd_addr		*gda_cur, *gda_next;
	gdr_name	*gdn_cur, *gdn_next;

	for (gda_cur = gd_addr_head;  NULL != gda_cur; gda_cur = gda_next)
	{
		gda_next = gda_cur->link;
		if (gda_cur->end)
		{
			gd_ht_kill(gda_cur->tab_ptr, TRUE);
			free(gda_cur->tab_ptr);		/* free up hashtable malloced in gd_load() */
			free(gda_cur->id);		/* free up gd_id malloced in gd_load()/fill_gd_addr_id() */
			free((char *)gda_cur - SIZEOF(header_struct));	/* free up global directory itself */
		} else
			free(gda_cur);	/* GT.CM gd_addr and hence header_struct wasn't malloced in cm_add_gdr_ptr */
	}
	assert(NULL == gv_target_list);
	gd_header = gd_addr_head = (gd_addr *)NULL;
	for (gdn_cur = gdr_name_head; NULL != gdn_cur; gdn_cur = gdn_next)
	{
		gdn_next = (gdr_name *)gdn_cur->link;
		if (gdn_cur->name.len)
			free(gdn_cur->name.addr);
		free(gdn_cur);
	}
	gdr_name_head = (gdr_name *)NULL;
}

void gd_ht_kill(hash_table_mname *table, boolean_t contents)	/* wipe out the hash table corresponding to a gld */
{
	ht_ent_mname	*tabent, *topent;
	gvnh_reg_t	*gvnh_reg;
	gv_namehead	*gvt;

	if (contents)
	{
		for (tabent = table->base, topent = tabent + table->size; tabent < topent; tabent++)
		{
			if (HTENT_VALID_MNAME(tabent, gvnh_reg_t, gvnh_reg))
			{
				gvt = gvnh_reg->gvt;
				gvt->regcnt--;
				if (!gvt->regcnt)
					targ_free(gvt);
				free(gvnh_reg);
			}
		}
	}
	free_hashtab_mname(table);
	/* We don't do a free(table) in this generic routine because it is called both by GT.M and GT.CM
	 * and GT.CM retains the table for reuse while GT.M doesn't. GT.M fgncal_rundown() takes care of
	 * this by freeing it up explicitly (after a call to ht_kill) in gd_rundown() [dpgbldir.c]
	 */
	return;
}
