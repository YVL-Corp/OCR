#pragma once

#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

// Librairies utilisées
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

typedef struct {                     // On crée un type de donnée pour simplifier la gestion du réseau de neurones
    size_t input_size;
    size_t hidden_size;
    size_t output_size;
    double **weights_input_hidden;
    double **weights_hidden_output;
    double *bias_hidden;
    double *bias_output;
} Network;

typedef struct                      // Pareil ce sera pratique pour entrainer le réseau de neurones
{
    double *pixels;
    int label;
} TrainingExample;


// Et on initialise les fonctions qui seront utilisées
Network* create_network(int input, int hidden, int output);
void free_network(Network* net);
void initialize_weights(Network* net);
double sigmoid(double x);
double *forward(Network* net, double *input, double *hidden, double *output);
void backpropagate(Network* net, double *input, double *target, double learning_rate);
void train(Network* net, TrainingExample* examples, size_t num_examples, int epochs);

#endif

