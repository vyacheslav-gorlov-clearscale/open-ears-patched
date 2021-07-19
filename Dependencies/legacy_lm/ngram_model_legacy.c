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
 * \file ngram_model.c N-Gram language models.
 *
 * Author: David Huggins-Daines, much code taken from sphinx3/src/libs3decoder/liblm
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <assert.h>

#include "sphinxbase/ngram_model.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/filename.h"
#include "sphinxbase/pio.h"
#include "sphinxbase/err.h"
#include "sphinxbase/logmath.h"
#include "sphinxbase/strfuncs.h"
#include "sphinxbase/case.h"

#include "ngram_model_internal_legacy.h"

ngram_file_type_t_legacy
ngram_file_name_to_type_legacy(const char *file_name)
{
    const char *ext;

    ext = strrchr(file_name, '.');
    if (ext == NULL) {
        return NGRAM_INVALID_legacy;
    }
    if (0 == strcmp_nocase(ext, ".gz")) {
        while (--ext >= file_name) {
            if (*ext == '.') break;
        }
        if (ext < file_name) {
            return NGRAM_INVALID_legacy;
         }
     }
     else if (0 == strcmp_nocase(ext, ".bz2")) {
         while (--ext >= file_name) {
             if (*ext == '.') break;
         }
         if (ext < file_name) {
             return NGRAM_INVALID_legacy;
         }
     }
     /* We use strncmp because there might be a .gz on the end. */
     if (0 == strncmp_nocase(ext, ".ARPA", 5))
         return NGRAM_ARPA_legacy;
     if (0 == strncmp_nocase(ext, ".DMP", 4))
         return NGRAM_DMP_legacy;
     return NGRAM_INVALID_legacy;
 }

ngram_file_type_t_legacy
ngram_str_to_type_legacy(const char *str_name)
{
    if (0 == strcmp_nocase(str_name, "arpa"))
        return NGRAM_ARPA_legacy;
    if (0 == strcmp_nocase(str_name, "dmp"))
        return NGRAM_DMP_legacy;
    return NGRAM_INVALID_legacy;
}

char const *
ngram_type_to_str_legacy(int type)
{
    switch (type) {
    case NGRAM_ARPA_legacy:
        return "arpa";
    case NGRAM_DMP_legacy:
        return "dmp";
    default:
        return NULL;
    }
}


 ngram_model_t_legacy *
 ngram_model_read_legacy(cmd_ln_t *config,
                  const char *file_name,
                  ngram_file_type_t_legacy file_type,
                  logmath_t *lmath)
 {
     ngram_model_t_legacy *model = NULL;

     switch (file_type) {
     case NGRAM_AUTO: {
         if ((model = ngram_model_arpa_read_legacy(config, file_name, lmath)) != NULL)
             break;
         if ((model = ngram_model_dmp_read_legacy(config, file_name, lmath)) != NULL)
             break;
         return NULL;
     }
     case NGRAM_ARPA_legacy:
         model = ngram_model_arpa_read_legacy(config, file_name, lmath);
         break;
     case NGRAM_DMP_legacy:
         model = ngram_model_dmp_read_legacy(config, file_name, lmath);
         break;
     default:
         E_ERROR("reading of this legacy language model file type not supported\n");
         return NULL;
     }

     /* Now set weights based on config if present. */
     if (config) {
         float32 lw = 1.0;
         float32 wip = 1.0;
         float32 uw = 1.0;

         if (cmd_ln_exists_r(config, "-lw"))
             lw = cmd_ln_float32_r(config, "-lw");
         if (cmd_ln_exists_r(config, "-wip"))
             wip = cmd_ln_float32_r(config, "-wip");
         if (cmd_ln_exists_r(config, "-uw"))
             uw = cmd_ln_float32_r(config, "-uw");

         ngram_model_apply_weights_legacy(model, lw, wip, uw);
     }

     return model;
 }

 int
 ngram_model_write_legacy(ngram_model_t_legacy *model, const char *file_name,
                   ngram_file_type_t_legacy file_type)
 {
     switch (file_type) {
     case NGRAM_AUTO_legacy: {
         file_type = ngram_file_name_to_type_legacy(file_name);
         /* Default to ARPA (catches .lm and other things) */
         if (file_type == NGRAM_INVALID_legacy)
             file_type = NGRAM_ARPA_legacy;
         return ngram_model_write_legacy(model, file_name, file_type);
     }
     case NGRAM_ARPA_legacy:
         return ngram_model_arpa_write_legacy(model, file_name);
     case NGRAM_DMP_legacy:
         return ngram_model_dmp_write_legacy(model, file_name);
     default:
         E_ERROR("writing of this legacy language model file type (neither ARPA nor DMP) not supported\n");
         return -1;
     }
     E_ERROR("writing of this legacy language model file type (unknown type) not supported\n");
     return -1;
 }

 int32
 ngram_model_init_legacy(ngram_model_t_legacy *base,
                  ngram_funcs_t_legacy *funcs,
                  logmath_t *lmath,
                  int32 n, int32 n_unigram)
 {
     base->refcount = 1;
     base->funcs = funcs;
     base->n = n;
     /* If this was previously initialized... */
    if (base->n_counts == NULL)
        base->n_counts = ckd_calloc(3, sizeof(*base->n_counts));
    /* Don't reset weights if logmath object hasn't changed. */
    if (base->lmath != lmath) {
        /* Set default values for weights. */
        base->lw = 1.0;
        base->log_wip = 0; /* i.e. 1.0 */
        base->log_uw = 0;  /* i.e. 1.0 */
        base->log_uniform = logmath_log(lmath, 1.0 / n_unigram);
        base->log_uniform_weight = logmath_get_zero(lmath);
        base->log_zero = logmath_get_zero(lmath);
        base->lmath = lmath;
    }
    /* Allocate or reallocate space for word strings. */
    if (base->word_str) {
        /* Free all previous word strings if they were allocated. */
        if (base->writable) {
            int32 i;
            for (i = 0; i < base->n_words; ++i) {
                ckd_free(base->word_str[i]);
                base->word_str[i] = NULL;
            }
        }
        base->word_str = ckd_realloc(base->word_str, n_unigram * sizeof(char *));
    }
    else
        base->word_str = ckd_calloc(n_unigram, sizeof(char *));
    /* NOTE: They are no longer case-insensitive since we are allowing
     * other encodings for word strings.  Beware. */
    if (base->wid)
        hash_table_empty(base->wid);
    else
        base->wid = hash_table_new(n_unigram, FALSE);
    base->n_counts[0] = base->n_1g_alloc = base->n_words = n_unigram;

    return 0;
}

