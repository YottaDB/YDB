/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <sys/types.h>
#include <signal.h>
#include "gtm_unistd.h"
#include "gtm_string.h"

#include "send_msg.h"
#include "error.h"
#include "stringpool.h"
#include "util.h"
#include "op.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "anticipatory_freeze.h"
#include "gtm_caseconv.h"

error_def(ERR_BADZPEEKARG);
error_def(ERR_BADZPEEKFMT);
error_def(ERR_BADZPEEKRANGE);
error_def(ERR_MAXSTRLEN);
error_def(ERR_ZPEEKNORPLINFO);

#define FMTHEXDGT(spfree, digit) *spfree++ = digit + ((digit <= 9) ? '0' : ('A' - 0x0A))
#define ZPEEKDEFFMT		"C"
#define ZPEEKDEFFMT_LEN 	(SIZEOF(ZPEEKDEFFMT) - 1)
#define ARGUMENT_MAX_LEN	MAX_MIDENT_LEN

/* Codes for peek operation mnemonics */
#define PO_CSAREG	0	/* Region information - sgmnt_addrs struct - process private structure */
#define PO_FHREG	1	/* Fileheader information from sgmnt_data for specified region */
#define PO_GDRREG	2	/* Region information - gd_region struct - process private structure */
#define PO_NLREG	3	/* Fileheader information from node_local for specified region (transient - non permanent) */
#define PO_NLREPL	4	/* Fileheader information from node_local for replication dummy region */
#define PO_GLFREPL	5	/* Replication information from gtmsrc_lcl_array structure */
#define PO_GSLREPL	6	/* Replication information from gtmsource_local_array structure */
#define PO_JPCREPL	7	/* Replication information from jnlpool_ctl structure */
#define PO_PEEK		8	/* Generalized peek specifying (base) address argument */
#define PO_RIHREPL	9	/* Replication information from repl_inst_hdr structure */
#define PO_RPCREPL	10	/* Replication information from recvpool_ctl_struct */
#define PO_UPLREPL	11	/* Replication information from upd_proc_local_struct */
#define PO_GRLREPL	12	/* Replication information from gtmrecv_local_struct */
#define PO_UHCREPL	13	/* Replication information from upd_helper_ctl */

GBLREF boolean_t        created_core;
GBLREF sigset_t		blockalrm;
GBLREF gd_addr		*gd_header;
GBLREF boolean_t	pool_init;
GBLREF boolean_t	jnlpool_init_needed;
GBLREF jnlpool_addrs	jnlpool;
GBLREF recvpool_addrs	recvpool;
DEBUG_ONLY(GBLREF boolean_t ok_to_UNWIND_in_exit_handling;)

LITDEF mval literal_zpeekdeffmt = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, ZPEEKDEFFMT_LEN, (char *)ZPEEKDEFFMT, 0, 0);
LITREF unsigned char lower_to_upper_table[];

STATICFNDCL void op_fnzpeek_signal_handler(int sig, siginfo_t *info, void *context);
STATICFNDCL int op_fnzpeek_stpcopy(char *zpeekadr, int len, mval *ret, char fmtcode);
STATICFNDCL uchar_ptr_t op_fnzpeek_uint64fmt(uchar_ptr_t p, gtm_uint64_t n);
STATICFNDCL uchar_ptr_t op_fnzpeek_hexfmt(uchar_ptr_t p, gtm_uint64_t n, int fmtlen);
STATICFNDEF boolean_t op_fnzpeek_attach_jnlpool(void);
STATICFNDEF boolean_t op_fnzpeek_attach_recvpool(void);

typedef struct
{
	int		peekop;		/* Peek operation mnemonic id */
	boolean_t	allowargs;	/* Number of arguments allowed */
} zpeek_data_typ;

