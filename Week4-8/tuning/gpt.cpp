#include "/home/suhas/libraries/chess-library/include/chess.hpp"
#include "/home/suhas/libraries/json/single_include/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <limits>
#include <chrono>
#include <iomanip>

using namespace chess;
using namespace std;
using json = nlohmann::json;

#ifndef WEIGHTS
#define WEIGHTS {0.1f, 0.45f, 0.4f, 0.16f}
#endif

const float my_inf = numeric_limits<float>::infinity();
const float DRAW_SCORE = 0.0f;

struct CacheEntry {
    vector<pair<Move, float>> scored_moves;
};

class MoveGen {
public:
    unordered_map<uint64_t, CacheEntry> move_cache;
    vector<uint64_t> hash_history;
    static constexpr int MAX_PHASE = 16; // 2*(0+1+1+2+4)

    // Material-based phase metric: 1.0 opening -> 0.0 endgame
    float computePhase(const Board &board) const {
        int phase = 0;
        // Pawn weight = 0
        // Knight, Bishop = 1
        // Rook = 2
        // Queen = 4
        auto sumCount = [&](PieceType pt){
            return board.pieces(pt, Color::WHITE).count()
                 + board.pieces(pt, Color::BLACK).count();
        };
        phase += sumCount(PieceType::KNIGHT) * 1;
        phase += sumCount(PieceType::BISHOP) * 1;
        phase += sumCount(PieceType::ROOK)   * 2;
        phase += sumCount(PieceType::QUEEN)  * 4;
        return float(phase) / float(MAX_PHASE);
    }

    int getDynamicMaxDepth(int baseDepth, const Board &board) const {
        float phase = computePhase(board); // 1 = opening, 0 = endgame
        return int(baseDepth + (1.0f - phase) * 2.0f);
    }

    float piece_value(PieceType p) const {
        if (p == PieceType::PAWN)   return 1.0f;
        if (p == PieceType::KNIGHT) return 3.5f;
        if (p == PieceType::BISHOP) return 3.5f;
        if (p == PieceType::ROOK)   return 5.5f;
        if (p == PieceType::QUEEN)  return 10.5f;
        if (p == PieceType::KING)   return 100000000.0f;
        return 0.0f;
    }

    float piece_value_attack(PieceType p) const {
        if (p == PieceType::PAWN)   return 1.0f;
        if (p == PieceType::KNIGHT) return 4.0f;
        if (p == PieceType::BISHOP) return 5.0f;
        if (p == PieceType::ROOK)   return 8.0f;
        if (p == PieceType::QUEEN)  return 15.0f;
        if (p == PieceType::KING)   return 20.0f;
        return 0.0f;
    }

    int king_mobility(const Board& board, Color us) const {
        Color them = (us == Color::WHITE ? Color::BLACK : Color::WHITE);
        Square ksq = board.kingSq(us);
        Bitboard zone = attacks::king(ksq);
        Bitboard occ = 0ULL;
        for (auto pt: {PieceType::PAWN,PieceType::KNIGHT,PieceType::BISHOP,
                       PieceType::ROOK,PieceType::QUEEN,PieceType::KING}) {
            occ |= board.pieces(pt);
        }
        Bitboard free_bb = zone & ~occ;
        int cnt = 0;
        while (free_bb.count() > 0) {
            Square sq = free_bb.pop();
            if (!board.isAttacked(sq, them)) ++cnt;
        }
        return cnt;
    }

    float promotion_potential(const Board &board, Color us) const {
        float total = 0.0f;
        Bitboard pawns = board.pieces(PieceType::PAWN, us);
        while (pawns.count() > 0) {
            int sq = pawns.pop();
            int rank = (sq / 8) + 1;
            float frac = (us == Color::WHITE)
                ? float(rank - 1) / 6.0f
                : float(6 - (rank - 1)) / 6.0f;
            if (fabs(frac - (5.0f/6.0f)) < 0.01f) frac *= 2.3f;
            total += frac;
        }
        return total;
    }

    int count_threats(const Board& board, Color us) const {
        Color them = (us == Color::WHITE ? Color::BLACK : Color::WHITE);
        int tot = 0;
        for (auto pt: {PieceType::PAWN,PieceType::KNIGHT,PieceType::BISHOP,
                       PieceType::ROOK,PieceType::QUEEN,PieceType::KING}) {
            Bitboard bb = board.pieces(pt, us);
            while (bb.count() > 0) {
                Square sq = bb.pop();
                if (board.isAttacked(sq, them)) tot += piece_value_attack(pt);
            }
        }
        return tot;
    }

    float utility(Board& board, float w[]) const {
        Movelist moves;
        movegen::legalmoves(moves, board);
        Board fake = board;
        fake.makeNullMove();
        Movelist fake_moves;
        movegen::legalmoves(fake_moves, fake);

        if (board.isHalfMoveDraw()) {
            bool mate = (board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE);
            return mate ? ((board.sideToMove()==Color::WHITE)?-my_inf:my_inf) : DRAW_SCORE;
        }
        if (board.isRepetition()) return DRAW_SCORE;
        if (moves.empty()) {
            return board.inCheck()
                ? ((board.sideToMove()==Color::WHITE)?-my_inf:my_inf)
                : DRAW_SCORE;
        }

        float u = 0.0f;
        for (auto pt: {PieceType::PAWN,PieceType::KNIGHT,PieceType::BISHOP,
                       PieceType::ROOK,PieceType::QUEEN,PieceType::KING}) {
            int cw = board.pieces(pt,Color::WHITE).count();
            int cb = board.pieces(pt,Color::BLACK).count();
            u += float(cw - cb) * piece_value(pt);
        }
        u += w[0] * ((board.sideToMove()==Color::WHITE)
            ? float(moves.size() - fake_moves.size())
            : float(fake_moves.size() - moves.size()));
        u += w[1] * float(king_mobility(board,Color::WHITE) - king_mobility(fake,Color::BLACK));
        u += w[2] * (promotion_potential(board,Color::WHITE) - promotion_potential(board,Color::BLACK));
        u -= w[3] * float(count_threats(board,Color::WHITE) - count_threats(board,Color::BLACK));
        return u;
    }

