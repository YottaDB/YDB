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

#include "ddphdr.h"
#include "ddpcom.h"
#include "route_table.h"

GBLREF unsigned short	my_group_mask;
GBLREF volset_tab	volset_table[DDP_MAX_VOLSETS];

static routing_tab 	routing_table[MAXIMUM_CIRCUITS];
static circuit_tab 	circuit_table[MAXIMUM_CIRCUITS * DDP_MAX_VOLSETS];

void remove_circuits(ddp_hdr_t *dp)
{
	unsigned short	target_circuit, ckt;
	routing_tab	*rp1, *rp2;
	circuit_tab	*ct1, *ct2;

	if (0 == (((ddp_announce_msg_t *)dp->txt)->group_mask & my_group_mask)) /* we are not part of any of the announcer's	 */
		return;								/* groups; ignore announce/status change message */
	target_circuit = dp->source_circuit_name;
	for (rp1 = routing_table; (0 != (ckt = rp1->circuit_name)) && (ckt < target_circuit); rp1++)
		;
	if (ckt == target_circuit)
	{
		for (rp2 = rp1 + 1; 0 != rp2->circuit_name; rp2++)
			;
		memmove(rp1, rp1 + 1, (rp2 - rp1) * SIZEOF(*rp1));
	}
	for (ct2 = ct1 = circuit_table; 0 != ct1->volset_name; ct1++)
	{
		if (ct1->circuit_name != target_circuit)
			*ct2++ = *ct1;
	}
	*ct2 = *ct1;
	return;
}

boolean_t enter_circuits(ddp_hdr_t *dp)
{
	unsigned short 		target_circuit, ckt, vol, *fb;
	routing_tab		*rp1, *rp2;
	circuit_tab		*ct1, *ctop;
	int 			volset_index;
	ddp_announce_msg_t	*ap;

	ap = (ddp_announce_msg_t *)dp->txt;
	if (0 == (ap->group_mask & my_group_mask)) /* we are not part of any of the announcer's groups; ignore announce */
		return FALSE;
	target_circuit = dp->source_circuit_name;
	for (rp1 = routing_table; (0 != (ckt = rp1->circuit_name)) && (ckt < target_circuit); rp1++)
		;
	if (ckt != target_circuit)
	{
		for (rp2 = rp1 + 1; 0 != rp2->circuit_name; rp2++)
			;
		memmove(rp1 + 1, rp1, (rp2 - rp1) * SIZEOF(*rp1));
		rp1->circuit_name = target_circuit;
		memcpy(rp1->ether_addr, ap->ether_addr, ETHERADDR_LENGTH);
		memset(&rp1->incoming_users[0], 0, SIZEOF(rp1->incoming_users));
		memset(&rp1->outgoing_users[0], 0, SIZEOF(rp1->outgoing_users));
	}
	for (ctop = circuit_table; 0 != ctop->volset_name; ctop++)
		;
	for (volset_index = 0; volset_index < DDP_MAX_VOLSETS; volset_index++)
	{
		if (0 != (vol = ap->volset[volset_index]))
		{
			for (ct1 = circuit_table; (0 != ct1->volset_name) && (ct1->volset_name < vol); ct1++)
				;
			if (ct1->volset_name != vol)
			{
				memmove(ct1 + 1, ct1, (ctop - ct1) * SIZEOF(ct1));
				ctop++;
				ct1->volset_name = vol;
				ct1->circuit_name = target_circuit;
			}
		}
	}
	return TRUE;
}

unsigned short find_circuit(unsigned short vol)
{ /* given volume set, find circuit */
	circuit_tab	*ct;
	unsigned short	volset;

	for (volset = vol, ct = circuit_table; 0 != ct->volset_name && ct->volset_name < volset; ct++)
		;
	if (volset == ct->volset_name)
		return ct->circuit_name;
	return 0;
}

routing_tab *find_route(unsigned short ckt)
{ /* given circuit, find the corresponding routing table entry */
	routing_tab	*rt;
	unsigned short	circuit;

	for (circuit = ckt, rt = routing_table; 0 != rt->circuit_name && rt->circuit_name < circuit; rt++)
		;
	if (circuit == rt->circuit_name)
		return rt;
	return 0;
}

void reset_user_count(int jobindex)
{
	routing_tab *rt;

	for (rt = routing_table; 0 != rt->circuit_name; rt++)
		rt->outgoing_users[jobindex] = 0;
}

boolean_t enter_vug(unsigned short vol, unsigned short uci, mstr *gld)
{
	boolean_t	new_entry;
	volset_tab	*volset_entry, *volset_top;
	uci_gld_pair	*ug, **ugp;

	assert(0 != vol);
	assert(0 != uci);
	assert(NULL != gld);
	assert(NULL != gld->addr);
	assert(0 != gld->len);
	for (volset_entry = volset_table; 0 != volset_entry->vol && volset_entry->vol < vol; volset_entry++)
		;
	if (volset_entry->vol != vol)
	{
		for (volset_top = volset_entry; 0 != volset_top->vol; volset_top++)
			;
		assert(DDP_MAX_VOLSETS >= volset_top + 1 - volset_table); /* We expect the callers of this function to make sure
									   * that no more than DDP_MAX_VOLSETS vols are entered */
		memmove(volset_entry + 1, volset_entry, (volset_top - volset_entry) * SIZEOF(*volset_entry));/* make space */
		volset_entry->vol = vol;
		volset_entry->ug = NULL;
	}
	for (ugp = &volset_entry->ug; NULL != *ugp && (*ugp)->uci < uci; ugp = &(*ugp)->next)
		;
	if (FALSE != (new_entry = (NULL == *ugp || (*ugp)->uci != uci)))
	{
		ug = (uci_gld_pair *)malloc(SIZEOF(uci_gld_pair));
		ug->gld.addr = malloc(gld->len);
		ug->next = *ugp;
		*ugp = ug;
		ug->uci = uci;
	} else /* if there are multiple entries in the configuration file for the same <vol, uci> pair, the last one wins */
	{
		ug = *ugp;
		free(ug->gld.addr);
		ug->gld.addr = malloc(gld->len);
	}
	memcpy(ug->gld.addr, gld->addr, gld->len);
	ug->gld.len = gld->len;
	return new_entry;
}

void clear_volset_table(void)
{
	volset_tab	*volset_entry;
	uci_gld_pair	*ug, *free_ug;

	for (volset_entry = volset_table; 0 != volset_entry->vol; volset_entry++)
	{
		for (ug = volset_entry->ug; ug != NULL; )
		{
			free_ug = ug;
			ug = ug->next;
			free(free_ug->gld.addr);
			free(free_ug);
		}
	}
	memset(volset_table, 0, SIZEOF(volset_table) * SIZEOF(volset_table[0]));
	return;
}

mstr *find_gld(unsigned short vol, unsigned short uci)
{ /* given <vol,uci> pair, find the global directory used by the server */
	volset_tab	*volset_entry;
	uci_gld_pair	*ug;

	if (0 == vol || 0 == uci)
		return NULL;
	for (volset_entry = volset_table; 0 != volset_entry->vol && volset_entry->vol < vol; volset_entry++)
		;
	if (vol == volset_entry->vol)
	{
		assert(NULL != volset_entry->ug); /* there has to be at least one uci/gld entry */
		for (ug = volset_entry->ug; ug != NULL && ug->uci < uci; ug = ug->next)
			;
		if (NULL != ug && ug->uci == uci)
			return &ug->gld;
	}
	return NULL;
}
