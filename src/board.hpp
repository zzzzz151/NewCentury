// clang-format off

#pragma once

#include <random>
#include "types.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"

u64 ZOBRIST_COLOR = 0;
MultiArray<u64, 2, 6, 64> ZOBRIST_PIECES = {}; // [color][pieceType][square]
std::array<u64, 8> ZOBRIST_FILES = {}; // [file]

inline void initZobrist()
{
    std::mt19937_64 gen(12345); // 64 bit Mersenne Twister rng with seed 12345
    std::uniform_int_distribution<u64> distribution; // distribution(gen) returns random u64

    ZOBRIST_COLOR = distribution(gen);

    for (int pt = 0; pt < 6; pt++)
        for (int sq = 0; sq < 64; sq++)
        {
            ZOBRIST_PIECES[WHITE][pt][sq] = distribution(gen);
            ZOBRIST_PIECES[BLACK][pt][sq] = distribution(gen);
        }

    for (int file = 0; file < 8; file++)
        ZOBRIST_FILES[file] = distribution(gen);
}

constexpr int CASTLE_SHORT = 0, CASTLE_LONG = 1;

class Board {
    private:

    Color mColorToMove = Color::NONE;

    std::array<u64, 2> mColorBitboards = { };   // [color]
    MultiArray<u64, 6> mPiecesBitboards = { }; // [pieceType]

    u64 mCastlingRights = 0;

    Square mEnPassantSquare = SQUARE_NONE;

    u8 mPliesSincePawnOrCapture = 0;
    u16 mCurrentMoveCounter = 1;

    u64 mZobristHash = 0;
    std::vector<u64> mZobristHashes = {};

    Move mLastMove = MOVE_NONE;
    PieceType mCaptured = PieceType::NONE;

    public:

    inline Board() = default;

    // overload <<= operator to be a fast copy (this = other)
    inline void operator<<=(const Board &other)
    {
        mColorToMove = other.mColorToMove;
        mColorBitboards = other.mColorBitboards;
        mPiecesBitboards = other.mPiecesBitboards;
        mCastlingRights = other.mCastlingRights;
        mEnPassantSquare = other.mEnPassantSquare;
        mPliesSincePawnOrCapture = other.mPliesSincePawnOrCapture;
        mCurrentMoveCounter = other.mCurrentMoveCounter;
        mZobristHash = other.mZobristHash;
        mLastMove = other.mLastMove;
        mCaptured = other.mCaptured;

        assert(mZobristHashes.size() >= other.mZobristHashes.size());
        mZobristHashes.resize(other.mZobristHashes.size());
        assert(mZobristHashes == other.mZobristHashes);
    }

