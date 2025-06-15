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

const float my_inf = std::numeric_limits<float>::infinity(); 

// Struct to cache stuff
struct CacheEntry {
    vector<pair<Move, float>> scored_moves;

    CacheEntry() {}
};

// Use board hash as key to caching
unordered_map<uint64_t, CacheEntry> move_cache;

class MoveGen {
public:
    Board board;

    MoveGen(string fen) : board(Board(fen)){}

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

    float utility(Board board, float wheights[]) {
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

    pair<Move, float> alphaBeta(Board& board, float alpha, float beta, int depth, Color player, float wheights[]) {
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
                    auto [childMove, child_val] = alphaBeta(board, alpha, beta, depth - 1, Color::BLACK, wheights);
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
                    auto [childMove, child_val] = alphaBeta(board, alpha, beta, depth - 1, Color::WHITE, wheights);
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
};

int main(int argc, char* argv[]) {
    // Still have to fix the repitition problem
    // It works for whites, what about blacks
    // Review the caching strategy to avoid incorrect cache hits, and see how possible is it to get hash similarity
    // See the tuning.txt to tune parameters
    // See which ones im getting wrong for (Mate in 2,3,4)
    // Can I inc depth as we go down the tree cuz less time
    // Memory exeeding badly (now its not but lru can be good for us)
    // Train on end game data base
    // Compile with `g++ -O3`
    // If else copying issue
    // Do Rohans suggestions

    float a, b, c, d;
    a = stof(argv[1]);
    b = stof(argv[2]);
    c = stof(argv[3]);
    d = stof(argv[4]);
    float wheights[4] = {a, b, c, d};
    int depth = 7;

    // Track time
    chrono::high_resolution_clock::time_point start_time;
    chrono::high_resolution_clock::time_point move_start_time;
    bool timer_started = false;
    double cumulative_time = 0.0;

    // Take as input
    if (argc == 5) {
        string fen;
        getline(cin, fen);
        while  (fen!="0") {
            if (!timer_started) {
                start_time = chrono::high_resolution_clock::now();
                timer_started = true;
            }
            move_start_time = chrono::high_resolution_clock::now();
            float alpha = -my_inf;
            float beta  =  my_inf;
            MoveGen my_solver(fen);
            string activeColor;
            if (my_solver.board.sideToMove()==Color::WHITE) activeColor = "w";
            else if (my_solver.board.sideToMove()==Color::BLACK) activeColor = "b";
            auto my_move_pair = my_solver.alphaBeta(my_solver.board, alpha, beta, depth, ((activeColor=="w")? Color::WHITE : Color::BLACK), wheights);
            auto my_move = my_move_pair.first;
            auto my_score = my_move_pair.second;
            // Time Tracking
            auto move_end_time = chrono::high_resolution_clock::now();
            auto move_duration = chrono::duration_cast<chrono::microseconds>(move_end_time - move_start_time);
            double move_time_ms = move_duration.count() / 1000000.0;
            cumulative_time += move_time_ms;
            // If mate can be forced
            if ((activeColor=="w" && my_score==my_inf) || (activeColor=="b" && my_score==-my_inf)) {
                cout << "He is fucked" << '\n';
            }
            if (my_move==Move() || (my_solver.board.at(my_move.from()) == Piece::NONE)) {
                cout << "You are fucked" << '\n';
            }
            else {
                cout << uci::moveToSan(my_solver.board, my_move) << '\n';
            }
            cout << fixed << setprecision(2) << cumulative_time << " | " << move_time_ms << '\n';
            getline(cin, fen);
        }
        return 0;
    }
    
    // Read from files
    int file_no;
    string filename;
    file_no = stof(argv[5]);
    if (file_no==2) filename = "../Week3/mate_in_2.json";
    if (file_no==3) filename = "../Week3/mate_in_3.json";
    if (file_no==4) filename = "../Week3/mate_in_4.json";

    ifstream file(filename);
    json data = json::parse(file);

    for(auto [fen,soln]:data.items()){
        float alpha = -my_inf;
        float beta  =  my_inf;
        MoveGen my_solver(fen);
        auto my_move = my_solver.alphaBeta(my_solver.board, alpha, beta, depth, my_solver.board.sideToMove(), wheights).first;
        string sub = uci::moveToSan(my_solver.board, my_move);
        if (string(soln).find(sub) == string::npos) {
            cout << fen << '\n';
        }
    }
}