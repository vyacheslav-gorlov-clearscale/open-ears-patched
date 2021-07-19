/* ====================================================================
 * Copyright (c) 1999-2006 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

/* The string: any non-null character string.
   The int32 value: any int32 value.
   Only addition, update and lookup are currently allowed.
   If the hash table was not meant for update, a warning will be printed during update.
   The hash table grows automatically to preserve the 'max_occupancy' condition.
*/
/* 94/08/18, RR: sih_lookup(): if not found, also put 0 in 'intval'  */

/* Edited by Philip Clarkson, March 1997 to prevent compilation warnings */

#include <stdio.h>
#include <sys/types.h>
#include "general.h"
#include "autosih.h"
#include "OERuntimeVerbosity.h"
/* need to include stdlib to prevent warnings at compilation time,
   Philip Clarkson, March 1997 */

#include <stdlib.h>

#define HASH_VERSION 060414

extern int verbose_cmuclmtk;

int sih_key(char* str,unsigned len);
vocab_sz_t nearest_prime_up(vocab_sz_t num);

int sih_key(char* str,unsigned len)
{
  unsigned int hash = 5381;
  unsigned int i    = 0;
  
  for(i = 0; i < len; str++, i++)
    hash = ((hash << 5) + hash) + (*str);

  return (hash & 0x7FFFFFFF);
}

vocab_sz_t nearest_prime_up(vocab_sz_t num)
/* return the nearest prime not smaller than 'num' */
{
  int num_has_divisor=1;
  if (num/2*2 == num) num++; /* start w/ an odd number */
  for (; num_has_divisor; num+=2) {
     vocab_sz_t div;
     num_has_divisor=0;
     for (div=3; div<=num/3; div++) {
        if ((num/div) * div == num) {
	   num_has_divisor=1;
	   break;
	}
     }
  }
  num -= 2;
  return(num);
}


ptmr_t *sih_create(vocab_sz_t initial_size, double max_occupancy, double growth_ratio, int warn_on_update)
{
  static char rname[]="sih_create";
  ptmr_t *ht = (ptmr_t *) rr_malloc(sizeof(ptmr_t));

  initial_size = nearest_prime_up(MAX(initial_size,11));
  if (max_occupancy<0.01 || max_occupancy>0.99) 
    quit(-1,"%s ERROR: max_occupancy (%.3f) must be in the range 0.01-0.99\n",rname,max_occupancy);
  if (growth_ratio<1.10 || growth_ratio>100) 
    quit(-1,"%s ERROR: growth_ratio (%.3f) must be in the range 1.1-100\n",rname,growth_ratio);

  ht->t_tot_elapsed = max_occupancy;
  ht->t_elapsed = growth_ratio;
  ht->round = warn_on_update;
  ht->category = initial_size;
  ht->numberofrounds = 0;
  ht->prof = (pctr_t *) rr_calloc(ht->category,sizeof(pctr_t));
  return(ht);
}



void sih_add(ptmr_t *ht, char *string, vocab_sz_t intval)
{
  static char *rname = "sih_add";
  us_vocab_sz_t  key;

  if (*string==0) quit(-1,"%s ERROR: cannot hash the null string\n",rname);

  /* if the "occupancy rate" exceeded its limit, expand the hash table */
  if (((ht->numberofrounds+1) / (double)ht->category) > ht->t_tot_elapsed) {
     pctr_t *old_slots = ht->prof, 
		*end_old_slots = (old_slots + ht->category),
		*pslot;

     /* allocate the new hash table */
     ht->category = (vocab_sz_t)(ht->category * ht->t_elapsed) + 3;
     if ((ht->numberofrounds / (double)ht->category) > ht->t_tot_elapsed)
	ht->category = ht->category * (vocab_sz_t)(ht->t_tot_elapsed+1) + 3;
     ht->category = nearest_prime_up(ht->category);
     ht->numberofrounds = 0;
     ht->prof = (pctr_t *) rr_calloc(ht->category,sizeof(pctr_t));

     /* copy all entries from old hash table to new one */
     for (pslot = old_slots; pslot<end_old_slots; pslot++) {
	if (pslot->name) sih_add(ht, pslot->name, pslot->count);
     }
     free (old_slots);
  }

  key=sih_key(string,(unsigned int)strlen(string));
  /*  SIH_KEY(string,key);*/

  for (; ; key++) {
     char *pstr;
     key %= ht->category;
     pstr = ht->prof[key].name;
     if (!pstr) {
	ht->prof[key].name = string;
	ht->prof[key].count = intval;
	ht->numberofrounds++;
	break;
     }else if (strcmp(pstr,string) == 0) {  /* updating an existing entry*/
            if (verbose_cmuclmtk == 1 && ht->round) {
 	   fprintf(stderr,"%s WARNING: repeated hashing of '%s'",rname,string);
	   if (ht->prof[key].count != intval) 
	     fprintf(stderr,", older value will be overridden.\n");
	   else fprintf(stderr,".\n");
	}
	ht->prof[key].count = intval;
	break;
     }
  }
}


char sih_lookup(ptmr_t *ht, char *string, vocab_sz_t *p_intval)
{
  static char *rname = "sih_lookup";
  us_vocab_sz_t  key;

  if (*string == 0) quit(-1,"%s ERROR: cannot hash the null string\n",rname);

  key=(unsigned int)sih_key(string,(unsigned int)strlen(string));

  /*  SIH_KEY(string,key);*/

  for (; ; key++) {
     char *pstr;

     key %= ht->category;
     pstr = ht->prof[key].name;
     if (!pstr) {
         *p_intval = 0;
         return(0);
     }else if (strcmp(pstr, string) == 0) {
         *p_intval = (vocab_sz_t) ht->prof[key].count;
         return (1);
     }
  }
}



