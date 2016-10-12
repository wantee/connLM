#!/bin/bash

# Begin configuration section.
output_config_file=""
init_config_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <vocab-clm> <topo-file> <exp-dir>"
  echo "e.g.: $0 exp/vocab.clm conf/rnn/topo exp/rnn"
  echo "options: "
  echo "     --init-config-file <config-file>    # init config file."
  echo "     --output-config-file <config-file>  # output config file."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 3 ]; then
  print_help 1>&2
  exit 1;
fi

vocab_mdl=$1
topo=$2
dir=$3

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`
mkdir -p $dir
mkdir -p "$dir/log"

init_mdl="$dir/init.clm"
init_log="$dir/log/init.log"
output_mdl="$dir/output.clm"
output_log="$dir/log/output.log"

if [ -e "$init_mdl" ]; then
  echo "$0: Skipping Initializing $init_mdl ..."
  exit 0
fi

echo "$0: Initializing model $vocab_mdl to $init_mdl ..."

if [ -z "$output_config_file" ]; then
shu-run connlm-output --log-file="$output_log" \
           "$vocab_mdl" "$output_mdl" \
|| exit 1;
else
shu-run connlm-output --log-file="$output_log" \
           --config="$output_config_file" \
           "$vocab_mdl" "$output_mdl" \
|| exit 1;
fi

if [ -z "$init_config_file" ]; then
shu-run connlm-init --log-file="$init_log" \
           "$output_mdl" "$topo" "$init_mdl" \
|| exit 1;
else
shu-run connlm-init --log-file="$init_log" \
           --config="$init_config_file" \
           "$output_mdl" "$topo" "$init_mdl" \
|| exit 1;
fi

../utils/check_log.sh -b "$begin_date" "$init_log.wf" "$output_log.wf"

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
