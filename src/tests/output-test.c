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

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <stutils/st_utils.h>
#include <stutils/st_log.h>

#include "output.h"

#include "vocab-test.h"
#include "output-test.h"

#define MAX_LEN 1024

static char *refs[MAX_LEN] = {
"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 30\\n# Leaf: 17\\nmethod: TopDown\\nmax depth: 0\\nmax branch: 3\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    0 [label=\"{{0|\\</s\\>}|{17|18,21}}\"];\n"
"    1 [label=\"{{1|\\<unk\\>}|{16|18,21}}\"];\n"
"    2 [label=\"{{2|CCC}|{15|18,22}}\"];\n"
"    3 [label=\"{{3|DDD}|{14|18,22}}\"];\n"
"    4 [label=\"{{4|EEE}|{13|18,23}}\"];\n"
"    5 [label=\"{{5|FFF}|{12|19,24}}\"];\n"
"    6 [label=\"{{6|GGG}|{11|19,24}}\"];\n"
"    7 [label=\"{{7|HHH}|{10|19,25}}\"];\n"
"    8 [label=\"{{8|III}|{9|19,25}}\"];\n"
"    9 [label=\"{{9|JJJ}|{8|19,26}}\"];\n"
"    10 [label=\"{{10|KKK}|{7|20,27}}\"];\n"
"    11 [label=\"{{11|LLL}|{6|20,27}}\"];\n"
"    12 [label=\"{{12|MMM}|{5|20,28}}\"];\n"
"    13 [label=\"{{13|NNN}|{4|20,28}}\"];\n"
"    14 [label=\"{{14|OOO}|{3|20,29}}\"];\n"
"    15 [label=\"{{15|PPP}|{2|20,29}}\"];\n"
"    16 [label=\"{{16|QQQ}|{1|20,29}}\"];\n"
"\n"
"    21 -> 0;\n"
"    21 -> 1;\n"
"    18 -> 21;\n"
"    22 -> 2;\n"
"    22 -> 3;\n"
"    18 -> 22;\n"
"    23 -> 4;\n"
"    18 -> 23;\n"
"    17 -> 18;\n"
"    24 -> 5;\n"
"    24 -> 6;\n"
"    19 -> 24;\n"
"    25 -> 7;\n"
"    25 -> 8;\n"
"    19 -> 25;\n"
"    26 -> 9;\n"
"    19 -> 26;\n"
"    17 -> 19;\n"
"    27 -> 10;\n"
"    27 -> 11;\n"
"    20 -> 27;\n"
"    28 -> 12;\n"
"    28 -> 13;\n"
"    20 -> 28;\n"
"    29 -> 14;\n"
"    29 -> 15;\n"
"    29 -> 16;\n"
"    20 -> 29;\n"
"    17 -> 20;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 30\\n# Leaf: 17\\nmethod: TopDown\\nmax depth: 0\\nmax branch: 3\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    0 [label=\"{{0|\\</s\\>}|{|18,21}}\"];\n"
"    1 [label=\"{{1|\\<unk\\>}|{|18,21}}\"];\n"
"    2 [label=\"{{2|CCC}|{|18,22}}\"];\n"
"    3 [label=\"{{3|DDD}|{|18,22}}\"];\n"
"    4 [label=\"{{4|EEE}|{|18,23}}\"];\n"
"    5 [label=\"{{5|FFF}|{|19,24}}\"];\n"
"    6 [label=\"{{6|GGG}|{|19,24}}\"];\n"
"    7 [label=\"{{7|HHH}|{|19,25}}\"];\n"
"    8 [label=\"{{8|III}|{|19,25}}\"];\n"
"    9 [label=\"{{9|JJJ}|{|19,26}}\"];\n"
"    10 [label=\"{{10|KKK}|{|20,27}}\"];\n"
"    11 [label=\"{{11|LLL}|{|20,27}}\"];\n"
"    12 [label=\"{{12|MMM}|{|20,28}}\"];\n"
"    13 [label=\"{{13|NNN}|{|20,28}}\"];\n"
"    14 [label=\"{{14|OOO}|{|20,29}}\"];\n"
"    15 [label=\"{{15|PPP}|{|20,29}}\"];\n"
"    16 [label=\"{{16|QQQ}|{|20,29}}\"];\n"
"\n"
"    21 -> 0;\n"
"    21 -> 1;\n"
"    18 -> 21;\n"
"    22 -> 2;\n"
"    22 -> 3;\n"
"    18 -> 22;\n"
"    23 -> 4;\n"
"    18 -> 23;\n"
"    17 -> 18;\n"
"    24 -> 5;\n"
"    24 -> 6;\n"
"    19 -> 24;\n"
"    25 -> 7;\n"
"    25 -> 8;\n"
"    19 -> 25;\n"
"    26 -> 9;\n"
"    19 -> 26;\n"
"    17 -> 19;\n"
"    27 -> 10;\n"
"    27 -> 11;\n"
"    20 -> 27;\n"
"    28 -> 12;\n"
"    28 -> 13;\n"
"    20 -> 28;\n"
"    29 -> 14;\n"
"    29 -> 15;\n"
"    29 -> 16;\n"
"    20 -> 29;\n"
"    17 -> 20;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 30\\n# Leaf: 17\\nmethod: TopDown\\nmax depth: 0\\nmax branch: 3\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    0 [label=\"{{0|}|{|18,21}}\"];\n"
"    1 [label=\"{{1|}|{|18,21}}\"];\n"
"    2 [label=\"{{2|}|{|18,22}}\"];\n"
"    3 [label=\"{{3|}|{|18,22}}\"];\n"
"    4 [label=\"{{4|}|{|18,23}}\"];\n"
"    5 [label=\"{{5|}|{|19,24}}\"];\n"
"    6 [label=\"{{6|}|{|19,24}}\"];\n"
"    7 [label=\"{{7|}|{|19,25}}\"];\n"
"    8 [label=\"{{8|}|{|19,25}}\"];\n"
"    9 [label=\"{{9|}|{|19,26}}\"];\n"
"    10 [label=\"{{10|}|{|20,27}}\"];\n"
"    11 [label=\"{{11|}|{|20,27}}\"];\n"
"    12 [label=\"{{12|}|{|20,28}}\"];\n"
"    13 [label=\"{{13|}|{|20,28}}\"];\n"
"    14 [label=\"{{14|}|{|20,29}}\"];\n"
"    15 [label=\"{{15|}|{|20,29}}\"];\n"
"    16 [label=\"{{16|}|{|20,29}}\"];\n"
"\n"
"    21 -> 0;\n"
"    21 -> 1;\n"
"    18 -> 21;\n"
"    22 -> 2;\n"
"    22 -> 3;\n"
"    18 -> 22;\n"
"    23 -> 4;\n"
"    18 -> 23;\n"
"    17 -> 18;\n"
"    24 -> 5;\n"
"    24 -> 6;\n"
"    19 -> 24;\n"
"    25 -> 7;\n"
"    25 -> 8;\n"
"    19 -> 25;\n"
"    26 -> 9;\n"
"    19 -> 26;\n"
"    17 -> 19;\n"
"    27 -> 10;\n"
"    27 -> 11;\n"
"    20 -> 27;\n"
"    28 -> 12;\n"
"    28 -> 13;\n"
"    20 -> 28;\n"
"    29 -> 14;\n"
"    29 -> 15;\n"
"    29 -> 16;\n"
"    20 -> 29;\n"
"    17 -> 20;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 30\\n# Leaf: 17\\nmethod: TopDown\\nmax depth: 0\\nmax branch: 3\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    0 [label=\"{{0|}|{17|18,21}}\"];\n"
"    1 [label=\"{{1|}|{16|18,21}}\"];\n"
"    2 [label=\"{{2|}|{15|18,22}}\"];\n"
"    3 [label=\"{{3|}|{14|18,22}}\"];\n"
"    4 [label=\"{{4|}|{13|18,23}}\"];\n"
"    5 [label=\"{{5|}|{12|19,24}}\"];\n"
"    6 [label=\"{{6|}|{11|19,24}}\"];\n"
"    7 [label=\"{{7|}|{10|19,25}}\"];\n"
"    8 [label=\"{{8|}|{9|19,25}}\"];\n"
"    9 [label=\"{{9|}|{8|19,26}}\"];\n"
"    10 [label=\"{{10|}|{7|20,27}}\"];\n"
"    11 [label=\"{{11|}|{6|20,27}}\"];\n"
"    12 [label=\"{{12|}|{5|20,28}}\"];\n"
"    13 [label=\"{{13|}|{4|20,28}}\"];\n"
"    14 [label=\"{{14|}|{3|20,29}}\"];\n"
"    15 [label=\"{{15|}|{2|20,29}}\"];\n"
"    16 [label=\"{{16|}|{1|20,29}}\"];\n"
"\n"
"    21 -> 0;\n"
"    21 -> 1;\n"
"    18 -> 21;\n"
"    22 -> 2;\n"
"    22 -> 3;\n"
"    18 -> 22;\n"
"    23 -> 4;\n"
"    18 -> 23;\n"
"    17 -> 18;\n"
"    24 -> 5;\n"
"    24 -> 6;\n"
"    19 -> 24;\n"
"    25 -> 7;\n"
"    25 -> 8;\n"
"    19 -> 25;\n"
"    26 -> 9;\n"
"    19 -> 26;\n"
"    17 -> 19;\n"
"    27 -> 10;\n"
"    27 -> 11;\n"
"    20 -> 27;\n"
"    28 -> 12;\n"
"    28 -> 13;\n"
"    20 -> 28;\n"
"    29 -> 14;\n"
"    29 -> 15;\n"
"    29 -> 16;\n"
"    20 -> 29;\n"
"    17 -> 20;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 25\\n# Leaf: 17\\nmethod: BottomUp\\nmax depth: 0\\nmax branch: 3\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    10 [label=\"{{10|\\</s\\>}|{17|3}}\"];\n"
"    7 [label=\"{{7|\\<unk\\>}|{16|2}}\"];\n"
"    8 [label=\"{{8|CCC}|{15|2}}\"];\n"
"    4 [label=\"{{4|DDD}|{14|1}}\"];\n"
"    5 [label=\"{{5|EEE}|{13|1}}\"];\n"
"    6 [label=\"{{6|FFF}|{12|1}}\"];\n"
"    19 [label=\"{{19|GGG}|{11|3,12}}\"];\n"
"    20 [label=\"{{20|HHH}|{10|3,12}}\"];\n"
"    21 [label=\"{{21|III}|{9|3,12}}\"];\n"
"    16 [label=\"{{16|JJJ}|{8|3,11}}\"];\n"
"    17 [label=\"{{17|KKK}|{7|3,11}}\"];\n"
"    13 [label=\"{{13|LLL}|{6|2,9}}\"];\n"
"    14 [label=\"{{14|MMM}|{5|2,9}}\"];\n"
"    15 [label=\"{{15|NNN}|{4|2,9}}\"];\n"
"    22 [label=\"{{22|OOO}|{3|3,11,18}}\"];\n"
"    23 [label=\"{{23|PPP}|{2|3,11,18}}\"];\n"
"    24 [label=\"{{24|QQQ}|{1|3,11,18}}\"];\n"
"\n"
"    1 -> 4;\n"
"    1 -> 5;\n"
"    1 -> 6;\n"
"    0 -> 1;\n"
"    2 -> 7;\n"
"    2 -> 8;\n"
"    9 -> 13;\n"
"    9 -> 14;\n"
"    9 -> 15;\n"
"    2 -> 9;\n"
"    0 -> 2;\n"
"    3 -> 10;\n"
"    11 -> 16;\n"
"    11 -> 17;\n"
"    18 -> 22;\n"
"    18 -> 23;\n"
"    18 -> 24;\n"
"    11 -> 18;\n"
"    3 -> 11;\n"
"    12 -> 19;\n"
"    12 -> 20;\n"
"    12 -> 21;\n"
"    3 -> 12;\n"
"    0 -> 3;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 27\\n# Leaf: 17\\nmethod: BottomUp\\nmax depth: 2\\nmax branch: 2\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    26 [label=\"{{26|\\</s\\>}|{17|9}}\"];\n"
"    24 [label=\"{{24|\\<unk\\>}|{16|8}}\"];\n"
"    25 [label=\"{{25|CCC}|{15|8}}\"];\n"
"    22 [label=\"{{22|DDD}|{14|7}}\"];\n"
"    23 [label=\"{{23|EEE}|{13|7}}\"];\n"
"    20 [label=\"{{20|FFF}|{12|6}}\"];\n"
"    21 [label=\"{{21|GGG}|{11|6}}\"];\n"
"    18 [label=\"{{18|HHH}|{10|5}}\"];\n"
"    19 [label=\"{{19|III}|{9|5}}\"];\n"
"    16 [label=\"{{16|JJJ}|{8|4}}\"];\n"
"    17 [label=\"{{17|KKK}|{7|4}}\"];\n"
"    14 [label=\"{{14|LLL}|{6|3}}\"];\n"
"    15 [label=\"{{15|MMM}|{5|3}}\"];\n"
"    12 [label=\"{{12|NNN}|{4|2}}\"];\n"
"    13 [label=\"{{13|OOO}|{3|2}}\"];\n"
"    10 [label=\"{{10|PPP}|{2|1}}\"];\n"
"    11 [label=\"{{11|QQQ}|{1|1}}\"];\n"
"\n"
"    1 -> 10;\n"
"    1 -> 11;\n"
"    0 -> 1;\n"
"    2 -> 12;\n"
"    2 -> 13;\n"
"    0 -> 2;\n"
"    3 -> 14;\n"
"    3 -> 15;\n"
"    0 -> 3;\n"
"    4 -> 16;\n"
"    4 -> 17;\n"
"    0 -> 4;\n"
"    5 -> 18;\n"
"    5 -> 19;\n"
"    0 -> 5;\n"
"    6 -> 20;\n"
"    6 -> 21;\n"
"    0 -> 6;\n"
"    7 -> 22;\n"
"    7 -> 23;\n"
"    0 -> 7;\n"
"    8 -> 24;\n"
"    8 -> 25;\n"
"    0 -> 8;\n"
"    9 -> 26;\n"
"    0 -> 9;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 18\\n# Leaf: 17\\nmethod: BottomUp\\nmax depth: 1\\nmax branch: 5\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    1 [label=\"{{1|\\</s\\>}|{17|}}\"];\n"
"    2 [label=\"{{2|\\<unk\\>}|{16|}}\"];\n"
"    3 [label=\"{{3|CCC}|{15|}}\"];\n"
"    4 [label=\"{{4|DDD}|{14|}}\"];\n"
"    5 [label=\"{{5|EEE}|{13|}}\"];\n"
"    6 [label=\"{{6|FFF}|{12|}}\"];\n"
"    7 [label=\"{{7|GGG}|{11|}}\"];\n"
"    8 [label=\"{{8|HHH}|{10|}}\"];\n"
"    9 [label=\"{{9|III}|{9|}}\"];\n"
"    10 [label=\"{{10|JJJ}|{8|}}\"];\n"
"    11 [label=\"{{11|KKK}|{7|}}\"];\n"
"    12 [label=\"{{12|LLL}|{6|}}\"];\n"
"    13 [label=\"{{13|MMM}|{5|}}\"];\n"
"    14 [label=\"{{14|NNN}|{4|}}\"];\n"
"    15 [label=\"{{15|OOO}|{3|}}\"];\n"
"    16 [label=\"{{16|PPP}|{2|}}\"];\n"
"    17 [label=\"{{17|QQQ}|{1|}}\"];\n"
"\n"
"    0 -> 1;\n"
"    0 -> 2;\n"
"    0 -> 3;\n"
"    0 -> 4;\n"
"    0 -> 5;\n"
"    0 -> 6;\n"
"    0 -> 7;\n"
"    0 -> 8;\n"
"    0 -> 9;\n"
"    0 -> 10;\n"
"    0 -> 11;\n"
"    0 -> 12;\n"
"    0 -> 13;\n"
"    0 -> 14;\n"
"    0 -> 15;\n"
"    0 -> 16;\n"
"    0 -> 17;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 24\\n# Leaf: 17\\nmethod: TopDown\\nmax depth: 3\\nmax branch: 2\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    0 [label=\"{{0|\\</s\\>}|{17|18,20}}\"];\n"
"    1 [label=\"{{1|\\<unk\\>}|{16|18,20}}\"];\n"
"    2 [label=\"{{2|CCC}|{15|18,20}}\"];\n"
"    3 [label=\"{{3|DDD}|{14|18,20}}\"];\n"
"    4 [label=\"{{4|EEE}|{13|18,21}}\"];\n"
"    5 [label=\"{{5|FFF}|{12|18,21}}\"];\n"
"    6 [label=\"{{6|GGG}|{11|18,21}}\"];\n"
"    7 [label=\"{{7|HHH}|{10|19,22}}\"];\n"
"    8 [label=\"{{8|III}|{9|19,22}}\"];\n"
"    9 [label=\"{{9|JJJ}|{8|19,22}}\"];\n"
"    10 [label=\"{{10|KKK}|{7|19,22}}\"];\n"
"    11 [label=\"{{11|LLL}|{6|19,23}}\"];\n"
"    12 [label=\"{{12|MMM}|{5|19,23}}\"];\n"
"    13 [label=\"{{13|NNN}|{4|19,23}}\"];\n"
"    14 [label=\"{{14|OOO}|{3|19,23}}\"];\n"
"    15 [label=\"{{15|PPP}|{2|19,23}}\"];\n"
"    16 [label=\"{{16|QQQ}|{1|19,23}}\"];\n"
"\n"
"    20 -> 0;\n"
"    20 -> 1;\n"
"    20 -> 2;\n"
"    20 -> 3;\n"
"    18 -> 20;\n"
"    21 -> 4;\n"
"    21 -> 5;\n"
"    21 -> 6;\n"
"    18 -> 21;\n"
"    17 -> 18;\n"
"    22 -> 7;\n"
"    22 -> 8;\n"
"    22 -> 9;\n"
"    22 -> 10;\n"
"    19 -> 22;\n"
"    23 -> 11;\n"
"    23 -> 12;\n"
"    23 -> 13;\n"
"    23 -> 14;\n"
"    23 -> 15;\n"
"    23 -> 16;\n"
"    19 -> 23;\n"
"    17 -> 19;\n"
"  }\n"
"}\n",

"digraph output {\n"
"  rankdir=TB;\n"
"  labelloc=t;\n"
"  label=\"Output Tree\";\n"
"\n"
"  subgraph cluster_param {\n"
"    label=\"Params\";\n"
"    node [shape=plaintext, style=solid];\n"
"    edge [style=invis];\n"
"    legend [label=\"# Node: 18\\n# Leaf: 17\\nmethod: TopDown\\nmax depth: 1\\nmax branch: 2\\nNormalization: Undefined\"];\n"
"  }\n"
"\n"
"  subgraph cluster_tree {\n"
"    label=\"\"\n"
"    graph [ranksep=0];\n"
"    node [shape=record];\n"
"\n"
"    0 [label=\"{{0|\\</s\\>}|{17|}}\"];\n"
"    1 [label=\"{{1|\\<unk\\>}|{16|}}\"];\n"
"    2 [label=\"{{2|CCC}|{15|}}\"];\n"
"    3 [label=\"{{3|DDD}|{14|}}\"];\n"
"    4 [label=\"{{4|EEE}|{13|}}\"];\n"
"    5 [label=\"{{5|FFF}|{12|}}\"];\n"
"    6 [label=\"{{6|GGG}|{11|}}\"];\n"
"    7 [label=\"{{7|HHH}|{10|}}\"];\n"
"    8 [label=\"{{8|III}|{9|}}\"];\n"
"    9 [label=\"{{9|JJJ}|{8|}}\"];\n"
"    10 [label=\"{{10|KKK}|{7|}}\"];\n"
"    11 [label=\"{{11|LLL}|{6|}}\"];\n"
"    12 [label=\"{{12|MMM}|{5|}}\"];\n"
"    13 [label=\"{{13|NNN}|{4|}}\"];\n"
"    14 [label=\"{{14|OOO}|{3|}}\"];\n"
"    15 [label=\"{{15|PPP}|{2|}}\"];\n"
"    16 [label=\"{{16|QQQ}|{1|}}\"];\n"
"\n"
"    17 -> 0;\n"
"    17 -> 1;\n"
"    17 -> 2;\n"
"    17 -> 3;\n"
"    17 -> 4;\n"
"    17 -> 5;\n"
"    17 -> 6;\n"
"    17 -> 7;\n"
"    17 -> 8;\n"
"    17 -> 9;\n"
"    17 -> 10;\n"
"    17 -> 11;\n"
"    17 -> 12;\n"
"    17 -> 13;\n"
"    17 -> 14;\n"
"    17 -> 15;\n"
"    17 -> 16;\n"
"  }\n"
"}\n"
};

