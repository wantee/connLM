/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Wang Jian
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

#include <string.h>

#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>
#include <stutils/st_mem.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>

bool g_binary;

st_opt_t *g_cmd_opt;

int connlm_merge_parse_opt(int *argc, const char *argv[])
{
    st_log_opt_t log_opt;
    bool b;

    g_cmd_opt = st_opt_create();
    if (g_cmd_opt == NULL) {
        ST_WARNING("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (st_opt_parse(g_cmd_opt, argc, argv) < 0) {
        ST_WARNING("Failed to st_opt_parse.");
        goto ST_OPT_ERR;
    }

    if (st_log_load_opt(&log_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to st_log_load_opt");
        goto ST_OPT_ERR;
    }

    if (st_log_open(&log_opt) != 0) {
        ST_WARNING("Failed to open log");
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_BOOL(g_cmd_opt, "BINARY", g_binary, true,
            "Save file as binary format");

    ST_OPT_GET_BOOL(g_cmd_opt, "help", b, false, "Print help");

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Merge Models",
            "<model-in-filter1> <model-in-filter2> ... <model-out>",
            "mdl,c{maxent}:exp/maxent/final.clm mdl,c{rnn}:exp/rnn/final.clm exp/maxent~rnn/init.clm",
            g_cmd_opt, model_filter_help());
}

int main(int argc, const char *argv[])
{
    char args[1024] = "";
    FILE *fp = NULL;
    connlm_t *connlm = NULL;
    connlm_t *connlm1 = NULL;
    model_filter_t *mfs = NULL;
    char *fnames = NULL;
    char **comp_names = NULL;
    int *num_comp = NULL;
    int ret;
    int i, c;

    if (st_mem_usage_init() < 0) {
        ST_WARNING("Failed to st_mem_usage_init.");
        goto ERR;
    }

    (void)st_escape_args(argc, argv, args, 1024);

    ret = connlm_merge_parse_opt(&argc, argv);
    if (ret < 0) {
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (strcmp(connlm_revision(), CONNLM_GIT_COMMIT) != 0) {
        ST_WARNING("Binary revision[%s] not match with library[%s].",
                CONNLM_GIT_COMMIT, connlm_revision());
    }

    if (argc < 4) {
        show_usage(argv[0]);
        goto ERR;
    }

    ST_CLEAN("Command-line: %s", args);
    st_opt_show(g_cmd_opt, "connLM Merge Options");
    for (i = 1; i < argc - 1; i++) {
        ST_CLEAN("Model-in-%d: %s", i, argv[i]);
    }
    ST_CLEAN("Model-out: %s", argv[argc - 1]);

    mfs = (model_filter_t *)st_malloc(sizeof(model_filter_t) * (argc - 2));
    if (mfs == NULL) {
        ST_WARNING("Failed to st_malloc mfs.");
        goto ERR;
    }

    fnames = (char *)st_malloc(sizeof(char) * MAX_DIR_LEN * (argc - 2));
    if (fnames == NULL) {
        ST_WARNING("Failed to st_malloc fnames.");
        goto ERR;
    }

    comp_names = (char **)st_malloc(sizeof(char*) * (argc - 2));
    if (comp_names == NULL) {
        ST_WARNING("Failed to st_malloc comp_names.");
        goto ERR;
    }
    num_comp = (int *)st_malloc(sizeof(int) * (argc - 2));
    if (num_comp == NULL) {
        ST_WARNING("Failed to st_malloc num_comp.");
        goto ERR;
    }
    for (i = 0; i < argc - 2; i++) {
        comp_names[i] = NULL;
        num_comp[i] = 0;
    }

    for (i = 0; i < argc - 2; i++) {
        mfs[i] = parse_model_filter(argv[i + 1],
                fnames + i * MAX_DIR_LEN, MAX_DIR_LEN,
                comp_names + i, num_comp + i);
        if (mfs[i] == MF_ERR) {
            ST_WARNING("Failed to parse_model_filter. [%s]", argv[i]);
            goto ERR;
        }
    }

    fp = st_fopen(fnames, "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", fnames);
        goto ERR;
    }

    connlm1 = connlm_load(fp);
    if (connlm1 == NULL) {
        ST_WARNING("Failed to connlm_load. [%s]", fnames);
        goto ERR;
    }
    safe_st_fclose(fp);

    if (connlm1->vocab == NULL || connlm1->output == NULL) {
        ST_WARNING("Can't merge: No vocab or output for model[%s].", fnames);
        goto ERR;
    }

    connlm = connlm_new(connlm1->vocab, connlm1->output, NULL, -1);
    if (connlm == NULL) {
        ST_WARNING("Failed to connlm_new.");
        goto ERR;
    }

    i = 0;
    ST_NOTICE("Merging Model %d(%s)...", i, argv[i + 1]);
    if (connlm_filter(connlm1, mfs[i], comp_names[i], num_comp[i]) < 0) {
        ST_WARNING("Failed to connlm_filter for model [%d/%s]", i, argv[i + 1]);
        goto ERR;
    }
    for (c = 0; c < connlm1->num_comp; c++) {
        if (connlm_add_comp(connlm, connlm1->comps[c]) < 0) {
            ST_WARNING("Failed to connlm_add_comp. [%s]",
                    connlm1->comps[c]->name);
            goto ERR;
        }
    }

    safe_connlm_destroy(connlm1);

    for (i = 1; i < argc - 2; i++) {
        fp = st_fopen(fnames + i * MAX_DIR_LEN, "rb");
        if (fp == NULL) {
            ST_WARNING("Failed to st_fopen. [%s]", fnames + i*MAX_DIR_LEN);
            goto ERR;
        }

        connlm1 = connlm_load(fp);
        if (connlm1 == NULL) {
            ST_WARNING("Failed to connlm_load. [%s]", fnames + i*MAX_DIR_LEN);
            goto ERR;
        }
        safe_st_fclose(fp);

        if (connlm1->vocab == NULL || connlm1->output == NULL) {
            ST_WARNING("Can't merge: No vocab or output for model[%s].",
                    fnames + i * MAX_DIR_LEN);
            goto ERR;
        }

        ST_NOTICE("Merging Model %d(%s)...", i, argv[i + 1]);
        if (!vocab_equal(connlm->vocab, connlm1->vocab)) {
            ST_WARNING("Can't merge: Vocab not equal. [%s].",
                    fnames + i * MAX_DIR_LEN);
            goto ERR;
        }

        if (!output_equal(connlm->output, connlm1->output)) {
            ST_WARNING("Can't merge: Output not equal. [%s].",
                    fnames + i * MAX_DIR_LEN);
            goto ERR;
        }

        if (connlm_filter(connlm1, mfs[i], comp_names[i], num_comp[i]) < 0) {
            ST_WARNING("Failed to connlm_filter for model [%d/%s]", i, argv[i + 1]);
            goto ERR;
        }
        for (c = 0; c < connlm1->num_comp; c++) {
            if (connlm_add_comp(connlm, connlm1->comps[c]) < 0) {
                ST_WARNING("Failed to connlm_add_comp. [%s]",
                        connlm1->comps[c]->name);
                goto ERR;
            }
        }

        safe_connlm_destroy(connlm1);
    }

    fp = st_fopen(argv[argc - 1], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[argc - 1]);
        goto ERR;
    }

    if (connlm_save(connlm, fp, g_binary) < 0) {
        ST_WARNING("Failed to connlm_save. [%s]", argv[argc - 1]);
        goto ERR;
    }

    safe_st_fclose(fp);
    safe_st_free(mfs);
    safe_st_free(fnames);
    if (comp_names != NULL) {
        for (i = 0; i < argc - 2; i++) {
            safe_st_free(comp_names[i]);
        }
    }
    safe_st_free(comp_names);
    safe_st_free(num_comp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_mem_usage_report();
    st_mem_usage_destroy();
    st_log_close(0);
    return 0;

ERR:
    safe_st_fclose(fp);
    safe_st_free(mfs);
    safe_st_free(fnames);
    if (comp_names != NULL) {
        for (i = 0; i < argc - 2; i++) {
            safe_st_free(comp_names[i]);
        }
    }
    safe_st_free(comp_names);
    safe_st_free(num_comp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);
    safe_connlm_destroy(connlm1);

    st_mem_usage_destroy();
    st_log_close(1);
    return -1;
}