ngram_model_t_legacy *
ngram_model_retain_legacy(ngram_model_t_legacy *model)
{
    ++model->refcount;
    return model;
}


void
ngram_model_flush_legacy(ngram_model_t_legacy *model)
{
    if (model->funcs && model->funcs->flush_legacy)
        (*model->funcs->flush_legacy)(model);
}

int
ngram_model_free_legacy(ngram_model_t_legacy *model)
{
    int i;

    if (model == NULL)
        return 0;
    if (--model->refcount > 0)
        return model->refcount;
    if (model->funcs && model->funcs->free_legacy)
        (*model->funcs->free_legacy)(model);
    if (model->writable) {
        /* Free all words. */
        for (i = 0; i < model->n_words; ++i) {
            ckd_free(model->word_str[i]);
        }
    }
    else {
        /* Free all class words. */
        for (i = 0; i < model->n_classes; ++i) {
            ngram_class_t_legacy *lmclass;
            int32 j;

            lmclass = model->classes[i];
            for (j = 0; j < lmclass->n_words; ++j) {
                ckd_free(model->word_str[lmclass->start_wid + j]);
            }
            for (j = 0; j < lmclass->n_hash; ++j) {
                if (lmclass->nword_hash[j].wid != -1) {
                    ckd_free(model->word_str[lmclass->nword_hash[j].wid]);
                }
            }
        }
    }
    for (i = 0; i < model->n_classes; ++i) {
        ngram_class_free_legacy(model->classes[i]);
    }
    ckd_free(model->classes);
    hash_table_free(model->wid);
    ckd_free(model->word_str);
    ckd_free(model->n_counts);
    ckd_free(model);
    return 0;
}

