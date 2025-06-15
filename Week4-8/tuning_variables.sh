#!/usr/bin/env bash
set -euo pipefail

# Loop fifth argument over 2, 3, 4
for f5 in 2 3 4; do
  # Loop first argument: 0 → 0.2 in steps of 0.01
  for f1 in $(seq 0 0.02 0.2); do
    # Loop second argument: 0.2 → 0.6 in steps of 0.04
    for f2 in $(seq 0.2 0.05 0.6); do
      # Loop third argument: same as second
      for f3 in $(seq 0.2 0.05 0.6); do
        # Loop fourth argument: 0.05 → 0.35 in steps of 0.02
        for f4 in $(seq 0.05 0.05 0.35); do
          echo "Running: $f1 $f2 $f3 $f4 $f5"
          output1=$(./a.out "$f1" "$f2" "$f3" "$f4" "$f5" | grep "b" | grep "-" | wc -l)
          output2=$(./a.out "$f1" "$f2" "$f3" "$f4" "$f5" | grep "w" | grep "-" | wc -l)
          output3=$(./a.out "$f1" "$f2" "$f3" "$f4" "$f5" | grep "b" | wc -l)
          output4=$(./a.out "$f1" "$f2" "$f3" "$f4" "$f5" | grep "w" | wc -l)
          echo "Output: $output1 / $output3 and $output2 / $output4"
          echo "scale=7; ($output1 / $output3) - ($output2 / $output4)" | bc
          echo "----------------------------------------"
        done
      done
    done
  done
done
