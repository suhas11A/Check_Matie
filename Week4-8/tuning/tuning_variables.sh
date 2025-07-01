#!/bin/bash

for i in {1..100}
do
    w1=$(awk "BEGIN {print $i * 0.005}")
    w2=$(awk "BEGIN {print 0.5 - $w1}")
    w3=0.3
    w4=$(awk "BEGIN {print 1 - $w1 - $w2 - $w3}")

    g++ -DWEIGHTS="\{$w1, $w2, $w3, $w4\}" main.cpp -o matie_$i.out
done