int
ngram_model_casefold_legacy(ngram_model_t_legacy *model, int kase)
{
    int writable, i;
    hash_table_t *new_wid;

    /* Were word strings already allocated? */
    writable = model->writable;
    /* Either way, we are going to allocate some word strings. */
    model->writable = TRUE;

    /* And, don't forget, we need to rebuild the word to unigram ID
     * mapping. */
    new_wid = hash_table_new(model->n_words, FALSE);
    for (i = 0; i < model->n_words; ++i) {
        char *outstr;
        if (writable) {
            outstr = model->word_str[i];
        }
        else {
            outstr = ckd_salloc(model->word_str[i]);
        }
        /* Don't case-fold <tags> or [classes] */
        if (outstr[0] == '<' || outstr[0] == '[') {
        }
        else {
            switch (kase) {
            case NGRAM_UPPER_legacy:
                ucase(outstr);
                break;
            case NGRAM_LOWER_legacy:
                lcase(outstr);
                break;
            default:
                ;
            }
        }
        model->word_str[i] = outstr;

        /* Now update the hash table.  We might have terrible
         * collisions here, so warn about them. */
        if (hash_table_enter_int32(new_wid, model->word_str[i], i) != i) {
            E_WARN("Duplicate word in dictionary after conversion: %s\n",
                   model->word_str[i]);
        }
    }
    /* Swap out the hash table. */
    hash_table_free(model->wid);
    model->wid = new_wid;
    return 0;
}

int
ngram_model_apply_weights_legacy(ngram_model_t_legacy *model,
                          float32 lw, float32 wip, float32 uw)
{
    return (*model->funcs->apply_weights_legacy)(model, lw, wip, uw);
}

float32
ngram_model_get_weights_legacy(ngram_model_t_legacy *model, int32 *out_log_wip,
                        int32 *out_log_uw)
{
    if (out_log_wip) *out_log_wip = model->log_wip;
    if (out_log_uw) *out_log_uw = model->log_uw;
    return model->lw;
}


int32
ngram_ng_score_legacy(ngram_model_t_legacy *model, int32 wid, int32 *history,
               int32 n_hist, int32 *n_used)
{
    int32 score, class_weight = 0;
    int i;

    /* Closed vocabulary, OOV word probability is zero */
    if (wid == NGRAM_INVALID_WID_legacy)
        return model->log_zero;

    /* "Declassify" wid and history */
    if (NGRAM_IS_CLASSWID_legacy(wid)) {
        ngram_class_t_legacy *lmclass = model->classes[NGRAM_CLASSID_legacy(wid)];

        class_weight = ngram_class_prob_legacy(lmclass, wid);
        if (class_weight == 1) /* Meaning, not found in class. */
            return model->log_zero;
        wid = lmclass->tag_wid;
    }
    for (i = 0; i < n_hist; ++i) {
        if (history[i] != NGRAM_INVALID_WID_legacy && NGRAM_IS_CLASSWID_legacy(history[i]))
            history[i] = model->classes[NGRAM_CLASSID_legacy(history[i])]->tag_wid;
    }
    score = (*model->funcs->score_legacy)(model, wid, history, n_hist, n_used);

    /* Multiply by unigram in-class weight. */
    return score + class_weight;
}

int32
ngram_score_legacy(ngram_model_t_legacy *model, const char *word, ...)
{
    va_list history;
    const char *hword;
    int32 *histid;
    int32 n_hist;
    int32 n_used;
    int32 prob;

    va_start(history, word);
    n_hist = 0;
    while ((hword = va_arg(history, const char *)) != NULL)
        ++n_hist;
    va_end(history);

    histid = ckd_calloc(n_hist, sizeof(*histid));
    va_start(history, word);
    n_hist = 0;
    while ((hword = va_arg(history, const char *)) != NULL) {
        histid[n_hist] = ngram_wid_legacy(model, hword);
        ++n_hist;
    }
    va_end(history);

    prob = ngram_ng_score_legacy(model, ngram_wid_legacy(model, word),
                          histid, n_hist, &n_used);
    ckd_free(histid);
    return prob;
}

int32
ngram_tg_score_legacy(ngram_model_t_legacy *model, int32 w3, int32 w2, int32 w1, int32 *n_used)
{
    int32 hist[2];
    hist[0] = w2;
    hist[1] = w1;
    return ngram_ng_score_legacy(model, w3, hist, 2, n_used);
}

int32
ngram_bg_score_legacy(ngram_model_t_legacy *model, int32 w2, int32 w1, int32 *n_used)
{
    return ngram_ng_score_legacy(model, w2, &w1, 1, n_used);
}

