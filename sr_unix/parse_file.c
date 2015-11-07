/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"		/* for struct in_addr */
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_socket.h" /* for using sockaddr and sockaddr_storage */
#include "gtm_netdb.h"
#include "gtm_ipv6.h"

#include "parse_file.h"
#include "io.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "trans_log_name.h"
#include "setzdir.h"
#include "gtmmsg.h" /* for gtm_putmsg */

#define LOCALHOSTNAME "localhost"
#define LOCALHOSTNAME6 "::1"

error_def(ERR_FILENOTFND);
error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_PARBUFSM);
error_def(ERR_PARNORMAL);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

enum	parse_state
{
	NOSTATE,
	NAME,
	DOT1,
	DOT2,
	SLASH
};

GBLREF mval	dollar_zdir;

int4	parse_file(mstr *file, parse_blk *pblk)
{
	struct stat		statbuf;
	struct addrinfo		*ai_ptr = NULL, *localhost_ai_ptr = NULL, *temp_ai_ptr = NULL;
	struct addrinfo		hints;
	mstr			trans, tmp;
	int			status, diff, local_node_len, query_node_len, node_name_len;
	parse_blk		def;
	char			local_node_name[MAX_HOST_NAME_LEN + 1], query_node_name[MAX_HOST_NAME_LEN + 1];
	char			*base, *ptr, *top, *del, *node, *name, *ext, ch;
	char			**hostaddrlist;
	char			def_string[MAX_FBUFF + 1];
	boolean_t		hasnode, hasdir, hasname, hasext, wilddir, wildname;
	enum parse_state	state;
	struct sockaddr_storage	query_sas;
	struct sockaddr		localhost_sa, *localhost_sa_ptr;
	mval			def_trans;
	int			errcode;

	pblk->fnb = 0;
	assert(((unsigned int)pblk->buff_size + 1) <= (MAX_FBUFF + 1));
	/* All callers of parse_blk set buff_size to 1 less than the allocated buffer. This is because buff_size is a char
	 * type (for historical reasons) and so cannot go more than 255 whereas we support a max of 255 characters. So we
	 * allocate buffers that contain one more byte (for the terminating '\0') but dont set that in buff_size. Use
	 * that extra byte for the trans_log_name call.
	 */
	status = TRANS_LOG_NAME(file, &trans, pblk->buffer, pblk->buff_size + 1, dont_sendmsg_on_log2long);
	if (SS_LOG2LONG == status)
		return ERR_PARBUFSM;
	assert(trans.addr == pblk->buffer);

	memset(&def, 0, SIZEOF(def));	/* initial the defaults to zero */
	if (pblk->def1_size > 0)
	{	/* Parse default filespec if supplied */
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
	{	/* No file given, no full path given, or a nodename was specified */
		setzdir(NULL, &def_trans); /* Default current directory if none given */
		assert(0 == dollar_zdir.str.len || /* dollar_zdir not initialized yet, possible thru main() -> gtm_chk_dist() */
		       (def_trans.str.len == dollar_zdir.str.len && /* check if cwd and cached value are the same */
			0 == memcmp(def_trans.str.addr, dollar_zdir.str.addr, def_trans.str.len)));
		if (pblk->fop & F_PARNODE)
		{	/* What we have could be a nodename */
			assert(pblk->fop & F_SYNTAXO);
			while (node < top)
			{
				ch = *node++;
				if (':' == ch)		/* We have nodeness */
					break;
				if ('/' == ch)
				{	/* Not a node - bypass node checking */
					node = top;
					break;
				}
			}

			if (node < top)
			{
				hasnode = TRUE;
				ptr = base = node;	/* Update pointers past node name */

				/* See if the desired (query) node is the local node */
				node_name_len = (int)(node - trans.addr);	/* Scanned node including ':' */
				query_node_len = node_name_len - 1;	/* Pure name length, no ':' on end */
				assert(MAX_HOST_NAME_LEN >= query_node_len);
				assert(0 < query_node_len);
				assert(':' == *(trans.addr + query_node_len));
				memcpy(query_node_name, trans.addr, query_node_len);
				query_node_name[query_node_len] = 0;
				localhost_sa_ptr = NULL;	/* null value needed if not find query node (remote default) */
				CLIENT_HINTS(hints);
				if (0 != (errcode = getaddrinfo(query_node_name, NULL, &hints, &ai_ptr)))
					ai_ptr = NULL;		/* skip additional lookups */
				else
					memcpy((sockaddr_ptr)&query_sas, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
				CLIENT_HINTS(hints);
				if (0 == (errcode = getaddrinfo(LOCALHOSTNAME, NULL, &hints, &localhost_ai_ptr)))
				{
					if (0 == memcmp(localhost_ai_ptr->ai_addr, (sockaddr_ptr)&query_sas,
							localhost_ai_ptr->ai_addrlen))
						localhost_sa_ptr = localhost_ai_ptr->ai_addr;
				}
				FREEADDRINFO(localhost_ai_ptr);
				if (ai_ptr && !localhost_sa_ptr)
				{	/* Have not yet established this is not a local node -- check further */
					GETHOSTNAME(local_node_name, MAX_HOST_NAME_LEN, status);
					if (-1 == status)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								LEN_AND_LIT("gethostname"), CALLFROM, errno);
					CLIENT_HINTS(hints);
					if (0 != (errcode = getaddrinfo(local_node_name, NULL, &hints, &localhost_ai_ptr)))
						localhost_ai_ptr = NULL;	/* empty address list */
					for (temp_ai_ptr = localhost_ai_ptr; temp_ai_ptr!= NULL;
					     temp_ai_ptr = temp_ai_ptr->ai_next)
					{
						if (0 == memcmp((sockaddr_ptr)&query_sas, temp_ai_ptr->ai_addr,
								 temp_ai_ptr->ai_addrlen))
						{
							localhost_sa_ptr = temp_ai_ptr->ai_addr;
							break;		/* Tiz truly a local node */
						}
					}
					FREEADDRINFO(localhost_ai_ptr);
				}
				if (ai_ptr && !localhost_sa_ptr)
				{
					CLIENT_HINTS(hints);
					if (0 != (errcode = getaddrinfo(LOCALHOSTNAME6, NULL, &hints, &localhost_ai_ptr)))
						localhost_ai_ptr = NULL;	/* empty address list */
					for (temp_ai_ptr = localhost_ai_ptr; temp_ai_ptr!= NULL;
					     temp_ai_ptr = temp_ai_ptr->ai_next)
					{
						if (0 == memcmp((sockaddr_ptr)&query_sas, temp_ai_ptr->ai_addr,
								 temp_ai_ptr->ai_addrlen))
						{
							localhost_sa_ptr = temp_ai_ptr->ai_addr;
							break;		/* Tiz truly a local node */
						}
					}
					FREEADDRINFO(localhost_ai_ptr);
				}
				if (!localhost_sa_ptr)		/* Not local (or an unknown) host given */
				{	/* Remote node specified -- don't apply any defaults */
					FREEADDRINFO(ai_ptr);
					pblk->l_node = trans.addr;
					pblk->b_node = node_name_len;
					pblk->l_dir = base;
					pblk->b_dir = top - base;
					pblk->l_name = pblk->l_ext = base + pblk->b_dir;
					pblk->b_esl = pblk->b_node + pblk->b_dir;
					pblk->b_name = pblk->b_ext = 0;
					pblk->fnb |= (hasnode << V_HAS_NODE);
					return ERR_PARNORMAL;
				}
				FREEADDRINFO(ai_ptr);
				/* Remove local node name from filename buffer */
				assert(0 < trans.len - node_name_len);
				memmove(trans.addr, node, trans.len - node_name_len);
				ptr = base = node -= node_name_len;
				top -= node_name_len;
				trans.len -= node_name_len;
				if ('/' == *base)	/* No default directory if full path given */
					def_trans.str.len = 0;
			} else
			{	/* Supplied text was not a node -- reset pointer back to beginning for rescan */
				node = trans.addr;
			}
		}

		/* If parse buffer is not large enough, return error */
		if (def_trans.str.len + trans.len > pblk->buff_size)
			return ERR_PARBUFSM;

		/* Construct full filename to parse prefixing given filename with default path prefix */
		if (0 < def_trans.str.len)
		{
			memmove(ptr + def_trans.str.len, ptr, trans.len);
			memcpy(ptr, def_trans.str.addr, def_trans.str.len);
			assert('/' == ptr[def_trans.str.len - 1]);
			ptr += def_trans.str.len;
			top += def_trans.str.len;
		}
	}

	name = ptr;
	state = NOSTATE;
	for (;ptr < top;)
	{
		ch = *ptr;
		if ('.' == ch)
		{	/* Could be /./ or /../ or name.name */
			ptr++;
			state = (DOT1 == state) ? ((DOT2 == state) ? NAME : DOT2) : DOT1;
		} else if (ch == '/')
		{	/* We must still be doing the path */
			ptr++;

			hasdir = TRUE;
			hasname = FALSE;
			hasext = FALSE;
			wilddir |= wildname;
			wildname = FALSE;

			if (DOT1 != state  &&  DOT2 != state && SLASH != state)
			{	/* No dots seen recently so scan as if this is start of filename */
				state = SLASH;
				name = ptr;
				continue;
			}
			if (DOT1 == state)
			{	/* Just remove "./" chars from path */
				del = ptr - 2;
			} else if (DOT2 == state)
			{	/* Have xx/../ construct. Remove /../ and previous level directory from path */
				del = ptr - 4;		/* /../ characters being removed */
				assert ('/' == *del);
				if (del > base)
				{
					del--;
					while ('/' != *del)
						del--;
				}
				assert(del >= base && '/' == *del);
				del++;
			} else if (SLASH == state)
			{	/* Remove duplicate slash from path */
				del = ptr - 1;
				while (ptr < top && '/' == *ptr)
					ptr++;
			}
			memmove(del, ptr, top - ptr);
			diff = (int)(ptr - del);
			ptr -= diff;
			top -= diff;
			state = SLASH;
			name = ptr;
		} else
		{	/* Hopeful of filename */
			hasname = TRUE;
			while (ptr < top)	/* Do small scan looking for filename end */
			{
				ch = *ptr;
				if ('/' == ch)
					break;	/* Ooops, still doing path */
				if ('.' == ch)
				{/* Filename has an extension */
					hasext = TRUE;
					ext = ptr;
				} else if ('?' == ch || '*' == ch)
					wildname = TRUE;
				ptr++;
			}
			state = NAME;
		}
	}
	/* Handle scan end with non-normal state */
	if (SLASH == state || DOT1 == state || DOT2 == state)
	{
		assert(!hasname && !hasext);
		hasdir = TRUE;
		if (state == DOT1)
		{	/* Ignore ./ */
			top--;
			ptr--;
		}
		if (DOT2 == state)
		{	/* ignore ../ plus last directory level specified */
			del = ptr - 3;		/* on the end */
			assert ('/' == *del);
			if (del > base)
			{
				del--;
				while ('/' == *del)
					del--;
			}
			assert(del >= base && '/' == *del);
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
		{	/* Use default filename if we didn't find one */
			diff = (int)(name - node);
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
		{	/* Use default file extension if we didn't find one */
			diff = (int)((ext - node));
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
		diff = (int)(name - base);
		diff = def.b_dir - diff;
		if (def.b_dir + pblk->b_name + pblk->b_ext > pblk->buff_size)
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
	pblk->b_esl = ptr - base;
	pblk->l_dir = base;
	pblk->l_name = base + pblk->b_dir;
	pblk->l_ext = pblk->l_name + pblk->b_name;

	pblk->fnb |= (hasdir << V_HAS_DIR);
	pblk->fnb |= (hasname << V_HAS_NAME);
	pblk->fnb |= (hasext << V_HAS_EXT);
	pblk->fnb |= (wildname << V_WILD_NAME);
	pblk->fnb |= (wilddir << V_WILD_DIR);

	if (!(pblk->fop & F_SYNTAXO)  &&  !wilddir)
	{
		assert('/' == pblk->l_dir[pblk->b_dir - 1]);
		if (pblk->b_dir > 1)
		{
			pblk->l_dir[pblk->b_dir - 1] = 0;
			STAT_FILE(pblk->l_dir, &statbuf, status);
			pblk->l_dir[pblk->b_dir - 1] = '/';
			if (-1 == status || !(statbuf.st_mode & S_IFDIR))
				return ERR_FILENOTFND;
		}
	}

	return ERR_PARNORMAL;
}