/* Lookup tables for first argument - Note names are limited to NAME_ENTRY_SZ bytes each */
LITDEF nametabent zpeek_names[] =
{					/* Array offsets */
	{3, "CSA"}, {6, "CSAREG"}	/* 0, 1 */
	,{2, "FH"}, {5, "FHREG"}	/* 2, 3 */
	,{3, "GDR"}, {6, "GDRREG"}	/* 4, 5 */
	,{3, "GLF"}, {7, "GLFREPL"}	/* 6, 7 */
	,{3, "GRL"}, {7, "GRLREPL"}	/* 8, 9 */
	,{3, "GSL"}, {7, "GSLREPL"}	/* 10, 11 */
	,{3, "JPC"}, {7, "JPCREPL"}	/* 12, 13 */
	,{2, "NL"}, {5, "NLREG"}	/* 14, 15 */
	,{6, "NLREPL"}			/* 16 */
	,{4, "PEEK"}			/* 17 */
	,{3, "RIH"}, {7, "RIHREPL"}	/* 18, 19 */
	,{3, "RPC"}, {7, "RPCREPL"}	/* 20, 21 */
	,{3, "UHC"}, {7, "UHCREPL"}	/* 22, 23 */
	,{3, "UPL"}, {7, "UPLREPL"}	/* 24, 25 */
	                                /* Total length 26 */
};
LITDEF unsigned char zpeek_index[] =
{
	 0,  0,  0,  2,  2,  2,  4, 12, 12,	/* a b c d e f g h i */
	12, 14, 14, 14, 14, 17, 17, 18, 18,	/* j k l m n o p q r */
	22, 22, 22, 26, 26, 26, 26, 26, 26	/* s t u v w x y z ~ */
};
LITDEF zpeek_data_typ zpeek_data[] =
{
	{PO_CSAREG, 1}, {PO_CSAREG, 1}
	,{PO_FHREG, 1}, {PO_FHREG, 1}
	,{PO_GDRREG, 1}, {PO_GDRREG, 1}
	,{PO_GLFREPL, 1}, {PO_GLFREPL, 1}
	,{PO_GRLREPL, 0}, {PO_GRLREPL, 0}
	,{PO_GSLREPL, 1}, {PO_GSLREPL, 1}
	,{PO_JPCREPL, 0}, {PO_JPCREPL, 0}
	,{PO_NLREG, 1}, {PO_NLREG, 1}
	,{PO_NLREPL, 0}
	,{PO_PEEK, 1}
	,{PO_RIHREPL, 0}, {PO_RIHREPL, 0}
	,{PO_RPCREPL, 0}, {PO_RPCREPL, 0}
	,{PO_UHCREPL, 0}, {PO_UHCREPL, 0}
	,{PO_UPLREPL, 0}, {PO_UPLREPL, 0}
};

/* Condition handler for use during copy of memory range to the stringpool for return. Note this condition handler is itself
 * never tripped but serves as an unwind target for the signal handler defined below (see its comments).
 */
CONDITION_HANDLER(op_fnzpeek_ch)
{
	START_CH;
	NEXTCH;		/* In the unlikely event it gets driven, just be a pass-thru */
}

/* $ZPEEK() is processing a process memory range specified by an M routine so is definitely capable of getting
 * user inspired address type exceptions. We protect against this by setting up our signal handler to catch any
 * such exceptions for the duration of this routine and just unwind them so we can throw a non-fatal error
 * message instead.
 */
void op_fnzpeek_signal_handler(int sig, siginfo_t *info, void *context)
{
	/* We basically want to do UNWIND(NULL, NULL) logic but the UNWIND macro can only be used in a condition
	 * handler so next is a block that pretends it is our condition handler and does the needful. Note in order
	 * for this to work, we need to be wrapped in a condition handler even if that condition handler is never
	 * actually invoked to serve as the target for the UNWIND().
	 */
	{	/* Needs new block since START_CH declares a new var used in UNWIND() */
		int arg = 0;	/* Needed for START_CH macro if debugging enabled */
		START_CH;
		DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
		UNWIND(NULL, NULL);
	}
}

/* Routine to convert gtm_uint64_t to ascii value not losing any precision. Routine is based on i2asc() but
 * uses gtm_uint64_t as the type.
 */
