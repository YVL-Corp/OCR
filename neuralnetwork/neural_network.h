#pragma once

#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

// libs used
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

typedef struct {                     // creation of a data type to simplify the neural network management 
    size_t input_size;
    size_t hidden_size;
    size_t output_size;
    double **weights_input_hidden;
    double **weights_hidden_output;
    double *bias_hidden;
    double *bias_output;
} Network;

typedef struct                      // same thing here it'll be easier to train the neural network
{
    double *pixels;
    int label;
} TrainingExample;


// prototypes of functions
Network* create_network(int input, int hidden, int output);
void free_network(Network* net);
void initialize_weights(Network* net);
double sigmoid(double x);
double *forward(Network* net, double *input, double *hidden, double *output);
void backpropagate(Network* net, double *input, double *target, double learning_rate);
void train(Network* net, TrainingExample* examples, size_t num_examples, int epochs);

#endif

