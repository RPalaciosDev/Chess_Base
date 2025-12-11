#pragma once

#include "Game.h"
#include "Grid.h"
#include "GameState.h"
#include "Bitboard.h"
#include "ChessEval.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    bool gameHasAI() override { return true; }
    void updateAI() override;

    Grid* getGrid() override { return _grid; }

    // Tournament support methods
    void setBoardFromFEN(const std::string& fen);
    BitMove getLastAIMove() const { return _lastAIMove; }
    std::string getFEN() const;

    // Get current player color (WHITE=1, BLACK=-1)
    int getCurrentPlayerColor() const;

    // you can make this variable private, it's just grouped with the public methods for convenience
    BitMove _lastAIMove;  // Stores the last move calculated by AI (for tournament)

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;
    bool placePieceFromFEN(char fenChar, int x, int y);
    void syncEngineFromGrid();
    void regenerateLegalMoves();
    int negamax(GameState& gamestate, int depth, int alpha, int beta);
    
    // Evaluation functions
    int evaluateMaterial(const GameState& gamestate) const;
    uint64_t computeZobristHash(const GameState& gamestate) const;
    bool isCriticalPosition(const GameState& gamestate, const std::vector<BitMove>& moves, int depth) const;
    int hybridEvaluate(GameState& gamestate, int depth);

    Grid* _grid;
    GameState _engineState;
    std::vector<BitMove> _legalMoves;
    int _countMoves;
    ChessEval _evaluate;  // Neural network evaluator (loaded with trained model)
    
    // Transposition table for material evaluations (Zobrist hash -> material score)
    mutable std::unordered_map<uint64_t, int> _materialCache;
};