STATICFNDCL uchar_ptr_t op_fnzpeek_uint64fmt(uchar_ptr_t p, gtm_uint64_t n)
{
	unsigned char	ar[MAX_DIGITS_IN_INT8], *q;
	gtm_uint64_t	m;
	int		len;

	q = ar + SIZEOF(ar);
	if (!n)
		*--q = '0';
	else
	{
		while (n)
		{
			m = n / 10;
			*--q = n - (m * 10) + '0';
			n = m;
		}
	}
	assert((uintszofptr_t)q >= (uintszofptr_t)ar);
	len = (unsigned int)(ar + SIZEOF(ar) - q);
	memcpy(p, q, len);
	return p + len;
}

/* Routine to format hex output to given length with format 0xhh<hh<hhhh<hhhhhhhh>>>. Similar to i2asclx().
 *
 * p		- Output buffer (generally stringpool.free)
 * n		- Hex value to format
 * fmtlen	- Length in bytes of output value
 */
STATICFNDCL uchar_ptr_t op_fnzpeek_hexfmt(uchar_ptr_t p, gtm_uint64_t n, int fmtlen)
{
	unsigned char	ar[MAX_HEX_DIGITS_IN_INT8], *q;
	int		m, digits;

	q = ar + SIZEOF(ar);
	for (digits = fmtlen; (0 < digits); --digits)
	{
		m = n & 0xF;
		if (m <= 9)
			*--q = m + '0';
		else
			*--q = m - 0xa + 'A';
		n = n >> 4;
	}
	assert(0 == n);		/* Verify complete number has been output (no truncated digits) */
	memcpy(p, q, fmtlen);
	return p + fmtlen;
}

/* Routine to extract and optionally format the requested data leaving it in the stringpool. This routine is protected
 * by a signal handler for data access against SIGSEGV or SIGBUS signals. Note the fields that are sub-integer (1 or
 * 2 bytes) are pulled into integer forms before processing.
 */
