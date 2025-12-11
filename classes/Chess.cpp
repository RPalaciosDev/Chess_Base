#include "Chess.h"
#include <limits>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <random>
#include "ChessSquare.h"
#include "ChessEval.h"

Chess::Chess()
{
    _grid = new Grid(8, 8);
    _countMoves = 0;
    _lastAIMove = BitMove();
    
    // Load trained neural network model
    if (!_evaluate.loadModel("resources/models/neural_final.bin")) {
        std::cout << "Warning: Failed to load neural network model. Using untrained network." << std::endl;
    } else {
        std::cout << "Successfully loaded trained neural network model." << std::endl;
    }
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
    syncEngineFromGrid();
    regenerateLegalMoves();

    // Enable AI for player 1 (black)
    if (gameHasAI()) {
        setAIPlayer(AI_PLAYER);
        _gameOptions.AIMAXDepth = 3; // Set search depth
    }

    startGame();
}

bool Chess::placePieceFromFEN(char fenChar, int x, int y)
{
    // Map FEN character to piece type, player, and gameTag
    static const std::unordered_map<char, std::tuple<int, int, int>> fenPieceMap = {
        // White pieces
        {'P', {Pawn, 0, 1}},
        {'N', {Knight, 0, 2}},
        {'B', {Bishop, 0, 3}},
        {'R', {Rook, 0, 4}},
        {'Q', {Queen, 0, 5}},
        {'K', {King, 0, 6}},

        // Black pieces
        {'p', {Pawn, 1, 129}},
        {'n', {Knight, 1, 130}},
        {'b', {Bishop, 1, 131}},
        {'r', {Rook, 1, 132}},
        {'q', {Queen, 1, 133}},
        {'k', {King, 1, 134}}};

    // Replace nasty type with auto
    auto it = fenPieceMap.find(fenChar);

    // if key is found
    if (it != fenPieceMap.end())
    {
        // destructure the data the iterator is pointing at into its respective parts
        auto [pieceId, playerNumber, gameTag] = it->second;
        ChessPiece piece = static_cast<ChessPiece>(pieceId);

        // Create and place the piece
        Bit *bit = PieceForPlayer(playerNumber, piece);
        bit->setGameTag(gameTag);
        ChessSquare *square = _grid->getSquare(x, y);
        if (square)
        {
            bit->setPosition(square->getPosition());
            square->setBit(bit);
        }
        return true;
    }
    return false;
}

void Chess::FENtoBoard(const std::string &fen)
{
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)

    // Clear the board first
    _grid->forEachSquare([](ChessSquare *square, int x, int y)
                         { square->destroyBit(); });

    int y = 7; // Start at rank 8
    int x = 0; // Start at file a

    for (size_t i = 0; i < fen.length() && y >= 0; i++)
    {
        char c = fen[i];

        // Shift to next rank on slashes and reset the file
        if (c == '/')
        {
            y--;
            x = 0;
        }
        // Skip consecutive empty squares
        else if (c >= '1' && c <= '8')
        {
            x += (c - '0');
        }
        else
        {
            // Place piece if valid FEN character
            if (placePieceFromFEN(c, x, y) && x < 8)
            {
                x++;
            }
        }
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    auto srcSquare = dynamic_cast<ChessSquare *>(&src);
    if (!srcSquare)
    {
        return false;
    }

    regenerateLegalMoves();

    Player *current = getCurrentPlayer();
    int currentPlayer = current ? current->playerNumber() : 0;
    int pieceColor = (bit.gameTag() & 128) ? 1 : 0;
    if (pieceColor != currentPlayer)
    {
        return false;
    }

    int fromIndex = srcSquare->getSquareIndex();
    return std::any_of(_legalMoves.begin(), _legalMoves.end(), [fromIndex](const BitMove &move)
                       { return move.from == fromIndex; });
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    auto srcSquare = dynamic_cast<ChessSquare *>(&src);
    auto dstSquare = dynamic_cast<ChessSquare *>(&dst);
    if (!srcSquare || !dstSquare)
    {
        return false;
    }

    if (_legalMoves.empty())
    {
        regenerateLegalMoves();
    }

    int fromIndex = srcSquare->getSquareIndex();
    int toIndex = dstSquare->getSquareIndex();

    auto it = std::find_if(_legalMoves.begin(), _legalMoves.end(), [&](const BitMove &move)
                           { return move.from == fromIndex && move.to == toIndex; });
    return it != _legalMoves.end();
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) { 
        square->destroyBit(); 
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
         s += pieceNotation(x, y ); 
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, static_cast<ChessPiece>(Pawn)));
        } else {
            square->setBit(nullptr);
        } });
    syncEngineFromGrid();
    regenerateLegalMoves();
}

