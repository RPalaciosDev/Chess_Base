#ifndef CHESS_EVAL_H
#define CHESS_EVAL_H

#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <limits>

/**
 * Structure to hold neural network layer activations during forward pass.
 * Used for both evaluation and training to avoid redundant computation.
 */
struct LayerActivations {
    std::vector<float> input;    // Input layer activations (one-hot encoded board)
    std::vector<float> hidden1;  // First hidden layer activations
    std::vector<float> hidden2;  // Second hidden layer activations
    float output;                // Final output value
};

/**
 * Structure to track training metrics and progress
 */
struct TrainingMetrics {
    TrainingMetrics() : 
        positions_trained(0),
        iterations(0),
        last_loss(0.0f),
        average_loss(0.0f),
        best_loss(std::numeric_limits<float>::max()),
        initial_average_error(0.0f),
        running_average_error(0.0f),
        error_window_size(100) {}
        
    int positions_trained;
    int iterations;
    float last_loss;
    float average_loss;
    float best_loss;
    float initial_average_error;  // Store initial average error
    float running_average_error;  // Current moving average of errors
    int error_window_size;       // Window size for moving average
};

struct PositionContext {
    bool whiteToMove = true;
    bool whiteCastleKingside = false;
    bool whiteCastleQueenside = false;
    bool blackCastleKingside = false;
    bool blackCastleQueenside = false;
};

/**
 * Neural network-based chess position evaluator that learns from Stockfish.
 * Implements a feedforward neural network with two hidden layers that takes
 * a chess position as input and outputs an evaluation in centipawns.
 */
class ChessEval {
public:
    /**
     * Initializes the neural network with random weights and biases.
     * Sets up the network architecture and initializes board state tracking.
     */
    ChessEval();
    
    /**
     * Evaluates a chess position and returns a score in centipawns.
     * @param state 64-char array representing the board state (0 for empty, standard piece notation)
     * @param context Additional metadata (side to move, castling rights)
     * @return Evaluation score in centipawns (positive for white advantage)
     */
    int evaluate(const char* state, const PositionContext& context = PositionContext());
    
    /**
     * Trains the network on a single position using Stockfish's evaluation as ground truth.
     * @param state 64-char array representing the board state (0 for empty, standard piece notation)
     * @param stockfish_eval Stockfish evaluation of the position
     * @param learning_rate Step size for gradient descent updates
     * @param verbose Whether to print verbose output
     */
    void train(const char *state,
               int stockfish_eval,
               float learning_rate = 0.001f,
               bool verbose = false,
               const PositionContext& context = PositionContext());

    /**
     * Reports current training status and metrics
     * @return String containing formatted training statistics
     */
    std::string getTrainingStatus() const;

    /**
     * Saves the neural network weights and training metrics to a file
     * @param filename Path to save the model
     * @return true if save was successful, false otherwise
     */
    bool saveModel(const std::string& filename) const;

    /**
     * Loads neural network weights and training metrics from a file
     * @param filename Path to load the model from
     * @return true if load was successful, false otherwise
     */
    bool loadModel(const std::string& filename);

    /**
     * Updates training metrics based on the difference between pre- and post-training evaluations
     * @param pre_error Evaluation before training
     * @param post_error Evaluation after training
     */
    void updateTrainingMetrics(float pre_error, float post_error, bool whiteToMove);
    float getRunningAverageError() const;

private:
    // Network architecture constants
    static constexpr int BOARD_SIZE = 64;    // Standard chess board size
    static constexpr int PIECE_TYPES = 12;   // 6 pieces * 2 colors
    static constexpr int EXTRA_FEATURES = 5; // side-to-move + castling rights
    static constexpr int INPUT_SIZE = BOARD_SIZE * PIECE_TYPES + EXTRA_FEATURES;
    static constexpr int HIDDEN1_SIZE = 256; // First hidden layer size
    static constexpr int HIDDEN2_SIZE = 64;  // Second hidden layer size
    static constexpr int OUTPUT_SIZE = 1;    // Single evaluation output
    static constexpr float MAX_EVAL = 2000.0f;

    static constexpr int BAD_EVAL = 0xDEADDEAD; // used to indicate an error in evaluation
    // Neural network parameters
    std::vector<std::vector<float> > weights1;  // Input to Hidden1 weights
    std::vector<float> bias1;                   // Hidden1 bias terms
    std::vector<std::vector<float> > weights2;  // Hidden1 to Hidden2 weights
    std::vector<float> bias2;                   // Hidden2 bias terms
    std::vector<std::vector<float> > weights3;  // Hidden2 to Output weights
    std::vector<float> bias3;                   // Output bias terms

    // Neural network helper functions
    float relu(float x) const;                  // ReLU activation function
    float forward(const std::vector<float>& input) const;  // Forward pass
    LayerActivations forwardWithActivations(const std::vector<float>& input) const;  // Forward pass with stored activations
    float dotProduct(const std::vector<float>& a, const std::vector<float>& b) const;
    void initializeWeights(std::vector<std::vector<float> >& weights, std::vector<float>& bias);
    
    // Position representation helpers
    std::vector<float> encodePosition(const char* state, const PositionContext& context) const;  // One-hot encoding
    
    // Training helper
    void backpropagate(const LayerActivations& activations,
                      float target, 
                      float learning_rate);

    // Board state tracking
    int castleStatus;   // Tracks castling rights using bit flags
    int currentTurnNo;  // Current move number (0-based)

    // Random number generation for weight initialization
    std::mt19937 rng;
    std::normal_distribution<float> weight_dist;

    // Training parameters
    static constexpr float MAX_GRADIENT = 1.0f;  // Maximum allowed gradient magnitude
    static constexpr float CLIP_THRESHOLD = 5.0f; // Gradient clipping threshold
    static constexpr float MOVING_AVG_FACTOR = 0.99f; // For loss averaging

    // Gradient clipping helper
    float clipGradient(float gradient) const;
    
    // Training metrics
    TrainingMetrics metrics;

    // Serialization helpers
    void writeVector(std::ofstream& out, const std::vector<float>& vec) const;
    void readVector(std::ifstream& in, std::vector<float>& vec);
    void writeMatrix(std::ofstream& out, const std::vector<std::vector<float> >& matrix) const;
    void readMatrix(std::ifstream& in, std::vector<std::vector<float> >& matrix);
    void FENtoState(const std::string& fen, char* state);
};

#endif // CHESS_EVAL_H 