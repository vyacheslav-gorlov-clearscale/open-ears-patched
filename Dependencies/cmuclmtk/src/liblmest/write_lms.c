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



#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pc_general.h"
#include "idngram2lm.h"
#include "general.h"
#include "ngram.h"
#include "disc_meth.h"
#include "stats.h"
#include <assert.h>
 


// BEGINE HLW ADDITION
#define MAX_LENGTH_OF_SINGLE_LINE 2048

int unigram_count = 0;
int bigram_count = 0;
int trigram_count = 0;
int skipped_unigrams = 0;
int skipped_bigrams = 0;
int skipped_trigrams = 0;
// END HLW ADDITION
void final_ngram_count_replacement (int number_of_ngrams,ng_t *ng);
void write_arpa_copyright(FILE *fp, int N, wordid_t vocab_size, char* firstwd, char* secondwd, char* thirdwd)
{
    fprintf(fp,"#############################################################################\n");
    fprintf(fp,"## Copyright (c) 1996, Carnegie Mellon University, Cambridge University,\n");
    fprintf(fp,"## Ronald Rosenfeld and Philip Clarkson\n");
    fprintf(fp,"## Version 3, Copyright (c) 2006, Carnegie Mellon University \n");
    fprintf(fp,"## Contributors includes Wen Xu, Ananlada Chotimongkol, \n");
    fprintf(fp,"## David Huggins-Daines, Arthur Chan and Alan Black \n");
    fprintf(fp,"#############################################################################\n");
    fprintf(fp,"=============================================================================\n");
    fprintf(fp,"===============  This file was produced by the CMU-Cambridge  ===============\n");
    fprintf(fp,"===============     Statistical Language Modeling Toolkit     ===============\n"); 
    fprintf(fp,"=============================================================================\n");
    fprintf(fp,"This is a %d-gram language model, based on a vocabulary of %d words,\n",N, vocab_size);
    fprintf(fp,"  which begins \"%s\", \"%s\", \"%s\"...\n",firstwd, secondwd, thirdwd);
}

void write_arpa_format(FILE *fp, int n){ 
    
    int i,j;
    fprintf(fp,"This file is in the ARPA-standard format introduced by Doug Paul.\n");
    fprintf(fp,"\n");
    fprintf(fp,"p(wd3|wd1,wd2)= if(trigram exists)           p_3(wd1,wd2,wd3)\n");
    fprintf(fp,"                else if(bigram w1,w2 exists) bo_wt_2(w1,w2)*p(wd3|wd2)\n");
    fprintf(fp,"                else                         p(wd3|w2)\n");
    fprintf(fp,"\n");
    fprintf(fp,"p(wd2|wd1)= if(bigram exists) p_2(wd1,wd2)\n");
    fprintf(fp,"            else              bo_wt_1(wd1)*p_1(wd2)\n");
    fprintf(fp,"\n");
    fprintf(fp,"All probs and back-off weights (bo_wt) are given in log10 form.\n");
    fprintf(fp,"\n");
    fprintf(fp,"Data formats:\n");
    fprintf(fp,"\n");
    fprintf(fp,"Beginning of data mark: \\data\\\n");
    
    for (i=1;i<=n;i++)
        fprintf(fp,"ngram %d=nr            # number of %d-grams\n",i,i);
    
    fprintf(fp,"\n");
    for (i=1;i<=n;i++) {
        fprintf(fp,"\\%d-grams:\n",i);
        fprintf(fp,"p_%d     ",i);
        for (j=1;j<=i;j++)
            fprintf(fp,"wd_%d ",j);
        
        if (i == n) 
            fprintf(fp,"\n");
        else 
            fprintf(fp,"bo_wt_%d\n",i);
    }  
    
    fprintf(fp,"\n");
    fprintf(fp,"end of data mark: \\end\\\n");
    fprintf(fp,"\n");
}