void Chess::syncEngineFromGrid()
{
    std::string state = stateString();
    Player *current = getCurrentPlayer();
    char playerColor = (current && current->playerNumber() == 0) ? WHITE : BLACK;
    _engineState.init(state.c_str(), playerColor);
}

void Chess::regenerateLegalMoves()
{
    syncEngineFromGrid();
    _legalMoves = _engineState.generateAllMoves();
}

int Chess::negamax(GameState& gamestate, int depth, int alpha, int beta)
{
    _countMoves++;

    // Terminal node evaluation
    if (depth == 0) {
        return hybridEvaluate(gamestate, depth);
    }

    // Generate all legal moves
    std::vector<BitMove> newMoves = gamestate.generateAllMoves();
    
    // Check for terminal conditions (checkmate or stalemate)
    if (newMoves.empty()) {
        // Check if king is in check (checkmate) or not (stalemate)
        // For simplicity, return a very negative value for checkmate, 0 for stalemate
        // You may want to improve this check
        return -10000; // Assume checkmate for now
    }

    int bestVal = std::numeric_limits<int>::min();

    // code to generate moves and setup negamax here
    for(const auto& move : newMoves) {
        gamestate.pushMove(move);

        bestVal = std::max(bestVal, -negamax(gamestate, depth - 1, -beta, -alpha));

        // Undo the move
        gamestate.popState();

        // alpha beta cut-off
        alpha = std::max(alpha, bestVal);
        if (alpha >= beta) {
            break;
        }
    }

    // code to return bestVal here
    return bestVal;
}

void Chess::updateAI()
{
    if (!gameHasAI()) return;

    _lastAIMove = BitMove();  // Reset last AI move

    const auto searchStart = std::chrono::steady_clock::now();
    _countMoves = 0;

    syncEngineFromGrid();
    std::vector<BitMove> moves = _engineState.generateAllMoves();

    if (moves.empty()) {
        endTurn();
        return;
    }

    // Shuffle moves to add variety and prevent repetitive games
    // This ensures different move orders are tried, leading to different games
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(moves.begin(), moves.end(), gen);

    const int negInfinite = std::numeric_limits<int>::min();
    int bestVal = negInfinite;
    std::vector<BitMove> bestMoves;  // Store all moves with best evaluation
    int depth = getAIMAXDepth();
    if (depth <= 0) depth = 3; // Default depth

    // Threshold for considering moves "equal" (in centipawns)
    // Moves within this threshold will be randomly selected from
    const int EQUALITY_THRESHOLD = 10; // 10 centipawns = 0.1 pawns

    // Try each move and find the best one(s)
    for (const auto& move : moves) {
        _engineState.pushMove(move);
        int moveVal = -negamax(_engineState, depth - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        _engineState.popState();

        if (moveVal > bestVal) {
            bestVal = moveVal;
            bestMoves.clear();
            bestMoves.push_back(move);
        } else if (moveVal >= bestVal - EQUALITY_THRESHOLD && !bestMoves.empty()) {
            // If this move is within the threshold of the best, add it to candidates
            bestMoves.push_back(move);
        }
    }

    // Randomly select from best moves (or moves within threshold)
    BitMove bestMove;
    if (!bestMoves.empty()) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, bestMoves.size() - 1);
        bestMove = bestMoves[dis(gen)];
    } else {
        // Fallback (shouldn't happen, but safety check)
        bestMove = moves[0];
    }

    // Make the best move
    if(bestVal != negInfinite) {
        _lastAIMove = bestMove;
        
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - searchStart).count();
        const double boardsPerSecond = seconds > 0.0 ? static_cast<double>(_countMoves) / seconds : 0.0;
        std::cout << "Moves checked: " << _countMoves
                  << " (" << std::fixed << std::setprecision(2) << boardsPerSecond
                  << " boards/s)" << std::defaultfloat << std::endl;

        int srcSquare = bestMove.from;
        int dstSquare = bestMove.to;
        BitHolder& src = getHolderAt(srcSquare & 7, srcSquare / 8);
        BitHolder& dst = getHolderAt(dstSquare & 7, dstSquare / 8);
        Bit* bit = src.bit();
        if (bit && dst.dropBitAtPoint(bit, ImVec2(0, 0))) {
            src.setBit(nullptr);
            bitMovedFromTo(*bit, src, dst);
        }
    }
}

