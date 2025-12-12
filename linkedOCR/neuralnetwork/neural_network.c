#include "neural_network.h" 

double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

// On utilise bien size_t pour les dimensions en mémoire
Network *create_network(size_t input, size_t hidden, size_t output) {
    Network *net = (Network *)malloc(sizeof(Network));
    net->input_size = input;
    net->hidden_size = hidden;
    net->output_size = output;

    net->weights_input_hidden = malloc(input * sizeof(double*));
    for (size_t i = 0; i < input; i++) {
        net->weights_input_hidden[i] = malloc(hidden * sizeof(double));
    }
    
    net->weights_hidden_output = malloc(hidden * sizeof(double*));
    for (size_t i = 0; i < hidden; i++) {
        net->weights_hidden_output[i] = malloc(output * sizeof(double));
    }
    
    net->bias_hidden = (double *)malloc(hidden * sizeof(double));
    net->bias_output = (double *)malloc(output * sizeof(double));

    initialize_weights(net);
    return net;
}

void initialize_weights(Network *net) {
    static int seeded = 0;
    if (!seeded) {
        srand(time(NULL));
        seeded = 1;
    }
    
    // Utilisation de size_t pour les boucles -> plus de warnings
    for (size_t i = 0; i < net->input_size; i++) {
        for (size_t j = 0; j < net->hidden_size; j++) {
            net->weights_input_hidden[i][j] = ((double)rand() / RAND_MAX) - 0.5;
        }
    }
    
    for (size_t i = 0; i < net->hidden_size; i++) {
        for (size_t j = 0; j < net->output_size; j++) {
            net->weights_hidden_output[i][j] = ((double)rand() / RAND_MAX) - 0.5;
        }
    }
    
    for (size_t i = 0; i < net->hidden_size; i++) {
        net->bias_hidden[i] = 0.0;
    }
    
    for (size_t i = 0; i < net->output_size; i++) {
        net->bias_output[i] = 0.0;
    }
}

void free_network(Network *net) {
    for (size_t i = 0; i < net->input_size; i++) {
        free(net->weights_input_hidden[i]);
    }
    free(net->weights_input_hidden);
    
    for (size_t i = 0; i < net->hidden_size; i++) {
        free(net->weights_hidden_output[i]);
    }
    free(net->weights_hidden_output);
    
    free(net->bias_hidden);
    free(net->bias_output);
    free(net);
}

double *forward(Network *net, double *input, double *hidden, double *output) {
    for (size_t i = 0; i < net->hidden_size; i++) {
        hidden[i] = net->bias_hidden[i];
        for (size_t j = 0; j < net->input_size; j++) {
            hidden[i] += input[j] * net->weights_input_hidden[j][i];
        }
        hidden[i] = sigmoid(hidden[i]);
    }
    
    for (size_t i = 0; i < net->output_size; i++) {
        output[i] = net->bias_output[i];
        for (size_t j = 0; j < net->hidden_size; j++) {
            output[i] += hidden[j] * net->weights_hidden_output[j][i];
        }
    }

    if (net->output_size == 1) {
        output[0] = sigmoid(output[0]);
    } else {
        double max_val = output[0];
        for (size_t i = 1; i < net->output_size; i++) {
            if (output[i] > max_val) max_val = output[i];
        }
        double sum = 0.0;
        for (size_t i = 0; i < net->output_size; i++) {
            output[i] = exp(output[i] - max_val);
            sum += output[i];
        }
        for (size_t i = 0; i < net->output_size; i++) {
            output[i] /= sum;
        }
    }
    return output;
}

void backpropagate(Network* net, double *input, double *target, double learning_rate) {
    double *hidden = malloc(net->hidden_size * sizeof(double));
    double *output = malloc(net->output_size * sizeof(double));
    
    forward(net, input, hidden, output);
    
    double *output_errors = malloc(net->output_size * sizeof(double));
    if (net->output_size == 1) {
        output_errors[0] = (output[0] - target[0]) * output[0] * (1.0 - output[0]);
    } else {
        for (size_t i = 0; i < net->output_size; i++) {
            output_errors[i] = output[i] - target[i];
        }
    }

    for (size_t i = 0; i < net->hidden_size; i++) {
        for (size_t j = 0; j < net->output_size; j++) {
            double gradient = hidden[i] * output_errors[j];
            net->weights_hidden_output[i][j] -= learning_rate * gradient;
        }
    }  

    for (size_t i = 0; i < net->output_size; i++) {
        net->bias_output[i] -= learning_rate * output_errors[i];
    }

    double *delta_hidden = malloc(net->hidden_size * sizeof(double));
    for (size_t i = 0; i < net->hidden_size; i++) {
        delta_hidden[i] = 0.0;
        for (size_t j = 0; j < net->output_size; j++) {
            delta_hidden[i] += output_errors[j] * net->weights_hidden_output[i][j];
        }
        delta_hidden[i] *= hidden[i] * (1.0 - hidden[i]);
    }

    for (size_t i = 0; i < net->input_size; i++) {
        for (size_t j = 0; j < net->hidden_size; j++) {
            double gradient = input[i] * delta_hidden[j];
            net->weights_input_hidden[i][j] -= learning_rate * gradient;
        }
    }
    for (size_t i = 0; i < net->hidden_size; i++) {
        net->bias_hidden[i] -= learning_rate * delta_hidden[i];
    }

    free(output_errors);
    free(delta_hidden);
    free(hidden);
    free(output);
}

void train(Network* net, TrainingExample* examples, size_t num_examples, int epochs, float learning_rate) {
    printf("Training started...\n");
    for (int epoch = 0; epoch < epochs; epoch++) {
        for (size_t i = 0; i < num_examples; i++) {
            double *target = calloc(net->output_size, sizeof(double));
            target[examples[i].label] = 1.0;
            backpropagate(net, examples[i].pixels, target, learning_rate);
            free(target);
        }

        if ((epoch + 1) % 100 == 0) {
            int correct = 0;
            double *hidden = malloc(net->hidden_size * sizeof(double));
            double *output = malloc(net->output_size * sizeof(double));

            for (size_t i = 0; i < num_examples; i++) {
                forward(net, examples[i].pixels, hidden, output);
                int predicted = 0;
                double max_prob = output[0];
                for (size_t j = 1; j < net->output_size; j++) {
                    if (output[j] > max_prob) {
                        max_prob = output[j];
                        predicted = (int)j;
                    }
                }
                if (predicted == examples[i].label) {
                    correct++;
                }
            }
            free(hidden);
            free(output);

            double accuracy = (double)correct / num_examples * 100.0;
            printf("Epoch %d/%d - Accuracy: %.2f%%\n", epoch + 1, epochs, accuracy);

            if (accuracy > 99.0) {
                break;
            }
        }
    }
}
