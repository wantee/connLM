#!/bin/bash

function date2timestamp()
{
  if date --version >/dev/null 2>&1; then # GNU date 
    date --date "$1" +"%s"
  else # BSD date
    date -jf "%Y-%m-%d %H:%M:%S" "$1" +"%s"
  fi
}

begin=""
end=""
while getopts b:e: o
do  case "$o" in
  b)  begin="$OPTARG";;
  e)  end="$OPTARG";;
  [?])  print >&2 "Usage: $0 [-b begin_time] [-e end_time] file1 ..."
    exit 1;;
  esac
done

shift $((OPTIND-1))

warns=()
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
    done < <(grep -n "OPEN LOG WF" $log)
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
    done < <(grep -n "OPEN LOG WF" $log)
  fi

  if [ -n "$begin_line" ] && [ -n "$end_line" ]; then
    warns+=($log:`sed -n "$begin_line,$end_line p" $log | grep -c "WARNING" `)
  elif [ -n "$begin_line" ]; then
    warns+=($log:`tail -n "+$begin_line" $log | grep -c "WARNING"`)
  elif [ -n "$end_line" ]; then
    warns+=($log:`sed -n "1,$end_line p" $log | grep -c "WARNING"`)
  else
    warns+=($log:`grep -c "WARNING" $log`)
  fi
done

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
