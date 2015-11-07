/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* dtgbldir.c - test global directory functions */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include <fab.h>
#include <rmsdef.h>
#include <iodef.h>
#include <ssdef.h>
#include <descrip.h>
#include <psldef.h>
#include <lnmdef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gbldirnam.h"
#include "dpgbldir.h"

main()
{
	struct FAB	fab;
	header_struct	*header;
	gd_addr		*addr, *addr1, *addr2, *addr3;
	gd_region	*region;
	gd_segment	*segment;
	int4		*long_ptr, ret_addr;
	short		iosb[4];
	mval		v;
	char		file_name1[] = "dtgbld01.gld", file_name2[] = "dtgbld02.gld", file_name3[] = "dtgbld03.gld";
	char		label[] = GDE_LABEL_LITERAL;
	char		file_name4[]="dtlog1";
	uint4		status, size;
	$DESCRIPTOR(proc_tab, "LNM$PROCESS");
	$DESCRIPTOR(gbldir, "GTM$GBLDIR");
	$DESCRIPTOR(dtlog, "DTLOG1");
	typedef struct
	{	short	buf_len;
		short	item;
		int4	buf_addr;
		int4	ret_addr;
		int4	term;
	}item_list;
	item_list	ilist;
	char		acmo=PSL$C_USER;

/************************* Create logical names for tests **********************************************************/
	ilist.item = LNM$_STRING;
	ilist.buf_len = SIZEOF(file_name1) - 1;
	ilist.buf_addr = file_name1;
	ilist.term = 0;
	ilist.ret_addr = &ret_addr;
	status = sys$crelnm(0, &proc_tab, &gbldir, &acmo, &ilist);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	ilist.buf_len = SIZEOF(file_name2) - 1;
	ilist.buf_addr = file_name2;
	status = sys$crelnm(0, &proc_tab, &dtlog, &acmo, &ilist);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);

/************************* Create global directory files for tests *************************************************/
	fab = cc$rms_fab;
	fab.fab$l_alq = 5;
	fab.fab$l_fna = file_name1;
	fab.fab$b_fns = SIZEOF(file_name1) - 1;
	fab.fab$l_fop = (FAB$M_UFO | FAB$M_CBT);
	fab.fab$b_fac = (FAB$M_PUT | FAB$M_GET | FAB$M_BIO);
	fab.fab$b_shr = (FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI);
	status = sys$create(&fab);
	if (status != RMS$_CREATED && status != RMS$_FILEPURGED && status != RMS$_NORMAL)
		sys$exit(status);
	size = SIZEOF(header_struct) + SIZEOF(gd_addr) + 3 * SIZEOF(gd_binding) + SIZEOF(gd_region) + SIZEOF(gd_segment);
	header = malloc(((size  + 511) / 512) * 512);
	header->filesize = size;
	size = ((size + 511) / 512) * 512;
	memcpy(header->label, label, SIZEOF(label)-1);
	addr = (char*)header + SIZEOF(header_struct);
	addr->maps = SIZEOF(gd_addr);
	addr->n_maps = 3;
	addr->regions = (int4)(addr->maps) + 3 * SIZEOF(gd_binding);
	addr->n_regions = 1;
	addr->segments = (int4)(addr->regions) + SIZEOF(gd_region);
	addr->n_segments = 1;
	addr->link = addr->tab_ptr = addr->id = addr->local_locks = 0;
	addr->max_rec_size = 100;
	addr->end = addr->segments + SIZEOF(gd_segment);
	long_ptr = (char*)addr + (int4)(addr->maps);
	*long_ptr++ = 0xFFFF2F23;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = addr->regions;
	*long_ptr++ = 0xFFFFFF24;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = addr->regions;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = addr->regions;
	region = (char*)addr + (int4)(addr->regions);
	segment = (char*)addr + (int4)(addr->segments);
	memset(region, 0, SIZEOF(gd_region));
	region->rname_len = 5;
	memcpy(region->rname,"TEMP1",5);
	region->dyn.offset = addr->segments;
	region->max_rec_size = 100;
	region->max_key_size = 64;
	segment->sname_len = 5;
	memcpy(segment->sname, "TEMP1", 5);
	memcpy(segment->fname, "MUMPS1.DAT", 10);
	segment->fname_len = 10;
	segment->blk_size = 2 * DISK_BLOCK_SIZE;
	segment->allocation = 100;
	segment->ext_blk_count = 100;
	segment->cm_blk = 0;
	segment->lock_space = 20;
	memcpy(segment->defext, ".DAT", 4);
	segment->global_buffers = 64;
	segment->buckets = 0;
	segment->windows = 0;
	segment->acc_meth = dba_bg;
	segment->defer_time = 0;
	segment->file_cntl = 0;
	status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_WRITEVBLK, &iosb[0], 0, 0, header, size, 1, 0, 0, 0);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	if (!(iosb[0] & 1))
		rts_error(VARLSTCNT(1) status);
	sys$dassgn(fab.fab$l_stv);
	region->rname_len = 5;
	memcpy(region->rname,"TEMP2",5);
	segment->sname_len = 5;
	memcpy(segment->sname,"TEMP2",5);
	memcpy(segment->fname,"MUMPS2.DAT",10);
	segment->fname_len = 10;
	fab.fab$l_fna = file_name2;
	fab.fab$b_fns = SIZEOF(file_name3) - 1;
	status = sys$create(&fab);
	if (status != RMS$_CREATED && status != RMS$_FILEPURGED && status != RMS$_NORMAL)
		sys$exit(status);
	status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_WRITEVBLK, &iosb[0], 0, 0, header, size, 1, 0, 0, 0);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	if (!(iosb[0] & 1))
		rts_error(VARLSTCNT(1) status);
	sys$dassgn(fab.fab$l_stv);
	region->rname_len = 5;
	memcpy(region->rname, "TEMP3", 5);
	segment->sname_len = 5;
	memcpy(segment->sname, "TEMP3", 5);
	memcpy(segment->fname, "MUMPS3.DAT", 10);
	segment->fname_len = 10;
	fab.fab$l_fna = file_name3;
	fab.fab$b_fns = SIZEOF(file_name3) - 1;
	status = sys$create(&fab);
	if (status != RMS$_CREATED && status != RMS$_FILEPURGED && status != RMS$_NORMAL)
		sys$exit(status);
	status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_WRITEVBLK, &iosb[0], 0, 0, header, size, 1, 0, 0, 0);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	if (!(iosb[0] & 1))
		rts_error(VARLSTCNT(1) status);
	sys$dassgn(fab.fab$l_stv);