int32
ngram_ng_prob_legacy(ngram_model_t_legacy *model, int32 wid, int32 *history,
              int32 n_hist, int32 *n_used)
{
    int32 prob, class_weight = 0;
    int i;

    /* Closed vocabulary, OOV word probability is zero */
    if (wid == NGRAM_INVALID_WID_legacy)
        return model->log_zero;

    /* "Declassify" wid and history */
    if (NGRAM_IS_CLASSWID_legacy(wid)) {
        ngram_class_t_legacy *lmclass = model->classes[NGRAM_CLASSID_legacy(wid)];

        class_weight = ngram_class_prob_legacy(lmclass, wid);
        if (class_weight == 1) /* Meaning, not found in class. */
            return class_weight;
        wid = lmclass->tag_wid;
    }
    for (i = 0; i < n_hist; ++i) {
        if (history[i] != NGRAM_INVALID_WID_legacy && NGRAM_IS_CLASSWID_legacy(history[i]))
            history[i] = model->classes[NGRAM_CLASSID_legacy(history[i])]->tag_wid;
    }
    prob = (*model->funcs->raw_score_legacy)(model, wid, history,
                                      n_hist, n_used);
    /* Multiply by unigram in-class weight. */
    return prob + class_weight;
}

int32
ngram_probv_legacy(ngram_model_t_legacy *model, const char *word, ...)
{
    va_list history;
    const char *hword;
    int32 *histid;
    int32 n_hist;
    int32 n_used;
    int32 prob;

    va_start(history, word);
    n_hist = 0;
    while ((hword = va_arg(history, const char *)) != NULL)
        ++n_hist;
    va_end(history);

    histid = ckd_calloc(n_hist, sizeof(*histid));
    va_start(history, word);
    n_hist = 0;
    while ((hword = va_arg(history, const char *)) != NULL) {
        histid[n_hist] = ngram_wid_legacy(model, hword);
        ++n_hist;
    }
    va_end(history);

    prob = ngram_ng_prob_legacy(model, ngram_wid_legacy(model, word),
                         histid, n_hist, &n_used);
    ckd_free(histid);
    return prob;
}

int32
ngram_prob_legacy(ngram_model_t_legacy *model, const char *const *words, int32 n)
{
    int32 *ctx_id;
    int32 nused;
    int32 prob;
    int32 wid;
    uint32 i;

    ctx_id = (int32 *)ckd_calloc(n - 1, sizeof(*ctx_id));
    for (i = 1; i < n; ++i)
      ctx_id[i - 1] = ngram_wid_legacy(model, words[i]);

    wid = ngram_wid_legacy(model, *words);
    prob = ngram_ng_prob_legacy(model, wid, ctx_id, n - 1, &nused);
    ckd_free(ctx_id);
    
    return prob;
}

int32
ngram_score_to_prob_legacy(ngram_model_t_legacy *base, int32 score)
{
    int32 prob;

    /* Undo insertion penalty. */
    prob = score - base->log_wip;
    /* Undo language weight. */
    prob = (int32)(prob / base->lw);

    return prob;
}

int32
ngram_unknown_wid_legacy(ngram_model_t_legacy *model)
{
    int32 val;

    /* FIXME: This could be memoized for speed if necessary. */
    /* Look up <UNK>, if not found return NGRAM_INVALID_WID_legacy. */
    if (hash_table_lookup_int32(model->wid, "<UNK>", &val) == -1)
        return NGRAM_INVALID_WID_legacy;
    else
        return val;
}

int32
ngram_zero_legacy(ngram_model_t_legacy *model)
{
    return model->log_zero;
}

int32
ngram_model_get_size_legacy(ngram_model_t_legacy *model)
{
  if (model != NULL)
    return model->n;
  return 0;
}

int32 const *
ngram_model_get_counts_legacy(ngram_model_t_legacy *model)
{
  if (model != NULL)
    return model->n_counts;
  return NULL;
}

void
ngram_iter_init_legacy(ngram_iter_t_legacy *itor, ngram_model_t_legacy *model,
                int m, int successor)
{
    itor->model = model;
    itor->wids = ckd_calloc(model->n, sizeof(*itor->wids));
    itor->m = m;
    itor->successor = successor;
}

ngram_iter_t_legacy *
ngram_model_mgrams_legacy(ngram_model_t_legacy *model, int m)
{
    ngram_iter_t_legacy *itor;
    /* The fact that m=n-1 is not exactly obvious.  Prevent accidents. */
    if (m >= model->n)
        return NULL;
    if (model->funcs->mgrams_legacy == NULL)
        return NULL;
    itor = (*model->funcs->mgrams_legacy)(model, m);
    return itor;
}