static int check_output(output_t *output, count_t *cnts,
        st_alphabet_t *vocab, int ncase)
{
    char *buf = NULL;
    FILE *fp = NULL;
    size_t sz;
    int ret;

    fp = tmpfile();
    if (fp == NULL) {
        ST_WARNING("Failed to tmpfile.");
        goto ERR;
    }

    if (output_draw(output, fp, cnts, vocab) < 0) {
        ST_WARNING("Failed to output_draw.");
        goto ERR;
    }

    sz = ftell(fp);

    buf = malloc(sz + 1);
    if (buf == NULL) {
        ST_WARNING("Failed to malloc buf.");
        goto ERR;
    }

    rewind(fp);

    if (fread(buf, 1, sz, fp) != sz) {
        ST_WARNING("Failed to read back buf.");
        goto ERR;
    }
    buf[sz] = '\0';

#ifdef _OUTPUT_TEST_DEBUG_
    fprintf(stdout, "case %d\n", ncase - 1);
    fprintf(stdout, "%s", buf);
    fprintf(stdout, "REF\n%s", refs[ncase - 1]);
#endif
    ret = strcmp(refs[ncase - 1], buf);

    safe_fclose(fp);
    safe_free(buf);

    return ret;
ERR:
    safe_fclose(fp);
    safe_free(buf);

    return -1;
}

