# Integrate GameState and Movement Code

## Summary

The work on this branch represents my efforts in joining the GameState and Bitboard code that Graeme Devine gave us in class so that those of us who have fallen behind can catchup for the tournament.

The chess engine now use the shared game state and parses the FEN string to validate the move logic. It then updates the UI to sync with the bitboard backend for further move generation and validation. However, the game currently requires two players too work as it does not contain any AI features.

## Changes

The visual frontend comprised by the Chess and Grid classes are now wired to GameState. Gamestate handles move generation by piece type. GameState uses Bitboard arrays for fast bitwise representation in move generation and attack validation. Most of thisis handled in generateAllMoves and filterOutIllegalMoves(). I use this code in syncEngineFromGrid(), where we initialize the engineState and regenerateLegalMoves() where we call generateAllMoves() to update our available legal moves. 