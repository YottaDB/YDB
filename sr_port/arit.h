/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
	arit.h : defines constants for arithmetic operations.
		gdsroot.h has a definition for the MAX_NUM_SUBSC_LEN that
		depends on the number of significant digits available.  If
		it changes from 18, the define must be updated
*/
#define NUM_DEC_DG_1L		   9			/* Number of decimal digits in a int4 		*/
#define NUM_DEC_DG_2L		  18			/* Number of decimal digits in two longs 	*/

#define EXP_IDX_BIAL		  (9+MV_XBIAS)		/* Max. biased exponent index within a int4 	*/
#define EXP_IDX_BIAQ		 (18+MV_XBIAS)		/* Max. biased exponent index within two longs 	*/
#define EXPLO			(-42+MV_XBIAS)		/* Min. biased exponent  			*/
#define EXPHI			 (48+MV_XBIAS)		/* Max. biased exponent  			*/
/* Note: The above values for EXPLO and EXPHI set up a range of 20 thru 109 which still leaves room for expansion
 * in the 7-bit exponent whose full range is 0 thru 127. One might be tempted to increase EXPLO/EXPHI to accommodate
 * the available range but should not because these values also affect how numeric subscripts are stored in the database.
 *
 * The entire numeric range currently supported by GT.M along with how it represents them internally as well as
 * inside the database (the first byte of the numeric subscript) is captured in the table below.
 *
 * -------------------------|----------------------------------------------------------------------------------------
 *            Numeric value |     ...     [-1E46, -1E-43]   ...       [0]      ...     [1E-43, 1E46]     ...
 *  mval.e   representation |     ...      [ 109,   20]     ...       [0]      ...       [ 20, 109]      ...
 *  mval.sgn representation |     ...      [   1,    1]     ...       [0]      ...       [  0,   0]      ...
 *                          |      |            |            |         |        |            |            |
 *                          |      |            |            |         |        |            |            |
 *                          |      |            |            |         |        |            |            |
 *                          |      v            v            v         v        v            v            v
 * subscript representation | [0x00, 0x11] [0x12, 0x6b] [0x6c,0x7F] [0x80] [0x81, 0x93] [0x94, 0xED] [0xEE, 0xFF]
 *       in database        |
 * -------------------------|----------------------------------------------------------------------------------------
 *
 * Any increase in EXPHI will encroach the currently unused interval [0x00,0x11] and has to be done with caution
 * as a few of those are used for different purposes (0x01 to represent a null subscript in standard null collation,
 * 0x02 to be used for spanning node subscripts etc.)
 */

#define EXP_INT_OVERF		  (7+MV_XBIAS)		/* Upper bound on MV_INT numbers 		*/
#define EXP_INT_UNDERF		 (-3+MV_XBIAS)		/* Lower bound on MV_INT numbers (includes
							 *   integers & fractions upto 3 decimal places */
#define MANT_LO			100000000		/* mantissa.1 >= MANT_LO 			*/
#define MANT_HI			1000000000		/* mantissa.1 <  MANT_HI , mantissa.0 < MANT_HI */
#define INT_HI			1000000			/* -INT_HI < mv_int < INT_HI 			*/