ngram_iter_t_legacy *
ngram_iter_legacy(ngram_model_t_legacy *model, const char *word, ...)
{
    va_list history;
    const char *hword;
    int32 *histid;
    int32 n_hist;
    ngram_iter_t_legacy *itor;

    va_start(history, word);
    n_hist = 0;
    while ((hword = va_arg(history, const char *)) != NULL)
        ++n_hist;
    va_end(history);

    histid = ckd_calloc(n_hist, sizeof(*histid));
    va_start(history, word);
    n_hist = 0;
    while ((hword = va_arg(history, const char *)) != NULL) {
        histid[n_hist] = ngram_wid_legacy(model, hword);
        ++n_hist;
    }
    va_end(history);

    itor = ngram_ng_iter_legacy(model, ngram_wid_legacy(model, word), histid, n_hist);
    ckd_free(histid);
    return itor;
}

ngram_iter_t_legacy *
ngram_ng_iter_legacy(ngram_model_t_legacy *model, int32 wid, int32 *history, int32 n_hist)
{
    if (n_hist >= model->n)
        return NULL;
    if (model->funcs->iter_legacy == NULL)
        return NULL;
    return (*model->funcs->iter_legacy)(model, wid, history, n_hist);
}

ngram_iter_t_legacy *
ngram_iter_successors_legacy(ngram_iter_t_legacy *itor)
{
    /* Stop when we are at the highest order N-Gram. */
    if (itor->m == itor->model->n - 1)
        return NULL;
    return (*itor->model->funcs->successors_legacy)(itor);
}

int32 const *
ngram_iter_get_legacy(ngram_iter_t_legacy *itor,
               int32 *out_score,
               int32 *out_bowt)
{
    return (*itor->model->funcs->iter_get_legacy)(itor, out_score, out_bowt);
}

ngram_iter_t_legacy *
ngram_iter_next_legacy(ngram_iter_t_legacy *itor)
{
    return (*itor->model->funcs->iter_next_legacy)(itor);
}

void
ngram_iter_free_legacy(ngram_iter_t_legacy *itor)
{
    ckd_free(itor->wids);
    (*itor->model->funcs->iter_free_legacy)(itor);
}

int32
ngram_wid_legacy(ngram_model_t_legacy *model, const char *word)
{
    int32 val;

    if (hash_table_lookup_int32(model->wid, word, &val) == -1)
        return ngram_unknown_wid_legacy(model);
    else
        return val;
}

const char *
ngram_word_legacy(ngram_model_t_legacy *model, int32 wid)
{
    /* Remove any class tag */
    wid = NGRAM_BASEWID_legacy(wid);
    if (wid >= model->n_words)
        return NULL;
    return model->word_str[wid];
}

/**
 * Add a word to the word string and ID mapping.
 */
int32
ngram_add_word_internal_legacy(ngram_model_t_legacy *model,
                        const char *word,
                        int32 classid)
{

    /* Check for hash collisions. */
    int32 wid;
    if (hash_table_lookup_int32(model->wid, word, &wid) == 0) {
        E_WARN("Omit duplicate word '%s'\n", word);
        return wid;
    }

    /* Take the next available word ID */
    wid = model->n_words;
    if (classid >= 0) {
        wid = NGRAM_CLASSWID_legacy(wid, classid);
    }

    /* Reallocate word_str if necessary. */
    if (model->n_words >= model->n_1g_alloc) {
        model->n_1g_alloc += UG_ALLOC_STEP_legacy;
        model->word_str = ckd_realloc(model->word_str,
                                      sizeof(*model->word_str) * model->n_1g_alloc);
    }
    /* Add the word string in the appropriate manner. */
    /* Class words are always dynamically allocated. */
    model->word_str[model->n_words] = ckd_salloc(word);
    /* Now enter it into the hash table. */
    if (hash_table_enter_int32(model->wid, model->word_str[model->n_words], wid) != wid) {
        E_ERROR("Hash insertion failed for word %s => %p (should not happen)\n",
                model->word_str[model->n_words], (void *)(long)(wid));
    }
    /* Increment number of words. */
    ++model->n_words;
    return wid;
}

int32
ngram_model_add_word_legacy(ngram_model_t_legacy *model,
                     const char *word, float32 weight)
{
    int32 wid, prob = model->log_zero;

    /* If we add word to unwritable model, we need to make it writable */
    if (!model->writable) {
      E_WARN("Can't add word '%s' to read-only language model. "
             "Disable mmap with '-mmap no' to make it writable\n", word);
      return -1;
    }

    wid = ngram_add_word_internal_legacy(model, word, -1);
    if (wid == NGRAM_INVALID_WID_legacy)
        return wid;

    /* Do what needs to be done to add the word to the unigram. */
    if (model->funcs && model->funcs->add_ug_legacy)
      prob = (*model->funcs->add_ug_legacy)(model, wid, logmath_log(model->lmath, weight));
    if (prob == 0)
      return -1;

    return wid;
}

