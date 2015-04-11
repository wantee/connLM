#!/bin/bash

function in_range () 
{
    t=$1
    range=$2

    if [ -z "$range" ]; then
        return 0;
    fi

    for r in ${range//,/ }; do
        echo $r | grep '-' > /dev/null
        hyphen=$?

        if [ $hyphen -eq 0 ]; then
            s=`echo $r | cut -d'-' -f1`
            e=`echo $r | cut -d'-' -f2`

            if [ -n "$s" -a -z "$e" ]; then
                if [ $t -ge $s ]; then
                    return 0;
                fi
            elif [ -z "$s" -a -n "$e" ]; then
                if [ $t -le $e ]; then
                    return 0;
                fi
            elif [ -n "$s" -a -n "$e" ]; then
                if [ $s -le $t -a $t -le $e ]; then
                    return 0;
                fi
            fi
        else
            if [ $r -eq $t ]; then
                return 0;
            fi
        fi
    done

    return 1;
}