// Material piece values (in centipawns)
namespace {
    const int PIECE_VALUES[] = {
        0,      // NoPiece
        100,    // Pawn
        320,    // Knight
        330,    // Bishop
        500,    // Rook
        900,    // Queen
        20000   // King (very high to prioritize king safety)
    };
}

int Chess::evaluateMaterial(const GameState& gamestate) const
{
    int material = 0;
    
    // Count pieces for both sides
    for (int i = 0; i < 64; ++i) {
        char piece = gamestate.state[i];
        if (piece == '0') continue;
        
        // Determine piece type and color
        bool isWhite = (piece >= 'A' && piece <= 'Z');
        char pieceUpper = isWhite ? piece : (piece - 32); // Convert to uppercase
        
        int pieceValue = 0;
        switch (pieceUpper) {
            case 'P': pieceValue = PIECE_VALUES[Pawn]; break;
            case 'N': pieceValue = PIECE_VALUES[Knight]; break;
            case 'B': pieceValue = PIECE_VALUES[Bishop]; break;
            case 'R': pieceValue = PIECE_VALUES[Rook]; break;
            case 'Q': pieceValue = PIECE_VALUES[Queen]; break;
            case 'K': pieceValue = PIECE_VALUES[King]; break;
        }
        
        // Add for white, subtract for black
        material += isWhite ? pieceValue : -pieceValue;
    }
    
    // Return from white's perspective (positive = white advantage)
    return material;
}

uint64_t Chess::computeZobristHash(const GameState& gamestate) const
{
    // Simple Zobrist hash based on piece positions and color to move
    // For a full implementation, you'd use precomputed random numbers
    // This is a simplified version that still works for material caching
    
    uint64_t hash = 0;
    
    // Hash piece positions
    for (int i = 0; i < 64; ++i) {
        char piece = gamestate.state[i];
        if (piece != '0') {
            // Combine square index and piece type into hash
            hash ^= (static_cast<uint64_t>(piece) << (i % 8)) + (static_cast<uint64_t>(i) << 8);
        }
    }
    
    // Hash side to move
    if (gamestate.color == BLACK) {
        hash ^= 0x123456789ABCDEF0ULL;
    }
    
    return hash;
}

bool Chess::isCriticalPosition(const GameState& gamestate, const std::vector<BitMove>& moves, int depth) const
{
    // Use neural network evaluation at critical positions:
    
    // 1. Positions with captures (tactical situations)
    bool hasCapture = std::any_of(moves.begin(), moves.end(), 
        [](const BitMove& m) { return m.flags & IsCapture; });
    if (hasCapture) return true;
    
    // 2. Near the root of search tree (top 2 plies) - use NN for important decisions
    // Note: depth decreases as we go deeper, so depth >= 2 means we're near the root
    // Actually, we want to use NN at the root level, so we need to track original depth
    // For now, use NN at all leaf nodes (depth == 0 means we're evaluating)
    // This will be called from depth 0, so we'll use a different approach
    
    // 3. Endgame positions (few pieces remaining) - positional nuances matter more
    int pieceCount = 0;
    for (int i = 0; i < 64; ++i) {
        if (gamestate.state[i] != '0') pieceCount++;
    }
    if (pieceCount <= 12) return true; // Endgame threshold
    
    // 4. Positions with promotions (important tactical moments)
    bool hasPromotion = std::any_of(moves.begin(), moves.end(), 
        [](const BitMove& m) { return m.flags & IsPromotion; });
    if (hasPromotion) return true;
    
    return false;
}

