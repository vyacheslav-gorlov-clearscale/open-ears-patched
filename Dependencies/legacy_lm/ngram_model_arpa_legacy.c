/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2007 Carnegie Mellon University.  All rights
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
/*
 * \file ngram_model_arpa.c ARPA format language models
 *
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#include "sphinxbase/ckd_alloc.h"
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "sphinxbase/err.h"
#include "sphinxbase/pio.h"
#include "sphinxbase/listelem_alloc.h"
#include "sphinxbase/strfuncs.h"

#include "ngram_model_arpa_legacy.h"
#include <CoreFoundation/CoreFoundation.h>  

static ngram_funcs_t_legacy ngram_model_arpa_funcs_legacy;

#define TSEG_BASE_legacy(m,b)		((m)->lm3g.tseg_base[(b)>>LOG_BG_SEG_SZ_legacy])
#define FIRST_BG_legacy(m,u)		((m)->lm3g.unigrams[u].bigrams)
#define FIRST_TG_legacy(m,b)		(TSEG_BASE_legacy((m),(b))+((m)->lm3g.bigrams[b].trigrams))

/*
 * Read and return #unigrams, #bigrams, #trigrams as stated in input file.
 */
static int
ReadNgramCounts_legacy(lineiter_t **li, int32 * n_ug, int32 * n_bg, int32 * n_tg)
{
    int32 ngram, ngram_cnt;

    /* skip file until past the '\data\' marker */
    while (*li) {
        string_trim((*li)->buf, STRING_BOTH);
        if (strcmp((*li)->buf, "\\data\\") == 0)
            break;
        *li = lineiter_next(*li);
    }
    if (*li == NULL || strcmp((*li)->buf, "\\data\\") != 0) {
        E_INFO("No \\data\\ mark in LM file\n");
        return -1;
    }

    *n_ug = *n_bg = *n_tg = 0;
    while ((*li = lineiter_next(*li))) {
        if (sscanf((*li)->buf, "ngram %d=%d", &ngram, &ngram_cnt) != 2)
            break;
        switch (ngram) {
        case 1:
            *n_ug = ngram_cnt;
            break;
        case 2:
            *n_bg = ngram_cnt;
            break;
        case 3:
            *n_tg = ngram_cnt;
            break;
        default:
            E_ERROR("Unknown ngram (%d)\n", ngram);
            return -1;
        }
    }
    if (*li == NULL) {
        E_ERROR("EOF while reading ngram counts\n");
        return -1;
    }

    /* Position iterator to the unigrams header '\1-grams:\' */
    while ((*li = lineiter_next(*li))) {
        string_trim((*li)->buf, STRING_BOTH);
        if (strcmp((*li)->buf, "\\1-grams:") == 0)
            break;
    }
    if (*li == NULL) {
        E_ERROR_SYSTEM("Failed to read \\1-grams: mark");
        return -1;
    }

    if ((*n_ug <= 0) || (*n_bg < 0) || (*n_tg < 0)) {
        E_ERROR("Bad or missing ngram count\n");
        return -1;
    }
    return 0;
}

/*
 * Read in the unigrams from given file into the LM structure model.
 * On entry to this procedure, the iterator is positioned to the
 * header line '\1-grams:'.
 */
static int
ReadUnigrams_legacy(lineiter_t **li, ngram_model_arpa_t_legacy * model)
{
    ngram_model_t_legacy *base = &model->base;
    int32 wcnt;
    float p1;

    E_INFO("Reading unigrams\n");

    wcnt = 0;
    while ((*li = lineiter_next(*li))) {
        char *wptr[3], *name;
        float32 bo_wt = 0.0f;
        int n;

        string_trim((*li)->buf, STRING_BOTH);
        if (strcmp((*li)->buf, "\\2-grams:") == 0
            || strcmp((*li)->buf, "\\end\\") == 0)
            break;

        if ((n = str2words((*li)->buf, wptr, 3)) < 2) {
            if ((*li)->buf[0] != '\0')
                E_WARN("Format error; unigram ignored: %s\n", (*li)->buf);
            continue;
        }
        else {
            p1 = (float)atof_c(wptr[0]);
            name = wptr[1];
            if (n == 3)
                bo_wt = (float)atof_c(wptr[2]);
        }

        if (wcnt >= base->n_counts[0]) {
            E_ERROR("Too many unigrams\n");
            return -1;
        }

        /* Associate name with word id */
        base->word_str[wcnt] = ckd_salloc(name);
        if ((hash_table_enter(base->wid, base->word_str[wcnt], (void *)(long)wcnt))
            != (void *)(long)wcnt) {
                E_WARN("Duplicate word in dictionary: %s\n", base->word_str[wcnt]);
        }
        model->lm3g.unigrams[wcnt].prob1.l = logmath_log10_to_log(base->lmath, p1);
        model->lm3g.unigrams[wcnt].bo_wt1.l = logmath_log10_to_log(base->lmath, bo_wt);
        wcnt++;
    }

    if (base->n_counts[0] != wcnt) {
        E_WARN("lm_t_legacy.ucount(%d) != #unigrams read(%d)\n",
               base->n_counts[0], wcnt);
        base->n_counts[0] = wcnt;
        base->n_words = wcnt;
    }
    return 0;
}

/*
 * Read bigrams from given file into given model structure.
 */
