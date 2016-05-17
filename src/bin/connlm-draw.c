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

#include <stutils/st_log.h>
#include <stutils/st_io.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>

st_opt_t *g_cmd_opt;

int connlm_draw_parse_opt(int *argc, const char *argv[])
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

    ST_OPT_GET_BOOL(g_cmd_opt, "help", b, false, "Print help");

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Convert connLM File into Graphviz Format",
            "<model-filter> <gv.dot>",
            g_cmd_opt, model_filter_help());
}

int main(int argc, const char *argv[])
{
    char fname[MAX_DIR_LEN];
    FILE *fp = NULL;
    connlm_t *connlm = NULL;

    char *comp_names = NULL;
    int num_comp;
    model_filter_t mf;
    int ret;

    ret = connlm_draw_parse_opt(&argc, argv);
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

    if (argc < 3) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Draw Options");
    ST_CLEAN("Model-in: %s, Graphviz-out: %s", argv[1], argv[2]);

    mf = parse_model_filter(argv[1], fname, MAX_DIR_LEN,
            &comp_names, &num_comp);
    if (mf == MF_NONE) {
        ST_WARNING("Failed to parse_model_filter.");
        goto ERR;
    }

    fp = st_fopen(fname, "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", fname);
        goto ERR;
    }

    connlm = connlm_load(fp);
    if (connlm == NULL) {
        ST_WARNING("Failed to connlm_load. [%s]", argv[1]);
        goto ERR;
    }
    safe_st_fclose(fp);

    if (connlm_filter(connlm, mf, comp_names, num_comp) < 0) {
        ST_WARNING("Failed to connlm_filter.");
        goto ERR;
    }

    fp = st_fopen(argv[2], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[2]);
        goto ERR;
    }

    if (connlm_draw(connlm, fp) < 0) {
        ST_WARNING("Failed to connlm_draw. [%s]", argv[2]);
        goto ERR;
    }

    safe_free(comp_names);
    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_log_close(0);
    return 0;

ERR:
    safe_free(comp_names);
    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_log_close(1);
    return -1;
}
