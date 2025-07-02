#!/usr/bin/env bash
set -euo pipefail

# Baseline weights
BASE1=0.1   # mobility
BASE2=0.45  # king mobility
BASE3=0.3   # pawn promotion
# w4 is implicit = 1 - (w1+w2+w3)

# Perturbation steps around each base (±0.02 in 0.01 increments)
DELTAS=(-0.02 -0.01 0 0.01 0.02)

# Prepare output
mkdir -p engines results
echo "w1,w2,w3,w4,score" > results/scores.csv
ID=0

for d1 in "${DELTAS[@]}"; do
  for d2 in "${DELTAS[@]}"; do
    for d3 in "${DELTAS[@]}"; do

      # Compute weights, ensure w4>=0
      w1=$(awk "BEGIN{v=${BASE1}+${d1}; printf \"%.4f\", v}")
      w2=$(awk "BEGIN{v=${BASE2}+${d2}; printf \"%.4f\", v}")
      w3=$(awk "BEGIN{v=${BASE3}+${d3}; printf \"%.4f\", v}")
      w4=$(awk "BEGIN{v=1-(${w1}+${w2}+${w3}); printf \"%.4f\", v}")

      # Skip invalid sets
      if (( $(awk "BEGIN{print ($w4 <= 0)}") )); then
        continue
      fi

      ((ID++))
      BIN=engines/engine_${ID}
      echo "Compiling variant $ID → {${w1},${w2},${w3},${w4}}"
      g++ -O3 \
        -DWEIGHTS="\{${w1},${w2},${w3},${w4}\}" \
        uci.cpp -o "${BIN}"

      # Run a small Swiss tournament: 20 games, 4 threads, Stockfish limited to Elo 1700
      echo "Running games for variant $ID..."
      cutechess-cli \
        -engine cmd="${BIN}" name=MyEng proto=uci option.Threads=4 \
        -engine cmd=stockfish name=SF proto=uci \
          option.LimitStrength=true option.Elo=1700 \
        -games 20 -tournament swiss \
        -resign movecount 3 -resignmate 4 \
        -draw movenumber 40 scorecondition ½-½ \
        -concurrency 4 -silent \
        -pgnout results/games_${ID}.pgn \
        | tee results/log_${ID}.txt

      # Extract MyEng’s score (wins*1 + draws*0.5)
      SCORE=$(grep -oP 'MyEng wins\s+\K[0-9]+' -m1 results/log_${ID}.txt || echo 0)
      DRAWS=$(grep -oP 'draws\s+\K[0-9]+' -m1 results/log_${ID}.txt || echo 0)
      TOTAL=$(awk "BEGIN{printf \"%.2f\", ${SCORE} + ${DRAWS}*0.5}")

      echo "${w1},${w2},${w3},${w4},${TOTAL}" >> results/scores.csv

    done
  done
done

# Finally, show the top 3
echo -e "\nTop 3 variants by score:"
sort -t, -k5 -nr results/scores.csv | head -n 4
