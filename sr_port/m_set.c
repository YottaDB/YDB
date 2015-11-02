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

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "nametabtyp.h"
#include "toktyp.h"
#include "funsvn.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "namelook.h"
#include "cmd.h"
#include "svnames.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLDEF bool		temp_subs;
GBLREF triple		*curtchain;
GBLREF char		director_token, window_token;
GBLREF mident		window_ident;
GBLREF boolean_t	gtm_utf8_mode;
GBLREF boolean_t	badchar_inhibit;

LITREF unsigned char	svn_index[], fun_index[];
LITREF nametabent	svn_names[], fun_names[];
LITREF svn_data_type	svn_data[];
LITREF fun_data_type	fun_data[];

/* This macro is used to insert the conditional jump triples (in SET $PIECE/$EXTRACT) ahead of the global variable
 * reference of the SET $PIECE target. This is to ensure the naked indicator is not touched in cases where the M-standard
 * says it should not be. e.g. set $piece(^x,"delim",2,1) should not touch naked indicator since 2>1.
 */
#define	DQINSCURTARGCHAIN(curtargtriple)			\
{								\
	dqins(curtargchain, exorder, curtargtriple);		\
	assert(curtargchain->exorder.fl == curtargtriple);	\
	curtargchain = curtargtriple;				\
}

void	m_set_create_temporaries(triple *sub, opctype put_oc);