int Chess::hybridEvaluate(GameState& gamestate, int depth)
{
    // Compute Zobrist hash for this position
    uint64_t hash = computeZobristHash(gamestate);
    
    // Check cache first
    auto it = _materialCache.find(hash);
    if (it != _materialCache.end()) {
        // Found in cache, use material evaluation
        return it->second;
    }
    
    // Generate moves to check if this is a critical position
    std::vector<BitMove> moves = gamestate.generateAllMoves();
    bool isCritical = isCriticalPosition(gamestate, moves, depth);
    
    int evaluation;
    
    if (isCritical) {
        // Use neural network for critical positions
        PositionContext context;
        context.whiteToMove = (gamestate.color == WHITE);
        evaluation = _evaluate.evaluate(gamestate.state, context);
    } else {
        // Use fast material evaluation for non-critical positions
        evaluation = evaluateMaterial(gamestate);
    }
    
    // Cache the material evaluation (even if we used NN, cache material for future use)
    int materialEval = evaluateMaterial(gamestate);
    _materialCache[hash] = materialEval;
    
    // Limit cache size to prevent memory issues
    if (_materialCache.size() > 100000) {
        // Clear half the cache (simple FIFO-like behavior)
        auto it = _materialCache.begin();
        std::advance(it, _materialCache.size() / 2);
        _materialCache.erase(_materialCache.begin(), it);
    }
    
    return evaluation;
}

// Tournament support: Get current player color (WHITE=1, BLACK=-1)
int Chess::getCurrentPlayerColor() const
{
    // Use engine state color directly since this is a const method
    return _engineState.color;
}

// Tournament support: Set board from FEN and reinitialize game state for AI
void Chess::setBoardFromFEN(const std::string& fen) {
    // Parse FEN string - can be full FEN or just piece placement
    std::string piecePlacement = fen;
    std::string activeColor = "w";
    std::string castling = "KQkq";
    std::string enPassant = "-";

    // Check if this is a full FEN string (has spaces)
    size_t spacePos = fen.find(' ');
    if (spacePos != std::string::npos) {
        // Parse full FEN
        std::istringstream fenStream(fen);
        fenStream >> piecePlacement >> activeColor >> castling >> enPassant;
    }

    // Set visual board from piece placement
    FENtoBoard(piecePlacement);

    // Determine current player from FEN
    char playerColor = (activeColor == "w" || activeColor == "W") ? WHITE : BLACK;

    // Reinitialize game state so AI sees correct board
    syncEngineFromGrid();
    _engineState.init(stateString().c_str(), playerColor);

    // TODO: Parse castling rights and en passant from FEN for more accurate state
    // For now, the basic state is sufficient for AI to calculate moves

    // Generate legal moves for the new position
    regenerateLegalMoves();

    std::cout << "[Tournament] Board set from FEN. Player: "
              << (playerColor == WHITE ? "White" : "Black")
              << ", Legal moves: " << _legalMoves.size() << std::endl;
}

// Tournament support: Generate FEN string from current board
std::string Chess::getFEN() const {
    std::string fen;
    fen.reserve(90);

    // Piece placement (from rank 8 to rank 1)
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            char piece = pieceNotation(file, rank);
            if (piece == '0') {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                fen += piece;
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (rank > 0) {
            fen += '/';
        }
    }

    // Active color
    fen += ' ';
    char currentColor = _engineState.color;
    fen += (currentColor == WHITE) ? 'w' : 'b';

    // Castling availability (simplified - always report based on piece positions)
    fen += ' ';
    std::string castling;

    // Check if white can castle (king on e1, rooks on a1/h1)
    char e1 = pieceNotation(4, 0);
    char a1 = pieceNotation(0, 0);
    char h1 = pieceNotation(7, 0);
    if (e1 == 'K') {
        if (h1 == 'R') castling += 'K';
        if (a1 == 'R') castling += 'Q';
    }

    // Check if black can castle (king on e8, rooks on a8/h8)
    char e8 = pieceNotation(4, 7);
    char a8 = pieceNotation(0, 7);
    char h8 = pieceNotation(7, 7);
    if (e8 == 'k') {
        if (h8 == 'r') castling += 'k';
        if (a8 == 'r') castling += 'q';
    }

    fen += castling.empty() ? "-" : castling;

    // En passant target square (simplified - report as '-')
    fen += " -";

    // Halfmove clock (simplified)
    fen += " 0";

    // Fullmove number (simplified)
    fen += " 1";

    return fen;
}
