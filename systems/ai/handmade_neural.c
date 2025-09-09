// handmade_neural.c - Basic neural network implementation for game AI
// Zero-dependency neural network with SIMD optimizations

#include "../../game/game_types.h"
#include <immintrin.h>
#include <string.h>
#include <math.h>

// Structure definitions
typedef struct layer {
    f32* weights;
    f32* biases;
    f32* outputs;
    f32* gradients;
    u32 input_size;
    u32 output_size;
    u32 weight_count;
    activation_type activation;
} layer;

typedef struct neural_network {
    layer* layers;
    u32 layer_count;
    u32 max_layers;
    f32* inputs;
    f32* outputs;
    f32 learning_rate;
    
    // Memory pool
    u8* memory;
    u64 memory_size;
    u64 memory_used;
} neural_network;

// Activation functions
static f32 relu(f32 x) {
    return x > 0 ? x : 0;
}

static f32 relu_derivative(f32 x) {
    return x > 0 ? 1 : 0;
}

static f32 sigmoid(f32 x) {
    return 1.0f / (1.0f + expf(-x));
}

static f32 sigmoid_derivative(f32 x) {
    f32 s = sigmoid(x);
    return s * (1.0f - s);
}

static f32 tanh_activation(f32 x) {
    return tanhf(x);
}

static f32 tanh_derivative(f32 x) {
    f32 t = tanhf(x);
    return 1.0f - t * t;
}

// Network creation
neural_network* neural_create(void) {
    neural_network* net = (neural_network*)malloc(sizeof(neural_network));
    if (!net) return NULL;
    
    memset(net, 0, sizeof(neural_network));
    
    // Allocate memory pool (16MB)
    net->memory_size = 16 * 1024 * 1024;
    net->memory = (u8*)malloc(net->memory_size);
    if (!net->memory) {
        free(net);
        return NULL;
    }
    
    net->memory_used = 0;
    net->max_layers = 16;
    net->layers = (layer*)net->memory;
    net->memory_used += sizeof(layer) * net->max_layers;
    net->layer_count = 0;
    net->learning_rate = 0.01f;
    
    return net;
}

// Add layer to network
void neural_add_layer(neural_network* net, u32 input_size, u32 output_size, 
                     activation_type activation) {
    if (!net || net->layer_count >= net->max_layers) return;
    
    layer* l = &net->layers[net->layer_count];
    l->input_size = input_size;
    l->output_size = output_size;
    l->weight_count = input_size * output_size;
    l->activation = activation;
    
    // Allocate from memory pool
    u8* mem_ptr = net->memory + net->memory_used;
    
    // Weights
    l->weights = (f32*)mem_ptr;
    mem_ptr += sizeof(f32) * l->weight_count;
    
    // Biases
    l->biases = (f32*)mem_ptr;
    mem_ptr += sizeof(f32) * output_size;
    
    // Outputs
    l->outputs = (f32*)mem_ptr;
    mem_ptr += sizeof(f32) * output_size;
    
    // Gradients
    l->gradients = (f32*)mem_ptr;
    mem_ptr += sizeof(f32) * output_size;
    
    net->memory_used = mem_ptr - net->memory;
    
    // Initialize weights (Xavier initialization)
    f32 scale = sqrtf(2.0f / (f32)input_size);
    for (u32 i = 0; i < l->weight_count; i++) {
        l->weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 2.0f * scale;
    }
    
    // Initialize biases to small values
    for (u32 i = 0; i < output_size; i++) {
        l->biases[i] = 0.01f;
    }
    
    net->layer_count++;
    
    // Set network output pointer
    if (net->layer_count == 1) {
        net->inputs = (f32*)(net->memory + net->memory_used);
        net->memory_used += sizeof(f32) * input_size;
    }
    net->outputs = l->outputs;
}