    inline Board(std::string fen)
    {
        mZobristHashes.reserve(512);

        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move
        mColorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;

        if (mColorToMove == Color::BLACK)
            mZobristHash ^= ZOBRIST_COLOR;

        // Parse pieces

        mColorBitboards = {};
        mPiecesBitboards = {};

        std::string fenRows = fenSplit[0];
        int currentRank = 7, currentFile = 0; // iterate ranks from top to bottom, files from left to right

        for (u64 i = 0; i < fenRows.length(); i++)
        {
            char thisChar = fenRows[i];
            if (thisChar == '/')
            {
                currentRank--;
                currentFile = 0;
            }
            else if (isdigit(thisChar))
                currentFile += charToInt(thisChar);
            else
            {
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                PieceType pt = CHAR_TO_PIECE_TYPE[thisChar];
                Square sq = currentRank * 8 + currentFile;
                placePiece(color, pt, sq);
                currentFile++;
            }
        }

        // Parse castling rights
        mCastlingRights = 0;
        std::string fenCastlingRights = fenSplit[2];
        if (fenCastlingRights != "-") 
        {
            for (u64 i = 0; i < fenCastlingRights.length(); i++)
            {
                char thisChar = fenCastlingRights[i];
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;

                int castlingRight = thisChar == 'K' || thisChar == 'k' 
                                    ? CASTLE_SHORT : CASTLE_LONG;

                mCastlingRights |= CASTLING_MASKS[(int)color][castlingRight];
            }

            mZobristHash ^= mCastlingRights;
        }

        // Parse en passant target square
        mEnPassantSquare = SQUARE_NONE;
        std::string strEnPassantSquare = fenSplit[3];
        if (strEnPassantSquare != "-")
        {
            mEnPassantSquare = strToSquare(strEnPassantSquare);
            int file = (int)squareFile(mEnPassantSquare);
            mZobristHash ^= ZOBRIST_FILES[file];
        }

        // Parse last 2 fen tokens
        mPliesSincePawnOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        mCurrentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

    inline Color sideToMove() { return mColorToMove; }

    inline u64 getBitboard(PieceType pt) { return mPiecesBitboards[(int)pt]; }

    inline u64 getBitboard(Color color) { return mColorBitboards[(int)color]; }

    inline u64 us() { return mColorBitboards[(int)mColorToMove]; }
    
    inline u64 them() { return mColorBitboards[(int)oppColor(mColorToMove)]; }

    inline u64 occupancy() { return us() | them(); }

    inline bool isOccupied(Square square) {
        return occupancy() & (1ULL << square);
    }

    inline u64 zobristHash() { return mZobristHash; }

    inline PieceType pieceTypeAt(Square square) 
    { 
        if (!isOccupied(square)) return PieceType::NONE;

        u64 sqBitboard = 1ULL << square;
        for (u64 i = 0; i < mPiecesBitboards.size(); i++)
            if (sqBitboard & mPiecesBitboards[i])
                return (PieceType)i;

        return PieceType::NONE;
    }

    private:

    inline void placePiece(Color color, PieceType pieceType, Square square) 
    {
        assert(!isOccupied(square));

        mColorBitboards[(int)color] |= 1ULL << square;
        mPiecesBitboards[(int)pieceType] |=  1ULL << square;

        mZobristHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];
    }