ngram_class_t_legacy *
ngram_class_new_legacy(ngram_model_t_legacy *model, int32 tag_wid, int32 start_wid, glist_t classwords)
{
    ngram_class_t_legacy *lmclass;
    gnode_t *gn;
    float32 tprob;
    int i;

    lmclass = ckd_calloc(1, sizeof(*lmclass));
    lmclass->tag_wid = tag_wid;
    /* wid_base is the wid (minus class tag) of the first word in the list. */
    lmclass->start_wid = start_wid;
    lmclass->n_words = glist_count(classwords);
    lmclass->prob1 = ckd_calloc(lmclass->n_words, sizeof(*lmclass->prob1));
    lmclass->nword_hash = NULL;
    lmclass->n_hash = 0;
    tprob = 0.0;
    for (gn = classwords; gn; gn = gnode_next(gn)) {
        tprob += gnode_float32(gn);
    }
    if (tprob > 1.1 || tprob < 0.9) {
        E_INFO("Total class probability is %f, will normalize\n", tprob);
        for (gn = classwords; gn; gn = gnode_next(gn)) {
            gn->data.fl /= tprob;
        }
    }
    for (i = 0, gn = classwords; gn; ++i, gn = gnode_next(gn)) {
        lmclass->prob1[i] = logmath_log(model->lmath, gnode_float32(gn));
    }

    return lmclass;
}

int32
ngram_class_add_word_legacy(ngram_class_t_legacy *lmclass, int32 wid, int32 lweight)
{
    int32 hash;

    if (lmclass->nword_hash == NULL) {
        /* Initialize everything in it to -1 */
        lmclass->nword_hash = ckd_malloc(NGRAM_HASH_SIZE_legacy * sizeof(*lmclass->nword_hash));
        memset(lmclass->nword_hash, 0xff, NGRAM_HASH_SIZE_legacy * sizeof(*lmclass->nword_hash));
        lmclass->n_hash = NGRAM_HASH_SIZE_legacy;
        lmclass->n_hash_inuse = 0;
    }
    /* Stupidest possible hash function.  This will work pretty well
     * when this function is called repeatedly with contiguous word
     * IDs, though... */
    hash = wid & (lmclass->n_hash - 1);
    if (lmclass->nword_hash[hash].wid == -1) {
        /* Good, no collision. */
        lmclass->nword_hash[hash].wid = wid;
        lmclass->nword_hash[hash].prob1 = lweight;
        ++lmclass->n_hash_inuse;
        return hash;
    }
    else {
        int32 next; /**< Next available bucket. */
        /* Collision... Find the end of the hash chain. */
        while (lmclass->nword_hash[hash].next != -1)
            hash = lmclass->nword_hash[hash].next;
        assert(hash != -1);
        /* Does we has any more bukkit? */
        if (lmclass->n_hash_inuse == lmclass->n_hash) {
            /* Oh noes!  Ok, we makes more. */
            lmclass->nword_hash = ckd_realloc(lmclass->nword_hash, 
                                              lmclass->n_hash * 2 * sizeof(*lmclass->nword_hash));
            memset(lmclass->nword_hash + lmclass->n_hash,
                   0xff, lmclass->n_hash * sizeof(*lmclass->nword_hash));
            /* Just use the next allocated one (easy) */
            next = lmclass->n_hash;
            lmclass->n_hash *= 2;
        }
        else {
            /* Look for any available bucket.  We hope this doesn't happen. */
            for (next = 0; next < lmclass->n_hash; ++next)
                if (lmclass->nword_hash[next].wid == -1)
                    break;
            /* This should absolutely not happen. */
            assert(next != lmclass->n_hash);
        }
        lmclass->nword_hash[next].wid = wid;
        lmclass->nword_hash[next].prob1 = lweight;
        lmclass->nword_hash[hash].next = next;
        ++lmclass->n_hash_inuse;
        return next;
    }
}

void
ngram_class_free_legacy(ngram_class_t_legacy *lmclass)
{
    ckd_free(lmclass->nword_hash);
    ckd_free(lmclass->prob1);
    ckd_free(lmclass);
}