    pair<Move,float> alphaBeta(Board &board, float alpha, float beta,
                                int depth, Color player, float w[]) {
        uint64_t h = board.hash();
        if (board.isRepetition() || count(hash_history.begin(), hash_history.end(), h) >= 2)
            return {Move(), DRAW_SCORE};
        hash_history.push_back(h);

        auto gameRes = board.isGameOver();
        if (depth <= 0 || gameRes.first != GameResultReason::NONE) {
            float v = utility(board, w);
            hash_history.pop_back();
            return {Move(), v};
        }

        vector<pair<Move,float>> scored;
        if (move_cache.count(h)) scored = move_cache[h].scored_moves;
        else {
            CacheEntry &e = move_cache[h];
            Movelist ml;
            movegen::legalmoves(ml, board);
            e.scored_moves.reserve(ml.size());
            for (auto m: ml) {
                board.makeMove(m);
                float s = utility(board, w);
                board.unmakeMove(m);
                e.scored_moves.emplace_back(m, s);
            }
            sort(e.scored_moves.begin(), e.scored_moves.end(), [&](auto &a, auto &b){
                return (player==Color::WHITE) ? a.second > b.second : a.second < b.second;
            });
            scored = e.scored_moves;
        }

        Move bestMove;
        if (player == Color::WHITE) {
            float maxv = -my_inf;
            for (auto &pr: scored) {
                board.makeMove(pr.first);
                auto [_, v] = alphaBeta(board, alpha, beta, depth-1, Color::BLACK, w);
                board.unmakeMove(pr.first);
                if (v > maxv) { maxv = v; bestMove = pr.first; }
                alpha = max(alpha, v);
                if (beta <= alpha) break;
            }
            hash_history.pop_back();
            return {bestMove, maxv};
        } else {
            float minv = my_inf;
            for (auto &pr: scored) {
                board.makeMove(pr.first);
                auto [_, v] = alphaBeta(board, alpha, beta, depth-1, Color::WHITE, w);
                board.unmakeMove(pr.first);
                if (v < minv) { minv = v; bestMove = pr.first; }
                beta = min(beta, v);
                if (beta <= alpha) break;
            }
            hash_history.pop_back();
            return {bestMove, minv};
        }
    }

    pair<Move,float> searchWithIterativeDeepening(const Board &board, Color player,
                                                  float w[], int maxBaseDepth,
                                                  double timeLimitSec) {
        auto start = chrono::steady_clock::now();
        pair<Move,float> best = {Move(), DRAW_SCORE};
        for (int d=1; d<=maxBaseDepth; ++d) {
            int depth = getDynamicMaxDepth(d, board);
            hash_history.clear();
            auto res = alphaBeta(const_cast<Board&>(board), -my_inf, my_inf, depth, player, w);
            auto now = chrono::steady_clock::now();
            if (chrono::duration<double>(now - start).count() > timeLimitSec) break;
            best = res;
        }
        return best;
    }
};

Move parseUciMove(Board &board, const string &mvStr) {
    Movelist ms;
    movegen::legalmoves(ms, board);
    for (auto m: ms) if (uci::moveToUci(m) == mvStr) return m;
    return Move();
}

int main() {
    Board board;
    string line;
    float weights[4] = WEIGHTS;
    int maxDepth = 6;
    double timeLimit = 15.0; // seconds per move
    MoveGen solver;

    while (getline(cin, line)) {
        istringstream iss(line);
        string cmd;
        iss >> cmd;
        if (cmd == "uci") {
            cout << "id name Matie\n";
            cout << "id author Suhas\n";
            cout << "uciok\n";
        } else if (cmd == "isready") {
            cout << "readyok\n";
        } else if (cmd == "ucinewgame") {
            board.setFen(constants::STARTPOS);
            solver.move_cache.clear();
            solver.hash_history.clear();
        } else if (cmd == "position") {
            string pt; iss >> pt;
            if (pt == "startpos") {
                board.setFen(constants::STARTPOS);
                string lbl; iss >> lbl;
                if (lbl == "moves") {
                    string mvs;
                    while (iss >> mvs) {
                        Move m = parseUciMove(board, mvs);
                        if (m != Move()) board.makeMove(m);
                    }
                }
            } else if (pt == "fen") {
                vector<string> fparts(6);
                for (int i = 0; i < 6; ++i) iss >> fparts[i];
                board.setFen(
                    fparts[0] + " " + fparts[1] + " " + fparts[2] + " " +
                    fparts[3] + " " + fparts[4] + " " + fparts[5]
                );
            }
        } else if (cmd == "go") {
            Color stm = board.sideToMove();
            auto [bestMove, score] = solver.searchWithIterativeDeepening(
                board, stm, weights, maxDepth, timeLimit
            );
            if (bestMove != Move()) cout << "bestmove " << uci::moveToUci(bestMove) << "\n";
            else                 cout << "bestmove 0000\n";
        } else if (cmd == "quit") {
            break;
        }
    }
    return 0;
}
