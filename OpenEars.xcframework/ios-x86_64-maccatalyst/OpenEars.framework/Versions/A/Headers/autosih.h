#import "profile.h"
/* This file was automatically generated.  Do not edit! */
void *sih_val_read_from_file(ptmr_t *ht,FILE *fp,char *filename,int verbosity);
void *sih_val_write_to_file(ptmr_t *ht,FILE *fp,char *filename,int verbosity);
char sih_lookup(ptmr_t *ht,char *string,vocab_sz_t *p_intval);
void sih_add(ptmr_t *ht,char *string,vocab_sz_t intval);
ptmr_t *sih_create(vocab_sz_t initial_size,double max_occupancy,double growth_ratio,int warn_on_update);
vocab_sz_t nearest_prime_up(vocab_sz_t num);
vocab_sz_t nearest_prime_up(vocab_sz_t num);
int sih_key(char *str,unsigned len);
int sih_key(char *str,unsigned len);
void get_vocab_from_vocab_ht(ptmr_t *ht, vocab_sz_t vocab_size, int verbosity, char ***p_vocab)
;
void read_wlist_into_siht(char *wlist_filename, int verbosity, ptmr_t *p_word_id_ht, vocab_sz_t * p_n_wlist)
;
void read_wlist_into_array(char *wlist_filename, int verbosity,  char ***p_wlist, long long *p_n_wlist);
;