int32
ngram_model_add_class_word_legacy(ngram_model_t_legacy *model,
                           const char *classname,
                           const char *word,
                           float32 weight)
{
    ngram_class_t_legacy *lmclass;
    int32 classid, tag_wid, wid, i, scale;
    float32 fprob;

    /* Find the class corresponding to classname.  Linear search
     * probably okay here since there won't be very many classes, and
     * this doesn't have to be fast. */
    tag_wid = ngram_wid_legacy(model, classname);
    if (tag_wid == NGRAM_INVALID_WID_legacy) {
        E_ERROR("No such word or class tag: %s\n", classname);
        return tag_wid;
    }
    for (classid = 0; classid < model->n_classes; ++classid) {
        if (model->classes[classid]->tag_wid == tag_wid)
            break;
    }
    /* Hmm, no such class.  It's probably not a good idea to create one. */
    if (classid == model->n_classes) {
        E_ERROR("Word %s is not a class tag (call ngram_model_add_class_legacy() first)\n", classname);
        return NGRAM_INVALID_WID_legacy;
    }
    lmclass = model->classes[classid];

    /* Add this word to the model's set of words. */
    wid = ngram_add_word_internal_legacy(model, word, classid);
    if (wid == NGRAM_INVALID_WID_legacy)
        return wid;

    /* This is the fixed probability of the new word. */
    fprob = weight * 1.0f / (lmclass->n_words + lmclass->n_hash_inuse + 1);
    /* Now normalize everything else to fit it in.  This is
     * accomplished by simply scaling all the other probabilities
     * by (1-fprob). */
    scale = logmath_log(model->lmath, 1.0 - fprob);
    for (i = 0; i < lmclass->n_words; ++i)
        lmclass->prob1[i] += scale;
    for (i = 0; i < lmclass->n_hash; ++i)
        if (lmclass->nword_hash[i].wid != -1)
            lmclass->nword_hash[i].prob1 += scale;

    /* Now add it to the class hash table. */
    return ngram_class_add_word_legacy(lmclass, wid, logmath_log(model->lmath, fprob));
}

int32
ngram_model_add_class_legacy(ngram_model_t_legacy *model,
                      const char *classname,
                      float32 classweight,
                      char **words,
                      const float32 *weights,
                      int32 n_words)
{
    ngram_class_t_legacy *lmclass;
    glist_t classwords = NULL;
    int32 i, start_wid = -1;
    int32 classid, tag_wid;

    /* Check if classname already exists in model.  If not, add it.*/
    if ((tag_wid = ngram_wid_legacy(model, classname)) == ngram_unknown_wid_legacy(model)) {
        tag_wid = ngram_model_add_word_legacy(model, classname, classweight);
        if (tag_wid == NGRAM_INVALID_WID_legacy)
            return -1;
    }

    if (model->n_classes == 128) {
        E_ERROR("Number of classes cannot exceed 128 (sorry)\n");
        return -1;
    }
    classid = model->n_classes;
    for (i = 0; i < n_words; ++i) {
        int32 wid;

        wid = ngram_add_word_internal_legacy(model, words[i], classid);
        if (wid == NGRAM_INVALID_WID_legacy)
            return -1;
        if (start_wid == -1)
            start_wid = NGRAM_BASEWID_legacy(wid);
        classwords = glist_add_float32(classwords, weights[i]);
    }
    classwords = glist_reverse(classwords);
    lmclass = ngram_class_new_legacy(model, tag_wid, start_wid, classwords);
    glist_free(classwords);
    if (lmclass == NULL)
        return -1;

    ++model->n_classes;
    if (model->classes == NULL)
        model->classes = ckd_calloc(1, sizeof(*model->classes));
    else
        model->classes = ckd_realloc(model->classes,
                                     model->n_classes * sizeof(*model->classes));
    model->classes[classid] = lmclass;
    return classid;
}

int32
ngram_class_prob_legacy(ngram_class_t_legacy *lmclass, int32 wid)
{
    int32 base_wid = NGRAM_BASEWID_legacy(wid);

    if (base_wid < lmclass->start_wid
        || base_wid > lmclass->start_wid + lmclass->n_words) {
        int32 hash;

        /* Look it up in the hash table. */
        hash = wid & (lmclass->n_hash - 1);
        while (hash != -1 && lmclass->nword_hash[hash].wid != wid)
            hash = lmclass->nword_hash[hash].next;
        if (hash == -1)
            return 1;
        return lmclass->nword_hash[hash].prob1;
    }
    else {
        return lmclass->prob1[base_wid - lmclass->start_wid];
    }
}

