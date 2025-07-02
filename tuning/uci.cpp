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
using namespace std::chrono;
using json = nlohmann::json;

#ifndef WEIGHTS
#define WEIGHTS {0.1, 0.45, 0.4, 0.16}
#endif

const float my_inf = std::numeric_limits<float>::infinity(); 

// Struct to cache stuff
struct CacheEntry {
    vector<pair<Move, float>> scored_moves;

    CacheEntry() {}
};



class MoveGen {
public:

    // Use board hash as key to caching
    unordered_map<uint64_t, CacheEntry> move_cache;

    // Ierative Deepening
    time_point<high_resolution_clock> deadline;
    bool stopped = false;
    Move lastCompleteMove;
    int lastCompleteDepth;

    float piece_value(PieceType p) {
        switch (p) {
            case static_cast<int>(PieceType::PAWN): return 1.0f;
            case static_cast<int>(PieceType::KNIGHT): return 3.5f;
            case static_cast<int>(PieceType::BISHOP): return 3.5f;
            case static_cast<int>(PieceType::ROOK): return 5.5f;
            case static_cast<int>(PieceType::QUEEN): return 10.5f;
            case static_cast<int>(PieceType::KING): return 100000000.0f;
            default: return 0.0f;
        }
    }

    float piece_value_attack(PieceType p) {
        switch (p) {
            case static_cast<int>(PieceType::PAWN): return 1.0f;
            case static_cast<int>(PieceType::KNIGHT): return 4.0f;
            case static_cast<int>(PieceType::BISHOP): return 5.0f;
            case static_cast<int>(PieceType::ROOK): return 8.0f;
            case static_cast<int>(PieceType::QUEEN): return 15.0f;
            case static_cast<int>(PieceType::KING): return 20.0f;
            default: return 0.0f;
        }
    }

    int king_mobility(const Board& board, Color us) {
        Color them = (us == Color::WHITE ? Color::BLACK : Color::WHITE);
        Square ksq  = board.kingSq(us);
        Bitboard zone = attacks::king(ksq);
        Bitboard occ = 0ULL;
        for (auto pt : { PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
                        PieceType::ROOK, PieceType::QUEEN,  PieceType::KING }) {
            occ |= board.pieces(pt);
        }
        Bitboard free_bb = zone & ~occ;
        int cnt = 0;
        while (free_bb.count() > 0) {
            Square sq = free_bb.pop();
            if (!board.isAttacked(sq, them))
                ++cnt;
        }
        return cnt;
    }

    float promotion_potential(const Board &board, Color us) {
        float total = 0.0f;
        Bitboard pawns = board.pieces(PieceType::PAWN, us);
        while (pawns.count() > 0) {
            int sq = pawns.pop();
            int rank = (sq / 8) + 1;
            float frac;
            if (us == Color::WHITE) {
                frac = float(rank - 1) / 6.0f;
            } else {
                frac = float(6 - (rank - 1)) / 6.0f;
            }
            if (abs(frac-float(5.0/6.0))<0.01) frac*=2.3;
            total += frac;
        }
        return total;
    }

    int count_threats(const Board& board, Color us) {
        Color them = (us == Color::WHITE ? Color::BLACK : Color::WHITE);
        int total = 0;
        for (auto pt : { PieceType::PAWN,   PieceType::KNIGHT, PieceType::BISHOP,
                        PieceType::ROOK,   PieceType::QUEEN,  PieceType::KING }) {
            Bitboard bb = board.pieces(pt, us);
            while (bb.count() > 0) {
                Square sq = bb.pop();
                if (board.isAttacked(sq, them)) total += piece_value_attack(pt);
            }
        }

        return total;
    }

