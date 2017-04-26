#!/bin/bash

usage()
{
  echo >&2 "Usage: $0 [-b begin_time] [-e end_time] <mode> <file1> [file2 ...]"
  echo >&2 "       <mode> could be 'warn' or 'mem'"
}

date2timestamp()
{
  if date --version >/dev/null 2>&1; then # GNU date
    date --date "$1" +"%s"
  else # BSD date
    date -jf "%Y-%m-%d %H:%M:%S" "$1" +"%s"
  fi
}

datelog()
{
  log=$1
  begin_line=$2
  end_line=$3

  if [ -n "$begin_line" ] && [ -n "$end_line" ]; then
    sed -n "$begin_line,$end_line p" $log
  elif [ -n "$begin_line" ]; then
    tail -n "+$begin_line" $log
  elif [ -n "$end_line" ]; then
    sed -n "1,$end_line p" $log
  else
    cat $log
  fi
}

begin=""
end=""
while getopts b:e: o
do  case "$o" in
  b)  begin="$OPTARG";;
  e)  end="$OPTARG";;
  [?])  usage; exit 1;;
  esac
done

shift $((OPTIND-1))

if [ $# -lt 2 ]
then
  usage
  exit 1
fi

mode=$1
case "$mode" in
  "warn")  marker="= OPEN LOG WF =";;
  "mem")  marker="= OPEN LOG =";;
  *)  usage; exit 1;;
esac

shift

warns=()
mems=()
for log in $*; do
  begin_line=""
  end_line=""

  if [ -n "$begin" ]; then # find begin line
    begin_ts=`date2timestamp "$begin"`
    while read line; do
      d=`echo $line | awk -F'[()]' '{print $2}'`
      ts=`date2timestamp "$d"`
      if [ "$ts" -ge "$begin_ts" ]; then
        begin_line=`echo $line | awk -F':' '{print $1}'`
        break;
      fi
    done < <(grep -n "$marker" $log)
  fi

  if [ -n "$end" ]; then # find end line
    end_ts=`date2timestamp "$end"`
    while read line; do
      d=`echo $line | awk -F'[()]' '{print $2}'`
      ts=`date2timestamp "$d"`
      if [ "$ts" -ge "$end_ts" ]; then
        end_line=`echo $line | awk -F':' '{print $1}'`
        break;
      fi
    done < <(grep -n "$marker" $log)
  fi

  if [ "$mode" == "warn" ]; then
    warns+=($log:`datelog "$log" "$begin_line" "$end_line" | grep -c "WARNING"`)
  elif [ "$mode" == "mem" ]; then
    allocs=`datelog "$log" "$begin_line" "$end_line" | grep "#allocs" | tail -n 1 |awk -F'[: ]' '{print $NF}'`
    frees=`datelog "$log" "$begin_line" "$end_line" | grep "#frees" | tail -n 1 | awk -F'[: ]' '{print $NF}'`
    mems+=($log:$allocs:$frees)
  fi
done

if [ "$mode" == "warn" ]; then
  total=0
  for warn in "${warns[@]}" ; do
    log=${warn%%:*}
    num=${warn#*:}
    if [ "$num" -gt 0 ]; then
      echo "$num warnings in $log."
      total=$((total+num))
    fi
  done

  if [ "$total" -gt 0 ]; then
    echo "Total $total warnings found."
  fi
elif [ "$mode" == "mem" ]; then
  err=false
  for mem in "${mems[@]}" ; do
    log=`echo $mem | awk -F':' '{print $1}'`
    allocs=`echo $mem | awk -F':' '{print $2}'`
    frees=`echo $mem | awk -F':' '{print $3}'`
    if [ "$allocs" -ne "$frees" ]; then
      err=true
      echo "allocs[$allocs] and frees[$frees] not equal in $log."
    fi
  done

  if $err; then
    echo "You may run valgrind --leak-check=full to find the leaking variables."
  fi
fi
