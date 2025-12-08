#include "ChessEval.h"
#include "Chess.h"

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// Initialize neural network with random weights and set up board state
ChessEval::ChessEval() : 
    castleStatus(0), 
    currentTurnNo(0),
    rng(std::random_device{}()),
    weight_dist(0.0f, 0.1f) {
    
    // Allocate memory for weights and biases
    weights1 = std::vector<std::vector<float> >(HIDDEN1_SIZE, std::vector<float>(INPUT_SIZE));
    bias1 = std::vector<float>(HIDDEN1_SIZE);
    weights2 = std::vector<std::vector<float> >(HIDDEN2_SIZE, std::vector<float>(HIDDEN1_SIZE));
    bias2 = std::vector<float>(HIDDEN2_SIZE);
    weights3 = std::vector<std::vector<float> >(OUTPUT_SIZE, std::vector<float>(HIDDEN2_SIZE));
    bias3 = std::vector<float>(OUTPUT_SIZE);

    // Initialize weights and biases with random values
    initializeWeights(weights1, bias1);
    initializeWeights(weights2, bias2);
    initializeWeights(weights3, bias3);
}

// Initialize weights and biases using Xavier initialization
void ChessEval::initializeWeights(std::vector<std::vector<float> >& weights, std::vector<float>& bias) {
    for (auto& row : weights) {
        for (float& w : row) w = weight_dist(rng);
    }
    for (float& b : bias) b = weight_dist(rng);
}

// ReLU activation function: max(0, x)
float ChessEval::relu(float x) const {
    return std::max(0.0f, x);
}

