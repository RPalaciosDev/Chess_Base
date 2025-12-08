
#include <algorithm>
#include <iostream>
#include "GameState.h"
#include "MagicBitboards.h"

static int _bitboardLookup[128];
static bool _initedMagic = false;
static BitBoard _pawnAttacks[2][64]; // Precomputed pawn attacks for each square

void GameState::init(const char* newState, char player) {
    std::memcpy(state, newState, 64);
    color = player;
    flags = 0;
    _zobristHash[0] = 0;
    _zobristHash[1] = 0;
    _attackBitBoard.setData(0);
    // Clear all bitboards
    for (int i = 0; i < e_numBitboards; ++i) {
        _bitboards[i].setData(0);
    }

    if (!_initedMagic) {
        initMagicBitboards();
        // remove branching when we make the bitboards
        for(int i=0; i<128; i++) { _bitboardLookup[i] = 0; }

        _bitboardLookup['P'] = WHITE_PAWNS;
        _bitboardLookup['N'] = WHITE_KNIGHTS;
        _bitboardLookup['B'] = WHITE_BISHOPS;
        _bitboardLookup['R'] = WHITE_ROOKS;
        _bitboardLookup['Q'] = WHITE_QUEENS;
        _bitboardLookup['K'] = WHITE_KING;
        _bitboardLookup['p'] = BLACK_PAWNS;
        _bitboardLookup['n'] = BLACK_KNIGHTS;
        _bitboardLookup['b'] = BLACK_BISHOPS;
        _bitboardLookup['r'] = BLACK_ROOKS;
        _bitboardLookup['q'] = BLACK_QUEENS;
        _bitboardLookup['k'] = BLACK_KING;
        _bitboardLookup['0'] = EMPTY_SQUARES;

        for(int square = 0; square < 64; square++) {
            _pawnAttacks[0][square].setData(generatePawnAttacksBitBoard(square, WHITE));
            _pawnAttacks[1][square].setData(generatePawnAttacksBitBoard(square, BLACK));
        }

        _initedMagic = true;

        std::cout << "initialized magic bitboards and bitboard lookup" << std::endl;
    }
}

void GameState::shutdown() {
    cleanupMagicBitboards();
}

void GameState::addPawnBitboardMovesToList(std::vector<BitMove>& moves, const BitBoard bitboard, const int shift) {
    if (bitboard.getData() == 0)
        return;
    bitboard.forEachBit([&](int toSquare) {
        int fromSquare = toSquare - shift; // Correct calculation for fromSquare
        moves.emplace_back(fromSquare, toSquare, Pawn);
    });
}

void GameState::generatePawnMoveList(std::vector<BitMove>& moves, const BitBoard pawns, const BitBoard emptySquares, const BitBoard enemyPieces, char color) {
    if (pawns.getData() == 0)
        return;

    BitBoard demoRight(NotAFile);
    BitBoard demoLeft(NotHFile);

    // Calculate single pawn moves forward
    BitBoard singleMoves = (color == WHITE) ? (pawns.getData() << 8) & emptySquares.getData() : (pawns.getData() >> 8) & emptySquares.getData();
    // Calculate double pawn moves from starting rank
    BitBoard doubleMoves = (color == WHITE) ? ((singleMoves.getData() & Rank3) << 8) & emptySquares.getData() : ((singleMoves.getData() & Rank6) >> 8) & emptySquares.getData();
    // Calculate left and right pawn captures
    BitBoard capturesLeft = (color == WHITE) ? ((pawns.getData() & NotAFile) << 7) & enemyPieces.getData() : ((pawns.getData() & NotAFile) >> 9) & enemyPieces.getData();
    BitBoard capturesRight = (color == WHITE) ? ((pawns.getData() & NotHFile) << 9) & enemyPieces.getData() : ((pawns.getData() & NotHFile) >> 7) & enemyPieces.getData();

    int shiftForward = (color == WHITE) ? 8 : -8;
    int doubleShift = (color == WHITE) ? 16 : -16;
    int captureLeftShift = (color == WHITE) ? 7 : -9;
    int captureRightShift = (color == WHITE) ? 9 : -7;
    
    // Add single pawn moves to the list
    addPawnBitboardMovesToList(moves, singleMoves, shiftForward);

    // Add double pawn moves to the list
    addPawnBitboardMovesToList(moves, doubleMoves, doubleShift);

    // Add pawn captures to the list
    addPawnBitboardMovesToList(moves, capturesLeft, captureLeftShift);
    addPawnBitboardMovesToList(moves, capturesRight, captureRightShift);
}