void write_arpa_headers(FILE *fp, const char* header1, const char * header2)
{
    fprintf(fp,"\n");
    fprintf(fp,"-----------------------------------------------------------\n");
    fprintf(fp,"|              the header of the first LM                  \n");
    fprintf(fp,"-----------------------------------------------------------\n");
    fprintf(fp,"\n");
    fprintf(fp,"%s",header1);
    fprintf(fp,"\n");
    fprintf(fp,"-----------------------------------------------------------\n");
    fprintf(fp,"|              the header of the second LM                 \n");
    fprintf(fp,"-----------------------------------------------------------\n");
    fprintf(fp,"\n");
    fprintf(fp,"%s",header2);
    fprintf(fp,"\n");
}

void write_arpa_k_gram_header(FILE *fp, unsigned short n);

void write_arpa_k_gram_header(FILE *fp, unsigned short n)
{
    fprintf(fp,"\n\\%d-grams:\n",n);
}
// BEGIN HLW VERSION
void write_arpa_num_grams(FILE* fp,ng_t* ng, arpa_lm_t *arpa_ng, flag is_arpa)
{
    int i;
    fprintf(fp,"\\data\\\n");
    
    if(is_arpa){
        
        fprintf(fp,"ngram 1=####ONEGRAMPLACEHOLDER####");
        unigram_count = (1+arpa_ng->vocab_size-arpa_ng->first_id);
        for (i=1;i<=arpa_ng->n-1;i++) 
            // fprintf(fp,"ngram %d=%lld\n",i+1,arpa_ng->num_kgrams[i]);
            if(i==1) {
                bigram_count = arpa_ng->num_kgrams[i];
                fprintf(fp,"ngram 2=####TWOGRAMPLACEHOLDER####");
            } else if(i==2) {
                trigram_count = arpa_ng->num_kgrams[i];
                fprintf(fp,"ngram 3=####THREEGRAMPLACEHOLDER####");
            }
    }else{
        
        fprintf(fp,"ngram 1=####ONEGRAMPLACEHOLDER####");
        unigram_count = (1+ng->vocab_size-ng->first_id);
        for (i=1;i<=ng->n-1;i++) 
            // fprintf(fp,"ngram %d=%lld\n",i+1,ng->num_kgrams[i]);
            if(i==1) {
                bigram_count = ng->num_kgrams[i];
                fprintf(fp,"ngram 2=####TWOGRAMPLACEHOLDER####");
            } else if(i==2) {
                trigram_count = ng->num_kgrams[i];
                fprintf(fp,"ngram 3=####THREEGRAMPLACEHOLDER####");
            }
    }
}
// END HLW VERSION

// OLD VERSION:
/*
 void write_arpa_num_grams(FILE* fp,ng_t* ng, arpa_lm_t *arpa_ng, flag is_arpa)
 {
 int i;
 fprintf(fp,"\\data\\\n");
 
 if(is_arpa){
 
 fprintf(fp,"ngram 1=%lld\n",(1+arpa_ng->vocab_size-arpa_ng->first_id));
 
 for (i=1;i<=arpa_ng->n-1;i++) 
 fprintf(fp,"ngram %d=%lld\n",i+1,arpa_ng->num_kgrams[i]);
 
 }else{
 
 fprintf(fp,"ngram 1=%lld\n",(1+ng->vocab_size-ng->first_id));
 
 for (i=1;i<=ng->n-1;i++) 
 fprintf(fp,"ngram %d=%lld\n",i+1,ng->num_kgrams[i]);
 
 }
 }
 */

/* This is the format introduced and first used by Doug Paul.
 Optionally use a given symbol for the UNK word (id==0).
 */

/*
 Format of the .arpabo file:
 ------------------------------
 <header info - ignored by programs>
 \data\
 ngram 1=4989
 ngram 2=835668
 ngram 3=12345678
 
 \1-grams:
 ...
 -0.9792 ABC   -2.2031
 ...
 log10_uniprob(ZWEIG)   ZWEIG   log10_alpha(ZWEIG)
 
 \2-grams:
 ...
 -0.8328 ABC DEFG -3.1234
 ...
 log10_bo_biprob(WAS | ZWEIG)  ZWEIG  WAS   log10_bialpha(ZWEIG,WAS)
 
 \3-grams:
 ...
 -0.234 ABCD EFGHI JKL
 ...
 
 \end\
 */

