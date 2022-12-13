/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "have_crit.h"
#include "io.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "trans_log_name.h"
#include "setzdir.h"
#include "gtmmsg.h" /* for gtm_putmsg */
#include "min_max.h"
#include "dogetaddrinfo.h"

#include "gtm_limits.h" /* next two header for realpath */
#include "gtm_stdlib.h"
#define LOCALHOSTNAME "localhost"
#define LOCALHOSTNAME6 "::1"

/*
 *  Assert ensures that SRC.FIELD is within the bounds of SRC.buffer. This way when we do the SRC.FIELD - SRC.Buffer,
 *  we are guaranteed to get a positive number that is less than SRC.buff_size
 */
#define	COPY_POINTER(SRC, DST, FIELD)							\
{											\
	assert((uintptr_t)SRC.FIELD >= (uintptr_t)SRC.buffer);				\
	assert((uintptr_t)SRC.FIELD <= ((uintptr_t)SRC.buffer + SRC.buff_size));	\
	DST->FIELD = DST->buffer + ((uintptr_t)SRC.FIELD - (uintptr_t)SRC.buffer);	\
}

enum	parse_state
{
	NOSTATE,
	NAME,
	DOT1,
	DOT2,
	SLASH
};

GBLREF mval dollar_zdir;

int4 parse_file(mstr *file, parse_blk *pblk)
{
	struct stat		statbuf;
	struct addrinfo		*ai_ptr, *localhost_ai_ptr, *temp_ai_ptr, hints;
	mstr			trans, tmp, symlinkfile;
	int			status, diff;
	uint4			local_node_len, query_node_len;
	parse_blk		def, symlink_pblk;
	char			local_node_name[MAX_HOST_NAME_LEN + 1], query_node_name[MAX_HOST_NAME_LEN + 1];
	char			*base, *ptr, *top, *del, *node, *name, *ext, ch;
	char			**hostaddrlist;
	char			def_string[MAX_FN_LEN + 1], symlink_pblk_string[MAX_FN_LEN + 1];
	char			symlink_path[YDB_PATH_MAX]; /* buffer holding the target file name in case of a symbolic link */
	boolean_t		hasdir, hasname, hasext, wilddir, wildname;
	enum parse_state	state;
	struct sockaddr_storage	query_sas;
	struct sockaddr		localhost_sa, *localhost_sa_ptr;
	mval			def_trans;
	int			errcode;
<<<<<<< HEAD
	boolean_t		donot_short_circuit;
	/* Buffer Variables for symlink */
	char symlink_buffer[YDB_PATH_MAX];
=======
	intrpt_state_t		prev_intrpt_state;
	size_t			move_len;		/* try to give static analysis more assurance on mmemove() params */
>>>>>>> 732d6f04 (GT.M V7.0-005)

	pblk->fnb = 0;
	ai_ptr = localhost_ai_ptr = temp_ai_ptr = NULL;
	assert(((unsigned int)pblk->buff_size + 1) <= (MAX_FN_LEN + 1));
	/* All callers of parse_blk set buff_size to 1 less than the allocated buffer. This is because buff_size is a char
	 * type (for historical reasons) and so cannot go more than 255 whereas we support a max of 255 characters. So we
	 * allocate buffers that contain one more byte (for the terminating '\0') but dont set that in buff_size. Use
	 * that extra byte for the trans_log_name call.
	 */
	status = trans_log_name(file, &trans, pblk->buffer, pblk->buff_size + 1, dont_sendmsg_on_log2long);
	if (SS_LOG2LONG == status)
		return ERR_FILEPATHTOOLONG;
	assert(trans.addr == pblk->buffer);
	assert(0 <= trans.len);
	typedef char transbuf_t[trans.len];	/* let static analysis infer our buffer size */
	transbuf_t *delptr, *trans_ptr;
	memset(&def, 0, SIZEOF(def));	/* Initial the defaults to zero */
	if (pblk->def1_size > 0)
	{	/* Parse default filespec if supplied */
		def.fop = F_SYNTAXO;
		def.buffer = def_string;
		def.buff_size = MAX_FN_LEN;
		def.def1_size = pblk->def2_size;
		def.def1_buf = pblk->def2_buf;
		tmp.len = pblk->def1_size;
		tmp.addr = pblk->def1_buf;
		if (ERR_PARNORMAL != (status = parse_file(&tmp, &def)))	/* Note Assignment */
			return status;
		assert(!def.b_node);
		if (def.b_dir)	def.fnb |= F_HAS_DIR;
		if (def.b_name)	def.fnb |= F_HAS_NAME;
		if (def.b_ext)	def.fnb |= F_HAS_EXT;
	}
	wildname = wilddir = hasdir = hasname = hasext = FALSE;
	node = base = ptr = trans.addr;
	top = ptr + trans.len;
	if ((0 == trans.len) || ('/' != *ptr))
	{	/* No file given, no full path given, or a nodename was specified */
		setzdir(NULL, &def_trans); /* Default current directory if none given */
		assert((0 == dollar_zdir.str.len) /* dollar_zdir not initialized yet, possible thru main() -> ydb_chk_dist() */
			|| ((def_trans.str.len == dollar_zdir.str.len) /* Check if cwd and cached value are the same */
				&& (0 == memcmp(def_trans.str.addr, dollar_zdir.str.addr, def_trans.str.len))));
		if (pblk->fop & F_PARNODE)
		{	/* What we have could be a nodename */
			boolean_t hasnode;

			assert(pblk->fop & F_SYNTAXO);
			/* A file specification could be any of the the following forms
			 *	<filepath>
			 *	<hostname>:<filepath>
			 *	@<hostname>:<filepath>
			 * where <hostname> is a host name that does not contain either '@' or ':'.
			 * In case '@' is not specified, we will check <hostname> to see if it is a local host
			 * and if so short-circuit the remote access and not go through GTCM GNP.
			 * In case '@' is specified, we will go through GTCM GNP even if it is a local host (#191).
			 */
			if ('@' == *node)
			{
				donot_short_circuit = TRUE;
				/* Now that we have seen the '@', remove it from the nodename */
				ptr++;
				node = base = trans.addr = ptr;
				trans.len--;
			} else
				donot_short_circuit = FALSE;
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
				uint4 node_name_len;

				assert(':' == ch);
				hasnode = TRUE;
				base = node;				/* Update pointers past node name */
				/* See if the desired (query) node is the local node */
				node_name_len = (uint4)(node - trans.addr);	/* Scanned node including ':' */
<<<<<<< HEAD
				if (!donot_short_circuit)
				{
					query_node_len = node_name_len - 1;		/* Pure name length, no ':' on end */
					assert(MAX_HOST_NAME_LEN >= query_node_len);
					assert(0 < query_node_len);
					assert(':' == *(trans.addr + query_node_len));
					memcpy(query_node_name, trans.addr, query_node_len);
					query_node_name[query_node_len] = 0;
					localhost_sa_ptr = NULL; /* Null value needed if not find query node (remote default) */
					CLIENT_HINTS(hints);
					errcode = dogetaddrinfo(query_node_name, NULL, &hints, &ai_ptr);
					if (0 == errcode)
=======
				query_node_len = MIN((node_name_len - 1), MAX_HOST_NAME_LEN); /* Pure name length, no ':' on end */
				assert(MAX_HOST_NAME_LEN >= query_node_len);
				assert(0 < query_node_len);
				assert(':' == *(trans.addr + query_node_len));
				memcpy(query_node_name, trans.addr, query_node_len);
				query_node_name[query_node_len] = 0;
				localhost_sa_ptr = NULL;	/* Null value needed if not find query node (remote default) */
				CLIENT_HINTS(hints);
				DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				if (0 != (errcode = getaddrinfo(query_node_name, NULL, &hints, &ai_ptr)))	/* Assignment! */
					ai_ptr = NULL;		/* Skip additional lookups */
				else
					memcpy((sockaddr_ptr)&query_sas, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
				CLIENT_HINTS(hints);
				if (0 == (errcode = getaddrinfo(LOCALHOSTNAME, NULL, &hints, &localhost_ai_ptr))
					&& (0 == memcmp(localhost_ai_ptr->ai_addr, (sockaddr_ptr)&query_sas,
						localhost_ai_ptr->ai_addrlen)))
				{
					localhost_sa_ptr = localhost_ai_ptr->ai_addr;
				}
				FREEADDRINFO(localhost_ai_ptr);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				if (ai_ptr && !localhost_sa_ptr)
				{	/* Have not yet established this is not a local node -- check further */
					GETHOSTNAME(local_node_name, MAX_HOST_NAME_LEN, status);
					if (-1 == status)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_LIT("gethostname"), CALLFROM, errno);
					CLIENT_HINTS(hints);
					DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					if (0 != (errcode = getaddrinfo(local_node_name, NULL, &hints, &localhost_ai_ptr)))
						localhost_ai_ptr = NULL;	/* Empty address list */
					for (temp_ai_ptr = localhost_ai_ptr; temp_ai_ptr!= NULL;
					     temp_ai_ptr = temp_ai_ptr->ai_next)
>>>>>>> 732d6f04 (GT.M V7.0-005)
					{
						memcpy((sockaddr_ptr)&query_sas, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
						CLIENT_HINTS(hints);
						if (0 == dogetaddrinfo(LOCALHOSTNAME, NULL, &hints, &localhost_ai_ptr)
							&& (0 == memcmp(localhost_ai_ptr->ai_addr, (sockaddr_ptr)&query_sas,
												localhost_ai_ptr->ai_addrlen)))
						{
							localhost_sa_ptr = localhost_ai_ptr->ai_addr;
						}
<<<<<<< HEAD
						FREEADDRINFO(localhost_ai_ptr);
						if (!localhost_sa_ptr)
						{	/* Have not yet established this is not a local node -- check further */
							GETHOSTNAME(local_node_name, MAX_HOST_NAME_LEN, status);
							if (-1 == status)
								RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
										LEN_AND_LIT("gethostname"), CALLFROM, errno);
							CLIENT_HINTS(hints);
							errcode = dogetaddrinfo(local_node_name, NULL, &hints, &localhost_ai_ptr);
							if (0 != errcode)
								localhost_ai_ptr = NULL;	/* Empty address list */
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
						if (!localhost_sa_ptr)
=======
					}
					FREEADDRINFO(localhost_ai_ptr);
					ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				}
				if (ai_ptr && !localhost_sa_ptr)
				{
					CLIENT_HINTS(hints);
					DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					if (0 != (errcode = getaddrinfo(LOCALHOSTNAME6, NULL, &hints, &localhost_ai_ptr)))
						localhost_ai_ptr = NULL;	/* Empty address list */
					for (temp_ai_ptr = localhost_ai_ptr; temp_ai_ptr!= NULL;
					     temp_ai_ptr = temp_ai_ptr->ai_next)
					{
						if (0 == memcmp((sockaddr_ptr)&query_sas, temp_ai_ptr->ai_addr,
								 temp_ai_ptr->ai_addrlen))
>>>>>>> 732d6f04 (GT.M V7.0-005)
						{
							CLIENT_HINTS(hints);
							errcode = dogetaddrinfo(LOCALHOSTNAME6, NULL, &hints, &localhost_ai_ptr);
							if (0 != errcode)
								localhost_ai_ptr = NULL;	/* Empty address list */
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
						FREEADDRINFO(ai_ptr);
						/* At this point, if "localhost_sa_ptr" is NULL, a hostname that is not local
						 * (or an unknown hostname) is given. If it is non-NULL, a local hostname is given.
						 * Set "hasnode" to TRUE only for the non-local case.
						 * Treat local host as if node was not specified.
						 */
						hasnode = (NULL == localhost_sa_ptr);
					}
<<<<<<< HEAD
=======
					FREEADDRINFO(localhost_ai_ptr);
					ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
>>>>>>> 732d6f04 (GT.M V7.0-005)
				}
				if (hasnode)
				{	/* Remote node specified -- don't apply any defaults */
<<<<<<< HEAD
=======
					DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					FREEADDRINFO(ai_ptr);
					ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
>>>>>>> 732d6f04 (GT.M V7.0-005)
					pblk->l_node = trans.addr;
					pblk->b_node = node_name_len;
					pblk->l_dir = base;
					pblk->b_dir = top - base;
					pblk->l_name = pblk->l_ext = base + pblk->b_dir;
					pblk->b_esl = pblk->b_node + pblk->b_dir;
					pblk->b_name = pblk->b_ext = 0;
					pblk->fnb |= (1 << V_HAS_NODE);
					return ERR_PARNORMAL;
				}
<<<<<<< HEAD
=======
				DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				FREEADDRINFO(ai_ptr);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
>>>>>>> 732d6f04 (GT.M V7.0-005)
				/* Remove local node name from filename buffer */
				assert(trans.len > node_name_len);	/* this is a function of how we determined node_name_len */
				trans_ptr = (transbuf_t *) trans.addr;
				move_len =  MIN(trans.len - node_name_len, MAX_FN_LEN);
				assert (sizeof(transbuf_t) >= move_len);
				assert(NULL != trans_ptr);
				memmove((void *)trans_ptr, node, move_len);
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
			return ERR_FILEPATHTOOLONG;
		/* Construct full filename to parse prefixing given filename with default path prefix */
		if (0 < def_trans.str.len)
		{
			memmove(ptr + def_trans.str.len, ptr, MIN(trans.len, MAX_FN_LEN));
			memcpy(ptr, def_trans.str.addr, def_trans.str.len);
			assert('/' == ptr[def_trans.str.len - 1]);
			ptr += def_trans.str.len;
			top += def_trans.str.len;
		}
	}
	name = ptr;
	state = NOSTATE;
	for (; ptr < top;)
	{
		ch = *ptr;
		if (('.' == ch) && (DOT2 != state))
		{	/* Could be /./ or /../ or name.name */
			ptr++;
			state = ((DOT1 == state) ? DOT2 : DOT1);
		} else if (ch == '/')
		{	/* We must still be doing the path */
			ptr++;
			hasdir = TRUE;
			hasname = FALSE;
			hasext = FALSE;
			wilddir |= wildname;
			wildname = FALSE;
			if ((DOT1 != state) && (DOT2 != state) && (SLASH != state))
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
				assert((del >= base) && ('/' == *del));
				del++;
			} else if (SLASH == state)
			{	/* Remove duplicate slash from path */
				del = ptr - 1;
				while ((ptr < top) && ('/' == *ptr))
					ptr++;
			}
			assert(top >= ptr);	/* Use asserts to tell static analyis that we have thought about this memmove() */
			move_len = top - ptr;
			assert((0 <= move_len) && (trans.len >= move_len)) ;
			assert(NULL != ptr);
			assert(NULL != del);
			assert((del <= ptr) && (del >= trans.addr));
			delptr = (transbuf_t *) del;
			memmove((void *) delptr, ptr, move_len);
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
				} else if (('?' == ch) || ('*' == ch))
					wildname = TRUE;
				ptr++;
			}
			state = NAME;
		}
	}
	/* Handle scan end with non-normal state */
	if ((SLASH == state) || (DOT1 == state) || (DOT2 == state))
	{
		assert(!hasname && !hasext);
		hasdir = TRUE;
		if (state == DOT1)
		{	/* Ignore ./ */
			top--;
			ptr--;
		}
		if (DOT2 == state)
		{	/* Ignore ../ plus last directory level specified */
			del = ptr - 3;		/* on the end */
			assert ('/' == *del);
			if (del > base)
			{
				del--;
				while ('/' != *del)
					del--;
			}
			assert((del >= base) && ('/' == *del));
			del++;
			ptr = del;
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
				return ERR_FILEPATHTOOLONG;
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
				return ERR_FILEPATHTOOLONG;
			memcpy(ext, def.l_ext, def.b_ext);
			ptr += def.b_ext;
		}
	}
	pblk->b_name = ext - name;
	pblk->b_ext = ptr - ext;
	if (!hasdir && (def.fnb & F_HAS_DIR))
	{
		diff = (int)(name - base);
		diff = def.b_dir - diff;
		if ((pblk->b_name + pblk->b_ext + ((0 < def.b_dir) ? def.b_dir : 0 )) > pblk->buff_size)
			return ERR_FILEPATHTOOLONG;
		if (0 < diff)
			memmove(name + diff, name, pblk->b_name + pblk->b_ext);	/*return ERR_FILEPATHTOOLONG ensures this is safe*/
		else if (0 > diff)
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
	if (!(pblk->fop & F_SYNTAXO) && !wilddir)
	{
		assert('/' == pblk->l_dir[pblk->b_dir - 1]);
		if (pblk->b_dir > 1)
		{
			pblk->l_dir[pblk->b_dir - 1] = 0;
			STAT_FILE(pblk->l_dir, &statbuf, status);
			pblk->l_dir[pblk->b_dir - 1] = '/';
			if ((-1 == status) || !(statbuf.st_mode & S_IFDIR))
				return ERR_FILENOTFND;
		}
	}
	if (pblk->symlink)
	{	/* $ZPARSE(...,"SYMLINK") has been specified so check if the result file is a soft link.
		 * If so return the file that is linked to. "realpath()" call below does that symbolic link
		 * translation as well as absolute path name expansion for us.
		 */
		pblk->buffer[pblk->b_esl] = '\0';	/* It is okay because all callers of "parse_blk()" set buff_size
							 * to 1 less than the allocated buffer. Therefore we can safely
							 * access to set the null terminator.
							 */
		/* Now that the file name is null terminated, pass it to "realpath()" */
		if (NULL == realpath(pblk->buffer, symlink_path))
		{	/* There weren't any symbolic linked files found for the link. Return original "pblk" information as is. */
			return ERR_PARNORMAL;
		}
		/* Now that we know the input file is a soft link to another valid file, invoke "parse_file()" on that file
		 * to determine (and return) the name, dir, extension information of that new file.
		 */
		memset(&symlink_pblk, 0, SIZEOF(symlink_pblk));
		symlink_pblk.buffer = symlink_pblk_string; /* Buffer passed to child function */
		symlink_pblk.buff_size = SIZEOF(symlink_pblk_string) - 1;	/* leave space for null terminator at end */
		symlinkfile.addr = symlink_path;
		symlinkfile.len = strlen(symlink_path);
		if (ERR_PARNORMAL != parse_file(&symlinkfile, &symlink_pblk))
		{	/* The soft link existed at the time of "realpath()" a few lines above but failed the "parse_file()"
			 * call. Not sure why. In this case, just return the original "parse_file()" call information
			 * of the linking file (which is already there in "pblk" so return ERR_PARNORMAL.
			 */
			return ERR_PARNORMAL;
		}
		/* Now that "parse_file()" on the linked file succeeded, copy over information from "symlink_pblk" to "pblk".
		 * This involves copying over the full file name from the source buffer to the target buffer and adjusting
		 * all pointer members in "pblk" to reflect the respective elements (directory, file, extension etc.)
		 * of the copied over file name.
		 */
		assert(symlink_pblk.buff_size <= pblk->buff_size);
		memcpy(pblk->buffer, symlink_pblk.buffer, symlink_pblk.b_esl); /* Copy over file name */
		/* Copy over all length fields. They stay the same. */
		pblk->b_name = symlink_pblk.b_name;
		pblk->b_dir = symlink_pblk.b_dir;
		pblk->b_ext = symlink_pblk.b_ext;
		pblk->b_esl = symlink_pblk.b_esl;
		/* Note: pblk->b_node not copied over since it is applicable at db open only */
		/* Copy over result fields */
		pblk->fnb = symlink_pblk.fnb;
		/* Update all pointers in "pblk" so their relative offsets stay the same in "pblk" and "symlink_pblk" */
		COPY_POINTER(symlink_pblk, pblk, l_dir);
		COPY_POINTER(symlink_pblk, pblk, l_name);
		COPY_POINTER(symlink_pblk, pblk, l_ext);
		/* Note: pblk->l_node not updated since it is applicable at db open only */
	}
	return ERR_PARNORMAL;
}
