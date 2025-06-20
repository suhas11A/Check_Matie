#include "/home/suhas/libraries/chess-library/include/chess.hpp"
#include "/home/suhas/libraries/json/single_include/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
using namespace chess;
using namespace std;
using json = nlohmann::json;

class MoveGen {
public:
    Board board;
    Move opp_move;
    int depth;

    MoveGen(string fen, string san, int depth) : board(Board(fen)),  depth(depth){}

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

    float promotion_potential(const Board &boar_d, Color us) {
        float total = 0.0f;
        Bitboard pawns = boar_d.pieces(PieceType::PAWN, us);
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

    float utility(Board board, float wheights[]) {
        float utility = 0;
        // Check for mate
        {float SCORE;
        if (board.sideToMove() == Color::WHITE)  SCORE = -numeric_limits<float>::infinity();
        else SCORE = numeric_limits<float>::infinity();
        if (board.isHalfMoveDraw()) {
            return (board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE ? SCORE : 0.0f);
        }
        if (board.isRepetition()) {
            return 0.0f;
        }
        Movelist moves;
        movegen::legalmoves(moves, board);
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
        // Copy of the board
        Board boar_d = board;
        // No. of legal moves
        {float alpha = wheights[0];
        Color stm = boar_d.sideToMove();
        Movelist moves;
        movegen::legalmoves(moves, boar_d);
        float white_move_count = static_cast<float>(moves.size());
        moves.clear();
        Board boar_dd = boar_d;
        boar_dd.makeNullMove();
        movegen::legalmoves(moves, boar_dd);
        float black_move_count = static_cast<float>(moves.size());
        moves.clear();
        if (stm==Color::WHITE) {
            utility += alpha*(white_move_count-black_move_count);
        }
        else {
            utility -= alpha*(white_move_count-black_move_count);
        }}
        // King mobility
        {float gamma = wheights[1];
        int white_hits = (king_mobility(boar_d,Color::WHITE));
        Board boar_dd = boar_d;
        boar_dd.makeNullMove();
        int black_hits = (king_mobility(boar_dd,Color::BLACK));
        utility += gamma*float(white_hits-black_hits);}
        // Promotion and promotion potential
        float rho = wheights[2];
        float p_pot_w = promotion_potential(boar_d, Color::WHITE);  
        float p_pot_b = promotion_potential(boar_d, Color::BLACK);
        utility +=rho * (p_pot_w - p_pot_b);
        // Peices under attack
        float lambda = wheights[3];
        int th_w = count_threats(boar_d, Color::WHITE);
        int th_b = count_threats(boar_d, Color::BLACK);
        utility -= lambda * float(th_w - th_b);

        return utility;
    }

    pair<Move, float> alphaBeta(Board& board, float alpha, float beta, int depth, Color player, float wheights[]) {
        // If depth = 0
        if (depth == 0) {
            return { Move(), utility(board, wheights) };
        }
        // Generate all moves
        Movelist moves;
        movegen::legalmoves(moves, board);
        // Order all moves
        vector<pair<Move, float>> scored;
        scored.reserve(moves.size());
        for (auto m : moves) {
            board.makeMove(m);
            float score = utility(board, wheights);
            board.unmakeMove(m);
            scored.emplace_back(m, score);
        }
        // Sort descending if White to move (maximize), ascending if Black (minimize)
        if (player == Color::WHITE) {
            sort(scored.begin(), scored.end(),
                    [](auto &a, auto &b){ return a.second > b.second; });
        } else {
            sort(scored.begin(), scored.end(),
                    [](auto &a, auto &b){ return a.second < b.second; });
        }
        // Aplha-beta min-max value returning
        Move bestMove;
        if (player == Color::WHITE) {
            float maxEval = -numeric_limits<float>::infinity();
            for (auto &[m, staticscore] : scored) {
                board.makeMove(m);
                auto [childMove, val] = alphaBeta(board, alpha, beta, depth - 1, Color::BLACK, wheights);
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
            float minEval =  numeric_limits<float>::infinity();
            for (auto &[m, staticscore] : scored) {
                board.makeMove(m);
                auto [childMove, val] = alphaBeta(board, alpha, beta, depth - 1, Color::WHITE, wheights);
                board.unmakeMove(m);
                if (val < minEval) {
                    minEval   = val;
                    bestMove  = m;
                }
                beta = min(beta, val);
                if (beta <= alpha)
                    break;
            }
            return { bestMove, minEval };
        }
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
    return Move(); // Invalid move
}

int main() {
    Board board;
    string line;
    float weights[4] = {0.05, 0.05, 0.1, 0.1};
    int depth = 5;

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
            float alpha = -numeric_limits<float>::infinity();
            float beta = numeric_limits<float>::infinity();

            // Replace with how your MoveGen expects the board:
            // If it expects FEN:
            MoveGen my_solver(board.getFen(), "", depth);

            Color player = board.sideToMove();
            auto best = my_solver.alphaBeta(board, alpha, beta, depth, player, weights).first;

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