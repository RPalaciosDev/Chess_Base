#pragma once

#include "Game.h"
#include "Grid.h"
#include "GameState.h"
#include "Bitboard.h"
#include <vector>

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

    Grid* getGrid() override { return _grid; }

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string &fen);
    char pieceNotation(int x, int y) const;
    bool placePieceFromFEN(char fenChar, int x, int y);
    void syncEngineFromGrid();
    void regenerateLegalMoves();

    Grid* _grid;
    GameState _engineState;
    std::vector<BitMove> _legalMoves;
};