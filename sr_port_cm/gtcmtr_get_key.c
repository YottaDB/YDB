/****************************************************************
 *								*
 * Copyright (c) 2021 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_bind_name.h"
#include "gvcst_protos.h"	/* for gvcst_get prototype */
#include "gtcm_find_region.h"
#include "gtcmtr_protos.h"
#include "copy.h"

GBLREF connection_struct	*curr_entry;

error_def(ERR_BADGTMNETMSG);

unsigned char *gtcmtr_get_key(void *gvkey, unsigned char *ptr, unsigned short len)
{
	size_t cp_len;
	unsigned short key_end, key_prev;
	/* Fetch gv_key fields from message buffer and store into 'key' whose allocated buffer
	 * size is key->top, the maximum length allowed and enforced by the runtime. Return
	 * the advanced ptr which points to the byte after key */
	gv_key *key = (gv_key *)gvkey;
	gv_key_buf *keybuf_p = (gv_key_buf *) gvkey;	/* Make buffer size visible 4SCA */
	assert(NULL != key);
	assert(NULL != keybuf_p);
	assert(NULL != key->base);
	assert(NULL != ptr);
	assert(NULL != (keybuf_p->split).base);
	/* "len" is what the client said the key occupies and is not to be trusted. It has to be checked against the
	 * bytes actually received and against the buffer allocated for the key before any of it is used. Asserts will
	 * not do, since a PRO build compiles them out and leaves a client-controlled length driving the memcpy() below.
	 * Note the callers have already subtracted the region number byte, so a client that sent 0 gets here as 65535.
	 */
	CM_CHECK_AVAIL(curr_entry, ptr, len);
	if (SIZEOF(gv_key_nobase) >= len)  /* need the "top", "end", "prev" header plus at least one byte of key */
		CM_BADMSG(curr_entry);
	cp_len = len - SIZEOF(gv_key_nobase);
	if (cp_len > key->top)
		CM_BADMSG(curr_entry);
	ptr += SIZEOF(unsigned short);	/* the "top" in the message is the client's, not ours; skip it */
	/* Read "end" and "prev" into locals rather than into the key, so that a rejection below leaves "key", which
	 * is gv_currkey for every caller, untouched. Assigning first and rejecting afterwards leaves the wire value
	 * behind in a key the server goes on using: the next region open then dies in gvcst_reservedDB_funcs() on
	 * "assert((SIZEOF(gv_key) + gv_currkey->end + 1) <= SIZEOF(save_currkey))", and in a PRO build, where that
	 * assert is compiled out, the poisoned "end" indexes "base" instead.
	 */
	GET_USHORT(key_end, ptr);
	ptr += SIZEOF(unsigned short);
	GET_USHORT(key_prev, ptr);
	ptr += SIZEOF(unsigned short);
	memcpy((keybuf_p->split).base, ptr, cp_len);
	ptr += cp_len;
	/* "end" and "prev" came off the wire too, so reject anything that does not describe a key lying inside what
	 * was just copied. gtcmtr_query() writes through "end" as well as reading it, doing "gv_currkey->end -= 2"
	 * followed by "gv_currkey->base[gv_currkey->end] = KEY_DELIMITER".
	 */
	if (CM_BAD_KEY_SHAPE(key->base, key_end, key_prev, cp_len))
		CM_BADMSG(curr_entry);
	key->end = key_end;		/* only now that the shape is known good */
	key->prev = key_prev;
	return ptr;
}
