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

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_string.h"

#include "parse_file.h"
#include "io.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "gtm_uname.h"
#include "trans_log_name.h"

GBLREF mval dollar_zdir;

#define MAX_NODE_NAME 32

enum	parse_state
{
	NOSTATE,
	NAME,
	DOT1,
	DOT2,
	SLASH
};

int4	parse_file (mstr *file, parse_blk *pblk)
{
	struct stat		statbuf;
	mstr			trans, def_trans, tmp;
	int			status, diff;
	short			nodelen;
	parse_blk		def;
	char			node_name[MAX_NODE_NAME + 1];
	char			*base, *ptr, *top, *del, *node, *name, *ext, ch;
	char			def_string[MAX_FBUFF + 1];
	bool			hasnode, hasdir, hasname, hasext, wilddir, wildname;
	enum parse_state	state;

	error_def(ERR_PARNORMAL);
	error_def(ERR_PARBUFSM);
	error_def(ERR_PARTRNLNM);
	error_def(ERR_FILENOTFND);

	pblk->fnb = 0;
	assert(pblk->buff_size <= MAX_FBUFF);
	status = trans_log_name(file, &trans, pblk->buffer);
	assert(trans.addr == pblk->buffer);

	memset(&def, 0, sizeof(def));	/* initial the defaults to zero */
	if (pblk->def1_size > 0)
	{
		def.fop = F_SYNTAXO;
		def.buffer = def_string;
		def.buff_size = MAX_FBUFF;
		def.def1_size = pblk->def2_size;
		def.def1_buf = pblk->def2_buf;
		tmp.len = pblk->def1_size;
		tmp.addr = pblk->def1_buf;
		if ((status = parse_file(&tmp, &def)) != ERR_PARNORMAL)
			return status;

		assert(!def.b_node);
		if (def.b_dir)	def.fnb |= F_HAS_DIR;
		if (def.b_name)	def.fnb |= F_HAS_NAME;
		if (def.b_ext)	def.fnb |= F_HAS_EXT;
	}

	wildname = wilddir = hasnode = hasdir = hasname = hasext = FALSE;

	node = base = ptr = trans.addr;
	top = ptr + trans.len;
	if (trans.len == 0  ||  *ptr != '/')
	{
		def_trans = dollar_zdir.str;
		if (pblk->fop & F_PARNODE)
		{
			assert(pblk->fop & F_SYNTAXO);
			while (node < top)
			{
				ch = *node++;
				if (ch == ':')
					break;
				if (ch == '/')
				{
					node = top;
					break;
				}
			}

			if (node < top)
			{
				hasnode = TRUE;
				ptr = base = node;

				node_name[MAX_NODE_NAME] = 0;
				gtm_uname(node_name, MAX_NODE_NAME);
				nodelen = strlen(node_name);
				if (node - trans.addr - 1 != nodelen  ||  memcmp(trans.addr, node_name, nodelen) != 0)
				{
					/* if a non-local node, don't apply any defaults */
					pblk->l_node = trans.addr; pblk->b_node = node - trans.addr;
					pblk->l_dir = base; pblk->b_dir = top - base;
					pblk->l_name = pblk->l_ext = base + pblk->b_dir;
					pblk->b_esl = pblk->b_node + pblk->b_dir;
					pblk->b_name = pblk->b_ext = 0;
					pblk->fnb |= (hasnode << V_HAS_NODE);
					return ERR_PARNORMAL;
				}
				if (*base == '/')
					def_trans.len = 0;
			}
			node = trans.addr;
		}

		if (def_trans.len + trans.len > pblk->buff_size)
			return ERR_PARBUFSM;

		if (def_trans.len > 0)
		{
			memmove(ptr + def_trans.len, ptr, trans.len);
			memcpy(ptr, def_trans.addr, def_trans.len);
			assert(ptr[def_trans.len - 1] == '/');

			ptr += def_trans.len;
			top += def_trans.len;
		}
	}

	name = ptr;
	state = NOSTATE;
	for (;ptr < top;)
	{
		ch = *ptr;
		if (ch == '.')
		{
			ptr++;
			state = (state == DOT1) ? ((state == DOT2) ? NAME : DOT2) : DOT1;
		}
		else if (ch == '/')
		{
			ptr++;

			hasdir = TRUE;
			hasname = FALSE;
			hasext = FALSE;
			wilddir |= wildname;
			wildname = FALSE;

			if (state != DOT1  &&  state != DOT2  &&  state != SLASH)
			{
				state = SLASH;
				name = ptr;
				continue;
			}
			if (state == DOT1)
				del = ptr - 2;
			else if (state == DOT2)
			{
				del = ptr - 4;		/* /../ characters being removed */
				assert (*del == '/');
				if (del > base)
				{
					del--;
					while (*del != '/')
						del--;
				}
				assert(del >= base  &&  *del == '/');
				del++;
			}
			else if (state == SLASH)
			{
				del = ptr - 1;
				while (ptr < top  &&  *ptr == '/')
					ptr++;
			}
			memcpy (del, ptr, top - ptr);
			diff = ptr - del;
			ptr -= diff; top -= diff;
			state = SLASH;
			name = ptr;
		}
		else
		{
			hasname = TRUE;
			while (ptr < top)
			{
				ch = *ptr;
				if (ch == '/')
					break;
				if (ch == '.')
				{
					hasext = TRUE;
					ext = ptr;
				}
				else if (ch == '?'  ||  ch == '*')
					wildname = TRUE;
				ptr++;
			}
			state = NAME;
		}
	}
	if (state == SLASH  ||  state == DOT1  ||  state == DOT2)
	{
		assert(!hasname  &&  !hasext);
		hasdir = TRUE;
		if (state == DOT1)
		{
			top--; ptr--;
		}
		if (state == DOT2)
		{
			del = ptr - 3;		/* on the end */
			assert (*del == '/');
			if (del > base)
			{
				del--;
				while (*del != '/')
					del--;
			}
			assert(del >= base  &&  *del == '/');
			del++;
			ptr = top = del;
			name = ptr;
		}
	}

	if (!hasname)
	{
		assert(!hasext);
		name = ptr;
		if (def.fnb & F_HAS_NAME)
		{
			diff = name - node;
			if (def.b_name + diff > pblk->buff_size)
				return ERR_PARBUFSM;
			memcpy(name, def.l_name, def.b_name);
			ptr += def.b_name;
		}
		ext = ptr;
	}
	if (!hasext)
	{
		ext = ptr;
		if (def.fnb & F_HAS_EXT)
		{
			diff = ext - node;
			if (def.b_ext + diff > pblk->buff_size)
				return ERR_PARBUFSM;
			memcpy(ext, def.l_ext, def.b_ext);
			ptr += def.b_ext;
		}
	}
	pblk->b_name = ext - name;
	pblk->b_ext = ptr - ext;

	if (!hasdir  &&  (def.fnb & F_HAS_DIR))
	{
		diff = name - base;
		diff = def.b_dir - diff;
		if (pblk->b_node + def.b_dir + pblk->b_name + pblk->b_ext > pblk->buff_size)
			return ERR_PARBUFSM;
		if (diff > 0)
			memmove(name + diff, name, pblk->b_name + pblk->b_ext);
		else if (diff < 0)
			memcpy(name + diff, name, pblk->b_name + pblk->b_ext);
		memcpy(base, def.l_dir, def.b_dir);
		ptr += diff;
		name += diff;
	}
	pblk->b_dir = name - base;
	pblk->b_esl = pblk->b_node + ptr - base;
	pblk->l_node = node;
	pblk->l_dir = base;
	pblk->l_name = base + pblk->b_dir;
	pblk->l_ext = pblk->l_name + pblk->b_name;

	pblk->fnb |= (hasdir << V_HAS_DIR);
	pblk->fnb |= (hasname << V_HAS_NAME);
	pblk->fnb |= (hasext << V_HAS_EXT);
	pblk->fnb |= (wildname << V_WILD_NAME);
	pblk->fnb |= (wilddir << V_WILD_DIR);
	pblk->fnb |= (hasnode << V_HAS_NODE);

	if (!(pblk->fop & F_SYNTAXO)  &&  !wilddir)
	{
		assert(pblk->l_dir[pblk->b_dir - 1] == '/');
		if (pblk->b_dir > 1)
		{
			pblk->l_dir[pblk->b_dir - 1] = 0;
			STAT_FILE(pblk->l_dir, &statbuf, status);
			pblk->l_dir[pblk->b_dir - 1] = '/';
			if (status == -1  ||  !(statbuf.st_mode & S_IFDIR))
				return ERR_FILENOTFND;
		}
	}

	return ERR_PARNORMAL;
}
