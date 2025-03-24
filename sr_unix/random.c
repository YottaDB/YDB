/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_stdlib.h"

#include "random.h"

#define MAXNUM 2147483561L
#define MAX_RND_IDX	99
#define MAX_SEED_LEN	 85

/* "global" random table -- must be visible to get_rand_from_table and
   init_rand_table */
static int rannum_table[MAX_RND_IDX+1];

int get_rand_from_table (void)
{
  int ini_index = rannum_table[MAX_RND_IDX - 1]%MAX_RND_IDX;
  int fin_index = rannum_table[MAX_RND_IDX]%MAX_RND_IDX;

  int temp_val1, temp_val2;

  if ((temp_val2 = rannum_table[ini_index] - rannum_table[fin_index]) < 0) temp_val2 += 1000000000;

  rannum_table[ini_index] = temp_val2;

  rannum_table[MAX_RND_IDX - 1]--;
  rannum_table[MAX_RND_IDX]--;

  if (rannum_table[MAX_RND_IDX - 1] == 0) rannum_table[MAX_RND_IDX - 1] = 55;

  if (rannum_table[MAX_RND_IDX] == 0) rannum_table[MAX_RND_IDX] = 55;

  temp_val1 = rannum_table[MAX_RND_IDX]%42 + 56;
  rannum_table[MAX_RND_IDX] = rannum_table[temp_val1];

  rannum_table[temp_val1] = temp_val2;

  return(rannum_table[MAX_RND_IDX]);
}


int init_rand_table (void)
{
  char buf[MAX_RND_IDX+2];
  char c_seed[MAX_SEED_LEN + 1];
  int  i_seed, seed_len;
  int i, j, k;
  time_t now;

  now = time(NULL);
  i_seed = (int) now;
  if ((9999999999 < now) || (999999999 >= now)) /* punt: system clock is broken, or this code still in use after 20 Nov 2286 */
  {
  	i_seed = (int) 1693256989;	/* Set arbitrary 10 digit number */
  }
  SNPRINTF(c_seed, MAX_SEED_LEN + 1, "%d", i_seed);
  seed_len = 10; /* strlen(c_seed) always 10 */

  SNPRINTF(buf, MAX_RND_IDX + 2, "%s aEbFcGdHeI", c_seed);

  for (i = 1; i < MAX_RND_IDX; i++)
    rannum_table[i] = buf[i%seed_len] * 8171717 + i * 997;

  i = 97; j = 12;

  for (k = 1; k < MAX_RND_IDX; k++)
  {
    rannum_table[i] -= rannum_table[j];
    if (rannum_table[i] < 0)
    	rannum_table[i] = -rannum_table[i];

    i--; j--;
    if (i == 0)
    	i=97;
    if (j == 0)
    	j=97;
  }

  rannum_table[MAX_RND_IDX - 2] = 55;
  rannum_table[MAX_RND_IDX - 1] = 24;
  rannum_table[MAX_RND_IDX] = 77;

  return 1; /* No error. This is added to make compiler happy */
}