void write_arpa_lm(ng_t *ng,int verbosity) {
    
    int *current_pos;
    int *end_pos;
    ngram_sz_t i;
    double log_10_of_e = 1.0 / log(10.0);
    
    /* HEADER */
    
    pc_message(verbosity,1,"ARPA-style %d-gram will be written to %s\n",ng->n,ng->arpa_filename);
    
    write_arpa_copyright(ng->arpa_fp,ng->n,ng->vocab_size, ng->vocab[1],ng->vocab[2],ng->vocab[3]);
    
    display_vocabtype(ng->vocab_type,ng->oov_fraction, ng->arpa_fp);  
    display_discounting_method(ng,ng->arpa_fp);
    write_arpa_format(ng->arpa_fp,ng->n);
    write_arpa_num_grams(ng->arpa_fp,ng,NULL,0);
    write_arpa_k_gram_header(ng->arpa_fp,1);
    
    for (i=ng->first_id; i<= (int) ng->vocab_size;i++) {
        
        double log10_uniprob;
        double log10_alpha;
        double alpha;
        
        log10_uniprob = ng->uni_log_probs[i]*log_10_of_e;
        
        if (ng->uni_probs[i]<=0.0)
            log10_uniprob = BAD_LOG_PROB;
        
        alpha=ng_double_alpha(ng,0,i);
        
        if(alpha > 0.0)
            log10_alpha = log10(alpha);
        else
            log10_alpha = BAD_LOG_PROB;
        
        fprintf(ng->arpa_fp,"%.4f %s",log10_uniprob,ng->vocab[i]);
        if (ng->n>1)
            fprintf(ng->arpa_fp,"\t%.4f\n",log10_alpha);
        else
            fprintf(ng->arpa_fp,"\n");
    }
    
    current_pos = (int *) rr_malloc(ng->n*sizeof(int));
    end_pos = (int *) rr_malloc(ng->n*sizeof(int)); 
    
    /* Print 2-gram, ... (n-1)-gram info. */
    
    for (i=1;i<=ng->n-1;i++) {
        
        /* Print out the (i+1)-gram */
        
        int current_table, j;
        count_t ngcount, marg_count;
        double discounted_ngcount;    
        double ngprob, log_10_ngprob, ngalpha, log_10_ngalpha;
        
        /* Initialise variables for the sake of warning-free compilation */

        
        discounted_ngcount = 0.0;
        log_10_ngalpha = 0.0;

        write_arpa_k_gram_header(ng->arpa_fp,i+1);
        
        /* Go through the n-gram list in order */
        
        for (j=0;j<=ng->n-1;j++) {
            current_pos[j] = 0;
            end_pos[j] = 0;
        }
        
        for (current_pos[0]=ng->first_id;
             current_pos[0]<=(int) ng->vocab_size;
             current_pos[0]++) {
            
            if (return_count(ng->four_byte_counts,
                             ng->count_table[0], 
                             ng->marg_counts,
                             ng->marg_counts4,
                             current_pos[0]) > 0) {
                
                current_table = 1;
                
                if (current_pos[0] == (int) ng->vocab_size)
                    end_pos[1] = (int ) ng->num_kgrams[1]-1;
                else {
                    end_pos[1] = get_full_index(ng->ind[0][current_pos[0]+1],
                                                ng->ptr_table[0],
                                                ng->ptr_table_size[0],
                                                current_pos[0]+1)-1;
                }
                
                while (current_table > 0) {
                    
                    /*	  fprintf(stderr, "i %d, current_pos[i] %d, end_pos[i] %d\n",
                     i,
                     current_pos[i],
                     end_pos[i]);
                     fflush(stderr);*/
                    
                    
                    if (current_table == i) {
                        
                        if (current_pos[i] <= end_pos[i]) {
                            
                            /*	      fprintf(stderr, "%d\n",ng->count[i][current_pos[i]]);
                             fprintf(stderr, "%d\n",ng->count_table[i][ng->count[i][current_pos[i]]]);*/
                            
                            ngcount = return_count(ng->four_byte_counts,
                                                   ng->count_table[i],
                                                   ng->count[i],
                                                   ng->count4[i],
                                                   current_pos[i]);
                            
                            
                            if (i==1) {
                                marg_count = return_count(ng->four_byte_counts,
                                                          ng->count_table[0], 
                                                          ng->marg_counts,
                                                          ng->marg_counts4,
                                                          current_pos[0]);
                            }else {
                                marg_count = return_count(ng->four_byte_counts,
                                                          ng->count_table[i-1],
                                                          ng->count[i-1],
                                                          ng->count4[i-1],
                                                          current_pos[i-1]);
                            }
                            
                            if(ng->disc_meth==NULL)
                                ng->disc_meth=(disc_meth_t*) disc_meth_init(ng->discounting_method);
                            
                            assert(ng->disc_meth);
                            discounted_ngcount = 
                            NG_DISC_METH(ng)->dump_discounted_ngram_count(ng,i,ngcount,marg_count,current_pos);
                            
                            ngprob = (double) discounted_ngcount / marg_count;
                            
                            if (ngprob > 1.0) {
                                fprintf(stderr,
                                        "discounted_ngcount = %f marg_count = %d %d %d %d\n",
                                        discounted_ngcount,marg_count,current_pos[0],
                                        current_pos[1],current_pos[2]);
                                quit(-1,"Error : probablity of ngram is greater than one.\n");
                            }
                            
                            if (ngprob > 0.0) 
                                log_10_ngprob = log10(ngprob);
                            else 
                                log_10_ngprob = BAD_LOG_PROB;
                            
                            if (i <= ng->n-2) {
                                ngalpha = ng_double_alpha(ng, i, current_pos[i]);
                                
                                if (ngalpha > 0.0)
                                    log_10_ngalpha = log10(ngalpha);
                                else
                                    log_10_ngalpha = BAD_LOG_PROB;
                            }
                            // BEGIN HLW VERSION
                            if(((strstr (ng->vocab[current_pos[0]],"</s>")) == NULL)&&((i <= 1) || ((i > 1) && ((strstr (ng->vocab[(unsigned int) ng->word_id[i][current_pos[i]]],"<s>")) == NULL)))) { // if the overall entry is a trigram and it's going to end with <s>, skip it -- HLW
                                
                                fprintf(ng->arpa_fp,"%.4f ",log_10_ngprob);
                                fprintf(ng->arpa_fp,"%s ",ng->vocab[current_pos[0]]);
                                for (j=1;j<=i;j++){
                                    
                                    fprintf(ng->arpa_fp,"%s ",ng->vocab[(unsigned int) ng->word_id[j][current_pos[j]]]);
                                }
                                
                                if (i <= ng->n-2){
                                    fprintf(ng->arpa_fp,"%.4f\n",log_10_ngalpha);
                                } else{
                                    fprintf(ng->arpa_fp,"\n");
                                }
                            } else {
                                // something is being skipped  -- HLW
                                if(i==0) {
                                    skipped_unigrams++;
                                } else if(i==1) {
                                    skipped_bigrams++;
                                } else if (i==2) {
                                    skipped_trigrams++;
                                }
                            }
                            // END HLW VERSION
                            
                            // PREVIOUS VERSION:
                            
                            /*
                             if (i <= ng->n-2) {
                             ngalpha = ng_double_alpha(ng, i, current_pos[i]);
                             
                             if (ngalpha > 0.0)
                             log_10_ngalpha = log10(ngalpha);
                             else
                             log_10_ngalpha = BAD_LOG_PROB;
                             }
                             
                             fprintf(ng->arpa_fp,"%.4f ",log_10_ngprob);
                             fprintf(ng->arpa_fp,"%s ",ng->vocab[current_pos[0]]);
                             for (j=1;j<=i;j++){
                             
                             //		fprintf(stderr, "j %d, ng->wordid[j] %u, current_pos[j] %d, ng->word_id[j][current_pos[j]] %u\n",j, ng->word_id[j], current_pos[j], ng->word_id[j][current_pos[j]]);
                             
                             fprintf(ng->arpa_fp,"%s ",ng->vocab[(unsigned int) ng->word_id[j][current_pos[j]]]);
                             }
                             
                             if (i <= ng->n-2)
                             fprintf(ng->arpa_fp,"%.4f\n",log_10_ngalpha);
                             else
                             fprintf(ng->arpa_fp,"\n");
                             */
                            
                            current_pos[i]++;        
                        }else {
                            current_table--;
                            if (current_table > 0)
                                current_pos[current_table]++;
                        }
                    }else {
                        
                        if (current_pos[current_table] <= end_pos[current_table]) {
                            current_table++;
                            if (current_pos[current_table-1] == (int) ng->num_kgrams[current_table-1]-1)
                                end_pos[current_table] = (int) ng->num_kgrams[current_table]-1;
                            else {
                                end_pos[current_table] = get_full_index(ng->ind[current_table-1][current_pos[current_table-1]+1],
                                                                        ng->ptr_table[current_table-1],
                                                                        ng->ptr_table_size[current_table-1],
                                                                        current_pos[current_table-1]+1) - 1;
                            }
                        }else {
                            current_table--;
                            if (current_table > 0)
                                current_pos[current_table]++;
                        }
                    }
                }
            }
        }
    } 
    
    free(current_pos);
    free(end_pos);
    
    fprintf(ng->arpa_fp,"\n\\end\\\n");
    
    rr_oclose(ng->arpa_fp);
    
    // BEGIN HLW ADDITION
    
    // Now that the file is complete, let's go back and replace the placeholder ngram counts with the real final counts  -- HLW
    
    final_ngram_count_replacement(ng->n,ng);
    
    unigram_count = 0;
    bigram_count = 0;
    trigram_count = 0;
    skipped_unigrams = 0;
    skipped_bigrams = 0;
    skipped_trigrams = 0;
    
    // END HLW ADDITION
} 