static int
ReadBigrams_legacy(lineiter_t **li, ngram_model_arpa_t_legacy * model)
{
    ngram_model_t_legacy *base = &model->base;
    int32 w1, w2, prev_w1, bgcount;
    bigram_t_legacy *bgptr;

    E_INFO("Reading bigrams\n");

    bgcount = 0;
    bgptr = model->lm3g.bigrams;
    prev_w1 = -1;

    while ((*li = lineiter_next(*li))) {
        float32 p, bo_wt = 0.0f;
        int32 p2, bo_wt2;
        char *wptr[4], *word1, *word2;
        int n;

        string_trim((*li)->buf, STRING_BOTH);
        wptr[3] = NULL;
        if ((n = str2words((*li)->buf, wptr, 4)) < 3) {
            if ((*li)->buf[0] != '\0')
                break;
            continue;
        }
        else {
            p = (float32)atof_c(wptr[0]);
            word1 = wptr[1];
            word2 = wptr[2];
            if (wptr[3])
                bo_wt = (float32)atof_c(wptr[3]);
        }

        if ((w1 = ngram_wid_legacy(base, word1)) == NGRAM_INVALID_WID_legacy) {
            E_ERROR("Unknown word: %s, skipping bigram (%s %s)\n",
                    word1, word1, word2);
            continue;
        }
        if ((w2 = ngram_wid_legacy(base, word2)) == NGRAM_INVALID_WID_legacy) {
            E_ERROR("Unknown word: %s, skipping bigram (%s %s)\n",
                    word2, word1, word2);
            continue;
        }

        /* FIXME: Should use logmath_t quantization here. */
        /* HACK!! to quantize probs to 4 decimal digits */
        p = (float32)((int32)(p * 10000)) / 10000;
        bo_wt = (float32)((int32)(bo_wt * 10000)) / 10000;

        p2 = logmath_log10_to_log(base->lmath, p);
        bo_wt2 = logmath_log10_to_log(base->lmath, bo_wt);

        if (bgcount >= base->n_counts[1]) {
            E_ERROR("Too many bigrams\n");
            return -1;
        }

        bgptr->wid = w2;
        bgptr->prob2 = sorted_id_legacy(&model->sorted_prob2, &p2);
        if (base->n_counts[2] > 0)
            bgptr->bo_wt2 = sorted_id_legacy(&model->sorted_bo_wt2, &bo_wt2);

        if (w1 != prev_w1) {
            if (w1 < prev_w1) {
                E_ERROR("Bigram %s %s not in unigram order word id: %d prev word id: %d\n", word1, word2, w1, prev_w1);
                return -1;
            }

            for (prev_w1++; prev_w1 <= w1; prev_w1++)
                model->lm3g.unigrams[prev_w1].bigrams = bgcount;
            prev_w1 = w1;
        }
        bgcount++;
        bgptr++;

        if ((bgcount & 0x0000ffff) == 0) {
            E_INFOCONT(".");
        }
    }
    if (*li == NULL || ((strcmp((*li)->buf, "\\end\\") != 0)
                        && (strcmp((*li)->buf, "\\3-grams:") != 0))) {
        E_ERROR("Bad bigram: %s\n", (*li)->buf);
        return -1;
    }

    for (prev_w1++; prev_w1 <= base->n_counts[0]; prev_w1++)
        model->lm3g.unigrams[prev_w1].bigrams = bgcount;

    return 0;
}

/*
 * Very similar to ReadBigrams.
 */
