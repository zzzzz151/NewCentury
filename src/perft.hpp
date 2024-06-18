// clang-format off

#pragma once

#include "board.hpp"

inline u64 perft(Board board, int depth)
{
    if (depth <= 0) return 1;

    std::vector<Move> moves;
    board.legalMoves(moves);

    if (depth == 1) return moves.size();

    u64 nodes = 0;

    for (Move move : moves) 
    {
        Board copy = board;
        copy.makeMove(move);
        nodes += perft(copy, depth - 1);
    }

    return nodes;
}

inline void perftSplit(Board board, int depth)
{
    if (depth <= 0) return;

    std::cout << "Running split perft depth " << depth << " on " << board.fen() << std::endl;

    std::vector<Move> moves;
    board.legalMoves(moves);

    if (depth == 1) {
        for (Move move : moves) 
            std::cout << move.toUci() << ": 1" << std::endl;   

        std::cout << "Total: " << moves.size() << std::endl;
        return;
    }

    u64 totalNodes = 0;

    for (Move move : moves) 
    {
        Board copy = board;
        copy.makeMove(move);
        u64 nodes = perft(copy, depth - 1);
        std::cout << move.toUci() << ": " << nodes << std::endl;
        totalNodes += nodes;
    }

    std::cout << "Total: " << totalNodes << std::endl;
}

inline u64 perftBench(Board board, int depth)
{
    depth = std::max(depth, 0);
    std::string fen = board.fen();

    std::cout << "Running perft depth " << depth << " on " << fen << std::endl;

    std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
    u64 nodes = perft(board, depth);

    std::cout << "perft"
              << " depth " << depth 
              << " nodes " << nodes 
              << " nps "   << nodes * 1000 / std::max((u64)millisecondsElapsed(start), (u64)1)
              << " time "  << millisecondsElapsed(start)
              << " fen "   << fen
              << std::endl;

    return nodes;
}