void final_ngram_count_replacement (int number_of_ngrams,ng_t *ng) {
    char original_filename[512];
    char temporary_file_ending[7];
    char temporary_filename[512];
    strcpy (original_filename,ng->arpa_filename);
    strcpy (temporary_file_ending,".temp");
    strncat (original_filename, temporary_file_ending, 5);
    strcpy (temporary_filename,original_filename);
    
    const char text_to_search_for_ngram1 [128]= "ngram 1=####ONEGRAMPLACEHOLDER####";
    const char text_to_search_for_ngram2 [128]= "ngram 1=####ONEGRAMPLACEHOLDER####ngram 2=####TWOGRAMPLACEHOLDER####";
    const char text_to_search_for_ngram3 [128]= "ngram 1=####ONEGRAMPLACEHOLDER####ngram 2=####TWOGRAMPLACEHOLDER####ngram 3=####THREEGRAMPLACEHOLDER####";
    const char replacement_text[64];
    const char *text_to_search_for = "";
    int string_length = 0;
    
    if(number_of_ngrams == 1) {
        text_to_search_for = text_to_search_for_ngram1;
        sprintf((char *)replacement_text, "ngram 1=%d\n",  (unigram_count-skipped_unigrams));
    } else if(number_of_ngrams == 2) {
        text_to_search_for = text_to_search_for_ngram2;
        sprintf((char *)replacement_text, "ngram 1=%d\nngram 2=%d\n",  (unigram_count-skipped_unigrams),(bigram_count-skipped_bigrams));        
    } else if(number_of_ngrams == 3) {
        text_to_search_for = text_to_search_for_ngram3;
        sprintf((char *)replacement_text, "ngram 1=%d\nngram 2=%d\nngram 3=%d\n",  (unigram_count-skipped_unigrams),(bigram_count-skipped_bigrams),(trigram_count-skipped_trigrams));        
    }
    
    char buffer[MAX_LENGTH_OF_SINGLE_LINE+2];
    char *buffer_pointer, *find_pointer;
    FILE *input_file, *output_file;
    string_length = strlen(text_to_search_for);
    size_t length_of_search_string = string_length;
    
    if((input_file = fopen(ng->arpa_filename,"r"))==NULL) { // Re-open the arpa file
        printf("couldn't open original file\n");
        perror("fopen");
    }
    
    if((output_file = fopen(temporary_filename,"wb"))==NULL) { // Create a temp file we'll write to
        printf("couldn't open temp file\n");
        perror("fopen");
    }
    
    while(fgets(buffer,MAX_LENGTH_OF_SINGLE_LINE+2,input_file)) { // Search for the placeholder string and replace it with the real count string
        
        buffer_pointer = buffer;
        while ((find_pointer = strstr(buffer_pointer,text_to_search_for))) {
            
            while(buffer_pointer < find_pointer) {
                fputc((int)*buffer_pointer++,output_file);
            }
            
            fputs(replacement_text,output_file);
            
            buffer_pointer += length_of_search_string;
        }
        
        fputs(buffer_pointer,output_file);
    }
    
    fclose(output_file);
    fclose(input_file);
    
    if (remove(ng->arpa_filename) == -1) { // Now that we're done, get rid of the .arpa file
        printf("couldn't delete the file %s\n", ng->arpa_filename);
    }
    
    if (rename(temporary_filename, ng->arpa_filename) == -1) { // and rename the .temp file .arpa
        printf("couldn't rename the file %s to %s\n", temporary_filename, ng->arpa_filename);        
    }
}