static int
ReadTrigrams_legacy(lineiter_t **li, ngram_model_arpa_t_legacy * model)
{
    ngram_model_t_legacy *base = &model->base;
    int32 i, w1, w2, w3, prev_w1, prev_w2, tgcount, prev_bg, bg, endbg;
    int32 seg, prev_seg, prev_seg_lastbg;
    trigram_t_legacy *tgptr;
    bigram_t_legacy *bgptr;

    E_INFO("Reading trigrams\n");

    tgcount = 0;
    tgptr = model->lm3g.trigrams;
    prev_w1 = -1;
    prev_w2 = -1;
    prev_bg = -1;
    prev_seg = -1;

    while ((*li = lineiter_next(*li))) {
        float32 p;
        int32 p3;
        char *wptr[4], *word1, *word2, *word3;

        string_trim((*li)->buf, STRING_BOTH);
        if (str2words((*li)->buf, wptr, 4) != 4) {
            if ((*li)->buf[0] != '\0')
                break;
            continue;
        }
        else {
            p = (float32)atof_c(wptr[0]);
            word1 = wptr[1];
            word2 = wptr[2];
            word3 = wptr[3];
        }

        if ((w1 = ngram_wid_legacy(base, word1)) == NGRAM_INVALID_WID_legacy) {
            E_ERROR("Unknown word: %s, skipping trigram (%s %s %s)\n",
                    word1, word1, word2, word3);
            continue;
        }
        if ((w2 = ngram_wid_legacy(base, word2)) == NGRAM_INVALID_WID_legacy) {
            E_ERROR("Unknown word: %s, skipping trigram (%s %s %s)\n",
                    word2, word1, word2, word3);
            continue;
        }
        if ((w3 = ngram_wid_legacy(base, word3)) == NGRAM_INVALID_WID_legacy) {
            E_ERROR("Unknown word: %s, skipping trigram (%s %s %s)\n",
                    word3, word1, word2, word3);
            continue;
        }

        /* FIXME: Should use logmath_t quantization here. */
        /* HACK!! to quantize probs to 4 decimal digits */
        p = (float32)((int32)(p * 10000)) / 10000;
        p3 = logmath_log10_to_log(base->lmath, p);

        if (tgcount >= base->n_counts[2]) {
            E_ERROR("Too many trigrams\n");
            return -1;
        }

        tgptr->wid = w3;
        tgptr->prob3 = sorted_id_legacy(&model->sorted_prob3, &p3);

        if ((w1 != prev_w1) || (w2 != prev_w2)) {
            /* Trigram for a new bigram; update tg info for all previous bigrams */
            if ((w1 < prev_w1) || ((w1 == prev_w1) && (w2 < prev_w2))) {
                E_ERROR("Trigrams not in bigram order\n");
                return -1;
            }

            bg = (w1 !=
                  prev_w1) ? model->lm3g.unigrams[w1].bigrams : prev_bg + 1;
            endbg = model->lm3g.unigrams[w1 + 1].bigrams;
            bgptr = model->lm3g.bigrams + bg;
            for (; (bg < endbg) && (bgptr->wid != w2); bg++, bgptr++);
            if (bg >= endbg) {
                E_ERROR("Missing bigram for trigram: %s", (*li)->buf);
                return -1;
            }

            /* bg = bigram entry index for <w1,w2>.  Update tseg_base */
            seg = bg >> LOG_BG_SEG_SZ_legacy;
            for (i = prev_seg + 1; i <= seg; i++)
                model->lm3g.tseg_base[i] = tgcount;

            /* Update trigrams pointers for all bigrams until bg */
            if (prev_seg < seg) {
                int32 tgoff = 0;

                if (prev_seg >= 0) {
                    tgoff = tgcount - model->lm3g.tseg_base[prev_seg];
                    if (tgoff > 65535) {
                        E_ERROR("Size of trigram segment is bigger than 65535, such a big language models are not supported, use smaller vocabulary\n");
                        return -1;
                    }
                }

                prev_seg_lastbg = ((prev_seg + 1) << LOG_BG_SEG_SZ_legacy) - 1;
                bgptr = model->lm3g.bigrams + prev_bg;
                for (++prev_bg, ++bgptr; prev_bg <= prev_seg_lastbg;
                     prev_bg++, bgptr++)
                    bgptr->trigrams = tgoff;

                for (; prev_bg <= bg; prev_bg++, bgptr++)
                    bgptr->trigrams = 0;
            }
            else {
                int32 tgoff;

                tgoff = tgcount - model->lm3g.tseg_base[prev_seg];
                if (tgoff > 65535) {
                    E_ERROR("Size of trigram segment is bigger than 65535, such a big language models are not supported, use smaller vocabulary\n");
                    return -1;
                }

                bgptr = model->lm3g.bigrams + prev_bg;
                for (++prev_bg, ++bgptr; prev_bg <= bg; prev_bg++, bgptr++)
                    bgptr->trigrams = tgoff;
            }

            prev_w1 = w1;
            prev_w2 = w2;
            prev_bg = bg;
            prev_seg = seg;
        }

        tgcount++;
        tgptr++;

        if ((tgcount & 0x0000ffff) == 0) {
            E_INFOCONT(".");
        }
    }
    if (*li == NULL || strcmp((*li)->buf, "\\end\\") != 0) {
        E_ERROR("Bad trigram: %s\n", (*li)->buf);
        return -1;
    }

    for (prev_bg++; prev_bg <= base->n_counts[1]; prev_bg++) {
        if ((prev_bg & (BG_SEG_SZ_legacy - 1)) == 0)
            model->lm3g.tseg_base[prev_bg >> LOG_BG_SEG_SZ_legacy] = tgcount;
        if ((tgcount - model->lm3g.tseg_base[prev_bg >> LOG_BG_SEG_SZ_legacy]) > 65535) {
            E_ERROR("Size of trigram segment is bigger than 65535, such a big language models are not supported, use smaller vocabulary\n");
            return -1;
        }
        model->lm3g.bigrams[prev_bg].trigrams =
            tgcount - model->lm3g.tseg_base[prev_bg >> LOG_BG_SEG_SZ_legacy];
    }
    return 0;
}

static unigram_t_legacy *
new_unigram_table_legacy(int32 n_ug)
{
    unigram_t_legacy *table;
    int32 i;

    table = ckd_calloc(n_ug, sizeof(unigram_t_legacy));
    for (i = 0; i < n_ug; i++) {
        table[i].prob1.l = INT_MIN;
        table[i].bo_wt1.l = INT_MIN;
    }
    return table;
}