STATICFNDEF int op_fnzpeek_stpcopy(char *zpeekadr, int len, mval *ret, char fmtcode)
{
	unsigned int	uint;
	boolean_t	negative;
	gtm_uint64_t	uint64;
	unsigned char	*numstrstart, *numstrend;
	unsigned char	*hexchr, *hexchrend, hexc, hexdgt, *spfree;

	ESTABLISH_RET(op_fnzpeek_ch, ERR_BADZPEEKRANGE);		/* If get an exception, likely due to bad range */
	ret->mvtype = 0;						/* Prevent GC of incomplete field */
	switch(fmtcode)
	{
		case 'S':						/* Null terminated string processing */
			STRNLEN(zpeekadr, len, len);			/* Reset len to actual len, fall into "C" processing */
			/* warning - fall through */
		case 'C':						/* Character area (no processing - just copy */
			if (len > MAX_STRLEN)
			{	/* Requested string return is too large */
				REVERT;
				return ERR_MAXSTRLEN;
			}
			ENSURE_STP_FREE_SPACE(len);
			memcpy(stringpool.free, zpeekadr, len);
			ret->str.addr = (char *)stringpool.free;
			ret->str.len = len;
			stringpool.free += len;
			break;
		case 'I':						/* Initially, treat signed/unsigned the same */
		case 'U':
			negative = FALSE;
			switch(len)
			{
				case SIZEOF(gtm_uint64_t):
					/* Dealing with 8 byte integer style values is not GT.M's forte since its internal
					 * number scheme is limited to 20 digits. So use our own routine to do the conversion.
					 * Note: we could use this routine for all the below cases but on 32 bit platforms
					 * with no native 8 byte values, they would run far slower so only use this for the
					 * 8 byte values we deal with.
					 */
					uint64 = *(gtm_uint64_t *)zpeekadr;
					if ('I' == fmtcode)
					{	/* If signed, check if need to add minus sign to value and change value to
						 * positive.
						 */
						negative = (0 > (gtm_int64_t)uint64);
						if (negative)
							uint64 = (gtm_uint64_t)(-(gtm_int64_t)uint64);
					}
					fmtcode = 'u';			/* Change fmtcode to skip negative value check below */
					break;
				case SIZEOF(unsigned int):
					uint = *(unsigned int *)zpeekadr;
					break;
				case SIZEOF(short):
					uint = (unsigned int)*(unsigned short *)zpeekadr;
					break;
				case SIZEOF(char):
					uint = (unsigned int)*(unsigned char *)zpeekadr;
					break;
				default:
					REVERT;
					return ERR_BADZPEEKFMT;
			}
			if ('I' == fmtcode)
			{	/* If signed, check if need to add minus sign to value and change value to positive. Note this test
				 * is bypassed for uint64 types because the check is already made (in a differet/longer value).
				 */
				negative = (0 > (signed int)uint);
				if (negative)
					uint = (unsigned int)(-(signed int)uint);
			}
			ENSURE_STP_FREE_SPACE(MAX_DIGITS_IN_INT + negative);	/* Space to hold # */
			numstrstart = stringpool.free;
			if (negative)
				*stringpool.free++ = '-';		/* Value is negative, record in output */
			/* Use the correct formmating routine based on size */
			numstrend = (SIZEOF(gtm_uint64_t) != len) ? i2asc(stringpool.free, uint)
				: op_fnzpeek_uint64fmt(stringpool.free, uint64);
			ret->str.addr = (char *)numstrstart;
			ret->str.len = INTCAST(numstrend - numstrstart);
			stringpool.free = numstrend;
			break;
		case 'X':						/* Hex format for numeric values */
			switch(len)
			{
				case SIZEOF(gtm_uint64_t):
					uint64 = *(gtm_uint64_t *)zpeekadr;
					break;
				case SIZEOF(unsigned int):
					uint64 = (gtm_uint64_t)*(unsigned int *)zpeekadr;
					break;
				case SIZEOF(unsigned short):
					uint64 = (gtm_uint64_t)*(unsigned short *)zpeekadr;
					break;
				case SIZEOF(unsigned char):
					uint64 = (gtm_uint64_t)*(unsigned char *)zpeekadr;
					break;
				default:
					REVERT;
					return ERR_BADZPEEKFMT;
			}
			ENSURE_STP_FREE_SPACE((len * 2) + 2);
			numstrstart = stringpool.free;
			*stringpool.free++ = '0';
			*stringpool.free++ = 'x';
			numstrend = op_fnzpeek_hexfmt(stringpool.free, uint64, (len * 2));
			ret->str.addr = (char *)numstrstart;
			ret->str.len = INTCAST(numstrend - numstrstart);
			stringpool.free = numstrend;
			break;
		case 'Z':						/* Hex format (no 0x prefix) of storage as it exists */
			if ((len * 2) > MAX_STRLEN)
			{	/* Requested string return is too large */
				REVERT;
				return ERR_MAXSTRLEN;
			}
			ENSURE_STP_FREE_SPACE(len * 2);			/* Need enough space for hex string */
			spfree = stringpool.free;
			ret->str.addr = (char *)spfree;
			hexchr = (unsigned char *)zpeekadr;
			hexchrend = hexchr + len;
			if (hexchr > hexchrend)				/* Wrapped address - range error */
			{
				REVERT;
				return ERR_BADZPEEKRANGE;
			}
			for (; hexchr < hexchrend; ++hexchr)
			{	/* Format 2 digits in each character encountered */
				hexc = *hexchr;
				hexdgt = (hexc & 0xF0) >> 4;
				FMTHEXDGT(spfree, hexdgt);
				hexdgt = (hexc & 0x0F);
				FMTHEXDGT(spfree, hexdgt);
			}
			stringpool.free = spfree;			/* "commit" string to stringpool */
			ret->str.len = len * 2;
			break;
		default:
			REVERT;
			return ERR_BADZPEEKARG;
	}
	REVERT;
	ret->mvtype = MV_STR;
	return 0;
}

/* A condition handler for when we are attaching to either the jnlpool or the gtmrecv pool. We don't
 * care why we can't get to them. On the fact that we can't is material for $ZPEEK().
 */
CONDITION_HANDLER(op_fnzpeek_getpool_ch)
{
	START_CH;
	if (DUMPABLE)
		NEXTCH;		/* Let next (more robust) handler deal with it */
	UNWIND(NULL, NULL);
}

/* Attach to the journal pool. Separate routine so can be wrapped in a condition handler */
STATICFNDEF boolean_t op_fnzpeek_attach_jnlpool(void)
{
	ESTABLISH_RET(op_fnzpeek_getpool_ch, FALSE);
	jnlpool_init(GTMRELAXED, FALSE, NULL);		/* Attach to journal pool */
	REVERT;
	return pool_init;
}

