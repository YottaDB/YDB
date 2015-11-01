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

#ifndef GTCML_H_INCLUDED
#define GTCML_H_INCLUDED

void gtcml_blklck(cm_region_list *region, mlk_pvtblk *lock, uint4 wake);
void gtcml_chklck(cm_lckblkreg *reg, bool timed);
void gtcml_chkprc(cm_lckblklck *lck);
void gtcml_chkreg(void);
void gtcml_decrlock(void);
unsigned char gtcml_dolock(void);
char gtcml_incrlock(cm_region_list *reg);
void gtcml_lckclr(void);
bool gtcml_lcktime(cm_lckblklck *lck);
void gtcml_lkbckout(cm_region_list *reg_list);
unsigned char gtcml_lkcancel(void);
void gtcml_lkhold(void);
void gtcml_lklist(void);
void gtcml_lkrundown(void);
char gtcml_lock(cm_region_list *reg);
char gtcml_lock_internal(cm_region_list *reg, unsigned char action);
void gtcml_unlock(void);
void gtcml_zdeallocate(void);
char gtcml_zallocate(cm_region_list *reg);
#if defined(VMS)
void gtcml_lkstarve(connection_struct *connection);
#elif defined(UNIX)
void gtcml_lkstarve(TID timer_id, int4 data_len, connection_struct **connection);
#endif

#endif