ngram_model_t_legacy *
ngram_model_arpa_read_legacy(cmd_ln_t *config,
		      const char *file_name,
		      logmath_t *lmath)
{
    lineiter_t *li;
    FILE *fp;
    int32 is_pipe;
    int32 n_unigram;
    int32 n_bigram;
    int32 n_trigram;
    int32 n;
    ngram_model_arpa_t_legacy *model;
    ngram_model_t_legacy *base;

    int numberoffails = 0; // we try twice if it doesn't work, but if the first try doesn't fail we don't try a second time.
    
    if ((fp = fopen_comp(file_name, "r", &is_pipe)) == NULL) { // first try
        numberoffails++; // increment number of fails if we fail.
        CFDictionaryKeyCallBacks keyCallbacks = {0, NULL, NULL, CFCopyDescription, CFEqual, NULL}; 
        CFDictionaryValueCallBacks valueCallbacks  = {0, NULL, NULL, CFCopyDescription, CFEqual};
        CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, 
                                                                      &keyCallbacks, &valueCallbacks);
        
        CFStringRef path_cfstring = CFStringCreateWithCString(NULL, file_name, kCFStringEncodingUTF8) ;
        CFStringRef pathkey_cfstring = CFStringCreateWithCString(NULL, "Path", kCFStringEncodingUTF8) ;
        CFDictionaryAddValue(dictionary, pathkey_cfstring, path_cfstring);
        
        CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), CFSTR("RequestToUnarchive"), NULL, dictionary, TRUE);
        
        CFRelease(path_cfstring);
        CFRelease(pathkey_cfstring);
        CFRelease(dictionary);
        
        usleep(4999); 
    }
    
    if(numberoffails > 0) { // We give this second round a try if the first one failed, but if it didn't, we don't do this.
        if ((fp = fopen_comp(file_name, "r", &is_pipe)) == NULL) {
            E_ERROR("arpa file %s not found\n", file_name);
            return NULL;
        }
    }
    
    li = lineiter_start(fp);

 
    /* Read #unigrams, #bigrams, #trigrams from file */
    if (ReadNgramCounts_legacy(&li, &n_unigram, &n_bigram, &n_trigram) == -1) {
        lineiter_free(li);
        fclose_comp(fp, is_pipe);
        return NULL;
    }
    E_INFO("ngrams 1=%d, 2=%d, 3=%d\n", n_unigram, n_bigram, n_trigram);

    /* Allocate space for LM, including initial OOVs and placeholders; initialize it */
    model = ckd_calloc(1, sizeof(*model));
    base = &model->base;
    if (n_trigram > 0)
        n = 3;
    else if (n_bigram > 0)
        n = 2;
    else
        n = 1;
    /* Initialize base model. */
    ngram_model_init_legacy(base, &ngram_model_arpa_funcs_legacy, lmath, n, n_unigram);
    base->n_counts[0] = n_unigram;
    base->n_counts[1] = n_bigram;
    base->n_counts[2] = n_trigram;
    base->writable = TRUE;

    /*
     * Allocate one extra unigram and bigram entry: sentinels to terminate
     * followers (bigrams and trigrams, respectively) of previous entry.
     */
    model->lm3g.unigrams = new_unigram_table_legacy(n_unigram + 1);
    model->lm3g.bigrams =
        ckd_calloc(n_bigram + 1, sizeof(bigram_t_legacy));
    if (n_trigram > 0)
        model->lm3g.trigrams =
            ckd_calloc(n_trigram, sizeof(trigram_t_legacy));

    if (n_trigram > 0) {
        model->lm3g.tseg_base =
            ckd_calloc((n_bigram + 1) / BG_SEG_SZ_legacy + 1,
                       sizeof(int32));
    }
    if (ReadUnigrams_legacy(&li, model) == -1) {
        fclose_comp(fp, is_pipe);
        ngram_model_free_legacy(base);
        return NULL;
    }
    E_INFO("%8d = #unigrams created\n", base->n_counts[0]);

    if (base->n_counts[2] > 0)
        init_sorted_list_legacy(&model->sorted_bo_wt2);

    if (base->n_counts[1] > 0) {
        init_sorted_list_legacy(&model->sorted_prob2);

        if (ReadBigrams_legacy(&li, model) == -1) {
            fclose_comp(fp, is_pipe);
            ngram_model_free_legacy(base);
            return NULL;
        }

        base->n_counts[1] = FIRST_BG_legacy(model, base->n_counts[0]);
        model->lm3g.n_prob2 = model->sorted_prob2.free;
        model->lm3g.prob2 = vals_in_sorted_list_legacy(&model->sorted_prob2);
        free_sorted_list_legacy(&model->sorted_prob2);
        E_INFO("%8d = #bigrams created\n", base->n_counts[1]);
        E_INFO("%8d = #prob2 entries\n", model->lm3g.n_prob2);
    }

    if (base->n_counts[2] > 0) {
        /* Create trigram bo-wts array */
        model->lm3g.n_bo_wt2 = model->sorted_bo_wt2.free;
        model->lm3g.bo_wt2 = vals_in_sorted_list_legacy(&model->sorted_bo_wt2);
        free_sorted_list_legacy(&model->sorted_bo_wt2);
        E_INFO("%8d = #bo_wt2 entries\n", model->lm3g.n_bo_wt2);

        init_sorted_list_legacy(&model->sorted_prob3);

        if (ReadTrigrams_legacy(&li, model) == -1) {
            fclose_comp(fp, is_pipe);
            ngram_model_free_legacy(base);
            return NULL;
        }

        base->n_counts[2] = FIRST_TG_legacy(model, base->n_counts[1]);
        model->lm3g.n_prob3 = model->sorted_prob3.free;
        model->lm3g.prob3 = vals_in_sorted_list_legacy(&model->sorted_prob3);
        E_INFO("%8d = #trigrams created\n", base->n_counts[2]);
        E_INFO("%8d = #prob3 entries\n", model->lm3g.n_prob3);

        free_sorted_list_legacy(&model->sorted_prob3);

        /* Initialize tginfo */
        model->lm3g.tginfo = ckd_calloc(n_unigram, sizeof(tginfo_t_legacy *));
        model->lm3g.le = listelem_alloc_init(sizeof(tginfo_t_legacy));
    }

    lineiter_free(li);
    fclose_comp(fp, is_pipe);
    return base;
}