int32
read_classdef_file_legacy(hash_table_t *classes, const char *file_name)
{
    FILE *fp;
    int32 is_pipe;
    int inclass;  /**< Are we currently reading a list of class words? */
    int32 rv = -1;
    gnode_t *gn;
    glist_t classwords = NULL;
    glist_t classprobs = NULL;
    char *classname = NULL;

    if ((fp = fopen_comp(file_name, "r", &is_pipe)) == NULL) {
        E_ERROR("File %s not found\n", file_name);
        return -1;
    }

    inclass = FALSE;
    while (!feof(fp)) {
        char line[512];
        char *wptr[2];
        int n_words;

        if (fgets(line, sizeof(line), fp) == NULL)
            break;

        n_words = str2words(line, wptr, 2);
        if (n_words <= 0)
            continue;

        if (inclass) {
            /* Look for an end of class marker. */
            if (n_words == 2 && 0 == strcmp(wptr[0], "END")) {
                classdef_t_legacy *classdef;
                gnode_t *word, *weight;
                int32 i;

                if (classname == NULL || 0 != strcmp(wptr[1], classname))
                    goto error_out;
                inclass = FALSE;

                /* Construct a class from the list of words collected. */
                classdef = ckd_calloc(1, sizeof(*classdef));
                classwords = glist_reverse(classwords);
                classprobs = glist_reverse(classprobs);
                classdef->n_words = glist_count(classwords);
                classdef->words = ckd_calloc(classdef->n_words,
                                             sizeof(*classdef->words));
                classdef->weights = ckd_calloc(classdef->n_words,
                                               sizeof(*classdef->weights));
                word = classwords;
                weight = classprobs;
                for (i = 0; i < classdef->n_words; ++i) {
                    classdef->words[i] = gnode_ptr(word);
                    classdef->weights[i] = gnode_float32(weight);
                    word = gnode_next(word);
                    weight = gnode_next(weight);
                }
                
                /* Add this class to the hash table. */
                if (hash_table_enter(classes, classname, classdef) != classdef) {
                    classdef_free_legacy(classdef);
                    goto error_out;
                }

                /* Reset everything. */
                glist_free(classwords);
                glist_free(classprobs);
                classwords = NULL;
                classprobs = NULL;
                classname = NULL;
            }
            else {
                float32 fprob;

                if (n_words == 2)
                    fprob = (float32)atof_c(wptr[1]);
                else
                    fprob = 1.0f;
                /* Add it to the list of words for this class. */
                classwords = glist_add_ptr(classwords, ckd_salloc(wptr[0]));
                classprobs = glist_add_float32(classprobs, fprob);
            }
        }
        else {
            /* Start a new LM class if the LMCLASS marker is seen */
            if (n_words == 2 && 0 == strcmp(wptr[0], "LMCLASS")) {
                if (inclass)
                    goto error_out;
                inclass = TRUE;
                classname = ckd_salloc(wptr[1]);
            }
            /* Otherwise, just ignore whatever junk we got */
        }
    }
    rv = 0; /* Success. */

error_out:
    /* Free all the stuff we might have allocated. */
    fclose_comp(fp, is_pipe);
    for (gn = classwords; gn; gn = gnode_next(gn))
        ckd_free(gnode_ptr(gn));
    glist_free(classwords);
    glist_free(classprobs);
    ckd_free(classname);

    return rv;
}

void
classdef_free_legacy(classdef_t_legacy *classdef)
{
    int32 i;
    for (i = 0; i < classdef->n_words; ++i)
        ckd_free(classdef->words[i]);
    ckd_free(classdef->words);
    ckd_free(classdef->weights);
    ckd_free(classdef);
}


int32
ngram_model_read_classdef_legacy(ngram_model_t_legacy *model,
                          const char *file_name)
{
    hash_table_t *classes;
    glist_t hl = NULL;
    gnode_t *gn;
    int32 rv = -1;

    classes = hash_table_new(0, FALSE);
    if (read_classdef_file_legacy(classes, file_name) < 0) {
        hash_table_free(classes);
        return -1;
    }
    
    /* Create a new class in the language model for each classdef. */
    hl = hash_table_tolist(classes, NULL);
    for (gn = hl; gn; gn = gnode_next(gn)) {
        hash_entry_t *he = gnode_ptr(gn);
        classdef_t_legacy *classdef = he->val;

        if (ngram_model_add_class_legacy(model, he->key, 1.0,
                                  classdef->words,
                                  classdef->weights,
                                  classdef->n_words) < 0)
            goto error_out;
    }
    rv = 0;

error_out:
    for (gn = hl; gn; gn = gnode_next(gn)) {
        hash_entry_t *he = gnode_ptr(gn);
        ckd_free((char *)he->key);
        classdef_free_legacy(he->val);
    }
    glist_free(hl);
    hash_table_free(classes);
    return rv;
}
