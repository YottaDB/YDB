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

/*
 *	ZCALL Table Definitions
 *
 * NOTES:
 *   1.	Any changes made here must also be reflected in ZC_CALL.MAR (for VAX)
 *	or in ZC_CALL.M64 (for Alpha);
 *   2.	In order for these definitions to be portable across both VAX and Alpha
 *	platforms, the structure types defined here must be padded, if necessary,
 *	such that their declared sizes (as returned by the SIZEOF operator ) are
 *	a multiple of the alignment boundary of their most stringently aligned
 *	member.  (This is only because the respective compilers would otherwise
 *	compute SIZEOF differently .)
 */

typedef	struct zctabrtn_type
	{
		unsigned short		entry_length;
		unsigned char		n_inputs;
		unsigned char		n_outputs;
		unsigned char		*entrypoint;
		unsigned char		outbnd_reset;
		char			padding;	/* make SIZEOF(zctabrtn) == 12 */
		unsigned char		callnamelen;
		unsigned char		callname[1];	/* variable size > 0 */
	}	zctabrtn;

typedef	struct zctabret_type
	{
		unsigned char		class;
		unsigned char		type;
	}	zctabret;

typedef	struct zctabinput_type
	{
		unsigned char		mechanism;
		unsigned char		type;
		unsigned char		position;
		unsigned char		qualifier;
		char			*value;
	}	zctabinput;

typedef	struct zctaboutput_type
	{
		unsigned char		mechanism;
		unsigned char		type;
		unsigned char		position;
		unsigned char		qualifier;
		int4			value;
	}	zctaboutput;

typedef struct zcpackage_type
	{
		zctabrtn	*begin, *end;
		unsigned char	*packname;
	}	zcpackage;
