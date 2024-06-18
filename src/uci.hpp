// clang-format-off

#pragma once

#include "board.hpp"
#include "perft.hpp"
#include "search.hpp"
#include "bench.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(std::vector<std::string> &tokens);
inline void position(std::vector<std::string> &tokens, Board &board);
inline void go(std::vector<std::string> &tokens, Board &board);

inline void uciLoop()
{
    Board board = Board(START_FEN);

    while (true) {
        std::string received = "";
        getline(std::cin, received);
        trim(received);
        std::vector<std::string> tokens = splitString(received, ' ');

        if (received == "" || tokens.size() == 0)
            continue;

        try {

        if (received == "quit" || !std::cin.good())
            break;
        else if (received == "uci")
            uci();
        else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
            setoption(tokens);
        else if (received == "ucinewgame")
            board = Board(START_FEN);
        else if (received == "isready")
            std::cout << "readyok" << std::endl;
        else if (tokens[0] == "position")
            position(tokens, board);
        else if (tokens[0] == "go")
            go(tokens, board);
        else if (tokens[0] == "print" || tokens[0] == "d"
        || tokens[0] == "display" || tokens[0] == "show")
            board.print();
        else if (tokens[0] == "bench")
        {
            if (tokens.size() == 1)
                bench();
            else {
                int depth = stoi(tokens[1]);
                bench(depth);
            }
        }
        else if (tokens[0] == "perft" || (tokens[0] == "go" && tokens[1] == "perft"))
        {
            int depth = stoi(tokens.back());
            perftBench(board, depth);
        }
        else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
        || tokens[0] == "perftdivide" || tokens[0] == "divideperft")
        {
            int depth = stoi(tokens[1]);
            perftSplit(board, depth);
        }
        else if (tokens[0] == "makemove")
            board.makeMove(tokens[1]);

        } 
        catch (const char* errorMessage)
        {

        }
    }
}

inline void uci() {
    std::cout << "id name New Century" << std::endl;
    std::cout << "id author zzzzz" << std::endl;

    /*
    for (auto [paramName, tunableParam] : tunableParams) {
        std::cout << "option name " << paramName;

        std::visit([&] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            if (std::is_same<decltype(myParam->value), double>::value
            || std::is_same<decltype(myParam->value), float>::value)
            {
                std::cout << " type spin default " << round(myParam->value * 100.0)
                          << " min " << round(myParam->min * 100.0)
                          << " max " << round(myParam->max * 100.0);
            }
            else {
                std::cout << " type spin default " << myParam->value
                          << " min " << myParam->min
                          << " max " << myParam->max;
            }
        }, tunableParam);

        std::cout << std::endl;
    }
    */

    std::cout << "uciok" << std::endl;
}

inline void setoption(std::vector<std::string> &tokens)
{
    std::string optionName = tokens[2];
    trim(optionName);
    std::string optionValue = tokens[4];
    trim(optionValue);

    if (tunableParams.count(optionName) > 0) 
    {
        auto tunableParam = tunableParams[optionName];
        i64 newValue = stoll(optionValue);

        std::visit([optionName, newValue] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            myParam->value = std::is_same<decltype(myParam->value), double>::value 
                             || std::is_same<decltype(myParam->value), float>::value 
                             ? (double)newValue / 100.0 
                             : newValue;

            std::cout << optionName << " set to " << myParam->value << std::endl;
        }, tunableParam);
    }
}

inline void position(std::vector<std::string> &tokens, Board &board)
{
    int movesTokenIndex = -1;

    if (tokens[1] == "startpos") {
        board = Board(START_FEN);
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        u64 i = 0;
        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";
        fen.pop_back(); // remove last whitespace
        board = Board(fen);
        movesTokenIndex = i;
    }

    for (u64 i = movesTokenIndex + 1; i < tokens.size(); i++)
        board.makeMove(tokens[i]);
}

inline void go(std::vector<std::string> &tokens, Board &board)
{
    i64 milliseconds = I64_MAX;
    u64 maxDepth = I64_MAX;
    u64 maxNodes = I64_MAX;
    bool isMoveTime = false;

    for (int i = 1; i < (int)tokens.size() - 1; i += 2)
    {
        i64 value = std::stoll(tokens[i + 1]);

        if ((tokens[i] == "wtime" && board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && board.sideToMove() == Color::BLACK))
            milliseconds = std::max(value, (i64)0);

        else if (tokens[i] == "movetime")
            isMoveTime = true;
        else if (tokens[i] == "depth")
            maxDepth = std::max(value, (i64)1);
        else if (tokens[i] == "nodes")
            maxNodes = std::max(value, (i64)0);
    }

    u64 maxSearchTimeMs = std::max((i64)0, milliseconds - 10);

    u64 searchTimeMs = milliseconds == I64_MAX 
                       ? I64_MAX
                       : isMoveTime 
                       ? maxSearchTimeMs
                       : maxSearchTimeMs / 25.0;

    auto [bestMove, nodes] = search(board, searchTimeMs, maxDepth, maxNodes, true);

    std::cout << "bestmove " << bestMove.toUci() << std::endl;
}

} // namespace uci