// Forward propagation with SIMD
void neural_forward(neural_network* net, f32* inputs, f32* outputs) {
    if (!net || !inputs || !outputs || net->layer_count == 0) return;
    
    // Copy inputs
    memcpy(net->inputs, inputs, sizeof(f32) * net->layers[0].input_size);
    
    f32* layer_input = net->inputs;
    
    for (u32 l = 0; l < net->layer_count; l++) {
        layer* cur = &net->layers[l];
        
        // Clear outputs
        memset(cur->outputs, 0, sizeof(f32) * cur->output_size);
        
        // Matrix multiplication with SIMD
        for (u32 i = 0; i < cur->output_size; i++) {
            __m256 sum = _mm256_setzero_ps();
            u32 j = 0;
            
            // Process 8 elements at a time with AVX2
            for (; j + 7 < cur->input_size; j += 8) {
                __m256 w = _mm256_loadu_ps(&cur->weights[i * cur->input_size + j]);
                __m256 in = _mm256_loadu_ps(&layer_input[j]);
                sum = _mm256_fmadd_ps(w, in, sum);
            }
            
            // Sum the vector elements
            __m128 sum_high = _mm256_extractf128_ps(sum, 1);
            __m128 sum_low = _mm256_castps256_ps128(sum);
            __m128 sum_128 = _mm_add_ps(sum_high, sum_low);
            __m128 sum_64 = _mm_hadd_ps(sum_128, sum_128);
            __m128 sum_32 = _mm_hadd_ps(sum_64, sum_64);
            cur->outputs[i] = _mm_cvtss_f32(sum_32);
            
            // Process remaining elements
            for (; j < cur->input_size; j++) {
                cur->outputs[i] += cur->weights[i * cur->input_size + j] * layer_input[j];
            }
            
            // Add bias
            cur->outputs[i] += cur->biases[i];
            
            // Apply activation
            switch (cur->activation) {
                case ACTIVATION_RELU:
                    cur->outputs[i] = relu(cur->outputs[i]);
                    break;
                case ACTIVATION_SIGMOID:
                    cur->outputs[i] = sigmoid(cur->outputs[i]);
                    break;
                case ACTIVATION_TANH:
                    cur->outputs[i] = tanh_activation(cur->outputs[i]);
                    break;
                case ACTIVATION_LINEAR:
                    // No activation
                    break;
            }
        }
        
        layer_input = cur->outputs;
    }
    
    // Copy to output
    memcpy(outputs, net->outputs, sizeof(f32) * net->layers[net->layer_count - 1].output_size);
}

// Backpropagation
void neural_backward(neural_network* net, f32* targets) {
    if (!net || !targets || net->layer_count == 0) return;
    
    // Calculate output layer gradients
    layer* output_layer = &net->layers[net->layer_count - 1];
    for (u32 i = 0; i < output_layer->output_size; i++) {
        f32 error = targets[i] - output_layer->outputs[i];
        
        // Apply derivative of activation
        switch (output_layer->activation) {
            case ACTIVATION_RELU:
                output_layer->gradients[i] = error * relu_derivative(output_layer->outputs[i]);
                break;
            case ACTIVATION_SIGMOID:
                output_layer->gradients[i] = error * sigmoid_derivative(output_layer->outputs[i]);
                break;
            case ACTIVATION_TANH:
                output_layer->gradients[i] = error * tanh_derivative(output_layer->outputs[i]);
                break;
            case ACTIVATION_LINEAR:
                output_layer->gradients[i] = error;
                break;
        }
    }
    
    // Backpropagate through hidden layers
    for (i32 l = net->layer_count - 2; l >= 0; l--) {
        layer* cur = &net->layers[l];
        layer* next = &net->layers[l + 1];
        
        // Clear gradients
        memset(cur->gradients, 0, sizeof(f32) * cur->output_size);
        
        // Calculate gradients
        for (u32 i = 0; i < cur->output_size; i++) {
            f32 sum = 0;
            for (u32 j = 0; j < next->output_size; j++) {
                sum += next->weights[j * next->input_size + i] * next->gradients[j];
            }
            
            // Apply derivative of activation
            switch (cur->activation) {
                case ACTIVATION_RELU:
                    cur->gradients[i] = sum * relu_derivative(cur->outputs[i]);
                    break;
                case ACTIVATION_SIGMOID:
                    cur->gradients[i] = sum * sigmoid_derivative(cur->outputs[i]);
                    break;
                case ACTIVATION_TANH:
                    cur->gradients[i] = sum * tanh_derivative(cur->outputs[i]);
                    break;
                case ACTIVATION_LINEAR:
                    cur->gradients[i] = sum;
                    break;
            }
        }
    }
}