int
ngram_model_arpa_write_legacy(ngram_model_t_legacy *model,
		       const char *file_name)
{
    ngram_iter_t_legacy *itor;
    FILE *fh;
    int i;

    if ((fh = fopen(file_name, "w")) == NULL) {
        E_ERROR_SYSTEM("Failed to open %s for writing", file_name);
        return -1;
    }
    fprintf(fh, "This is an ARPA-format language model file, generated by CMU Sphinx\n");

    /* The ARPA format doesn't require any extra information that
     * N-Gram iterators can't give us, so this is very
     * straightforward compared with DMP writing. */

    /* Write N-gram counts. */
    fprintf(fh, "\\data\\\n");
    for (i = 0; i < model->n; ++i) {
        fprintf(fh, "ngram %d=%d\n", i+1, model->n_counts[i]);
    }

    /* Write N-grams */
    for (i = 0; i < model->n; ++i) {
        fprintf(fh, "\n\\%d-grams:\n", i + 1);
        for (itor = ngram_model_mgrams_legacy(model, i); itor; itor = ngram_iter_next_legacy(itor)) {
            int32 const *wids;
            int32 score, bowt;
            int j;

            wids = ngram_iter_get_legacy(itor, &score, &bowt);
            fprintf(fh, "%.4f ", logmath_log_to_log10(model->lmath, score));
            for (j = 0; j <= i; ++j) {
                assert(wids[j] < model->n_counts[0]);
                fprintf(fh, "%s ", model->word_str[wids[j]]);
            }
            if (i < model->n-1)
                fprintf(fh, "%.4f", logmath_log_to_log10(model->lmath, bowt));
            fprintf(fh, "\n");
        }
    }
    fprintf(fh, "\n\\end\\\n");
    return fclose(fh);
}

static int
ngram_model_arpa_apply_weights_legacy(ngram_model_t_legacy *base, float32 lw,
                              float32 wip, float32 uw)
{
    ngram_model_arpa_t_legacy *model = (ngram_model_arpa_t_legacy *)base;
    lm3g_apply_weights_legacy(base, &model->lm3g, lw, wip, uw);
    return 0;
}

/* Lousy "templating" for things that are largely the same in DMP and
 * ARPA models, except for the bigram and trigram types and some
 * names. */
#define NGRAM_MODEL_TYPE_legacy ngram_model_arpa_t_legacy

/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2007 Carnegie Mellon University.  All rights
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
/*
 * \file lm3g_templates.c Core Sphinx 3-gram code used in
 * DMP/ARPA (for now) model code.
 */

#include <assert.h>

/* Locate a specific bigram within a bigram list */
#define BINARY_SEARCH_THRESH_legacy	16
static int32
find_bg_legacy(bigram_t_legacy * bg, int32 n, int32 w)
{
    int32 i, b, e;
    
    /* Binary search until segment size < threshold */
    b = 0;
    e = n;
    while (e - b > BINARY_SEARCH_THRESH_legacy) {
        i = (b + e) >> 1;
        if (bg[i].wid < w)
            b = i + 1;
        else if (bg[i].wid > w)
            e = i;
        else
            return i;
    }
    
    /* Linear search within narrowed segment */
    for (i = b; (i < e) && (bg[i].wid != w); i++);
    return ((i < e) ? i : -1);
}

static int32
lm3g_bg_score_legacy(NGRAM_MODEL_TYPE_legacy *model,
                     int32 lw1, int32 lw2, int32 *n_used)
{
    int32 i, n, b, score;
    bigram_t_legacy *bg;
    
    if (lw1 < 0 || model->base.n < 2) {
        *n_used = 1;
        return model->lm3g.unigrams[lw2].prob1.l;
    }
    
    b = FIRST_BG_legacy(model, lw1);
    n = FIRST_BG_legacy(model, lw1 + 1) - b;
    bg = model->lm3g.bigrams + b;
    
    if ((i = find_bg_legacy(bg, n, lw2)) >= 0) {
        /* Access mode = bigram */
        *n_used = 2;
        score = model->lm3g.prob2[bg[i].prob2].l;
    }
    else {
        /* Access mode = unigram */
        *n_used = 1;
        score = model->lm3g.unigrams[lw1].bo_wt1.l + model->lm3g.unigrams[lw2].prob1.l;
    }
    
    return (score);
}

static void
load_tginfo_legacy(NGRAM_MODEL_TYPE_legacy *model, int32 lw1, int32 lw2)
{
    int32 i, n, b, t;
    bigram_t_legacy *bg;
    tginfo_t_legacy *tginfo;
    
    /* First allocate space for tg information for bg lw1,lw2 */
    tginfo = (tginfo_t_legacy *) listelem_malloc(model->lm3g.le);
    tginfo->w1 = lw1;
    tginfo->tg = NULL;
    tginfo->next = model->lm3g.tginfo[lw2];
    model->lm3g.tginfo[lw2] = tginfo;
    
    /* Locate bigram lw1,lw2 */
    b = model->lm3g.unigrams[lw1].bigrams;
    n = model->lm3g.unigrams[lw1 + 1].bigrams - b;
    bg = model->lm3g.bigrams + b;
    
    if ((n > 0) && ((i = find_bg_legacy(bg, n, lw2)) >= 0)) {
        tginfo->bowt = model->lm3g.bo_wt2[bg[i].bo_wt2].l;
        
        /* Find t = Absolute first trigram index for bigram lw1,lw2 */
        b += i;                 /* b = Absolute index of bigram lw1,lw2 on disk */
        t = FIRST_TG_legacy(model, b);
        
        tginfo->tg = model->lm3g.trigrams + t;
        
        /* Find #tg for bigram w1,w2 */
        tginfo->n_tg = FIRST_TG_legacy(model, b + 1) - t;
    }
    else {                      /* No bigram w1,w2 */
        tginfo->bowt = 0;
        tginfo->n_tg = 0;
    }
}

/* Similar to find_bg */
static int32
find_tg_legacy(trigram_t_legacy * tg, int32 n, uint32 w)
{
    int32 i, b, e;
    
    b = 0;
    e = n;
    while (e - b > BINARY_SEARCH_THRESH_legacy) {
        i = (b + e) >> 1;
        if (tg[i].wid < w)
            b = i + 1;
        else if (tg[i].wid > w)
            e = i;
        else
            return i;
    }
    
    for (i = b; (i < e) && (tg[i].wid != w); i++);
    return ((i < e) ? i : -1);
}