    inline void removePiece(Color color, PieceType pieceType, Square square) 
    {
        assert(mColorBitboards[(int)color] & (1ULL << square));
        assert(mPiecesBitboards[(int)pieceType] & (1ULL << square));

        mColorBitboards[(int)color] ^= 1ULL << square;
        mPiecesBitboards[(int)pieceType] ^= 1ULL << square;

        mZobristHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];
    }

    public:

    inline std::string fen() {
        std::string myFen = "";

        for (int rank = 7; rank >= 0; rank--)
        {
            int emptySoFar = 0;
            for (int file = 0; file < 8; file++)
            {
                Square square = rank * 8 + file;
                PieceType pt = pieceTypeAt(square);
                
                if (pt == PieceType::NONE)
                {
                    emptySoFar++;
                    continue;
                }

                if (emptySoFar > 0) 
                    myFen += std::to_string(emptySoFar);

                Color pieceColor = mColorBitboards[WHITE] & (1ULL << square) 
                                   ? Color::WHITE : Color::BLACK;

                Piece piece = makePiece(pt, pieceColor);

                myFen += std::string(1, PIECE_TO_CHAR[piece]);
                emptySoFar = 0;
            }
            if (emptySoFar > 0) 
                myFen += std::to_string(emptySoFar);
            myFen += "/";
        }

        myFen.pop_back(); // remove last '/'

        myFen += mColorToMove == Color::BLACK ? " b " : " w ";

        std::string strCastlingRights = "";
        if (mCastlingRights & CASTLING_MASKS[WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (mCastlingRights & CASTLING_MASKS[WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (mCastlingRights & CASTLING_MASKS[BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (mCastlingRights & CASTLING_MASKS[BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        myFen += " ";
        myFen += mEnPassantSquare == SQUARE_NONE 
                 ? "-" : SQUARE_TO_STR[mEnPassantSquare];
        
        myFen += " " + std::to_string(mPliesSincePawnOrCapture);
        myFen += " " + std::to_string(mCurrentMoveCounter);

        return myFen;
    }

    inline void print() { 
        std::string str = "";

        for (int i = 7; i >= 0; i--) {
            for (int j = 0; j < 8; j++) 
            {
                int square = i * 8 + j;

                if (!isOccupied(square))
                    str += ".";
                else {
                    PieceType pt = pieceTypeAt(square);

                    Color pieceColor = mColorBitboards[WHITE] & (1ULL << square) 
                                       ? Color::WHITE : Color::BLACK;

                    Piece piece = makePiece(pt, pieceColor);

                    str += std::string(1, PIECE_TO_CHAR[piece]);
                }

                str += " ";
            }
            str += "\n";
        }

        std::cout << str << std::endl;
        std::cout << fen() << std::endl;
        std::cout << "Zobrist hash: " << mZobristHash << std::endl;

        if (mLastMove != MOVE_NONE)
            std::cout << "Last move: " << mLastMove.toUci() << std::endl;
    }

    inline bool fiftyMovesDraw() {
        return mPliesSincePawnOrCapture >= 100;
    }

    inline bool insufficientMaterial()
    {
        int numPieces = std::popcount(occupancy());
        if (numPieces == 2) return true;

        // KB vs K
        // KN vs K
        return numPieces == 3 && (mPiecesBitboards[KNIGHT] > 0 || mPiecesBitboards[BISHOP] > 0);
    }

    inline bool isRepetition() {
        if (mZobristHashes.size() < 4 || mPliesSincePawnOrCapture < 4)
            return false;

        int idxAfterPawnOrCapture = std::max(0, (int)mZobristHashes.size() - (int)mPliesSincePawnOrCapture);

        for (int i = (int)mZobristHashes.size() - 2; i >= idxAfterPawnOrCapture; i -= 2)
            if (mZobristHash == mZobristHashes[i])
                return true;

        return false;
    }

    inline bool isSquareAttacked(Square square, Color colorAttacking)
    {
        u64 colorBb = mColorBitboards[(int)colorAttacking];

        if ((colorBb & mPiecesBitboards[PAWN])
        & attacks::pawnAttacks(square, oppColor(colorAttacking)))
            return true;

        if ((colorBb & mPiecesBitboards[KNIGHT])
        & attacks::knightAttacks(square))
            return true;

        u64 bishopsQueens = colorBb & mPiecesBitboards[BISHOP];
        bishopsQueens |= colorBb & mPiecesBitboards[QUEEN];

        if (bishopsQueens & attacks::bishopAttacks(square, occupancy()))
            return true;
 
        u64 rooksQueens = colorBb & mPiecesBitboards[ROOK];
        rooksQueens |= colorBb & mPiecesBitboards[QUEEN];

        if (rooksQueens & attacks::rookAttacks(square, occupancy()))
            return true;

        return (colorBb & mPiecesBitboards[KING]) & attacks::kingAttacks(square);
    }  

    inline u64 attackers(Square sq, Color colorAttacking) 
    {
        u64 attackers = mPiecesBitboards[PAWN] & attacks::pawnAttacks(sq, oppColor(colorAttacking));
        attackers |= mPiecesBitboards[KNIGHT] & attacks::knightAttacks(sq);
        attackers |= mPiecesBitboards[KING] & attacks::kingAttacks(sq);

        u64 bishopsQueens = mPiecesBitboards[BISHOP] | mPiecesBitboards[QUEEN];
        attackers |= bishopsQueens & attacks::bishopAttacks(sq, occupancy());

        u64 rooksQueens = mPiecesBitboards[ROOK] | mPiecesBitboards[QUEEN];
        attackers |= rooksQueens & attacks::rookAttacks(sq, occupancy());

        return attackers & mColorBitboards[(int)colorAttacking];
    }

    inline u64 attacks(Color color, u64 occ = 0)
    {
        if (occ == 0) occ = occupancy();

        u64 attacksBb = 0;
        u64 colorBb = mColorBitboards[(int)color];

        u64 pawns = colorBb & mPiecesBitboards[PAWN];
        while (pawns) {
            Square sq = poplsb(pawns);
            attacksBb |= attacks::pawnAttacks(sq, color);
        }

        u64 knights = colorBb & mPiecesBitboards[KNIGHT];
        while (knights) {
            Square sq = poplsb(knights);
            attacksBb |= attacks::knightAttacks(sq);
        }

        u64 bishopsQueens = colorBb & mPiecesBitboards[BISHOP];
        bishopsQueens |= colorBb & mPiecesBitboards[QUEEN];

        while (bishopsQueens) {
            Square sq = poplsb(bishopsQueens);
            attacksBb |= attacks::bishopAttacks(sq, occ);
        }

        u64 rooksQueens = colorBb & mPiecesBitboards[ROOK];
        rooksQueens |= colorBb & mPiecesBitboards[QUEEN];

        while (rooksQueens) {
            Square sq = poplsb(rooksQueens);
            attacksBb |= attacks::rookAttacks(sq, occ);
        }

        Square kingSquare = lsb(colorBb & mPiecesBitboards[KING]);
        attacksBb |= attacks::kingAttacks(kingSquare);

        return attacksBb;
    } 

    inline bool inCheck() {
        Square kingSquare = lsb(us() & mPiecesBitboards[KING]);
        return isSquareAttacked(kingSquare, oppColor(mColorToMove));
    }

    inline u64 checkers() {
        Square kingSquare = lsb(us() & mPiecesBitboards[KING]);
        return attackers(kingSquare, oppColor(mColorToMove));
    }

    inline std::pair<u64, u64> pinned()
    {
        Square kingSquare = lsb(us() & mPiecesBitboards[KING]);

        u64 pinnedNonDiagonal = 0;
        u64 pinnersNonDiagonal = (mPiecesBitboards[ROOK] | mPiecesBitboards[QUEEN])
                                 & attacks::xrayRook(kingSquare, occupancy(), us()) 
                                 & them();

        
        while (pinnersNonDiagonal) {
            u8 pinnerSquare = poplsb(pinnersNonDiagonal);
            pinnedNonDiagonal |= BETWEEN[pinnerSquare][kingSquare] & us();
        }

        u64 pinnedDiagonal = 0;
        u64 pinnersDiagonal = (mPiecesBitboards[BISHOP] | mPiecesBitboards[QUEEN])
                              & attacks::xrayBishop(kingSquare, occupancy(), us()) 
                              & them();

        while (pinnersDiagonal) {
            Square pinnerSquare = poplsb(pinnersDiagonal);
            pinnedDiagonal |= BETWEEN[pinnerSquare][kingSquare] & us();
        }

        return { pinnedNonDiagonal, pinnedDiagonal };
    }

    inline Move uciToMove(std::string uciMove)
    {
        Square from = strToSquare(uciMove.substr(0,2));
        Square to = strToSquare(uciMove.substr(2,4));
        PieceType pieceType = pieceTypeAt(from);
        u16 moveFlag = (u16)pieceType + 1;

        if (uciMove.size() == 5) // promotion
        {
            char promotionLowerCase = uciMove.back(); // last char of string
            if (promotionLowerCase == 'n') 
                moveFlag = Move::KNIGHT_PROMOTION_FLAG;
            else if (promotionLowerCase == 'b') 
                moveFlag = Move::BISHOP_PROMOTION_FLAG;
            else if (promotionLowerCase == 'r') 
                moveFlag = Move::ROOK_PROMOTION_FLAG;
            else
                moveFlag = Move::QUEEN_PROMOTION_FLAG;
        }
        else if (pieceType == PieceType::KING)
        {
            if (abs((int)to - (int)from) == 2)
                moveFlag = Move::CASTLING_FLAG;
        }
        else if (pieceType == PieceType::PAWN)
        { 
            int bitboardSquaresTraveled = abs((int)to - (int)from);
            if (bitboardSquaresTraveled == 16)
                moveFlag = Move::PAWN_TWO_UP_FLAG;
            else if (bitboardSquaresTraveled != 8 && !isOccupied(to))
                moveFlag = Move::EN_PASSANT_FLAG;
        }

        return Move(from, to, moveFlag);
    }

    inline void makeMove(std::string uciMove) {
        makeMove(uciToMove(uciMove));
    }

    inline void makeMove(Move move)
    {
        mZobristHashes.push_back(mZobristHash);

        Color oppSide = oppColor(mColorToMove);
        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.flag();
        PieceType promotion = move.promotion();
        PieceType pieceType = move.pieceType();
        Square capturedPieceSquare = to;

        //std::cout << fen() << " " << move.toUci() << std::endl;
        removePiece(mColorToMove, pieceType, from);
        //std::cout << "xd" << std::endl;

        if (moveFlag == Move::CASTLING_FLAG)
        {
            placePiece(mColorToMove, PieceType::KING, to);
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
            removePiece(mColorToMove, PieceType::ROOK, rookFrom);
            placePiece(mColorToMove, PieceType::ROOK, rookTo);
            mCaptured = PieceType::NONE;
        }
        else if (moveFlag == Move::EN_PASSANT_FLAG)
        {
            capturedPieceSquare = mColorToMove == Color::WHITE ? to - 8 : to + 8;
            removePiece(oppSide, PieceType::PAWN, capturedPieceSquare);
            placePiece(mColorToMove, PieceType::PAWN, to);
            mCaptured = PieceType::PAWN;
        }
        else {
            mCaptured = pieceTypeAt(to);

            if (mCaptured != PieceType::NONE)
                removePiece(oppSide, mCaptured, to);

            placePiece(mColorToMove, 
                       promotion != PieceType::NONE ? promotion : pieceType, 
                       to);
        }

        mZobristHash ^= mCastlingRights; // XOR old castling rights out

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            mCastlingRights &= ~CASTLING_MASKS[(int)mColorToMove][CASTLE_SHORT]; 
            mCastlingRights &= ~CASTLING_MASKS[(int)mColorToMove][CASTLE_LONG]; 
        }
        else if ((1ULL << from) & mCastlingRights)
            mCastlingRights &= ~(1ULL << from);
        if ((1ULL << to) & mCastlingRights)
            mCastlingRights &= ~(1ULL << to); 

        mZobristHash ^= mCastlingRights; // XOR new castling rights in

        // Update en passant square
        if (mEnPassantSquare != SQUARE_NONE)
        {
            mZobristHash ^= ZOBRIST_FILES[(int)squareFile(mEnPassantSquare)];
            mEnPassantSquare = SQUARE_NONE;
        }
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            mEnPassantSquare = mColorToMove == Color::WHITE ? to - 8 : to + 8;
            mZobristHash ^= ZOBRIST_FILES[(int)squareFile(mEnPassantSquare)];
        }

        mColorToMove = oppSide;
        mZobristHash ^= ZOBRIST_COLOR;

        if (pieceType == PieceType::PAWN || mCaptured != PieceType::NONE)
            mPliesSincePawnOrCapture = 0;
        else
            mPliesSincePawnOrCapture++;

        if (mColorToMove == Color::WHITE)
            mCurrentMoveCounter++;

        mLastMove = move;
    }

    inline void legalMoves(std::vector<Move> &moves, bool underpromotions = true)
    {
        moves = {};
        moves.reserve(32);
        
        Color enemyColor = oppColor(mColorToMove);
        u64 occ = occupancy();
        Square kingSquare = lsb(us() & mPiecesBitboards[KING]);
        u64 theirAttacks = attacks(enemyColor, occ ^ (1ULL << kingSquare));

        // King moves

        u64 targetSquares = attacks::kingAttacks(kingSquare) & ~us() & ~theirAttacks;

        while (targetSquares) {
            Square targetSquare = poplsb(targetSquares);
            moves.push_back(Move(kingSquare, targetSquare, Move::KING_FLAG));
        }

        u64 checkers = this->checkers();
        int numCheckers = std::popcount(checkers);
        assert(numCheckers <= 2);

        // If in double check, only king moves are allowed
        if (numCheckers > 1) {
            moves.shrink_to_fit();
            return;
        }

        u64 movableBb = ONES;
        
        if (numCheckers == 1) {
            movableBb = checkers;

            if (checkers & (mPiecesBitboards[BISHOP] | mPiecesBitboards[ROOK] | mPiecesBitboards[QUEEN]))
            {
                Square checkerSquare = lsb(checkers);
                movableBb |= BETWEEN[kingSquare][checkerSquare];
            }
        }

        // Castling
        if (numCheckers == 0)
        {
            if (mCastlingRights & CASTLING_MASKS[(int)mColorToMove][CASTLE_SHORT]) 
            {
                u64 throughSquares = squareToBitboard(kingSquare + 1) | squareToBitboard(kingSquare + 2);

                if ((occ & throughSquares) == 0 && (theirAttacks & throughSquares) == 0)
                    moves.push_back(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));
            }

            if (mCastlingRights & CASTLING_MASKS[(int)mColorToMove][CASTLE_LONG]) 
            {
                u64 throughSquares = squareToBitboard(kingSquare - 1) 
                                   | squareToBitboard(kingSquare - 2) 
                                   | squareToBitboard(kingSquare - 3);

                if ((occ & throughSquares) == 0 
                && (theirAttacks & (throughSquares ^ squareToBitboard(kingSquare - 3))) == 0)
                    moves.push_back(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
            }
        }

        // Other pieces moves (not king)

        auto [pinnedNonDiagonal, pinnedDiagonal] = pinned();

        u64 ourPawns   = us() & mPiecesBitboards[PAWN],
            ourKnights = (us() & mPiecesBitboards[KNIGHT]) & ~pinnedDiagonal & ~pinnedNonDiagonal,
            ourBishops = (us() & mPiecesBitboards[BISHOP]) & ~pinnedNonDiagonal,
            ourRooks   = (us() & mPiecesBitboards[ROOK]) & ~pinnedDiagonal,
            ourQueens  = us() & mPiecesBitboards[QUEEN];

        // En passant
        if (mEnPassantSquare != SQUARE_NONE)
        {
            u64 ourNearbyPawns = ourPawns & attacks::pawnAttacks(mEnPassantSquare, enemyColor);
            while (ourNearbyPawns) {
                Square ourPawnSquare = poplsb(ourNearbyPawns);
                auto colorBitboards = mColorBitboards;
                auto pawnBb = mPiecesBitboards[PAWN];
                auto zobristHash = mZobristHash;

                // Make the en passant move

                removePiece(mColorToMove, PieceType::PAWN, ourPawnSquare);
                placePiece(mColorToMove, PieceType::PAWN, mEnPassantSquare);

                Square capturedPawnSquare = mColorToMove == Color::WHITE
                                            ? mEnPassantSquare - 8 : mEnPassantSquare + 8;

                removePiece(enemyColor, PieceType::PAWN, capturedPawnSquare);

                if (!inCheck())
                    // en passant is legal
                    moves.push_back(Move(ourPawnSquare, mEnPassantSquare, Move::EN_PASSANT_FLAG));

                // Undo the en passant move
                mColorBitboards = colorBitboards;
                mPiecesBitboards[PAWN] = pawnBb;
                mZobristHash = zobristHash;
            }
        }

        while (ourPawns > 0)
        {
            Square sq = poplsb(ourPawns);
            u64 sqBb = 1ULL << sq;
            bool pawnHasntMoved = false, willPromote = false;
            Rank rank = squareRank(sq);

            if (rank == Rank::RANK_2) {
                pawnHasntMoved = mColorToMove == Color::WHITE;
                willPromote = mColorToMove == Color::BLACK;
            } else if (rank == Rank::RANK_7) {
                pawnHasntMoved = mColorToMove == Color::BLACK;
                willPromote = mColorToMove == Color::WHITE;
            }

            // Generate this pawn's captures 

            u64 pawnAttacks = attacks::pawnAttacks(sq, mColorToMove) & them() & movableBb;

            if (sqBb & (pinnedDiagonal | pinnedNonDiagonal)) 
                pawnAttacks &= LINE_THROUGH[kingSquare][sq];

            while (pawnAttacks > 0) {
                Square targetSquare = poplsb(pawnAttacks);

                if (willPromote) 
                    addPromotions(moves, sq, targetSquare, underpromotions);
                else 
                    moves.push_back(Move(sq, targetSquare, Move::PAWN_FLAG));
            }

            // Generate this pawn's pushes 

            if (sqBb & pinnedDiagonal) continue;

            u64 pinRay = LINE_THROUGH[sq][kingSquare];
            bool pinnedHorizontally = (sqBb & pinnedNonDiagonal) > 0 && (pinRay & (pinRay << 1)) > 0;

            if (pinnedHorizontally) continue;

            Square squareOneUp = mColorToMove == Color::WHITE ? sq + 8 : sq - 8;
            if (isOccupied(squareOneUp)) continue;

            if (movableBb & (1ULL << squareOneUp))
            {
                if (willPromote) {
                    addPromotions(moves, sq, squareOneUp, underpromotions);
                    continue;
                }

                moves.push_back(Move(sq, squareOneUp, Move::PAWN_FLAG));
            }

            if (!pawnHasntMoved) continue;

            Square squareTwoUp = mColorToMove == Color::WHITE 
                                 ? sq + 16 : sq - 16;

            if ((movableBb & (1ULL << squareTwoUp)) && !isOccupied(squareTwoUp))
                moves.push_back(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
        }

        while (ourKnights > 0) {
            Square sq = poplsb(ourKnights);
            u64 knightMoves = attacks::knightAttacks(sq) & ~us() & movableBb;

            while (knightMoves > 0) {
                Square targetSquare = poplsb(knightMoves);
                moves.push_back(Move(sq, targetSquare, Move::KNIGHT_FLAG));
            }
        }
        
        while (ourBishops > 0) {
            Square sq = poplsb(ourBishops);
            u64 bishopMoves = attacks::bishopAttacks(sq, occ) & ~us() & movableBb;

            if ((1ULL << sq) & pinnedDiagonal)
                bishopMoves &= LINE_THROUGH[kingSquare][sq];

            while (bishopMoves > 0) {
                Square targetSquare = poplsb(bishopMoves);
                moves.push_back(Move(sq, targetSquare, Move::BISHOP_FLAG));
            }
        }

        while (ourRooks > 0) {
            Square sq = poplsb(ourRooks);
            u64 rookMoves = attacks::rookAttacks(sq, occ) & ~us() & movableBb;

            if ((1ULL << sq) & pinnedNonDiagonal)
                rookMoves &= LINE_THROUGH[kingSquare][sq];

            while (rookMoves > 0) {
                Square targetSquare = poplsb(rookMoves);
                moves.push_back(Move(sq, targetSquare, Move::ROOK_FLAG));
            }
        }

        while (ourQueens > 0) {
            Square sq = poplsb(ourQueens);
            u64 queenMoves = attacks::queenAttacks(sq, occ) & ~us() & movableBb;

            if ((1ULL << sq) & (pinnedDiagonal | pinnedNonDiagonal))
                queenMoves &= LINE_THROUGH[kingSquare][sq];

            while (queenMoves > 0) {
                Square targetSquare = poplsb(queenMoves);
                moves.push_back(Move(sq, targetSquare, Move::QUEEN_FLAG));
            }
        }

        moves.shrink_to_fit();
    }

    private:

    inline void addPromotions(std::vector<Move> &moves, Square sq, Square targetSquare, bool underpromotions)
    {
        moves.push_back(Move(sq, targetSquare, Move::QUEEN_PROMOTION_FLAG));
        if (underpromotions) {
            moves.push_back(Move(sq, targetSquare, Move::ROOK_PROMOTION_FLAG));
            moves.push_back(Move(sq, targetSquare, Move::BISHOP_PROMOTION_FLAG));
            moves.push_back(Move(sq, targetSquare, Move::KNIGHT_PROMOTION_FLAG));
        }
    }

}; // class Board