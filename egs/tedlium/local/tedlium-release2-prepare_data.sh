#!/bin/bash
#
# Copyright  2014  Nickolay V. Shmyrev
#            2014  Brno University of Technology (Author: Karel Vesely)
#            2016  Johns Hopkins University (Author: Daniel Povey)
# Apache 2.0

# To be run from one directory above this script.

set -e
set -o pipefail

if [ $# -ne 3 ]; then
  echo "Usage: $0 <corpus_dir> <data_dir>"
  exit 1
fi

corpus=$1
num_valid=$2
data=$3

mkdir -p $data/tmp
export LC_ALL=C
# Prepare: dev, train,
for set in train dev; do
  # Merge transcripts into a single 'stm' file, do some mappings:
  # - <F0_M> -> <o,f0,male> : map dev stm labels to be coherent with train + test,
  # - <F0_F> -> <o,f0,female> : --||--
  # - (2) -> null : remove pronunciation variants in transcripts, keep in dictionary
  # - <sil> -> null : remove marked <sil>, it is modelled implicitly (in kaldi)
  # - (...) -> null : remove utterance names from end-lines of train
  # - it 's -> it's : merge words that contain apostrophe (if compound in dictionary, local/join_suffix.py)
  { # Add STM header, so sclite can prepare the '.lur' file
    echo ';;
;; LABEL "o" "Overall" "Overall results"
;; LABEL "f0" "f0" "Wideband channel"
;; LABEL "f2" "f2" "Telephone channel"
;; LABEL "male" "Male" "Male Talkers"
;; LABEL "female" "Female" "Female Talkers"
;;'
    # Process the STMs
    cat $corpus/TEDLIUM_release2/$set/stm/*.stm | sort -k1,1 -k2,2 -k4,4n | \
      sed -e 's:<F0_M>:<o,f0,male>:' \
          -e 's:<F0_F>:<o,f0,female>:' \
          -e 's:([0-9])::g' \
          -e 's:<sil>::g' \
          -e 's:([^ ]*)$::' | \
      awk '{ $2 = "A"; print $0; }'
  } | local/join_suffix.py > $data/tmp/$set.stm

  # Prepare 'text' file
  # - {NOISE} -> [NOISE] : map the tags to match symbols in dictionary
  cat $data/tmp/$set.stm | grep -v -e 'ignore_time_segment_in_scoring' -e ';;' | \
    awk '{ printf ("%s-%07d-%07d", $1, $4*100, $5*100);
           for (i=7;i<=NF;i++) { printf(" %s", $i); }
           printf("\n");
         }' | tr '{}' '[]' | sort -k1,1 > $data/tmp/$set.txt || exit 1
done

# Unzip TEDLIUM 6 data sources, normalize apostrophe+suffix to previous word, gzip the result.
gunzip -c $corpus/TEDLIUM_release2/LM/*.en.gz | sed 's/ <\/s>//g' | local/join_suffix.py | gzip -c > $data/train.gz
# use a subset of the annotated training data as the dev set .
head -n $num_valid < data/tmp/train.txt | cut -d " " -f 2-  > $data/valid
# .. and the rest of the training data as an additional data source.
tail -n +$[$num_valid+1] < data/tmp/train.txt | cut -d " " -f 2- | gzip -c >> $data/train.gz

# for reporting perplexities, we'll use the "real" dev set.
cut -d " " -f 2-  < $data/tmp/dev.txt  > $data/test

# get wordlist
awk '{print $1}' $corpus/TEDLIUM_release2/TEDLIUM.152k.dic | sed 's:([0-9])::g' | sort | uniq > $data/wordlist