// Compute dot product of two vectors
float ChessEval::dotProduct(const std::vector<float>& a, const std::vector<float>& b) const {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// Perform forward pass through the neural network and store activations
LayerActivations ChessEval::forwardWithActivations(const std::vector<float>& input) const {
    LayerActivations activations;
    activations.input = input;
    
    // First hidden layer
    activations.hidden1.resize(HIDDEN1_SIZE);
    for (size_t i = 0; i < HIDDEN1_SIZE; ++i) {
        activations.hidden1[i] = relu(dotProduct(weights1[i], input) + bias1[i]);
    }

    // Second hidden layer
    activations.hidden2.resize(HIDDEN2_SIZE);
    for (size_t i = 0; i < HIDDEN2_SIZE; ++i) {
        activations.hidden2[i] = relu(dotProduct(weights2[i], activations.hidden1) + bias2[i]);
    }

    // Output layer with tanh activation scaled to reasonable centipawn range
    float raw_output = dotProduct(weights3[0], activations.hidden2) + bias3[0];
    activations.output = 2000.0f * std::tanh(raw_output);  // Scale to ±2000 centipawns
    return activations;
}

// Forward pass wrapper that returns only the final output
float ChessEval::forward(const std::vector<float>& input) const {
    return forwardWithActivations(input).output;
}

// Convert board state to one-hot encoded input vector plus contextual features
std::vector<float> ChessEval::encodePosition(const char* state, const PositionContext& context) const {
    // Static lookup table: maps ASCII character to piece index (0-11)
    static bool initTable = false;
    static int pieceLookup[256];
    
    if (!initTable) {
        initTable = true;
        pieceLookup['P'] = 0; pieceLookup['R'] = 1; pieceLookup['N'] = 2; pieceLookup['B'] = 3; pieceLookup['Q'] = 4; pieceLookup['K'] = 5;
        pieceLookup['p'] = 6; pieceLookup['r'] = 7; pieceLookup['n'] = 8; pieceLookup['b'] = 9; pieceLookup['q'] = 10;pieceLookup['k'] = 11;
    }
    
    std::vector<float> encoded(INPUT_SIZE, 0.0f);
    
    // Single pass through the board: O(64) instead of O(64 * 12)
    for (int i = 0; i < BOARD_SIZE; ++i) {
        char piece = state[i];
        if (piece != '0') {
            int piece_idx = pieceLookup[static_cast<unsigned char>(piece)];
            // piece_idx will be 0 for unknown characters, but that's fine
            // since 'P' legitimately maps to 0
            encoded[i + piece_idx * BOARD_SIZE] = 1.0f;
        }
    }
    
    size_t base = BOARD_SIZE * PIECE_TYPES;
    encoded[base + 0] = context.whiteToMove ? 1.0f : 0.0f;
    encoded[base + 1] = context.whiteCastleKingside ? 1.0f : 0.0f;
    encoded[base + 2] = context.whiteCastleQueenside ? 1.0f : 0.0f;
    encoded[base + 3] = context.blackCastleKingside ? 1.0f : 0.0f;
    encoded[base + 4] = context.blackCastleQueenside ? 1.0f : 0.0f;

    return encoded;
}

/** convert FEN string to board state 
 *  @param fen FEN string
 *  @param state board state
*/

void ChessEval::FENtoState(const std::string& fen, char* state) {
    for (int i = 0; i < 64; i++) {
        state[i] = '0';
    }
    state[64] = 0;

    std::istringstream fenStream(fen);
    std::string boardPart;
    std::getline(fenStream, boardPart, ' ');

    int row = 7;
    int col = 0;
    for (char ch : boardPart) {
        if (ch == '/') {
            row--;
            col = 0;
        } else if (isdigit(ch)) {
            col += ch - '0'; // Skip empty squares
        } else {
            state[row * 8 + col] = ch;
            col++;
        }
    }
}


// Evaluate a chess position using the neural network
int ChessEval::evaluate(const char* state, const PositionContext& context) {
    std::vector<float> input = encodePosition(state, context);
    float output = forward(input);
    return -static_cast<int>(output);
}

// Clip gradient to prevent explosion
float ChessEval::clipGradient(float gradient) const {
    float abs_grad = std::abs(gradient);
    if (abs_grad > CLIP_THRESHOLD) {
        return (gradient * CLIP_THRESHOLD) / abs_grad;
    }
    return gradient;
}

// Get formatted training status report
std::string ChessEval::getTrainingStatus() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);  // More readable precision
    ss << "Training Status Report:\n";
    ss << "=====================\n";
    ss << "Positions Trained: " << metrics.positions_trained << "\n";
    ss << "Total Iterations: " << metrics.iterations << "\n";
    ss << "Average Error: " << metrics.running_average_error << " centipawns\n";
    ss << "Initial Error: " << metrics.initial_average_error << " centipawns\n";
    
    // Calculate progress as error reduction percentage
    float progress = 0.0f;
    if (metrics.initial_average_error > 1e-5f) {
        progress = 100.0f * (1.0f - metrics.running_average_error / metrics.initial_average_error);
    }
    ss << "Error Reduction: " << progress << "%\n";
    
    return ss.str();
}

float ChessEval::getRunningAverageError() const {
    return metrics.running_average_error;
}

// Adjust these constants at the top
static constexpr float INITIAL_LEARNING_RATE = 0.0000005f;  // Increase learning rate
static constexpr float LOSS_SCALE = 1.0f;  // Adjust loss scaling
static constexpr float MOMENTUM = 0.9f;  // Add momentum to help with learning

