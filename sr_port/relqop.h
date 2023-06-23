/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef RELQOP_INCLUDED
#define RELQOP_INCLUDED

void_ptr_t remqt(que_ent_ptr_t base);
void_ptr_t remqh (que_ent_ptr_t base);
void insqh(que_ent_ptr_t new, que_ent_ptr_t base);
void insqt(que_ent_ptr_t new, que_ent_ptr_t base);
void shuffqth(que_ent_ptr_t base1, que_ent_ptr_t base2);
void shuffqtt(que_ent_ptr_t base1, que_ent_ptr_t base2);

#endif