// Generate actual move objects from a bitboard
void GameState::generateKnightMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t occupancy) {
    knightBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard = BitBoard(KnightAttacks[fromSquare] & occupancy);
        // Efficiently iterate through only the set bits
        moveBitboard.forEachBit([&](int toSquare) {
           moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
}

// Generate actual move objects from a bitboard
void GameState::generateKingMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy) {
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard = BitBoard(KingAttacks[fromSquare] & occupancy);
        // Efficiently iterate through only the set bits
        moveBitboard.forEachBit([&](int toSquare) {
           moves.emplace_back(fromSquare, toSquare, King);
        });
    });
}

// Generate actual move objects from a bitboard
void GameState::generateBishopMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies)
{
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard = BitBoard(getBishopAttacks(fromSquare, occupancy) & ~friendlies);
        // Efficiently iterate through only the set bits
        moveBitboard.forEachBit([&](int toSquare) {
           moves.emplace_back(fromSquare, toSquare, Bishop);
        });
    });
}

void GameState::generateRooksMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies)
{
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard = BitBoard(getRookAttacks(fromSquare, occupancy) & ~friendlies);
        // Efficiently iterate through only the set bits
        moveBitboard.forEachBit([&](int toSquare) {
           moves.emplace_back(fromSquare, toSquare, Rook);
        });
    });
}

void GameState::generateQueensMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies)
{
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard = BitBoard(getQueenAttacks(fromSquare, occupancy) & ~friendlies);
        // Efficiently iterate through only the set bits
        moveBitboard.forEachBit([&](int toSquare) {
           moves.emplace_back(fromSquare, toSquare, Queen);
        });
    });
}

template <ChessPiece PIECE_TYPE>
inline BitBoard generatePieceAttackList(
    const BitBoard pieces, 
    const BitBoard occupancy
) {
    BitBoard attacks;

    pieces.forEachBit([&](int fromSquare) {
        // We'll branch on the piece type. 
        // This uses if constexpr for compile-time resolution
        if (PIECE_TYPE == Knight) {
            // Combine all knight moves from `fromSquare`
            attacks |= KnightAttacks[fromSquare];
        }
        else if (PIECE_TYPE == Bishop) {
            attacks |= BitBoard(getBishopAttacks(fromSquare, occupancy.getData())); 
        }
        else if (PIECE_TYPE == Rook) {
            attacks |= BitBoard(getRookAttacks(fromSquare, occupancy.getData())); 
        }
        else if (PIECE_TYPE == Queen) {
            // Queen is rook + bishop combined
            attacks |= (BitBoard(getBishopAttacks(fromSquare, occupancy.getData())) |
                        BitBoard(getRookAttacks(fromSquare, occupancy.getData())));
        }
        else if (PIECE_TYPE == King) {
            attacks |= KingAttacks[fromSquare];
        }
        else {
            // Optionally handle or static_assert for unexpected piece types
        }
    });

    return attacks;
}

uint64_t GameState::generatePawnAttacksBitBoard(int square, char color) {
    uint64_t bitboard = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    // Pawns can only attack diagonally forward
    // For white: up-right and up-left
    // For black: down-right and down-left
    const int direction = (color == WHITE) ? 1 : -1;
    
    // Check diagonal left attack
    if (file > 0) {  // Not on a-file
        int r = rank + direction;
        int f = file - 1;
        if (r >= 0 && r < 8) {  // Stay within board bounds
            bitboard |= 1ULL << (r * 8 + f);
        }
    }
    
    // Check diagonal right attack
    if (file < 7) {  // Not on h-file
        int r = rank + direction;
        int f = file + 1;
        if (r >= 0 && r < 8) {  // Stay within board bounds
            bitboard |= 1ULL << (r * 8 + f);
        }
    }
    return bitboard;
}

const BitBoard GameState::generatePawnAttacks(const BitBoard pawns, char color) {
    BitBoard result(0);

    pawns.forEachBit([&](int fromSquare) {
        // Using precomputed or dynamic logic
        result |= _pawnAttacks[color == WHITE ? 0 : 1][fromSquare];
    });

    return result;
}