static int32
lm3g_tg_score_legacy(NGRAM_MODEL_TYPE_legacy *model, int32 lw1,
                     int32 lw2, int32 lw3, int32 *n_used)
{
    ngram_model_t_legacy *base = &model->base;
    int32 i, n, score;
    trigram_t_legacy *tg;
    tginfo_t_legacy *tginfo, *prev_tginfo;
    
    if ((base->n < 3) || (lw1 < 0) || (lw2 < 0))
        return (lm3g_bg_score_legacy(model, lw2, lw3, n_used));
    
    prev_tginfo = NULL;
    for (tginfo = model->lm3g.tginfo[lw2]; tginfo; tginfo = tginfo->next) {
        if (tginfo->w1 == lw1)
            break;
        prev_tginfo = tginfo;
    }
    
    if (!tginfo) {
        load_tginfo_legacy(model, lw1, lw2);
        tginfo = model->lm3g.tginfo[lw2];
    }
    else if (prev_tginfo) {
        prev_tginfo->next = tginfo->next;
        tginfo->next = model->lm3g.tginfo[lw2];
        model->lm3g.tginfo[lw2] = tginfo;
    }
    
    tginfo->used = 1;
    
    /* Trigrams for w1,w2 now pointed to by tginfo */
    n = tginfo->n_tg;
    tg = tginfo->tg;
    if ((i = find_tg_legacy(tg, n, lw3)) >= 0) {
        /* Access mode = trigram */
        *n_used = 3;
        score = model->lm3g.prob3[tg[i].prob3].l;
    }
    else {
        score = tginfo->bowt + lm3g_bg_score_legacy(model, lw2, lw3, n_used);
    }
    
    return (score);
}

static int32
lm3g_template_score_legacy(ngram_model_t_legacy *base, int32 wid,
                           int32 *history, int32 n_hist,
                           int32 *n_used)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base;
    switch (n_hist) {
        case 0:
            /* Access mode: unigram */
            *n_used = 1;
            return model->lm3g.unigrams[wid].prob1.l;
        case 1:
            return lm3g_bg_score_legacy(model, history[0], wid, n_used);
        case 2:
        default:
            /* Anything greater than 2 is the same as a trigram for now. */
            return lm3g_tg_score_legacy(model, history[1], history[0], wid, n_used);
    }
}

static int32
lm3g_template_raw_score_legacy(ngram_model_t_legacy *base, int32 wid,
                               int32 *history, int32 n_hist,
                               int32 *n_used)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base;
    int32 score;
    
    switch (n_hist) {
        case 0:
            /* Access mode: unigram */
            *n_used = 1;
            /* Undo insertion penalty. */
            score = model->lm3g.unigrams[wid].prob1.l - base->log_wip;
            /* Undo language weight. */
            score = (int32)(score / base->lw);
            /* Undo unigram interpolation */
            if (strcmp(base->word_str[wid], "<s>") != 0) { /* FIXME: configurable start_sym */
                /* This operation is numerically unstable, so try to avoid it
                 * as possible */
                if (base->log_uniform + base->log_uniform_weight > logmath_get_zero(base->lmath)) {
                    score = logmath_log(base->lmath,
                                        logmath_exp(base->lmath, score)
                                        - logmath_exp(base->lmath, 
                                                      base->log_uniform + base->log_uniform_weight));
                }
            }
            return score;
        case 1:
            score = lm3g_bg_score_legacy(model, history[0], wid, n_used);
            break;
        case 2:
        default:
            /* Anything greater than 2 is the same as a trigram for now. */
            score = lm3g_tg_score_legacy(model, history[1], history[0], wid, n_used);
            break;
    }
    /* FIXME (maybe): This doesn't undo unigram weighting in backoff cases. */
    return (int32)((score - base->log_wip) / base->lw);
}

static int32
lm3g_template_add_ug_legacy(ngram_model_t_legacy *base,
                            int32 wid, int32 lweight)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base;
    return lm3g_add_ug_legacy(base, &model->lm3g, wid, lweight);
}

static void
lm3g_template_flush_legacy(ngram_model_t_legacy *base)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base;
    lm3g_tginfo_reset_legacy(base, &model->lm3g);
}

typedef struct lm3g_iter_s_legacy {
    ngram_iter_t_legacy base;
    unigram_t_legacy *ug;
    bigram_t_legacy *bg;
    trigram_t_legacy *tg;
} lm3g_iter_t_legacy;