/*************************** Run tests********************************************/
	v.str.len = SIZEOF(file_name1) - 1;
	v.str.addr = file_name1;
	PRINTF("Open first global directory:  dtgbld01.gld\n");
	addr1 = zgbldir(&v);
	PRINTF("Region name is %s, expected TEMP1\n", addr1->regions->rname);
	PRINTF("Segment name is %s, expected TEMP1\n", addr1->regions->dyn.addr->sname);
	PRINTF("File name is %s, expected MUMPS1.DAT\n", addr1->regions->dyn.addr->fname);
	v.str.len = SIZEOF(file_name2) - 1;
	v.str.addr = file_name2;
	PRINTF("Open second global directory:  dtgbld02.gld\n");
	addr2 = zgbldir(&v);
	PRINTF("Region name is %s, expected TEMP2\n", addr2->regions->rname);
	PRINTF("Segment name is %s, expected TEMP2\n", addr2->regions->dyn.addr->sname);
	PRINTF("File name is %s, expected MUMPS2.DAT\n", addr2->regions->dyn.addr->fname);
	v.str.len = SIZEOF(file_name3) - 1;
	v.str.addr = file_name3;
	PRINTF("Open third global directory:  dtgbld03.gld\n");
	addr3 = zgbldir(&v);
	PRINTF("Region name is %s, expected TEMP3\n", addr3->regions->rname);
	PRINTF("Segment name is %s, expected TEMP3\n", addr3->regions->dyn.addr->sname);
	PRINTF("File name is %s, expected MUMPS3.DAT\n", addr3->regions->dyn.addr->fname);
	PRINTF("Open null string global directory:  dtgbld01.gld\n");
	v.str.len = 0;
	addr = zgbldir(&v);
	if (addr != addr1)
		PRINTF("Expected pointer to previous struct, got new structure\n");
	else
		PRINTF("Got same pointer as expected.\n");
	PRINTF("Open dtlog1 global directory:  dtgbld02.gld\n");
	v.str.len = SIZEOF(file_name4) - 1;
	v.str.addr = file_name4;
	addr = zgbldir(&v);
	if (addr != addr2)
		PRINTF("Expected pointer to previous struct, got new structure\n");
	else
		PRINTF("Got same pointer as expected.\n");
	v.str.len = SIZEOF(file_name3) - 1;
	v.str.addr = file_name3;
	PRINTF("Reopen third global directory:  dtgbld03.gld\n");
	addr = zgbldir(&v);
	if (addr != addr3)
		PRINTF("Expected pointer to previous struct, got new structure\n");
	else
		PRINTF("Got same pointer as expected.\n");
	return;
}
