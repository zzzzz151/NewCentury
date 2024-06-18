// clang-format off

#pragma once

#include "board.hpp"
#include "search_params.hpp"

struct Node {
    public:

    Node *mParent;
    std::vector<Node> mChildren = {};
    std::vector<Move> mMoves = {};
    GameState mGameState;
    u32 mVisits = 0;
    float mResultsSum = 0;
    u16 mDepth;

    inline Node(Board &board, Node *parent, u16 depth) {
        mParent = parent;
        mDepth = depth;

        if (isRoot()) {
            mGameState = GameState::ONGOING;
            board.legalMoves(mMoves, false);
            assert(mMoves.size() > 0);
        }
        else if (board.insufficientMaterial() || board.isRepetition()) 
            mGameState = GameState::DRAW;
        else {
            board.legalMoves(mMoves, false);

            mGameState = mMoves.size() == 0
                         ? (board.inCheck() ? GameState::LOST : GameState::DRAW)
                         : board.fiftyMovesDraw()
                         ? GameState::DRAW
                         : GameState::ONGOING;
        }

        shuffleVector(mMoves);
    }

    inline bool isRoot() { return mParent == nullptr; }

    inline double UCT() {
        assert(mVisits > 0);
        assert(!isRoot() && mParent->mVisits > 0);

        return mResultsSum / (double)mVisits 
               + UCT_C() * sqrt(ln(mParent->mVisits) / (double)mVisits);
    }

    inline Node* select(Board &board) 
    {
        if (mGameState != GameState::ONGOING
        || mChildren.size() != mMoves.size())
            return this;

        assert(mMoves.size() > 0 && mChildren.size() > 0);

        double bestUct = mChildren[0].UCT();
        int bestChildIdx = 0;

        for (u64 i = 1; i < mChildren.size(); i++) 
        {
            double childUct = mChildren[i].UCT();

            if (childUct > bestUct) {
                bestUct = childUct;
                bestChildIdx = i;
            }
        }

        board.makeMove(mMoves[bestChildIdx]);
        return mChildren[bestChildIdx].select(board);
    }

    inline Node* expand(Board &board) {
        assert(mGameState == GameState::ONGOING);
        assert(mMoves.size() > 0);
        assert(mChildren.size() < mMoves.size());

        Move move = mMoves[mChildren.size()];
        board.makeMove(move);

        mChildren.push_back(Node(board, this, mDepth + 1));
        return &mChildren.back();
    }

    inline double simulate(Board &board) {
        if (mGameState != GameState::ONGOING)
            return (double)mGameState;

        constexpr std::array<int, 5> PIECE_VALUES = {100, 300, 315, 500, 900};
        int eval = int(randomU64() % 7) - 3;

        for (int pieceType = PAWN; pieceType <= QUEEN; pieceType++) 
        {
            u64 pieceBb = board.getBitboard((PieceType)pieceType);

            int numPiecesDiff = std::popcount(board.us() & pieceBb) - std::popcount(board.them() & pieceBb);

            eval += PIECE_VALUES[pieceType] * numPiecesDiff;
        }

        double wdl = 1.0 / (1.0 + exp(-eval / EVAL_SCALE())); // [0, 1]
        wdl *= 2; // [0, 2]
        wdl -= 1; // [-1, 1]

        assert(wdl >= -1 && wdl <= 1);
        return wdl;
    }

    inline void backprop(double wdl) {
        assert(!isRoot());
        assert(wdl >= -1 && wdl <= 1);

        Node *current = this;
        while (current != nullptr) 
        {
            current->mVisits++;
            wdl *= -1;
            current->mResultsSum += wdl;
            current = current->mParent;
        }
    }

    inline i16 scoreCp() {
        assert(mVisits > 0);

        double wdl = mResultsSum / (double)mVisits; // [-1, 1]
        assert(wdl >= -1 && wdl <= 1);

        wdl += 1; // [0, 2]
        wdl /= 2; // [0, 1]

        constexpr i32 WIN_SCORE = 30'000;

        if (wdl >= 0.99) return WIN_SCORE;

        if (wdl <= 0.01) return -WIN_SCORE;

        // inverse of sigmoid
        double cpScore = -EVAL_SCALE() * ln((1.0 - wdl) / wdl); 

        return std::clamp((i32)round(cpScore), -WIN_SCORE, WIN_SCORE);
    }

    inline Move mostVisitsMove() 
    {
        assert(mMoves.size() > 0 && mChildren.size() > 0);

        u32 mostVisits = mChildren[0].mVisits;
        Move bestMove = mMoves[0];

        for (u64 i = 1; i < mChildren.size(); i++)
            if (mChildren[i].mVisits > mostVisits)
            {
                mostVisits = mChildren[i].mVisits;
                bestMove = mMoves[i];
            }

        return bestMove;
    }

}; // struct Node

inline void printInfo(int depth, i32 scoreCp, u64 nodes, u64 milliseconds, Move bestMove) 
{
    std::cout << "info"
              << " depth "    << depth
              << " score cp " << scoreCp
              << " nodes "    << nodes
              << " nps "      << nodes * 1000 / std::max<u64>(milliseconds, 1)
              << " time "     << milliseconds
              << " pv "       << bestMove.toUci()
              << std::endl;
}

inline std::tuple<Move, u64> search(const Board &rootBoard, u64 searchTimeMs, u64 maxDepth, u64 maxNodes, bool boolPrintInfo)
{
    std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();

    resetRng();

    Board board = rootBoard;
    Node root = Node(board, nullptr, 0);
    u64 nodes = 0;

    u64 depthSum = 0;
    int lastPrintedDepth = 0;

    // MCTS iteration loop
    do {
        Node* node = root.select(board);

        if (node->mGameState == GameState::ONGOING)
            node = node->expand(board);

        double wdl = node->simulate(board);
        node->backprop(wdl);

        nodes++;
        board <<= rootBoard; // fast copy (board = rootBoard)

        depthSum += (u64)node->mDepth;
        double depthAvg = (double)depthSum / (double)nodes;

        if (depthAvg >= maxDepth) break;

        int depthAvgRounded = round(depthAvg);

        if (boolPrintInfo && depthAvgRounded != lastPrintedDepth) {
            printInfo(depthAvgRounded, root.scoreCp(), nodes, millisecondsElapsed(startTime), root.mostVisitsMove());
            lastPrintedDepth = depthAvgRounded;
        }
    }
    while (nodes < maxNodes && (nodes % 512 != 0 || millisecondsElapsed(startTime) < searchTimeMs));

    if (boolPrintInfo) {
        int depthAvg = round((double)depthSum / (double)nodes);
        printInfo(depthAvg, root.scoreCp(), nodes, millisecondsElapsed(startTime), root.mostVisitsMove());
    }

    return {root.mostVisitsMove(), nodes};
}