    float utility(Board& board, float wheights[]) {
        Movelist moves;
        Movelist fake_moves;
        movegen::legalmoves(moves, board);
        // Fake moves generation
        Board fake_board = board;
        fake_board.makeNullMove();
        movegen::legalmoves(fake_moves, fake_board);
        // Actual calculations (caching shit is done)
        float utility = 0;
        // Check for mate
        {float SCORE;
        if (board.sideToMove() == Color::WHITE)  SCORE = -my_inf;
        else SCORE = my_inf;
        if (board.isHalfMoveDraw()) {
            return (board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE ? SCORE : 0.0f);
        }
        if (board.isRepetition()) {
            return 0.0f;
        }
        if (moves.empty()){
            if (board.inCheck()) {
                return SCORE; 
            }
            else return 0.0f;
        }}
        // Peice value
        float one = 1.0f;
        for (auto peice : { PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING }) {
            int count_w = board.pieces(peice, Color::WHITE).count();
            int count_b = board.pieces(peice, Color::BLACK).count();
            utility += one*(count_w-count_b)*piece_value(peice);
        }
        // No. of legal moves
        float alpha = wheights[0];
        Color stm = board.sideToMove();
        float white_move_count = static_cast<float>(moves.size());
        float black_move_count = static_cast<float>(fake_moves.size());
        if (stm==Color::WHITE) {
            utility += alpha*(white_move_count-black_move_count);
        }
        else {
            utility -= alpha*(white_move_count-black_move_count);
        }
        // King mobility
        float gamma = wheights[1];
        int white_hits = (king_mobility(board,Color::WHITE));
        int black_hits = (king_mobility(fake_board,Color::BLACK));
        utility += gamma*float(white_hits-black_hits);
        // Promotion and promotion potential
        float rho = wheights[2];
        float p_pot_w = promotion_potential(board, Color::WHITE);  
        float p_pot_b = promotion_potential(board, Color::BLACK);
        utility += rho * (p_pot_w - p_pot_b);
        // Peices under attack
        float lambda = wheights[3];
        int th_w = count_threats(board, Color::WHITE);
        int th_b = count_threats(board, Color::BLACK);
        utility -= lambda * float(th_w - th_b);
        // Return utility
        return utility;
    }

    pair<Move, float> alphaBetaHelper(Board& board, float alpha, float beta, int depth, Color player, float wheights[]) {
        if (stopped) return {Move(), 0.0f};
        if (!stopped && high_resolution_clock::now() > deadline) {
            stopped = true;
            return {Move(), 0.0f};
        }

        uint64_t board_hash = board.hash();
        // Get ordered moves (will use cache if available)
        vector<pair<Move, float>> scored;
        if (move_cache.find(board_hash) != move_cache.end() && !move_cache[board_hash].scored_moves.empty()) {
            scored = move_cache[board_hash].scored_moves;
        }
        else {
            CacheEntry& entry = move_cache[board_hash];
            Movelist moves;
            movegen::legalmoves(moves, board);
            entry.scored_moves.reserve(moves.size());
            
            for (auto m : moves) {
                board.makeMove(m);
                float score = utility(board, wheights);
                board.unmakeMove(m);
                entry.scored_moves.emplace_back(m, score);
            }
            
            // Sort based on player
            if (player == Color::WHITE) {
                sort(entry.scored_moves.begin(), entry.scored_moves.end(),
                        [](auto &a, auto &b){ return a.second > b.second; });
            } else {
                sort(entry.scored_moves.begin(), entry.scored_moves.end(),
                        [](auto &a, auto &b){ return a.second < b.second; });
            }
            
            scored = entry.scored_moves;
        }
        
        // Alpha-beta min-max value returning
        Move bestMove;
        if (player == Color::WHITE) {
            float maxEval = -my_inf;
            for (auto &[m, staticscore] : scored) {
                board.makeMove(m);
                float val;
                if (depth == 1) {
                    val = staticscore;
                } else {
                    auto [childMove, child_val] = alphaBetaHelper(board, alpha, beta, depth - 1, Color::BLACK, wheights);
                    val = child_val;
                }
                
                board.unmakeMove(m);
                if (val > maxEval) {
                    maxEval  = val;
                    bestMove = m;
                }
                alpha = max(alpha, val);
                if (beta <= alpha)
                    break;
            }
            return { bestMove, maxEval };
        } else {
            float minEval =  my_inf;
            for (auto &[m, staticscore] : scored) {
                board.makeMove(m);
                float val;
                if (depth == 1) {
                    val = staticscore;
                } else {
                    auto [childMove, child_val] = alphaBetaHelper(board, alpha, beta, depth - 1, Color::WHITE, wheights);
                    val = child_val;
                }
                
                board.unmakeMove(m);
                if (val < minEval) {
                    minEval   = val;
                    bestMove  = m;
                }
                beta = min(beta, val);
                if (beta <= alpha)
                    break;
            }
            return {bestMove, minEval};
        }
    }