void write_bin_lm(ng_t *ng,int verbosity) {
    
    int l_chunk;
    int from_rec;
    int i;
    
    pc_message(verbosity,1,"Binary %d-gram language model will be written to %s\n",ng->n,ng->bin_filename);
    
    ng->version = BBO_FILE_VERSION;
    
    /* Scalar parameters */
    
    rr_fwrite((char*)&ng->version,sizeof(int),1,ng->bin_fp,"version");
    rr_fwrite((char*)&ng->n,sizeof(unsigned short),1,ng->bin_fp,"n");
    
    rr_fwrite((char*)&ng->vocab_size,sizeof(wordid_t),1,ng->bin_fp,"vocab_size");
    rr_fwrite((char*)&ng->no_of_ccs,sizeof(unsigned short),1,ng->bin_fp,"no_of_ccs");
    rr_fwrite((char*)&ng->vocab_type,sizeof(unsigned short),1,ng->bin_fp,"vocab_type");
    
    rr_fwrite((char*)&ng->count_table_size,sizeof(count_ind_t),1,ng->bin_fp,"count_table_size");
    rr_fwrite((char*)&ng->discounting_method,sizeof(unsigned short),1,ng->bin_fp,"discounting_method");
    
    rr_fwrite((char*)&ng->min_alpha,sizeof(double),1,ng->bin_fp,"min_alpha");
    rr_fwrite((char*)&ng->max_alpha,sizeof(double),1,ng->bin_fp,"max_alpha");
    rr_fwrite((char*)&ng->out_of_range_alphas,sizeof(unsigned short),1,ng->bin_fp,"out_of_range_alphas");
    rr_fwrite((char*)&ng->size_of_alpha_array,sizeof(unsigned short),1,ng->bin_fp,"size_of_alpha_array");  
    
    rr_fwrite((char*)&ng->n_unigrams,sizeof(ngram_sz_t),1,ng->bin_fp,"n_unigrams");
    rr_fwrite((char*)&ng->zeroton_fraction,sizeof(double),1,ng->bin_fp,"zeroton_fraction");
    rr_fwrite((char*)&ng->oov_fraction,sizeof(double),1,ng->bin_fp,"oov_fraction");
    rr_fwrite((char*)&ng->four_byte_counts,sizeof(flag),1,ng->bin_fp,"four_byte_counts");
    rr_fwrite((char*)&ng->four_byte_alphas,sizeof(flag),1,ng->bin_fp,"four_byte_alphas");
    
    rr_fwrite((char*)&ng->first_id,sizeof(unsigned short),1,
              ng->bin_fp,"first_id");
    
    /* Short and shortish arrays */
    
    sih_val_write_to_file(ng->vocab_ht,ng->bin_fp,ng->bin_filename,0);
    
    /* (ng->vocab is not stored in file - will be derived from ng->vocab_ht) */
    
    if (ng->four_byte_counts==1) {
        assert(ng->marg_counts4);
        rr_fwrite((char*)ng->marg_counts4,sizeof(count_t),
                  ng->vocab_size+1,ng->bin_fp,"marg_counts");
    }else {
        assert(ng->marg_counts);
        rr_fwrite((char*)ng->marg_counts,sizeof(count_ind_t),
                  ng->vocab_size+1,ng->bin_fp,"marg_counts");
    }
    
    rr_fwrite((char*)ng->alpha_array,sizeof(double),
              ng->size_of_alpha_array,ng->bin_fp,"alpha_array");
    
    if (!ng->four_byte_counts) {
        for (i=0;i<=ng->n-1;i++)
            rr_fwrite((char*)ng->count_table[i],sizeof(count_t),
                      ng->count_table_size+1,ng->bin_fp,"count_table");
    }
    
    /* Could write count_table as one block, but better to be safe and
     do it in chunks. For motivation, see comments about writing tree
     info. */
    
    rr_fwrite((char*)ng->ptr_table_size,sizeof(ptr_tab_sz_t),ng->n,ng->bin_fp,"ptr_table_size");
    
    for (i=0;i<=ng->n-1;i++)
        rr_fwrite((char*)ng->ptr_table[i],sizeof(ptr_tab_t),ng->ptr_table_size[i],ng->bin_fp,"ptr_table");
    
    /* Unigram statistics */
    
    rr_fwrite((char*)ng->uni_probs,sizeof(uni_probs_t), ng->vocab_size+1,
              ng->bin_fp,"uni_probs");
    rr_fwrite((char*)ng->uni_log_probs,sizeof(uni_probs_t),ng->vocab_size+1,
              ng->bin_fp,"uni_log_probs");
    rr_fwrite((char*)ng->context_cue,sizeof(flag),ng->vocab_size+1,
              ng->bin_fp,"context_cue");
    
    rr_fwrite((char*)ng->cutoffs,sizeof(cutoff_t),ng->n,ng->bin_fp,"cutoffs");
    
    switch (ng->discounting_method) {
        case GOOD_TURING:
            rr_fwrite((char*)ng->fof_size,sizeof(fof_sz_t),ng->n,ng->bin_fp,"fof_size");
            rr_fwrite((char*)ng->disc_range,sizeof(unsigned short),ng->n,
                      ng->bin_fp,"disc_range");
            for (i=0;i<=ng->n-1;i++) {
                rr_fwrite((char*)ng->freq_of_freq[i],sizeof(fof_t),
                          ng->fof_size[i]+1,ng->bin_fp,"freq_of_freq");
            }    
            for (i=0;i<=ng->n-1;i++) {
                rr_fwrite((char*)ng->gt_disc_ratio[i],sizeof(disc_val_t),
                          ng->disc_range[i]+1,ng->bin_fp,"gt_disc_ratio");
            }    
        case WITTEN_BELL:
            break;
        case LINEAR:
            rr_fwrite((char*)ng->lin_disc_ratio,sizeof(disc_val_t),
                      ng->n,ng->bin_fp,"lin_disc_ratio");
            break;
        case ABSOLUTE:
            rr_fwrite((char*)ng->abs_disc_const,sizeof(double),
                      ng->n,ng->bin_fp,"abs_disc_const");
            break;
    }
    
    /* Tree information */
    
    /* Unigram stuff first, since can be dumped all in one go */
    
    rr_fwrite((char*)ng->num_kgrams,sizeof(ngram_sz_t),ng->n,ng->bin_fp,"num_kgrams");
    
    if (ng->four_byte_counts)
        rr_fwrite((char*)ng->count4[0],sizeof(count_t),ng->vocab_size+1,
                  ng->bin_fp,"unigram counts");
    else 
        rr_fwrite((char*)ng->count[0],sizeof(count_ind_t),ng->vocab_size+1,
                  ng->bin_fp,"unigram counts");
    
    if (ng->four_byte_alphas)
        rr_fwrite((char*)ng->bo_weight4[0],sizeof(four_byte_t),ng->vocab_size+1,
                  ng->bin_fp,"unigram backoff weights");
    else
        rr_fwrite((char*)ng->bo_weight[0],sizeof(bo_weight_t),ng->vocab_size+1,
                  ng->bin_fp,"unigram backoff weights");
    
    if (ng->n > 1) 
        rr_fwrite((char*)ng->ind[0],sizeof(index__t),ng->vocab_size+1,
                  ng->bin_fp,"unigram -> bigram pointers");
    
    /* Write the rest of the tree structure in chunks, otherwise the
     kernel buffers are too big. */
    
    /* Need to do byte swapping */
    //swap_struct(ng);
    
    
    for (i=1;i<=ng->n-1;i++) {
        from_rec = 0;
        l_chunk = 100000;
        while(from_rec < ng->num_kgrams[i]) {
            if (from_rec+l_chunk > ng->num_kgrams[i]) 
                l_chunk = ng->num_kgrams[i] - from_rec;
            
            rr_fwrite((char*)&ng->word_id[i][from_rec],1,sizeof(id__t)*l_chunk,ng->bin_fp,"word ids");
            
            from_rec += l_chunk;
        }   
    }
    
    for (i=1;i<=ng->n-1;i++) {
        
        from_rec = 0;
        l_chunk = 100000;
        while(from_rec < ng->num_kgrams[i]) {
            if (from_rec+l_chunk > ng->num_kgrams[i])
                l_chunk = ng->num_kgrams[i] - from_rec;
            
            if (ng->four_byte_counts)
                rr_fwrite((char*)&ng->count4[i][from_rec],1,sizeof(count_t)*l_chunk,ng->bin_fp,"counts");
            else
                rr_fwrite((char*)&ng->count[i][from_rec],1,sizeof(count_ind_t)*l_chunk,ng->bin_fp,"counts");
            
            from_rec += l_chunk;
        }    
    }
    
    for (i=1;i<=ng->n-2;i++) {
        from_rec = 0;
        l_chunk = 100000;
        while(from_rec < ng->num_kgrams[i]) {
            if (from_rec+l_chunk > ng->num_kgrams[i]) 
                l_chunk = ng->num_kgrams[i] - from_rec;
            
            if (ng->four_byte_alphas)
                rr_fwrite((char*)&ng->bo_weight4[i][from_rec],1,sizeof(four_byte_t)*l_chunk,
                          ng->bin_fp,"backoff weights");
            else
                rr_fwrite((char*)&ng->bo_weight[i][from_rec],1,sizeof(bo_weight_t)*l_chunk,
                          ng->bin_fp,"backoff weights");
            from_rec += l_chunk;
        }
    }
    
    for (i=1;i<=ng->n-2;i++) {
        from_rec = 0;
        l_chunk = 100000;
        while(from_rec < ng->num_kgrams[i]) {
            if (from_rec+l_chunk > ng->num_kgrams[i])
                l_chunk = ng->num_kgrams[i] - from_rec;
            
            rr_fwrite((char*)&ng->ind[i][from_rec],1,sizeof(index__t)*l_chunk,ng->bin_fp,
                      "indices");
            from_rec += l_chunk;
        }
    }
    
    rr_oclose(ng->bin_fp);
    
    /* Swap back */
   // swap_struct(ng); 
}