// Perform backpropagation to update network weights
void ChessEval::backpropagate(const LayerActivations& activations,
                             float target,
                             float learning_rate) {
    float output = activations.output;
    float error = (target - output);
    
    // Scale down the error and loss for numerical stability
    error *= LOSS_SCALE;
    
    // Update training metrics with scaled loss
    float loss = error * error;  // MSE loss
    metrics.last_loss = loss / LOSS_SCALE;  // Unscale for reporting
    
    // Use exponential moving average for more stable loss tracking
    if (metrics.iterations == 0) {
        metrics.average_loss = loss;
        metrics.best_loss = loss;
    } else {
        metrics.average_loss = metrics.average_loss * 0.99f + loss * 0.01f;
        metrics.best_loss = std::min(metrics.best_loss, loss);
    }
    metrics.iterations++;
    
    // FIX 1: REMOVE AGGRESSIVE DECAY
    // Old: float effective_learning_rate = learning_rate / std::sqrt(metrics.iterations + 1);
    // New: Use fixed learning rate for now. 
    // For a simple MLP in this context, a small constant rate is often better.
    float effective_learning_rate = learning_rate; 
    
    // FIX 2: ADD TANH DERIVATIVE
    // Your output is 2000 * tanh(x). The derivative of tanh(x) is (1 - tanh(x)^2).
    // Without this, the gradients are treated as linear, which makes learning 
    // extreme values (like +/- 900 for a Queen) very slow.
    float norm_output = output / 2000.0f; // Map back to -1..1 range
    float tanh_derivative = 2000.0f * (1.0f - (norm_output * norm_output));

    // Output layer gradients with clipping
    std::vector<float> d_weights3(HIDDEN2_SIZE);
    
    // Apply derivative to the error signal
    float d_bias3 = clipGradient(error * tanh_derivative);
    
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        // Apply derivative here too
        d_weights3[i] = clipGradient(error * tanh_derivative * activations.hidden2[i]);
    }
    
    // Hidden2 layer gradients
    std::vector<float> d_hidden2(HIDDEN2_SIZE);
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        // Propagate the error * derivative back
        float grad = (error * tanh_derivative) * weights3[0][i];
        grad = clipGradient(grad);
        d_hidden2[i] = activations.hidden2[i] > 0 ? grad : 0; // ReLU derivative
    }
    
    // Hidden1 layer gradients
    std::vector<float> d_hidden1(HIDDEN1_SIZE);
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        float sum = 0;
        for (int j = 0; j < HIDDEN2_SIZE; ++j) {
            sum += d_hidden2[j] * weights2[j][i];
        }
        sum = clipGradient(sum);
        d_hidden1[i] = activations.hidden1[i] > 0 ? sum : 0; // ReLU derivative
    }
    
    // Update weights and biases using gradient descent
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        weights3[0][i] += effective_learning_rate * d_weights3[i];
    }
    bias3[0] += effective_learning_rate * d_bias3;
    
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        for (int j = 0; j < HIDDEN1_SIZE; ++j) {
            float update = clipGradient(d_hidden2[i] * activations.hidden1[j]);
            weights2[i][j] += effective_learning_rate * update;
        }
        bias2[i] += effective_learning_rate * d_hidden2[i];
    }
    
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        for (int j = 0; j < INPUT_SIZE; ++j) {
            float update = clipGradient(d_hidden1[i] * activations.input[j]);
            weights1[i][j] += effective_learning_rate * update;
        }
        bias1[i] += effective_learning_rate * d_hidden1[i];
    }
}