// Update weights
void neural_update_weights(neural_network* net) {
    if (!net || net->layer_count == 0) return;
    
    f32* layer_input = net->inputs;
    
    for (u32 l = 0; l < net->layer_count; l++) {
        layer* cur = &net->layers[l];
        
        // Update weights
        for (u32 i = 0; i < cur->output_size; i++) {
            for (u32 j = 0; j < cur->input_size; j++) {
                cur->weights[i * cur->input_size + j] += 
                    net->learning_rate * cur->gradients[i] * layer_input[j];
            }
            
            // Update bias
            cur->biases[i] += net->learning_rate * cur->gradients[i];
        }
        
        layer_input = cur->outputs;
    }
}

// Train network
void neural_train(neural_network* net, f32* inputs, f32* targets) {
    if (!net || !inputs || !targets) return;
    
    f32 outputs[256];  // Temporary output buffer
    neural_forward(net, inputs, outputs);
    neural_backward(net, targets);
    neural_update_weights(net);
}

// Destroy network
void neural_destroy(neural_network* net) {
    if (!net) return;
    
    if (net->memory) {
        free(net->memory);
    }
    free(net);
}

// Calculate loss
f32 neural_calculate_loss(neural_network* net, f32* outputs, f32* targets, u32 size) {
    if (!outputs || !targets) return 0;
    
    f32 loss = 0;
    for (u32 i = 0; i < size; i++) {
        f32 diff = outputs[i] - targets[i];
        loss += diff * diff;
    }
    return loss / (f32)size;
}

// Save/Load network
void neural_save(neural_network* net, const char* filename) {
    if (!net || !filename) return;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return;
    
    // Write header
    fwrite(&net->layer_count, sizeof(u32), 1, file);
    fwrite(&net->learning_rate, sizeof(f32), 1, file);
    
    // Write each layer
    for (u32 i = 0; i < net->layer_count; i++) {
        layer* l = &net->layers[i];
        fwrite(&l->input_size, sizeof(u32), 1, file);
        fwrite(&l->output_size, sizeof(u32), 1, file);
        fwrite(&l->activation, sizeof(activation_type), 1, file);
        fwrite(l->weights, sizeof(f32), l->weight_count, file);
        fwrite(l->biases, sizeof(f32), l->output_size, file);
    }
    
    fclose(file);
}

neural_network* neural_load(const char* filename) {
    if (!filename) return NULL;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    
    neural_network* net = neural_create();
    if (!net) {
        fclose(file);
        return NULL;
    }
    
    // Read header
    fread(&net->layer_count, sizeof(u32), 1, file);
    fread(&net->learning_rate, sizeof(f32), 1, file);
    
    // Read each layer
    for (u32 i = 0; i < net->layer_count; i++) {
        u32 input_size, output_size;
        activation_type activation;
        
        fread(&input_size, sizeof(u32), 1, file);
        fread(&output_size, sizeof(u32), 1, file);
        fread(&activation, sizeof(activation_type), 1, file);
        
        neural_add_layer(net, input_size, output_size, activation);
        
        layer* l = &net->layers[i];
        fread(l->weights, sizeof(f32), l->weight_count, file);
        fread(l->biases, sizeof(f32), l->output_size, file);
    }
    
    fclose(file);
    return net;
}