#!/bin/bash
for pid in $(ps ax | awk '{print $1}')
do
         taskset -pc 0-4 $pid
done
