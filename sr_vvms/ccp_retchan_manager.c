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
#include "gdsroot.h"
#include "ccp.h"
#include "ccp_retchan_manager.h"
#include <descrip.h>
#include <iodef.h>

static void ccp_retchan_cl1(unsigned short chan);

static short ccp_retchan_open(ccp_action_aux_value *v)
{
	struct dsc$descriptor mbxname;
	short return_channel;
	uint4 status;

	mbxname.dsc$w_length = v->str.len;
	mbxname.dsc$b_dtype = DSC$K_DTYPE_T;
	mbxname.dsc$b_class = DSC$K_CLASS_S;
	mbxname.dsc$a_pointer = v->str.txt;
	status = sys$assign(&mbxname, &return_channel, 0, 0);
	if ((status & 1) == 0)
		return_channel = 0;
	return return_channel;
}


static void ccp_retchan_close(unsigned short chan)
{
	sys$qio(0, chan, IO$_WRITEOF, 0, ccp_retchan_cl1, chan, 0, 0, 0, 0, 0, 0);
	return;
}

static void ccp_retchan_cl1(unsigned short chan)
{
	uint4 status;

	status = sys$dassgn(chan);
	assert(status & 1);
	return;
}

static uint4 ccp_retchan_write(
unsigned short chan,
unsigned char *addr,
unsigned short len,
void (*completion_addr)(),
uint4 completion_arg)
{
	uint4 status;

	status = sys$qio(0, chan, IO$_WRITEVBLK, 0, completion_addr, completion_arg, addr, len, 0, 0, 0, 0);
	return status;
}

/***********************************************************************
   The following routines are used to return a file of text records
   to cce.

   To start, call ccp_retchan_init(v), where v contains the return
	mailbox name.  This routine returns a pointer which is the
	context for this suite and must be preserved until the
	last call.

   To send a text line, call ccp_retchan_text(p,addr,len), where
	p is the context and addr and len define the text to be
	sent.

   To finish, call ccp_retchan_fini(p) with the context p.
	Nothing is sent out and there are no I/O waits until
	the last call.  The last call operates through a chain
	of completion ast's.  Therefore, consistent queue contents
	(for example) may be returned to the cce with these routines

   Note: error status returns will cause a clean-up to commence and
        processing to terminate on the assumption that the CCE
	either exited or malfunctioned under this circumstance.
***********************************************************************/

struct retchan_header *ccp_retchan_init(ccp_action_aux_value *v)
{
	struct retchan_header *p;

	p = malloc(SIZEOF(*p));
	p->head = 0;
	p->tail = p;
	p->chan = 0;
	p->mbxnam = *v;
	return p;
}

void ccp_retchan_text(
struct retchan_header *p,
unsigned char *addr,
unsigned short len)
{
	struct retchan_txt *v;
	v = malloc(SIZEOF(*v) - 1 + len);
	*(p->tail) = v;
	p->tail = v;
	v->next = 0;
	v->len = len;
	memcpy(v->txt, addr, len);
}

static void ccp_retchan_cleanup(struct retchan_header *p)
{
	struct retchan_txt *v, *w;

	for (v = p->head ; v ; v = w)
	{
		w = v->next;
		free(v);
	}
	if (p->chan)
		ccp_retchan_close(p->chan);
	free(p);
	return;
}

static void ccp_retchan_fini1(struct retchan_header *p)
{
	struct retchan_txt *v, *w;
	uint4 status;
	assert(p->head);
	v = p->head;
	w = v->next;
	free(v);
	if (!w)
	{
		ccp_retchan_close(p->chan);
		free(p);
	} else
	{
		p->head = w;
		status = ccp_retchan_write(p->chan, w->txt, w->len, ccp_retchan_fini1, p);
		if ((status & 1) == 0)
			ccp_retchan_cleanup(p);
	}
	return;
}

void ccp_retchan_fini(struct retchan_header *p)
{
	uint4 status;

	if(p->head == 0)
		return;
	p->chan = ccp_retchan_open(&p->mbxnam.str);
	if (p->chan)
	{
		status = ccp_retchan_write(p->chan, p->head->txt, p->head->len, ccp_retchan_fini1, p);
		if (status & 1)
			return;
	}
	ccp_retchan_cleanup(p);
	return;
}