static ngram_iter_t_legacy *
lm3g_template_iter_legacy(ngram_model_t_legacy *base, int32 wid,
                          int32 *history, int32 n_hist)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base;
    lm3g_iter_t_legacy *itor = (lm3g_iter_t_legacy *)ckd_calloc(1, sizeof(*itor));
    
    ngram_iter_init_legacy((ngram_iter_t_legacy *)itor, base, n_hist, FALSE);
    
    if (n_hist == 0) {
        /* Unigram is the easiest. */
        itor->ug = model->lm3g.unigrams + wid;
        return (ngram_iter_t_legacy *)itor;
    }
    else if (n_hist == 1) {
        int32 i, n, b;
        /* Find the bigram, as in bg_score above (duplicate code...) */
        itor->ug = model->lm3g.unigrams + history[0];
        b = FIRST_BG_legacy(model, history[0]);
        n = FIRST_BG_legacy(model, history[0] + 1) - b;
        itor->bg = model->lm3g.bigrams + b;
        /* If no such bigram exists then fail. */
        if ((i = find_bg_legacy(itor->bg, n, wid)) < 0) {
            ngram_iter_free_legacy((ngram_iter_t_legacy *)itor);
            return NULL;
        }
        itor->bg += i;
        return (ngram_iter_t_legacy *)itor;
    }
    else if (n_hist == 2) {
        int32 i, n;
        tginfo_t_legacy *tginfo, *prev_tginfo;
        /* Find the trigram, as in tg_score above (duplicate code...) */
        itor->ug = model->lm3g.unigrams + history[1];
        prev_tginfo = NULL;
        for (tginfo = model->lm3g.tginfo[history[0]];
             tginfo; tginfo = tginfo->next) {
            if (tginfo->w1 == history[1])
                break;
            prev_tginfo = tginfo;
        }
        
        if (!tginfo) {
            load_tginfo_legacy(model, history[1], history[0]);
            tginfo = model->lm3g.tginfo[history[0]];
        }
        else if (prev_tginfo) {
            prev_tginfo->next = tginfo->next;
            tginfo->next = model->lm3g.tginfo[history[0]];
            model->lm3g.tginfo[history[0]] = tginfo;
        }
        
        tginfo->used = 1;
        
        /* Trigrams for w1,w2 now pointed to by tginfo */
        n = tginfo->n_tg;
        itor->tg = tginfo->tg;
        if ((i = find_tg_legacy(itor->tg, n, wid)) >= 0) {
            itor->tg += i;
            /* Now advance the bigram pointer accordingly.  FIXME:
             * Note that we actually already found the relevant bigram
             * in load_tginfo. */
            itor->bg = model->lm3g.bigrams;
            while (FIRST_TG_legacy(model, (itor->bg - model->lm3g.bigrams + 1))
                   <= (itor->tg - model->lm3g.trigrams))
                ++itor->bg;
            return (ngram_iter_t_legacy *)itor;
        }
        else {
            ngram_iter_free_legacy((ngram_iter_t_legacy *)itor);
            return (ngram_iter_t_legacy *)NULL;
        }
    }
    else {
        /* Should not happen. */
        assert(n_hist == 0); /* Guaranteed to fail. */
        ngram_iter_free_legacy((ngram_iter_t_legacy *)itor);
        return NULL;
    }
}

static ngram_iter_t_legacy *
lm3g_template_mgrams_legacy(ngram_model_t_legacy *base, int m)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base;
    lm3g_iter_t_legacy *itor = (lm3g_iter_t_legacy *)ckd_calloc(1, sizeof(*itor));
    ngram_iter_init_legacy((ngram_iter_t_legacy *)itor, base, m, FALSE);
    
    itor->ug = model->lm3g.unigrams;
    itor->bg = model->lm3g.bigrams;
    itor->tg = model->lm3g.trigrams;
    
    /* Advance bigram pointer to match first trigram. */
    if (m > 1 && base->n_counts[1] > 1)  {
        while (FIRST_TG_legacy(model, (itor->bg - model->lm3g.bigrams + 1))
               <= (itor->tg - model->lm3g.trigrams))
            ++itor->bg;
    }
    
    /* Advance unigram pointer to match first bigram. */
    if (m > 0 && base->n_counts[0] > 1) {
        while (itor->ug[1].bigrams <= (itor->bg - model->lm3g.bigrams))
            ++itor->ug;
    }
    
    return (ngram_iter_t_legacy *)itor;
}

static ngram_iter_t_legacy *
lm3g_template_successors_legacy(ngram_iter_t_legacy *bitor)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)bitor->model;
    lm3g_iter_t_legacy *from = (lm3g_iter_t_legacy *)bitor;
    lm3g_iter_t_legacy *itor = (lm3g_iter_t_legacy *)ckd_calloc(1, sizeof(*itor));
    
    itor->ug = from->ug;
    switch (bitor->m) {
        case 0:
            /* Next itor bigrams is the same as this itor bigram or
             itor bigrams is more than total count. This means no successors */
            if (((itor->ug + 1) - model->lm3g.unigrams < bitor->model->n_counts[0] &&
                 itor->ug->bigrams == (itor->ug + 1)->bigrams) || 
                itor->ug->bigrams == bitor->model->n_counts[1])
                goto done;
            
            /* Start iterating from first bigram successor of from->ug. */
            itor->bg = model->lm3g.bigrams + itor->ug->bigrams;
            break;
        case 1:
            itor->bg = from->bg;
            
            /* This indicates no successors */
            if (((itor->bg + 1) - model->lm3g.bigrams < bitor->model->n_counts[1] &&
                 FIRST_TG_legacy (model, itor->bg - model->lm3g.bigrams) == 
                 FIRST_TG_legacy (model, (itor->bg + 1) - model->lm3g.bigrams)) ||
                FIRST_TG_legacy (model, itor->bg - model->lm3g.bigrams) == bitor->model->n_counts[2])
                goto done;
            
            /* Start iterating from first trigram successor of from->bg. */
            itor->tg = (model->lm3g.trigrams 
                        + FIRST_TG_legacy(model, (itor->bg - model->lm3g.bigrams)));
#if 0
            printf("%s %s => %d (%s)\n",
                   model->base.word_str[itor->ug - model->lm3g.unigrams],
                   model->base.word_str[itor->bg->wid],
                   FIRST_TG_legacy(model, (itor->bg - model->lm3g.bigrams)),
                   model->base.word_str[itor->tg->wid]);
#endif
            break;
        case 2:
        default:
            /* All invalid! */
            goto done;
    }
    
    ngram_iter_init_legacy((ngram_iter_t_legacy *)itor, bitor->model, bitor->m + 1, TRUE);
    return (ngram_iter_t_legacy *)itor;