    pair<Move,float> alphaBeta(Board& board, float alpha, float beta, int maxDepth, Color player, float wheights[], milliseconds timeLimit) {
        deadline = high_resolution_clock::now() + timeLimit;
        stopped = false;
        lastCompleteDepth = 0;
        lastCompleteMove = Move();

        // Iterative deepen from 1 to maxDepth
        for (int d = 1; d <= maxDepth; ++d) {
            auto [candMove, candScore] = alphaBetaHelper(board, alpha, beta, d, player, wheights);
            if (isinf(candScore)) {
                if (player==Color::WHITE && candScore>0) return {candMove, 0.0f };
                if (player==Color::BLACK && candScore<0) return {candMove, 0.0f };
            }
            if (stopped) break; 
            lastCompleteMove  = candMove;
            lastCompleteDepth = d;
        }

        return { lastCompleteMove, 0.0f };
    }
};

chess::Move parseUciMove(chess::Board& board, const std::string& moveStr) {
    using namespace chess;
    Movelist moves;
    movegen::legalmoves(moves, board);
    for (const auto& move : moves) {
        if (uci::moveToUci(move) == moveStr) {
            return move;
        }
    }
    return Move();
}

int main() {
    Board board;
    string line;
    float weights[4] = WEIGHTS;
    int maxdepth = 100;
    MoveGen my_solver;

    while (getline(cin, line)) {
        istringstream iss(line);
        string token;
        iss >> token;

        if (token == "uci") {
            cout << "id name Matie\n";
            cout << "id author Suhas\n";
            cout << "uciok\n";
        } else if (token == "isready") {
            cout << "readyok\n";
        } else if (token == "ucinewgame") {
            board.setFen(constants::STARTPOS);
            my_solver.move_cache.clear();
        } else if (token == "position") {
            string posType;
            iss >> posType;

            if (posType == "startpos") {
                board.setFen(constants::STARTPOS);
                string movesLabel;
                iss >> movesLabel;
                if (movesLabel == "moves") {
                    string moveStr;
                    while (iss >> moveStr) {
                        Move m = parseUciMove(board, moveStr);
                        if (m != Move()) board.makeMove(m);
                    }
                }
            } else if (posType == "fen") {
                string fen, temp;
                vector<string> fen_parts;
                int i = 0;
                while (i < 6 && iss >> temp) {
                    fen_parts.push_back(temp);
                    i++;
                }
                string fen_full = "";
                for (int j = 0; j < 6; j++) {
                    fen_full += fen_parts[j];
                    if (j < 5) fen_full += " ";
                }
                board.setFen(fen_full);
            }
        } else if (token == "go") {
            // 1) parse all the UCI time args
            int wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 35;
            std::string arg;
            while (iss >> arg) {
                if (arg == "wtime") iss >> wtime;
                else if (arg == "btime") iss >> btime;
                else if (arg == "winc") iss >> winc;
                else if (arg == "binc") iss >> binc;
                else if (arg == "movestogo") iss >> movestogo;
            }
            // 2) decide whose clock and remaining moves
            bool isWhite = (board.sideToMove() == Color::WHITE);
            int timeLeftMs = isWhite ? wtime : btime;
            int  incMs = isWhite ? winc : binc;
            int  movesDone = board.fullMoveNumber();               // approx full moves so far
            int  movesRemain = std::max(1, movestogo - movesDone);   // avoid div0
            // 3) allocate ~90% of timeLeft/movesRemain + increment
            int64_t allocMs = timeLeftMs / movesRemain + incMs;
            allocMs = static_cast<int64_t>(allocMs * 0.9);
            // 4) build a chrono deadline
            auto timelimit = std::chrono::milliseconds(7450);
            // 5) call to alpha-beta
            float alpha = -numeric_limits<float>::infinity();
            float beta = numeric_limits<float>::infinity();
            Color player = board.sideToMove();
            Board copp = board;
            auto best = my_solver.alphaBeta(copp, alpha, beta, maxdepth, player, weights, timelimit).first;

            if (best != Move()) {
                cout << "bestmove " << uci::moveToUci(best) << '\n';
            } else {
                cout << "bestmove 0000\n";
            }
        } else if (token == "quit") {
            break;
        }
    }

    return 0;
}