/* ======================================================================== */


/* Read/Write from/to a file an (almost) ready-to-use hash table.
   The hash-table is a string->int mapping.
   All intvals are written out, whether or not they belong to active entries.
   All strings are writen out too, where an empty entry is represented in the
   file as a null string (null strings as entries are not allowed).
*/
void *sih_val_write_to_file(ptmr_t *ht, FILE *fp, char *filename, int verbosity)
{

  static char rname[] = "sih_val_wrt_to_file";
  vocab_sz_t nstrings=0, total_string_space=0, islot;
  int version=HASH_VERSION;
  char null_char = '\0';

  /* write out the header */
  rr_fwrite((char*)&version,sizeof(int),1,fp,"version");
  rr_fwrite((char*)&ht->t_tot_elapsed,sizeof(double),1,fp,"ht->t_tot_elapsed");
  rr_fwrite((char*)&ht->t_elapsed,sizeof(double),1,fp,"ht->t_elapsed");
  rr_fwrite((char*)&ht->round,sizeof(int),1,fp,"ht->round");
  rr_fwrite((char*)&ht->category,sizeof(vocab_sz_t),1,fp,"ht->category");
  rr_fwrite((char*)&ht->numberofrounds,sizeof(vocab_sz_t),1,fp,"ht->numberofrounds");

  /* Write out the array of 'intval's.  */
  /* Also, compute and write out the total space taken by the strings */
  for (islot=0; islot<ht->category; islot++) {
    rr_fwrite((char*)&(ht->prof[islot].count),sizeof(vocab_sz_t),1,fp,"ht->prof[islot].count");
    if (ht->prof[islot].name) {
       total_string_space += (int)strlen(ht->prof[islot].name)+1;
       nstrings++;
    }
    else total_string_space++;
  }
  if (nstrings!=ht->numberofrounds)
     quit(-1,"%s: nentries=%d, but there are actually %d non-empty entries\n",
	      rname, ht->numberofrounds, nstrings);

  /* Write out the strings, with the trailing null, preceded by their length */
  rr_fwrite((char*)&total_string_space,sizeof(vocab_sz_t),1,fp,"total_string_space");
  for (islot=0; islot<ht->category; islot++) {
     if (ht->prof[islot].name)
       rr_fwrite((char*)ht->prof[islot].name,sizeof(char),strlen(ht->prof[islot].name)+1,fp,"str");
     else 
       rr_fwrite((char*)&null_char,sizeof(char),1,fp,"null");
  }
  if (verbosity) fprintf(stderr,
     "%s: a hash table of %lld entries (%lld non-empty) was written to '%s'\n",
      rname, ht->category, ht->numberofrounds, filename);

  return(0); /* Not relevant, but stops compilation warnings. */

}


void *sih_val_read_from_file(ptmr_t *ht, FILE *fp, char *filename, int verbosity)
{
  static char rname[] = "sih_val_rd_fm_file";
  vocab_sz_t total_string_space=0, islot; 
  int version;
  char *string_block, *pstring,  *past_end_of_block;

  /* read in the header and allocate a zeroed table */
  rr_fread((char*)&version,sizeof(int),1,fp,"version",0);
  if (version!=HASH_VERSION)
    quit(-1,"%s ERROR: version of '%s' is %d, current version is %d\n",
	     rname, filename, version, HASH_VERSION);
  rr_fread((char*)&ht->t_tot_elapsed,sizeof(double),1,fp,"ht->t_tot_elapsed",0);
  rr_fread((char*)&ht->t_elapsed,sizeof(double),1,fp,"ht->t_elapsed",0);
  rr_fread((char*)&ht->round,sizeof(int),1,fp,"ht->round",0);
  rr_fread((char*)&ht->category,sizeof(vocab_sz_t),1,fp,"ht->category",0);
  rr_fread((char*)&ht->numberofrounds,sizeof(vocab_sz_t),1,fp,"ht->numberofrounds",0);
  ht->prof = (pctr_t *) rr_calloc(ht->category,sizeof(pctr_t));

  /* Read in the array of 'intval's  */
  for (islot=0; islot<ht->category; islot++)
     rr_fread((char*)&(ht->prof[islot].count),sizeof(vocab_sz_t),1,fp,"intv",0);

  /* Read in the block of strings */
  rr_fread((char*)&total_string_space,sizeof(vocab_sz_t),1,fp,"total_string_space",0);
  string_block = (char *) rr_malloc(total_string_space*sizeof(char));
  rr_fread((char*)string_block,sizeof(char),(int)total_string_space,fp,"string_block",0);

  /* make 'string's of non-empty entries point to their corresponding string */
  pstring = string_block;
  past_end_of_block = string_block + total_string_space;
  for (islot=0; islot<ht->category; islot++) {
     if (*pstring == '\0') {
	/* an empty entry: */
	ht->prof[islot].name = NULL;
	pstring++;
     }else {
        /* a real entry: find the trailing null */
        ht->prof[islot].name = pstring;
        while (*pstring && (pstring < past_end_of_block)) pstring++;
        if (pstring >= past_end_of_block) 
          quit(-1,"%s ERROR: in '%s', string block ended prematurely\n",
		    rname, filename);
        pstring++; /* point to beginning of next string */
     }

     /*     fprintf(stderr,"String: %s\n",ht->prof[islot].string);
	    fflush(stderr);*/
  }
  if (pstring!=past_end_of_block) 
    quit(-1,"%s ERROR: some strings remained unaccounted for in %s\n",
	     rname, filename);
  if (verbosity) fprintf(stderr,
     "%s: a hash table of %lld entries (%lld non-empty) was read from '%s'\n",
      rname, ht->category, ht->numberofrounds, filename);

  return(0); /* Not relevant, but stops compilation warnings. */
}