// Returns true if 'square' is attacked by any piece belonging to 'attackerColor'
bool GameState::isSquareAttacked(int square, char attackerColor, const BitBoard (&boards)[e_numBitboards]) {
	const int pawnIdx   = (attackerColor == WHITE) ? WHITE_PAWNS : BLACK_PAWNS;
	const int knightIdx = (attackerColor == WHITE) ? WHITE_KNIGHTS : BLACK_KNIGHTS;
	const int bishopIdx = (attackerColor == WHITE) ? WHITE_BISHOPS : BLACK_BISHOPS;
	const int rookIdx   = (attackerColor == WHITE) ? WHITE_ROOKS : BLACK_ROOKS;
	const int queenIdx  = (attackerColor == WHITE) ? WHITE_QUEENS : BLACK_QUEENS;
	const int kingIdx   = (attackerColor == WHITE) ? WHITE_KING : BLACK_KING;

	// Get Occupancy of all pieces for sliding checks
	BitBoard occ = boards[OCCUPANCY];

	// Check Pawn Attacks
	char targetColor = (attackerColor == WHITE) ? BLACK : WHITE; 
	if ((generatePawnAttacksBitBoard(square, targetColor) & boards[pawnIdx].getData()) != 0) return true;

	// Check Knight Attacks
	if ((KnightAttacks[square] & boards[knightIdx].getData()) != 0) return true;

	// Check King Attacks (Neighboring kings)
	if ((KingAttacks[square] & boards[kingIdx].getData()) != 0) return true;

	// Check Bishop/Queen (Diagonal) Attacks
	uint64_t diagonalAttacks = getBishopAttacks(square, occ.getData());
	if ((diagonalAttacks & (boards[bishopIdx].getData() | boards[queenIdx].getData())) != 0) return true;

	// Check Rook/Queen (Straight) Attacks
	uint64_t straightAttacks = getRookAttacks(square, occ.getData());
	if ((straightAttacks & (boards[rookIdx].getData() | boards[queenIdx].getData())) != 0) return true;

	return false;
}

void GameState::filterOutIllegalMoves(std::vector<BitMove>& moves) {
	if (moves.empty()) return;

	const char myColor = color;
	const char opponentColor = (color == WHITE) ? BLACK : WHITE;
	const int myKingIdx = (myColor == WHITE) ? WHITE_KING : BLACK_KING;

	// Remove moves that leave the king in check
	moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const BitMove& move) {
		
		// Create a temporary copy of the board state
		BitBoard tempBoards[e_numBitboards];
		for (int i = 0; i < e_numBitboards; ++i) tempBoards[i] = _bitboards[i];

		// Apply the move to the temporary boards
		// Note: We just need occupancy correct for check detection.
		
		const uint64_t fromMask = 1ULL << move.from;
		const uint64_t toMask   = 1ULL << move.to;
		
		// Helper to determine which bitboard a piece belongs to
		auto getPieceIdx = [&](ChessPiece p, char c) {
			if (p == Pawn) return c == WHITE ? WHITE_PAWNS : BLACK_PAWNS;
			if (p == Knight) return c == WHITE ? WHITE_KNIGHTS : BLACK_KNIGHTS;
			if (p == Bishop) return c == WHITE ? WHITE_BISHOPS : BLACK_BISHOPS;
			if (p == Rook) return c == WHITE ? WHITE_ROOKS : BLACK_ROOKS;
			if (p == Queen) return c == WHITE ? WHITE_QUEENS : BLACK_QUEENS;
			return c == WHITE ? WHITE_KING : BLACK_KING; // King
		};

		int moverIdx = getPieceIdx(static_cast<ChessPiece>(move.piece), myColor);
		
		// Remove from 'from'
		tempBoards[moverIdx] &= ~fromMask;
		tempBoards[OCCUPANCY] &= ~fromMask;

		// Handle Captures (Remove opponent piece at 'to')
		// We scan opponent boards to find what was captured (slower than lookup, but safe for generic bitboards)
		int startOpp = (opponentColor == WHITE) ? WHITE_PAWNS : BLACK_PAWNS;
		int endOpp   = (opponentColor == WHITE) ? WHITE_KING : BLACK_KING;
		
		// Specialized handling for En Passant
		if (move.flags & EnPassant) {
			int capSq = (myColor == WHITE) ? (move.to - 8) : (move.to + 8);
			uint64_t capMask = 1ULL << capSq;
			tempBoards[startOpp] &= ~capMask; // Opponent Pawns
			tempBoards[OCCUPANCY] &= ~capMask;
		} else {
			// Standard capture
			for (int i = startOpp; i <= endOpp; ++i) {
				tempBoards[i] &= ~toMask;
			}
			tempBoards[OCCUPANCY] &= ~toMask; // Clear strictly to ensure no overlap before adding
		}

		// Handle Promotion
		if ((move.flags & IsPromotion)) {
			moverIdx = getPieceIdx(Queen, myColor); // Assume Queen promotion for check safety (mostly covers it)
		}

		// Add to 'to'
		tempBoards[moverIdx] |= toMask;
		tempBoards[OCCUPANCY] |= toMask;

		// Handle King Move (Update King Index tracking)
		int currentKingSquare = -1;
		if (move.piece == King) {
			currentKingSquare = move.to;
		} else {
			// If king didn't move, find him
			currentKingSquare = tempBoards[myKingIdx].firstBit();
		}

		// If the King is attacked by the opponent after this move, the move is illegal.
		return isSquareAttacked(currentKingSquare, opponentColor, tempBoards);

	}), moves.end());
}