// Train the network on a single position
void ChessEval::train(const char *state,
                      int stockfish_eval,
                      float learning_rate,
                      bool verbose,
                      const PositionContext& context) {

    if (std::abs(stockfish_eval) > 5000) {  // Or use a constant like MAX_EVAL
        std::cout << "skipping training on checkmate position" << std::endl;
        return;  // *cracks whip* No training on checkmates!
    }
    float effective_learning_rate = learning_rate > 0.0f ? learning_rate : INITIAL_LEARNING_RATE;

    // Get evaluation BEFORE training
    int preTrainEval = evaluate(state, context);
    
    // Perform training
    std::vector<float> input = encodePosition(state, context);
    LayerActivations activations = forwardWithActivations(input);
    float clamped_target = std::max(-MAX_EVAL, std::min(MAX_EVAL, (float)stockfish_eval));
    backpropagate(activations, clamped_target, effective_learning_rate);
    
    // Get evaluation AFTER training
    int postTrainEval = evaluate(state, context);
    
    const int nextPositionIndex = metrics.positions_trained + 1;
    updateTrainingMetrics(preTrainEval - clamped_target,
                          postTrainEval - clamped_target,
                          context.whiteToMove);
    metrics.positions_trained = nextPositionIndex;

    if (verbose) {
        // Detailed logging
        std::cout << "\nPosition " << nextPositionIndex << " training details:" << std::endl;
        std::cout << "Stockfish eval: " << stockfish_eval << std::endl;
        std::cout << "Our eval before: " << preTrainEval << std::endl;
        std::cout << "Our eval after:  " << postTrainEval << std::endl;
        std::cout << "Pre-training error:  " << std::abs(preTrainEval - stockfish_eval) << std::endl;
        std::cout << "Post-training error: " << std::abs(postTrainEval - stockfish_eval) << std::endl;
    }
    // Check if we're moving in the right direction
    bool improved = std::abs(postTrainEval - stockfish_eval) < std::abs(preTrainEval - stockfish_eval);
    // it's easier to see the improvement if we print a + or -
    std::cout << "Training " << (improved ? "++++++++" : "--------") << " position evaluation (" << std::abs(postTrainEval - stockfish_eval) << " centipawns)" << std::endl;
    // Verify our outputs are within reasonable bounds (-2000 to 2000 centipawns)
    if (std::abs(postTrainEval) > 2000) {
        std::cout << "WARNING: Evaluation outside expected range!" << std::endl;
    }
}

// Helper function to write a vector to file
void ChessEval::writeVector(std::ofstream& out, const std::vector<float>& vec) const {
    size_t size = vec.size();
    out.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(vec.data()), size * sizeof(float));
}

// Helper function to read a vector from file
void ChessEval::readVector(std::ifstream& in, std::vector<float>& vec) {
    size_t size;
    in.read(reinterpret_cast<char*>(&size), sizeof(size_t));
    vec.resize(size);
    in.read(reinterpret_cast<char*>(vec.data()), size * sizeof(float));
}

// Helper function to write a matrix to file
void ChessEval::writeMatrix(std::ofstream& out, const std::vector<std::vector<float> >& matrix) const {
    size_t rows = matrix.size();
    out.write(reinterpret_cast<const char*>(&rows), sizeof(size_t));
    for (const auto& row : matrix) {
        writeVector(out, row);
    }
}

// Helper function to read a matrix from file
void ChessEval::readMatrix(std::ifstream& in, std::vector<std::vector<float> >& matrix) {
    size_t rows;
    in.read(reinterpret_cast<char*>(&rows), sizeof(size_t));
    matrix.resize(rows);
    for (auto& row : matrix) {
        readVector(in, row);
    }
}

// Save model weights and training metrics to file
bool ChessEval::saveModel(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    // Add a magic number to verify file type
    const uint32_t MAGIC_NUMBER = 0xDEADBEAF; // My special touch
    out.write(reinterpret_cast<const char*>(&MAGIC_NUMBER), sizeof(uint32_t));

    // Save network architecture
    out.write(reinterpret_cast<const char*>(&INPUT_SIZE), sizeof(int));
    out.write(reinterpret_cast<const char*>(&HIDDEN1_SIZE), sizeof(int));
    out.write(reinterpret_cast<const char*>(&HIDDEN2_SIZE), sizeof(int));
    out.write(reinterpret_cast<const char*>(&OUTPUT_SIZE), sizeof(int));

    // Save weights and biases
    writeMatrix(out, weights1);
    writeVector(out, bias1);
    writeMatrix(out, weights2);
    writeVector(out, bias2);
    writeMatrix(out, weights3);
    writeVector(out, bias3);

    // Save training metrics
    out.write(reinterpret_cast<const char*>(&metrics), sizeof(TrainingMetrics));

    // Save board state
    out.write(reinterpret_cast<const char*>(&castleStatus), sizeof(int));
    out.write(reinterpret_cast<const char*>(&currentTurnNo), sizeof(int));

    out.close();
    return true;
}