int m_set(void)
{
	int		index, setop, delimlen;
	int		first_val_lit, last_val_lit;
	boolean_t	first_is_lit, last_is_lit, got_lparen, delim1char, is_extract, valid_char;
	opctype		put_oc;
	oprtype		v, delimval, firstval, lastval, *result, resptr;
	triple		*delimiter, *first, *put, *get, *last, *obp, *s, *sub, *s0, *s1;
	triple		targchain, *curtargchain, *save_curtchain, *tmp;
	triple		*jmptrp1, *jmptrp2;
	mint		delimlit;
	mval		*delim_mval;
	union
	{
		uint4		unichar_val;
		unsigned char	unibytes_val[4];
	} unichar;
	boolean_t	parse_warn;

	error_def(ERR_INVSVN);
	error_def(ERR_VAREXPECTED);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_EQUAL);
	error_def(ERR_COMMA);
	error_def(ERR_SVNOSET);

	temp_subs = FALSE;
	dqinit(&targchain, exorder);
	result = (oprtype *)mcalloc(sizeof(oprtype));
	resptr = put_indr(result);
	delimiter = sub = last = NULL;

	if (got_lparen = (window_token == TK_LPAREN))
	{
		advancewindow();
		temp_subs = TRUE;
	}

	/* Some explanation: The triples from the left hand side of the SET expression that are
	 * expressly associated with fetching (in case of set $piece/$extract) and/or storing of
	 * the target value are removed from curtchain and placed on the targchain. Later, these
	 * triples will be added to the end of curtchain to do the finishing store of the target
	 * after the righthand side has been evaluated. This is per the M standard.
	 *
	 * Note that SET $PIECE/$EXTRACT have special conditions in which the first argument is not referenced at all.
	 * (e.g. set $piece(^a," ",3,2) in this case 3 > 2 so this should not evaluate ^a and therefore should not
	 * modify the naked indicator). That is, the triples that do these conditional checks need to be inserted
	 * ahead of the OC_GVNAME of ^a, all of which need to be inserted on the targchain. But the conditionalization
	 * can be done only after parsing the first argument of the SET $PIECE and examining the remaining arguments.
	 * Therefore we maintain the "curtargchain" variable which stores the value of the "targchain" at the beginning
	 * of the iteration (at the start of the $PIECE parsing) and all the conditionalization will be inserted right
	 * here which is guaranteed to be ahead of where the OC_GVNAME gets inserted.
	 *
	 * For example, SET $PIECE(^A(x,y),delim,first,last)=RHS will generate a final triple chain as follows
	 *
	 *	A - Triples to evaluate subscripts (x,y) of the global ^A
	 *	A - Triples to evaluate delim
	 *	A - Triples to evaluate first
	 *	A - Triples to evaluate last
	 *	B - Triples to evaluate RHS
	 *	C - Triples to do conditional check (e.g. first > last etc.)
	 *	C - Triples to branch around if the checks indicate this is a null operation SET $PIECE
	 *	D - Triple that does OC_GVNAME of ^A
	 *	D - Triple that does OC_SETPIECE to determine the new value
	 *	D - Triple that does OC_GVPUT of the new value into ^A(x,y)
	 *	This is the point where the conditional check triples will branch around to if they chose to.
	 *
	 *	A - triples that evaluates the arguments/subscripts in the left-hand-side of the SET command
	 *		These triples are built in "curtchain"
	 *	B - triples that evaluates the arguments/subscripts in the right-hand-side of the SET command
	 *		These triples are built in "curtchain"
	 *	C - triples that do conditional check for any $PIECE/$EXTRACT in the left side of the SET command.
	 *		These triples are built in "curtargchain"
	 *	D - triples that generate the reference to the target of the SET and the store into the target.
	 *		These triples are built in "targchain"
	 */
	for (;;)
	{
		curtargchain = targchain.exorder.bl;
		jmptrp1 = jmptrp2 = NULL;
		delim1char = is_extract = FALSE;
		switch (window_token)
		{
		case TK_IDENT:
			if (!lvn(&v, OC_PUTINDX, 0))
				return FALSE;
			if (v.oprval.tref->opcode == OC_PUTINDX)
			{
				dqdel(v.oprval.tref, exorder);
				dqins(targchain.exorder.bl, exorder, v.oprval.tref);
				sub = v.oprval.tref;
				put_oc = OC_PUTINDX;
				if (temp_subs)
					m_set_create_temporaries(sub, put_oc);
			}
			put = maketriple(OC_STO);
			put->operand[0] = v;
			put->operand[1] = resptr;
			dqins(targchain.exorder.bl, exorder, put);
			break;
		case TK_CIRCUMFLEX:
			s1 = curtchain->exorder.bl;
			if (!gvn())
				return FALSE;
			for (sub = curtchain->exorder.bl; sub != s1; sub = sub->exorder.bl)
			{
				put_oc = sub->opcode;
				if (OC_GVNAME == put_oc || OC_GVNAKED == put_oc || OC_GVEXTNAM == put_oc)
					break;
			}
			assert(OC_GVNAME == put_oc || OC_GVNAKED == put_oc || OC_GVEXTNAM == put_oc);
			dqdel(sub, exorder);
			dqins(targchain.exorder.bl, exorder, sub);
			if (temp_subs)
				m_set_create_temporaries(sub, put_oc);
			put = maketriple(OC_GVPUT);
			put->operand[0] = resptr;
			dqins(targchain.exorder.bl, exorder, put);
			break;
		case TK_ATSIGN:
			if (!indirection(&v))
				return FALSE;
			if (!got_lparen && window_token != TK_EQUAL)
			{
				put = newtriple(OC_COMMARG);
				put->operand[0] = v;
				put->operand[1] = put_ilit(indir_set);
				return TRUE;
			}
			put = maketriple(OC_INDSET);
			put->operand[0] = v;
			put->operand[1] = resptr;
			dqins(targchain.exorder.bl, exorder, put);
			break;
		case TK_DOLLAR:
			advancewindow();
			if (window_token != TK_IDENT)
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			parse_warn = FALSE;
			if (director_token != TK_LPAREN)
			{	/* Look for intrinsic special variables */
				s1 = curtchain->exorder.bl;
				if (0 > (index = namelook(svn_index, svn_names, window_ident.addr, window_ident.len)))
				{
					STX_ERROR_WARN(ERR_INVSVN);	/* sets "parse_warn" to TRUE */
				} else if (!svn_data[index].can_set)
				{
					STX_ERROR_WARN(ERR_SVNOSET);	/* sets "parse_warn" to TRUE */
				}
				advancewindow();
				if (!parse_warn)
				{
					if (SV_ETRAP != svn_data[index].opcode && SV_ZTRAP != svn_data[index].opcode)
					{	/* Setting of $ZTRAP or $ETRAP must go through opp_svput because they
						 * may affect the stack pointer. All others directly to op_svput().
						 */
						put = maketriple(OC_SVPUT);
					} else
						put = maketriple(OC_PSVPUT);
					put->operand[0] = put_ilit(svn_data[index].opcode);
					put->operand[1] = resptr;
					dqins(targchain.exorder.bl, exorder, put);
				} else
				{	/* OC_RTERROR triple would have been inserted in curtchain by ins_errtriple
					 * (invoked by stx_error). To maintain consistency with the "if" portion of
					 * this code, we need to move this triple to the "targchain"
					 */
					tmp = curtchain->exorder.bl; /* corresponds to put_ilit(FALSE) in ins_errtriple */
					tmp = tmp->exorder.bl;	/* corresponds to put_ilit(in_error) in ins_errtriple */
					tmp = tmp->exorder.bl;	/* corresponds to newtriple(OC_RTERROR) in ins_errtriple */
					assert(OC_RTERROR == tmp->opcode);
					dqdel(tmp, exorder);
					dqins(targchain.exorder.bl, exorder, tmp);
					chktchain(&targchain);
				}
				break;
			}
			/* Only 4 function names allowed on left side: $[Z]Piece and $[Z]Extract */
			index = namelook(fun_index, fun_names, window_ident.addr, window_ident.len);
			if (0 > index)
			{
				STX_ERROR_WARN(ERR_INVSVN);	/* sets "parse_warn" to TRUE */
				/* OC_RTERROR triple would have been inserted in "curtchain" by ins_errtriple
				 * (invoked by stx_error). We need to switch it to "targchain" to be consistent
				 * with every other codepath in this module.
				 */
				tmp = curtchain->exorder.bl; /* corresponds to put_ilit(FALSE) in ins_errtriple */
				tmp = tmp->exorder.bl;	/* corresponds to put_ilit(in_error) in ins_errtriple */
				tmp = tmp->exorder.bl;	/* corresponds to newtriple(OC_RTERROR) in ins_errtriple */
				assert(OC_RTERROR == tmp->opcode);
				dqdel(tmp, exorder);
				dqins(targchain.exorder.bl, exorder, tmp);
				chktchain(&targchain);
				advancewindow();	/* skip past the function name */
				advancewindow();	/* skip past the left paren */
				/* Parse the remaining arguments until the corresponding RIGHT-PAREN/SPACE/EOL is reached */
				parse_until_rparen_or_space();
			} else
			{
				switch(fun_data[index].opcode)
				{
				case OC_FNPIECE:
					setop = OC_SETPIECE;
					break;
				case OC_FNEXTRACT:
					is_extract = TRUE;
					setop = OC_SETEXTRACT;
					break;
#ifdef UNICODE_SUPPORTED
				case OC_FNZPIECE:
					setop = OC_SETZPIECE;
					break;
				case OC_FNZEXTRACT:
					is_extract = TRUE;
					setop = OC_SETZEXTRACT;
					break;
#endif
				default:
					stx_error(ERR_VAREXPECTED);
					return FALSE;
				}
				advancewindow();
				advancewindow();
				/* Although we see the get (target) variable first, we need to save it's processing
				 * on another chain -- the targchain -- because the retrieval of the target is bypassed
				 * and the naked indicator is not reset if the first/last parameters are not set in a
				 * logical manner (must be > 0 and first <= last). So the evaluation order is
				 * delimiter (if $piece), first, last, RHS of the set and then the target if applicable.
				 * Set up primary action triple now since it is ref'd by the put triples generated below.
				 */
				s = maketriple(setop);
				/* Even for SET[Z]PIECE and SET[Z]EXTRACT, the SETxxxxx opcodes
				 * do not do the final store, they only create the final value TO be
				 * stored so generate the triples that will actually do the store now.
				 * Note we are still building triples on the original curtchain.
				 */
				switch (window_token)
				{
				case TK_IDENT:
					if (!lvn(&v, OC_PUTINDX, 0))
					{
						stx_error(ERR_VAREXPECTED);
						return FALSE;
					}
					if (v.oprval.tref->opcode == OC_PUTINDX)
					{
						dqdel(v.oprval.tref, exorder);
						dqins(targchain.exorder.bl, exorder, v.oprval.tref);
						sub = v.oprval.tref;
						put_oc = OC_PUTINDX;
						if (temp_subs)
							m_set_create_temporaries(sub, put_oc);
					}
					get = maketriple(OC_FNGET);
					get->operand[0] = v;
					put = maketriple(OC_STO);
					put->operand[0] = v;
					put->operand[1] = put_tref(s);
					break;
				case TK_ATSIGN:
					if (!indirection(&v))
					{
						stx_error(ERR_VAREXPECTED);
						return FALSE;
					}
					get = maketriple(OC_INDGET);
					get->operand[0] = v;
					get->operand[1] = put_str(0, 0);
					put = maketriple(OC_INDSET);
					put->operand[0] = v;
					put->operand[1] = put_tref(s);
					break;
				case TK_CIRCUMFLEX:
					s1 = curtchain->exorder.bl;
					if (!gvn())
						return FALSE;
					for (sub = curtchain->exorder.bl; sub != s1 ; sub = sub->exorder.bl)
					{
						put_oc = sub->opcode;
						if ((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc)
								|| (OC_GVEXTNAM == put_oc))
							break;
					}
					assert((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc)
						|| (OC_GVEXTNAM == put_oc));
					dqdel(sub, exorder);
					dqins(targchain.exorder.bl, exorder, sub);
					if (temp_subs)
						m_set_create_temporaries(sub, put_oc);
					get = maketriple(OC_FNGVGET);
					get->operand[0] = put_str(0, 0);
					put = maketriple(OC_GVPUT);
					put->operand[0] = put_tref(s);
					break;
				default:
					stx_error(ERR_VAREXPECTED);
					return FALSE;
				}
				s->operand[0] = put_tref(get);
				/* Code to fetch args for target triple are on targchain. Put get there now too. */
				dqins(targchain.exorder.bl, exorder, get);
				chktchain(&targchain);

				if (!is_extract)
				{	/* Set $[z]piece */
					delimiter = newtriple(OC_PARAMETER);
					s->operand[1] = put_tref(delimiter);
					first = newtriple(OC_PARAMETER);
					delimiter->operand[1] = put_tref(first);
					/* Process delimiter string ($[z]piece only) */
					if (window_token != TK_COMMA)
					{
						stx_error(ERR_COMMA);
						return FALSE;
					}
					advancewindow();
					if (!strexpr(&delimval))
						return FALSE;
					assert(delimval.oprclass == TRIP_REF);
				} else
				{	/* Set $[Z]Extract */
					first = newtriple(OC_PARAMETER);
					s->operand[1] = put_tref(first);
				}
				/* Process first integer value */
				if (window_token != TK_COMMA)
					firstval = put_ilit(1);
				else
				{
					advancewindow();
					if (!intexpr(&firstval))
						return FALSE;
					assert(firstval.oprclass == TRIP_REF);
				}
				first->operand[0] = firstval;
				if (first_is_lit = (firstval.oprval.tref->opcode == OC_ILIT))
				{
					assert(firstval.oprval.tref->operand[0].oprclass  == ILIT_REF);
					first_val_lit = firstval.oprval.tref->operand[0].oprval.ilit;
				}
				if (window_token != TK_COMMA)
				{	/* There is no "last" value. Only if 1 char literal delimiter and
					   no "last" value can we generate shortcut code to op_set[z]p1 entry
					   instead of op_set[z]piece. Note if UTF8 mode is in effect, then this
					   optimization applies if the literal is one unicode char which may in
					   fact be up to 4 bytes but will still be passed as a single unsigned
					   integer.
					*/
					if (!is_extract)
					{
						delim_mval = &delimval.oprval.tref->operand[0].oprval.mlit->v;
						valid_char = TRUE;	/* Basic assumption unles proven otherwise */
						if (delimval.oprval.tref->opcode == OC_LIT &&
						    (1 == (gtm_utf8_mode ?
							   MV_FORCE_LEN(delim_mval) : delim_mval->str.len)))
						{	/* Single char delimiter for set $piece */
							UNICODE_ONLY(
							if (gtm_utf8_mode)
							{	/* We have a supposed single char delimiter but it must be a
								   valid utf8 char to be used by op_setp1() and MV_FORCE_LEN
								   won't tell us that.
								*/
								valid_char = UTF8_VALID(delim_mval->str.addr,
											(delim_mval->str.addr
											 + delim_mval->str.len),
											delimlen);
								if (!valid_char && !badchar_inhibit)
									UTF8_BADCHAR(0, delim_mval->str.addr,
										(delim_mval->str.addr
											+ delim_mval->str.len),
											 0, NULL);
							}
							);
							if (valid_char || 1 == delim_mval->str.len)
							{	/* This reference to a one character literal or a single
								 * byte invalid utf8 character that needs to be turned into
								 * an explict formated integer literal instead */
								unichar.unichar_val = 0;
								if (!gtm_utf8_mode)
								{	/* Single byte delimiter */
									assert(1 == delim_mval->str.len);
									UNIX_ONLY(s->opcode = OC_SETZP1);
									VMS_ONLY(s->opcode = OC_SETP1);
									unichar.unibytes_val[0] = *delim_mval->str.addr;
								}
								UNICODE_ONLY(
								else
								{	/* Potentially multiple bytes in one int */
									assert(sizeof(int) >= delim_mval->str.len);
									memcpy(unichar.unibytes_val,
									       delim_mval->str.addr,
									       delim_mval->str.len);
									s->opcode = OC_SETP1;
								}
								);
								delimlit = (mint)unichar.unichar_val;
								delimiter->operand[0] = put_ilit(delimlit);
								delim1char = TRUE;
							}
						}
					}
					if (!delim1char)
					{	/* Was not handled as a single char delim by code above either bcause it
						   was (1) not set $piece, or (2) was not a single char delim or (3) it was
						   not a VALID utf8 single char delim and badchar was inhibited.
						*/
						if (!is_extract)
							delimiter->operand[0] = delimval;
						last = newtriple(OC_PARAMETER);
						first->operand[1] = put_tref(last);
						last->operand[0] = first->operand[0];	/* start = end range */
					}
					/* Generate test sequences for first/last to bypass the set operation if
					   first/last are not in a usable form */
					if (first_is_lit)
					{
						if (first_val_lit < 1)
						{
							jmptrp1 = maketriple(OC_JMP);
							DQINSCURTARGCHAIN(jmptrp1);
						}
						/* note else no test necessary since first == last and are > 0 */
					} else
					{	/* Generate test for first being <= 0 */
						jmptrp1 = maketriple(OC_COBOOL);
						jmptrp1->operand[0] = first->operand[0];
						DQINSCURTARGCHAIN(jmptrp1);
						jmptrp1 = maketriple(OC_JMPLEQ);
						DQINSCURTARGCHAIN(jmptrp1);
					}
				} else
				{	/* There IS a last value */
					if (!is_extract)
						delimiter->operand[0] = delimval;
					last = newtriple(OC_PARAMETER);
					first->operand[1] = put_tref(last);
					advancewindow();
					if (!intexpr(&lastval))
						return FALSE;
					assert(lastval.oprclass == TRIP_REF);
					last->operand[0] = lastval;
					/* Generate inline code to test first/last for usability and if found
					   lacking, branch around the getchain and the actual store so we avoid
					   setting the naked indicator so far as the target gvn is concerned.
					*/
					if (last_is_lit = (lastval.oprval.tref->opcode == OC_ILIT))
					{	/* Case 1: last is a literal */
						assert(lastval.oprval.tref->operand[0].oprclass  == ILIT_REF);
						last_val_lit = lastval.oprval.tref->operand[0].oprval.ilit;
						if (last_val_lit < 1 || (first_is_lit && first_val_lit > last_val_lit))
						{	/* .. and first is a literal and one or both of them is no good
							 * so unconditionally branch around the whole thing.
							 */
							jmptrp1 = maketriple(OC_JMP);
							DQINSCURTARGCHAIN(jmptrp1);
						} /* else case actually handled at next 'if' .. */
					} else
					{	/* Last is not literal. Do test if it is greater than 0 */
						jmptrp1 = maketriple(OC_COBOOL);
						jmptrp1->operand[0] = last->operand[0];
						DQINSCURTARGCHAIN(jmptrp1);
						jmptrp1 = maketriple(OC_JMPLEQ);
						DQINSCURTARGCHAIN(jmptrp1);
					}
					if (!last_is_lit || !first_is_lit)
					{	/* Compare to check that last >= first */
						jmptrp2 = maketriple(OC_VXCMPL);
						jmptrp2->operand[0] = first->operand[0];
						jmptrp2->operand[1] = last->operand[0];
						DQINSCURTARGCHAIN(jmptrp2);
						jmptrp2 = maketriple(OC_JMPGTR);
						DQINSCURTARGCHAIN(jmptrp2);
					}
				}
			}
			if (window_token != TK_RPAREN)
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
			advancewindow();
			if (!parse_warn)
			{
				dqins(targchain.exorder.bl, exorder, s);
				dqins(targchain.exorder.bl, exorder, put);
				chktchain(&targchain);
				/* Put result operand on the chain. End of chain depends on whether or not
				   we are calling the shortcut or the full set-piece code */
				if (delim1char)
					first->operand[1] = resptr;
				else
					last->operand[1] = resptr;
				/* Set jump targets if we did tests above. The function "tnxtarg" operates on "curtchain"
				 * but we want to set targets based on "targchain", so temporarily switch them
				 */
				save_curtchain = setcurtchain(&targchain);
				if (NULL != jmptrp1)
					tnxtarg(&jmptrp1->operand[0]);
				if (NULL != jmptrp2)
					tnxtarg(&jmptrp2->operand[0]);
				setcurtchain(save_curtchain);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		if (!got_lparen)
			break;
		if (window_token == TK_COMMA)
			advancewindow();
		else
		{
			if (window_token == TK_RPAREN)
			{
				advancewindow();
				break;
			} else
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
		}
	}
	if (window_token != TK_EQUAL)
	{
		stx_error(ERR_EQUAL);
		return FALSE;
	}
	advancewindow();

	/* Evaluate expression creating triples on the current chain */
	if (!expr(result))
		return FALSE;

	/* Now add in the left-hand side triples */
	obp = curtchain->exorder.bl;
	dqadd(obp, &targchain, exorder);		/* this is a violation of info hiding */
	chktchain(curtchain);
	return TRUE;
}

/* This function adds triples to the execution chain to store the values of subscripts (in glvns in compound SETs)
 * in temporaries (using OC_STOTEMP opcode). This function is only invoked when
 * 	a) The SET is a compound SET (i.e. there are multiple targets specified on the left side of the SET command).
 *	b) Subscripts are specified in glvns which are targets of the SET.
 *
 *	e.g. set a=0,(a,array(a))=1
 *
 * The expected result of the above command as per the M-standard is that array(0) (not array(1)) gets set to 1.
 * That is, the value of the subscript "a" should be evaluated at the start of the compound SET before any sets happen
 * and should be used in any subscripts that refer to the name "a".
 *
 * In the above example, since it is a compound SET and "a" is used in a subscript, we need to store the value of "a"
 *	before the start of the compound SET (i.e.a=0) in a temporary and use that as the subscript for "array".
 *
 * If in the above example the compound set was instead specified as set a=1,array(a)=1, the value of 1 gets substituted
 *	when used in "array(a)".
 *
 * This is where the compound set acts differently from a sequence of multiple sets. This is per the M-standard.
 *
 * In the above example, the subscript used was also a target within the compound SET. It is possible that the
 *	subscript is not also an individual target within the same compound SET. Even in that case, this function
 *	will be called to store the subscript in temporaries (as we dont know at compile time if a particular
 *	subscript is also used as a target within a compound SET).
 */
void	m_set_create_temporaries(triple *sub, opctype put_oc)
{
	oprtype	*sb1;
	triple	*s0, *s1;

	assert(temp_subs);
	sb1 = &sub->operand[1];
	if ((OC_GVNAME == put_oc) || (OC_PUTINDX == put_oc))
	{
		sub = sb1->oprval.tref;		/* global name */
		assert(sub->opcode == OC_PARAMETER);
		sb1 = &sub->operand[1];
	} else if (OC_GVEXTNAM == put_oc)
	{
		sub = sb1->oprval.tref;		/* first env */
		assert(sub->opcode == OC_PARAMETER);
		sb1 = &sub->operand[0];
		assert(sb1->oprclass == TRIP_REF);
		s0 = sb1->oprval.tref;
		if ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode))
		{
			s1 = maketriple(OC_STOTEMP);
			s1->operand[0] = *sb1;
			*sb1 = put_tref(s1);
			s0 = s0->exorder.fl;
			dqins(s0->exorder.bl, exorder, s1);
		}
		sb1 = &sub->operand[1];
		sub = sb1->oprval.tref;		/* second env */
		assert(sub->opcode == OC_PARAMETER);
		sb1 = &sub->operand[0];
		assert(sb1->oprclass == TRIP_REF);
		s0 = sb1->oprval.tref;
		if ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode))
		{
			s1 = maketriple(OC_STOTEMP);
			s1->operand[0] = *sb1;
			*sb1 = put_tref(s1);
			s0 = s0->exorder.fl;
			dqins(s0->exorder.bl, exorder, s1);
		}
		sb1 = &sub->operand[1];
		sub = sb1->oprval.tref;		/* global name */
		assert(sub->opcode == OC_PARAMETER);
		sb1 = &sub->operand[1];
	}
	while (sb1->oprclass)
	{
		assert(sb1->oprclass == TRIP_REF);
		sub = sb1->oprval.tref;
		assert(sub->opcode == OC_PARAMETER);
		sb1 = &sub->operand[0];
		assert(sb1->oprclass == TRIP_REF);
		s0 = sb1->oprval.tref;
		if ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode))
		{
			s1 = maketriple(OC_STOTEMP);
			s1->operand[0] = *sb1;
			*sb1 = put_tref(s1);
			s0 = s0->exorder.fl;
			dqins(s0->exorder.bl, exorder, s1);
		}
		sb1 = &sub->operand[1];
	}
}