std::vector<BitMove> GameState::generateAllMoves()
{
    std::vector<BitMove> moves;
    moves.reserve(32);

    for (int i=0; i<e_numBitboards; i++) {
        _bitboards[i] = 0;
    }

    for(int i = 0; i<64; i++) {
        int bitIndex = _bitboardLookup[(unsigned char)state[i]];
        _bitboards[bitIndex] |= 1ULL << i;
    }

    _bitboards[WHITE_ALL_PIECES] = _bitboards[WHITE_PAWNS].getData() | _bitboards[WHITE_KNIGHTS].getData() |
    _bitboards[WHITE_BISHOPS].getData() | _bitboards[WHITE_ROOKS].getData() |
    _bitboards[WHITE_QUEENS].getData() | _bitboards[WHITE_KING].getData();

    _bitboards[BLACK_ALL_PIECES] = _bitboards[BLACK_PAWNS].getData() | _bitboards[BLACK_KNIGHTS].getData() |
    _bitboards[BLACK_BISHOPS].getData() | _bitboards[BLACK_ROOKS].getData() |
    _bitboards[BLACK_QUEENS].getData() | _bitboards[BLACK_KING].getData();
    
    _bitboards[OCCUPANCY] = _bitboards[WHITE_ALL_PIECES].getData() | _bitboards[BLACK_ALL_PIECES].getData();

    int bitIndex = color == WHITE ? WHITE_PAWNS : BLACK_PAWNS;
    int oppBitIndex = color == WHITE ? BLACK_PAWNS : WHITE_PAWNS;

    generateKnightMoves(moves, _bitboards[WHITE_KNIGHTS + bitIndex], ~_bitboards[WHITE_ALL_PIECES + bitIndex].getData());
    generatePawnMoveList(moves, _bitboards[WHITE_PAWNS  + bitIndex], ~_bitboards[OCCUPANCY].getData(), _bitboards[WHITE_ALL_PIECES + oppBitIndex].getData(), color);
    generateKingMoves(moves, _bitboards[WHITE_KING + bitIndex], ~_bitboards[WHITE_ALL_PIECES + bitIndex].getData());
    generateBishopMoves(moves, _bitboards[WHITE_BISHOPS + bitIndex], _bitboards[OCCUPANCY].getData(), _bitboards[WHITE_ALL_PIECES + bitIndex].getData());
    generateRooksMoves(moves, _bitboards[WHITE_ROOKS + bitIndex], _bitboards[OCCUPANCY].getData(), _bitboards[WHITE_ALL_PIECES + bitIndex].getData());
    generateQueensMoves(moves, _bitboards[WHITE_QUEENS + bitIndex], _bitboards[OCCUPANCY].getData(), _bitboards[WHITE_ALL_PIECES + bitIndex].getData());

    filterOutIllegalMoves(moves);

    return moves;
}