// Load model weights and training metrics from file
bool ChessEval::loadModel(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }

    // Verify magic number
    uint32_t stored_magic;
    in.read(reinterpret_cast<char*>(&stored_magic), sizeof(uint32_t));
    if (stored_magic != 0xDEADBEAF) {
        std::cerr << "Not a valid model file." << std::endl;
        return false;
    }

    // Verify network architecture
    int stored_input_size, stored_h1_size, stored_h2_size, stored_output_size;
    in.read(reinterpret_cast<char*>(&stored_input_size), sizeof(int));
    in.read(reinterpret_cast<char*>(&stored_h1_size), sizeof(int));
    in.read(reinterpret_cast<char*>(&stored_h2_size), sizeof(int));
    in.read(reinterpret_cast<char*>(&stored_output_size), sizeof(int));

    if (stored_input_size != INPUT_SIZE || stored_h1_size != HIDDEN1_SIZE ||
        stored_h2_size != HIDDEN2_SIZE || stored_output_size != OUTPUT_SIZE) {
        return false;  // Architecture mismatch
    }

    // Load weights and biases
    readMatrix(in, weights1);
    readVector(in, bias1);
    readMatrix(in, weights2);
    readVector(in, bias2);
    readMatrix(in, weights3);
    readVector(in, bias3);

    // Load training metrics
    in.read(reinterpret_cast<char*>(&metrics), sizeof(TrainingMetrics));

    // Load board state
    in.read(reinterpret_cast<char*>(&castleStatus), sizeof(int));
    in.read(reinterpret_cast<char*>(&currentTurnNo), sizeof(int));

    in.close();

    metrics.initial_average_error = metrics.running_average_error;
    // Print loaded model statistics
    std::cout << "\n=== Model Statistics ===" << std::endl;
    std::cout << "Network Architecture:" << std::endl;
    std::cout << "  Input Layer:     " << INPUT_SIZE << " neurons" << std::endl;
    std::cout << "  Hidden Layer 1:  " << HIDDEN1_SIZE << " neurons" << std::endl;
    std::cout << "  Hidden Layer 2:  " << HIDDEN2_SIZE << " neurons" << std::endl;
    std::cout << "  Output Layer:    " << OUTPUT_SIZE << " neurons" << std::endl;
    std::cout << "\nTraining Progress:" << std::endl;
    std::cout << "  Positions Trained:   " << metrics.positions_trained << std::endl;
    std::cout << "  Initial Error:       " << metrics.initial_average_error << " centipawns" << std::endl;
    std::cout << "  Current Error:       " << metrics.running_average_error << " centipawns" << std::endl;
    std::cout << "  Error Reduction:     " << 
        (100.0f * (1.0f - metrics.running_average_error / metrics.initial_average_error)) << "%" << std::endl;
    std::cout << "=====================\n" << std::endl;

    return true;
}

void ChessEval::updateTrainingMetrics(float pre_error, float post_error, bool whiteToMove) {
    // Normalize errors based on side to move
    float normalized_pre = whiteToMove ? pre_error : -pre_error;
    float normalized_post = whiteToMove ? post_error : -post_error;
    
    metrics.last_loss = normalized_post * normalized_post;
    
    if (metrics.positions_trained == 0) {
        metrics.initial_average_error = std::abs(normalized_pre);
        metrics.running_average_error = std::abs(normalized_post);
    } else {
        // Use absolute difference between evaluation and target, just like in the training output
        const float alpha = 0.01f;
        float abs_error = std::abs(post_error);  // Changed from normalized_post
        metrics.running_average_error = 
            (1.0f - alpha) * metrics.running_average_error + 
            alpha * abs_error;
    }
}