/* Attach to the receive pool. Separate routine so can be wrapped in a condition handler */
STATICFNDEF boolean_t op_fnzpeek_attach_recvpool(void)
{
	ESTABLISH_RET(op_fnzpeek_getpool_ch, FALSE);
	recvpool_init(GTMZPEEK, FALSE);			/* Attach to receive pool */
	REVERT;
	return ((NULL != recvpool.recvpool_ctl) && recvpool.recvpool_ctl->initialized);
}

/* Generalized peek facility:
 *
 * structid - String that describes the structure
 * offset   - Offset of item within that structure.
 * len      - Length of the fetch.
 * format   - Option format character - codes described below
 * ret	    - Return mval
 */
void	op_fnzpeek(mval *structid, int offset, int len, mval *format, mval *ret)
{
	void			*zpeekadr;
	UINTPTR_T		prmpeekadr;
	struct sigaction	new_action, prev_action_bus, prev_action_segv;
	sigset_t		savemask;
	int			errtoraise, rslt;
	char			fmtcode;
	boolean_t		arg_supplied, attach_success;
	unsigned char		mnemonic[NAME_ENTRY_SZ], *nptr, *cptr, *cptrend, *argptr;
	int			mnemonic_len, mnemonic_index, mnemonic_opcode, arglen, arryidx;
	gd_region		*r_top, *r_ptr;
	replpool_identifier	replpool_id;
	unsigned int		full_len;
	unsigned char		argument_uc_buf[ARGUMENT_MAX_LEN];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Make sure lookup table is setup correctly */
	assert(zpeek_index[26] == (SIZEOF(zpeek_names) / SIZEOF(nametabent)));
	assert((SIZEOF(zpeek_names) / SIZEOF(nametabent)) == (SIZEOF(zpeek_data) / SIZEOF(zpeek_data_typ)));
	/* Initialize */
	fmtcode = 'C';			/* If arg is NULL string (noundef default), provide default */
	MV_FORCE_STR(structid);
	if (MV_DEFINED(format))
	{
		MV_FORCE_STR(format);
	} else format = (mval *)&literal_zpeekdeffmt;	/* Cast to avoid compiler warning about dropping readonly type attributes */
	/* Parse and lookup the first arg's mnemonic and arg (if supplied) */
	for (nptr = mnemonic, cptr = (unsigned char *)structid->str.addr, cptrend = cptr + structid->str.len;
	     cptr < cptrend; ++cptr)
	{
		if (':' == *cptr)
			break;		/* End of mnemonic, start of arg */
		*nptr++ = *cptr;
	}
	arg_supplied = (cptr < cptrend);
	mnemonic_len = INTCAST(nptr - mnemonic);
	mnemonic_index = namelook(zpeek_index, zpeek_names, (char *)mnemonic, mnemonic_len);
	if (0 > mnemonic_index)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("mnemonic type"));
	mnemonic_opcode = zpeek_data[mnemonic_index].peekop;
	if ((arg_supplied && !zpeek_data[mnemonic_index].allowargs) || (!arg_supplied && zpeek_data[mnemonic_index].allowargs))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("mnemonic argument"));
	if (arg_supplied)
	{	/* Parse supplied argument */
		argptr = ++cptr;	/* Bump past ":" - if now have end-of-arg then arg is missing */
		if (argptr == cptrend)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("mnemonic argument"));
		arglen = INTCAST(cptrend - cptr);
		if (ARGUMENT_MAX_LEN < arglen)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("mnemonic argument"));
		switch(mnemonic_opcode)
		{
			case PO_CSAREG:			/* These types have a region name argument */
			case PO_FHREG:
			case PO_GDRREG:
			case PO_NLREG:
				/* Uppercase the region name since that is what GDE does when creating them */
				lower_to_upper(argument_uc_buf, argptr, arglen);
				argptr = argument_uc_buf;	/* Reset now to point to upper case version */
				/* See if region recently used so can avoid lookup */
				if ((arglen == TREF(zpeek_regname_len)) && (0 == memcmp(argptr, TADR(zpeek_regname), arglen)))
				{	/* Fast path - no lookup necessary */
					r_ptr = TREF(zpeek_reg_ptr);
					assert(r_ptr->open && !r_ptr->was_open);	/* Make sure truly open */
					break;
				}
				/* Region now defined - make sure it is open */
				if (!gd_header)		/* If gd_header is NULL, open gbldir */
					gvinit();
				r_ptr = gd_header->regions;
				for (r_top = r_ptr + gd_header->n_regions; ; r_ptr++)
				{
					if (r_ptr >= r_top)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2,
							      RTS_ERROR_LITERAL("mnemonic argument (region name)"));
					if ((r_ptr->rname_len == arglen) && (0 == memcmp(r_ptr->rname, argptr, arglen)))
						break;
				}
				if (!r_ptr->open)
					gv_init_reg(r_ptr);
				/* Cache new region access for followup references */
				memcpy(TADR(zpeek_regname), argptr, arglen);
				TREF(zpeek_regname_len) = arglen;
				TREF(zpeek_reg_ptr) = r_ptr;
				/* r_ptr now points to (open) region */
				assert(r_ptr->open && !r_ptr->was_open);	/* Make sure truly open */
				break;
			case PO_GLFREPL:		/* These types have an array index argument */
			case PO_GSLREPL:
				arryidx = asc2i(argptr, arglen);
				if ((0 > arryidx) || (NUM_GTMSRC_LCL < arryidx))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2,
						      RTS_ERROR_LITERAL("mnemonic argument (array index)"));
				break;
			case PO_PEEK:			/* Argument is address of form 0Xhhhhhhhh[hhhhhhhh] */
				if (('0' != *cptr++) || ('x' != *cptr) && ('X' != *cptr))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2,
						      RTS_ERROR_LITERAL("mnemonic argument (peek base address)"));
				cptr++;			/* Bump past 'x' or 'X' - rest of arg should be hex value */
				prmpeekadr = (UINTPTR_T)GTM64_ONLY(asc_hex2l)NON_GTM64_ONLY(asc_hex2i)(cptr, arglen - 2);
				if (-1 == (INTPTR_T)prmpeekadr)
					/* Either an error occurred or the user specified the maximum address. So it's
					 * either an error from the conversion routine or an otherwise useless value.
					 */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2,
						      RTS_ERROR_LITERAL("mnemonic argument (peek base address)"));
				break;
			default:
				assert(FALSE);		/* Only the above types should ever have an argument */
		}
	}
	/* Figure out the address of each block to return */
	switch(mnemonic_opcode)
	{
		case PO_CSAREG:		/* r_ptr set from option processing */
			zpeekadr = &FILE_INFO(r_ptr)->s_addrs;
			break;
		case PO_FHREG:		/* r_ptr set from option processing */
			zpeekadr = (&FILE_INFO(r_ptr)->s_addrs)->hdr;
			break;
		case PO_GDRREG:		/* r_ptr set from option processing */
			zpeekadr = r_ptr;
			break;
		case PO_NLREG:		/* r_ptr set from option processing */
			zpeekadr = (&FILE_INFO(r_ptr)->s_addrs)->nl;
			break;
		case PO_GLFREPL:	/* This set of opcodes all require the journal pool to be initialized. Verify it */
		case PO_GSLREPL:
		case PO_JPCREPL:
		case PO_NLREPL:
		case PO_RIHREPL:
			/* Make sure jnlpool_addrs are availble */
			if (!REPL_INST_AVAILABLE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZPEEKNORPLINFO);
			if (!pool_init)
			{
				attach_success = op_fnzpeek_attach_jnlpool();
				if (!attach_success)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZPEEKNORPLINFO);
			}
			switch(mnemonic_opcode)
			{
				case PO_GLFREPL:	/* arryidx set by option processing */
					zpeekadr = (jnlpool.gtmsrc_lcl_array + arryidx);
					break;
				case PO_GSLREPL:	/* arryidx set by option processing */
					zpeekadr = (jnlpool.gtmsource_local_array + arryidx);
					break;
				case PO_NLREPL:
					zpeekadr = (&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs)->nl;
					break;
				case PO_JPCREPL:
					zpeekadr = jnlpool.jnlpool_ctl;
					break;
				case PO_RIHREPL:
					zpeekadr = jnlpool.repl_inst_filehdr;
					break;
				default:
					assert(FALSE);
			}
			break;
		case PO_RPCREPL:	/* This set of opcodes all require the receive pool to be initialized. Verify it */
		case PO_GRLREPL:
		case PO_UPLREPL:
		case PO_UHCREPL:
			/* Make sure recvpool_addrs are available */
			if (!REPL_INST_AVAILABLE)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZPEEKNORPLINFO);
			if (NULL == recvpool.recvpool_ctl)
			{
				attach_success = op_fnzpeek_attach_recvpool();
				if (!attach_success)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZPEEKNORPLINFO);
			}
			switch(mnemonic_opcode)
			{
				case PO_RPCREPL:
					zpeekadr = recvpool.recvpool_ctl;
					break;
				case PO_GRLREPL:
					zpeekadr = recvpool.gtmrecv_local;
					break;
				case PO_UPLREPL:
					zpeekadr = recvpool.upd_proc_local;
					break;
				case PO_UHCREPL:
					zpeekadr = recvpool.upd_helper_ctl;
					break;
				default:
					assert(FALSE);
			}
			break;
		case PO_PEEK:		/* prmpeekadr set up in argument processing */
			zpeekadr = (void *)prmpeekadr;
			break;
		default:
			assert(FALSE);
	}
	assert(NULL != zpeekadr);
	/* Check the rest of the args */
	if (0 > offset)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("offset"));
	zpeekadr = (void *)((char *)zpeekadr + offset);
	if ((0 > len) || (MAX_STRLEN < len))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("length"));
	if (1 < format->str.len)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("format"));
	else if (1 == format->str.len)
	{	/* Validate format option */
		fmtcode = *format->str.addr;
		fmtcode = lower_to_upper_table[fmtcode];
		switch(fmtcode)
		{
			case 'C':	/* Character data - returned as is */
			case 'I':	/* Signed integer format - up to 31 bits */
			case 'S':	/* String data - Same as 'C' except string is NULL terminated */
			case 'U':	/* Unsigned integer format - up to 64 bits */
			case 'X':	/* Humeric hex format: e.g. 0x12AB. Total length is (2 * bytes) + 2 */
			case 'Z':	/* Hex format - not treated as numeric. Shown as occurs in memory (subject to endian)
					 * and is returned with no 0x prefix.
					 */
				break;
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADZPEEKARG, 2, RTS_ERROR_LITERAL("format"));
		}
	}
	/* Block out timer calls that might trigger processing that could fail. We especially want to prevent
	 * nesting of signal handlers since the longjump() function used by the UNWIND macro is undefined on
	 * Tru64 when signal handlers are nested.
	 */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);
	/* Setup new signal handler to just drive condition handler which will do the right thing */
	memset(&new_action, 0, SIZEOF(new_action));
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
#	ifdef __sparc
	new_action.sa_handler = op_fnzpeek_signal_handler;
#	else
	new_action.sa_sigaction = op_fnzpeek_signal_handler;
#	endif
	sigaction(SIGBUS, &new_action, &prev_action_bus);
	sigaction(SIGSEGV, &new_action, &prev_action_segv);
	/* Attempt to copy return string to stringpool which protected by our handlers. If the copy completes, the return
	 * mval is updated to point to the return string. Even errors return here so these sigactions can be reversed.
	 */
	errtoraise = op_fnzpeek_stpcopy(zpeekadr, len, ret, fmtcode);
	/* Can restore handlers now that access verified */
	sigaction(SIGBUS, &prev_action_bus, NULL);
	sigaction(SIGSEGV, &prev_action_segv, NULL);
	/* Let the timers pop again.. */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
	/* If we didn't complete correctly, raise error */
	if (0 != errtoraise)
	{	/* The only time ERR_BADZPEEKARG is driven is when the format code is not recognized so give that error
		 * specifically with the additional args. Else just raise the error.
		 */
		if (ERR_BADZPEEKARG == errtoraise)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) errtoraise, 2, RTS_ERROR_LITERAL("format"));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errtoraise);
	}
	return;
}