static int unit_test_output_generate()
{
    vocab_t *vocab = NULL;
    int ncase = 0;
    output_opt_t output_opt;
    output_t *output = NULL;

    vocab = vocab_test_new();
    assert(vocab != NULL);

    fprintf(stderr, "  Testing output generate...\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = OM_TOP_DOWN;
    output_opt.max_depth = 0;
    output_opt.max_branch = 3;
    output = output_generate(&output_opt, vocab->cnts, vocab->vocab_size);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, vocab->cnts, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    fprintf(stderr, "Success\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    if (check_output(output, NULL, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    fprintf(stderr, "Success\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    if (check_output(output, NULL, NULL, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    fprintf(stderr, "Success\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    if (check_output(output, vocab->cnts, NULL, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    fprintf(stderr, "Success\n");
    safe_output_destroy(output);

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = OM_BOTTOM_UP;
    output_opt.max_depth = 0;
    output_opt.max_branch = 3;
    output = output_generate(&output_opt, vocab->cnts, vocab->vocab_size);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, vocab->cnts, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = OM_BOTTOM_UP;
    output_opt.max_depth = 2;
    output_opt.max_branch = 2;
    output = output_generate(&output_opt, vocab->cnts, vocab->vocab_size);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, vocab->cnts, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = OM_BOTTOM_UP;
    output_opt.max_depth = 1;
    output_opt.max_branch = 5;
    output = output_generate(&output_opt, vocab->cnts, vocab->vocab_size);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, vocab->cnts, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = OM_TOP_DOWN;
    output_opt.max_depth = 3;
    output_opt.max_branch = 2;
    output = output_generate(&output_opt, vocab->cnts, vocab->vocab_size);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, vocab->cnts, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = OM_TOP_DOWN;
    output_opt.max_depth = 1;
    output_opt.max_branch = 2;
    output = output_generate(&output_opt, vocab->cnts, vocab->vocab_size);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, vocab->cnts, vocab->alphabet, ncase) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");

    return 0;

ERR:
    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    return -1;
}

static int unit_test_output_read_topo()
{
    char line[MAX_LINE_LEN];

    vocab_t *vocab = NULL;
    output_t *output = NULL;

    output_ref_t ref;
    int ncase = 0;
    output_ref_t std_ref = {
        .norm = ON_SOFTMAX,
    };

    fprintf(stderr, "  Testing Reading topology file...\n");
    vocab = vocab_test_new();
    assert(vocab != NULL);
    output = output_test_new(vocab);
    assert(output != NULL);

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    output_test_mk_topo_line(line, MAX_LINE_LEN, &ref);
    if (output_parse_topo(output, line) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (output_test_check_output(output, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    fprintf(stderr, "Success\n");

    safe_vocab_destroy(vocab);
    safe_output_destroy(output);

    return 0;

ERR:
    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_output_generate() != 0) {
        ret = -1;
    }

    if (unit_test_output_read_topo() != 0) {
        ret = -1;
    }

    return ret;
}

int main(int argc, const char *argv[])
{
    int ret;

    fprintf(stderr, "Start testing...\n");
    ret = run_all_tests();
    if (ret != 0) {
        fprintf(stderr, "Tests failed.\n");
    } else {
        fprintf(stderr, "Tests succeeded.\n");
    }

    return ret;
}
