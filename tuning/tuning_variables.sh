#!/usr/bin/env bash
set -euo pipefail
set -x
trap 'echo "🚨 Unexpected error at line $LINENO: \"$BASH_COMMAND\" exited with code $?.">&2; exit 1' ERR

LOG="tune_errors.log"
: > "$LOG"    # truncate previous log

# Baseline weights
BASE1=0.1    # mobility
BASE2=0.45   # king mobility
BASE3=0.3    # pawn promotion
BASE4=0.16   # pieces under attack

# Perturbation steps (±0.02 in 0.01 increments)
DELTAS=(-0.02 -0.01 0 0.01 0.02)

mkdir -p engines results
echo "w1,w2,w3,w4,score" > results/scores.csv

ID=0
for d1 in "${DELTAS[@]}"; do
  for d2 in "${DELTAS[@]}"; do
    for d3 in "${DELTAS[@]}"; do
      for d4 in "${DELTAS[@]}"; do

        # Compute & format each weight to 4dp
        w1=$(awk "BEGIN{printf \"%.4f\", ${BASE1}+${d1}}")
        w2=$(awk "BEGIN{printf \"%.4f\", ${BASE2}+${d2}}")
        w3=$(awk "BEGIN{printf \"%.4f\", ${BASE3}+${d3}}")
        w4=$(awk "BEGIN{printf \"%.4f\", ${BASE4}+${d4}}")

        # Skip any non-positive weight
        if (( $(awk "BEGIN{print (${w1}<=0)||(${w2}<=0)||(${w3}<=0)||(${w4}<=0)}") )); then
          echo "⚠️ Skipping invalid {${w1},${w2},${w3},${w4}}"
          continue
        fi

        ID=$((ID+1))
        BIN=engines/engine_${ID}.out

        echo "=== Variant #${ID}: {${w1},${w2},${w3},${w4}} ==="

        # Compile
        echo "Compiling…" 
        if ! g++ -O3 \
             "-DWEIGHTS={${w1},${w2},${w3},${w4}}" \
             uci.cpp -o "${BIN}" \
             2>>"$LOG"; then
          echo "❌ Compile failed for variant #${ID}. See $LOG"
          continue
        fi

        # Run tournament
        echo "Playing 20 games vs Stockfish @ 1700…"
        if ! cutechess-cli \
             -engine cmd="${BIN}" name=MyEng proto=uci option.Threads=4 \
             -engine cmd=stockfish name=SF proto=uci \
               option.LimitStrength=true option.Elo=1700 \
             -games 20 -tournament swiss \
             -resign movecount 3 -resignmate 4 \
             -draw movenumber 40 scorecondition ½-½ \
             -concurrency 4 -silent \
             -pgnout results/games_${ID}.pgn \
             2>>"$LOG" \
             | tee results/log_${ID}.txt; then
          echo "❌ cutechess-cli failed for variant #${ID}. See $LOG"
          continue
        fi

        # Extract score
        WINS=$(grep -oP 'MyEng wins\s+\K[0-9]+' -m1 results/log_${ID}.txt || echo 0)
        DRAWS=$(grep -oP 'draws\s+\K[0-9]+'   -m1 results/log_${ID}.txt || echo 0)
        SCORE=$(awk "BEGIN{printf \"%.2f\", ${WINS} + ${DRAWS}*0.5}")

        echo "${w1},${w2},${w3},${w4},${SCORE}" >> results/scores.csv
        echo "→ Variant #${ID} scored ${SCORE}"
        echo

      done
    done
  done
done

echo "=== Top 3 Variants by Score ==="
sort -t, -k5 -nr results/scores.csv | head -n4