done:
    ckd_free(itor);
    return NULL;
}

static int32 const *
lm3g_template_iter_get_legacy(ngram_iter_t_legacy *base,
                              int32 *out_score, int32 *out_bowt)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base->model;
    lm3g_iter_t_legacy *itor = (lm3g_iter_t_legacy *)base;
    
    base->wids[0] = (int32)(itor->ug - model->lm3g.unigrams);
    if (itor->bg) base->wids[1] = itor->bg->wid;
    if (itor->tg) base->wids[2] = itor->tg->wid;
#if 0
    printf("itor_get: %d %d %d\n", base->wids[0], base->wids[1], base->wids[2]);
#endif
    
    switch (base->m) {
        case 0:
            *out_score = itor->ug->prob1.l;
            *out_bowt = itor->ug->bo_wt1.l;
            break;
        case 1:
            *out_score = model->lm3g.prob2[itor->bg->prob2].l;
            if (model->lm3g.bo_wt2)
                *out_bowt = model->lm3g.bo_wt2[itor->bg->bo_wt2].l;
            else
                *out_bowt = 0;
            break;
        case 2:
            *out_score = model->lm3g.prob3[itor->tg->prob3].l;
            *out_bowt = 0;
            break;
        default: /* Should not happen. */
            return NULL;
    }
    return base->wids;
}

static ngram_iter_t_legacy *
lm3g_template_iter_next_legacy(ngram_iter_t_legacy *base)
{
    NGRAM_MODEL_TYPE_legacy *model = (NGRAM_MODEL_TYPE_legacy *)base->model;
    lm3g_iter_t_legacy *itor = (lm3g_iter_t_legacy *)base;
    
    switch (base->m) {
        case 0:
            ++itor->ug;
            /* Check for end condition. */
            if (itor->ug - model->lm3g.unigrams >= base->model->n_counts[0])
                goto done;
            break;
        case 1:
            ++itor->bg;
            /* Check for end condition. */
            if (itor->bg - model->lm3g.bigrams >= base->model->n_counts[1])
                goto done;
            /* Advance unigram pointer if necessary in order to get one
             * that points to this bigram. */
            while (itor->bg - model->lm3g.bigrams >= itor->ug[1].bigrams) {
                /* Stop if this is a successor iterator, since we don't
                 * want a new unigram. */
                if (base->successor)
                    goto done;
                ++itor->ug;
                if (itor->ug == model->lm3g.unigrams + base->model->n_counts[0]) {
                    E_ERROR("Bigram %d has no valid unigram parent\n",
                            itor->bg - model->lm3g.bigrams);
                    goto done;
                }
            }
            break;
        case 2:
            ++itor->tg;
            /* Check for end condition. */
            if (itor->tg - model->lm3g.trigrams >= base->model->n_counts[2])
                goto done;
            /* Advance bigram pointer if necessary. */
            while (itor->tg - model->lm3g.trigrams >=
                   FIRST_TG_legacy(model, (itor->bg - model->lm3g.bigrams + 1))) {
                if (base->successor)
                    goto done;
                ++itor->bg;
                if (itor->bg == model->lm3g.bigrams + base->model->n_counts[1]) {
                    E_ERROR("Trigram %d has no valid bigram parent\n",
                            itor->tg - model->lm3g.trigrams);
                    
                    goto done;
                }
            }
            /* Advance unigram pointer if necessary. */
            while (itor->bg - model->lm3g.bigrams >= itor->ug[1].bigrams) {
                ++itor->ug;
                if (itor->ug == model->lm3g.unigrams + base->model->n_counts[0]) {
                    E_ERROR("Trigram %d has no valid unigram parent\n",
                            itor->tg - model->lm3g.trigrams);
                    goto done;
                }
            }
            break;
        default: /* Should not happen. */
            goto done;
    }
    
    return (ngram_iter_t_legacy *)itor;
done:
    ngram_iter_free_legacy(base);
    return NULL;
}

static void
lm3g_template_iter_free_legacy(ngram_iter_t_legacy *base)
{
    ckd_free(base);
}


static void
ngram_model_arpa_free_legacy(ngram_model_t_legacy *base)
{
    ngram_model_arpa_t_legacy *model = (ngram_model_arpa_t_legacy *)base;
    ckd_free(model->lm3g.unigrams);
    ckd_free(model->lm3g.bigrams);
    ckd_free(model->lm3g.trigrams);
    ckd_free(model->lm3g.prob2);
    ckd_free(model->lm3g.bo_wt2);
    ckd_free(model->lm3g.prob3);
    lm3g_tginfo_free_legacy(base, &model->lm3g);
    ckd_free(model->lm3g.tseg_base);
}

static ngram_funcs_t_legacy ngram_model_arpa_funcs_legacy = {
    ngram_model_arpa_free_legacy,          /* free */
    ngram_model_arpa_apply_weights_legacy, /* apply_weights */
    lm3g_template_score_legacy,            /* score */
    lm3g_template_raw_score_legacy,        /* raw_score */
    lm3g_template_add_ug_legacy,           /* add_ug */
    lm3g_template_flush_legacy,            /* flush */
    lm3g_template_iter_legacy,             /* iter */
    lm3g_template_mgrams_legacy,           /* mgrams */
    lm3g_template_successors_legacy,       /* successors */
    lm3g_template_iter_get_legacy,         /* iter_get */
    lm3g_template_iter_next_legacy,        /* iter_next */
    lm3g_template_iter_free_legacy        /* iter_free */
};
