/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Wang Jian
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef  _VOCAB_H_
#define  _VOCAB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>
#include <st_alphabet.h>

#include "config.h"

/** @defgroup vocab Vocab
 * Data structures and functions for Vocab.
 */

/**
 * Parameters for Vocab.
 * @ingroup vocab
 */
typedef struct _vocab_opt_t_ {
    int max_vocab_size; /**< max number of words in vocab. */
} vocab_opt_t;

/**
 * Vocab.
 * @ingroup vocab
 */
typedef struct _vocab_t_ {
    vocab_opt_t vocab_opt; /**< vocab options. */

    int vocab_size;   /**< number of words in vocab. */

    st_alphabet_t *alphabet; /**< alphabet of words in vocab. */
    count_t *cnts;    /**< counts of each words in vocab. */
} vocab_t;

/**
 * Load vocab option.
 * @ingroup vocab
 * @param[out] vocab_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int vocab_load_opt(vocab_opt_t *vocab_opt, st_opt_t *opt,
        const char *sec_name);

/**
 * Create a vocab with options.
 * @ingroup vocab
 * @param[in] vocab_opt options.
 * @return a new vocab.
 */
vocab_t *vocab_create(vocab_opt_t *vocab_opt);
/**
 * Destroy a vocab and set the pointer to NULL.
 * @ingroup vocab
 * @param[in] ptr pointer to vocab_t.
 */
#define safe_vocab_destroy(ptr) do {\
    if((ptr) != NULL) {\
        vocab_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a vocab.
 * @ingroup vocab
 * @param[in] vocab vocab to be destroyed.
 */
void vocab_destroy(vocab_t *vocab);
/**
 * Duplicate a vocab.
 * @ingroup vocab
 * @param[in] v vocab to be duplicated.
 * @return the duplicated vocab
 */
vocab_t* vocab_dup(vocab_t *v);

/**
 * Load vocab header and initialise a new vocab.
 * @ingroup vocab
 * @param[out] vocab initialised.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see vocab_load_body
 * @see vocab_save_header, vocab_save_body
 * @return non-zero value if any error.
 */
int vocab_load_header(vocab_t **vocab, FILE *fp, bool *binary, FILE *fo);
/**
 * Load vocab body.
 * @ingroup vocab
 * @param[in] vocab vocab to be loaded.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see vocab_load_header
 * @see vocab_save_header, vocab_save_body
 * @return non-zero value if any error.
 */
int vocab_load_body(vocab_t *vocab, FILE *fp, bool binary);
/**
 * Save vocab header.
 * @ingroup vocab
 * @param[in] vocab vocab to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see vocab_save_body
 * @see vocab_load_header, vocab_load_body
 * @return non-zero value if any error.
 */
int vocab_save_header(vocab_t *vocab, FILE *fp, bool binary);
/**
 * Save vocab body.
 * @ingroup vocab
 * @param[in] vocab vocab to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see vocab_save_header
 * @see vocab_load_header, vocab_load_body
 * @return non-zero value if any error.
 */
int vocab_save_body(vocab_t *vocab, FILE *fp, bool binary);

/**
 * Learn vocab form corpus.
 * @ingroup vocab
 * @param[in] vocab vocab to be learned.
 * @param[in] fp file stream for corpus.
 * @param[in] max_word_num if larger than 0, just learn form first max_word_num words
 * @return non-zero value if any error.
 */
int vocab_learn(vocab_t *vocab, FILE *fp, count_t max_word_num);

/**
 * Get id of a word.
 * @ingroup vocab
 * @param[in] vocab vocab used.
 * @param[in] word string of word.
 * @return id of the word, negative value indicates error.
 */
int vocab_get_id(vocab_t *vocab, const char *word);
/**
 * Get string of a word id.
 * @ingroup vocab
 * @param[in] vocab vocab used.
 * @param[in] id id of the word.
 * @return string of the word, NULL indicates error.
 */
char* vocab_get_word(vocab_t *vocab, int id);
/**
 * Add a word to vocab
 * @ingroup vocab
 * @param[in] vocab vocab used.
 * @param[in] word string of the word.
 * @return non-zero value if any error.
 */
int vocab_add_word(vocab_t *vocab, const char* word);

#ifdef __cplusplus
}
#endif

#endif
