#include "neural_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LETTER_WIDTH 5
#define LETTER_HEIGHT 5
#define LETTER_SIZE (LETTER_WIDTH * LETTER_HEIGHT)
#define NUM_LETTERS 3
#define TRAINING_SAMPLES 30

// Définir des lettres simples en grille 5x5
double letter_A[LETTER_SIZE] = {
    0, 1, 1, 1, 0,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 1,
    1, 0, 0, 0, 1,
    1, 0, 0, 0, 1
};

double letter_B[LETTER_SIZE] = {
    1, 1, 1, 1, 0,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 0,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 0
};

double letter_C[LETTER_SIZE] = {
    0, 1, 1, 1, 1,
    1, 0, 0, 0, 0,
    1, 0, 0, 0, 0,
    1, 0, 0, 0, 0,
    0, 1, 1, 1, 1
};

// Fonction pour ajouter du bruit à une lettre (créer des variations)
void add_noise(double *original, double *noisy, double noise_level) {
    for (int i = 0; i < LETTER_SIZE; i++) {
        noisy[i] = original[i];
        // Ajouter du bruit aléatoire
        if ((double)rand() / RAND_MAX < noise_level) {
            noisy[i] = 1.0 - noisy[i];
        }
    }
}

// Fonction pour afficher une lettre
void print_letter(double *letter) {
    for (int i = 0; i < LETTER_HEIGHT; i++) {
        for (int j = 0; j < LETTER_WIDTH; j++) {
            printf("%s", letter[i * LETTER_WIDTH + j] > 0.5 ? "█" : " ");
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    // Vérifier les arguments
    if (argc != 3) {
        if(argc == 1)
        {
            printf("No epochs/learning rate specified, using 1000 epochs and 0.5 learning rate by default\n");
        }
        if(argc == 2)
        {
            printf("No learning rate specified, using 0.5 by default\n");
        }
    }

    int epochs = atoi(argv[1]);
    float learning_rate = atof(argv[2]);

    if (epochs <= 0) {
        fprintf(stderr, "Error: epochs must be positive\n");
        return 1;
    }

    if (learning_rate <= 0 || learning_rate > 1.0) {
        fprintf(stderr, "Error: learning_rate must be between 0 and 1\n");
        return 1;
    }

    printf("=== Neural Network Letter Recognition ===\n");
    printf("Epochs: %d\n", epochs);
    printf("Learning Rate: %.4f\n\n", learning_rate);

    // Créer et afficher les lettres de base
    printf("Training letters:\n");
    printf("Letter A:\n");
    print_letter(letter_A);

    printf("Letter B:\n");
    print_letter(letter_B);

    printf("Letter C:\n");
    print_letter(letter_C);

    // Créer le réseau de neurones
    // Input: 25 (5x5 pixels), Hidden: 16, Output: 3 (A, B, C)
    Network *net = create_network(LETTER_SIZE, 16, NUM_LETTERS);

    // Allouer de la mémoire pour les exemples d'entraînement
    TrainingExample *examples = malloc(TRAINING_SAMPLES * sizeof(TrainingExample));

    // Générer les données d'entraînement avec du bruit
    for (int i = 0; i < TRAINING_SAMPLES; i++) {
        examples[i].pixels = malloc(LETTER_SIZE * sizeof(double));

        if (i < TRAINING_SAMPLES / 3) {
            // Ajouter des variations de la lettre A
            add_noise(letter_A, examples[i].pixels, 0.1);
            examples[i].label = 0;
        } else if (i < 2 * TRAINING_SAMPLES / 3) {
            // Ajouter des variations de la lettre B
            add_noise(letter_B, examples[i].pixels, 0.1);
            examples[i].label = 1;
        } else {
            // Ajouter des variations de la lettre C
            add_noise(letter_C, examples[i].pixels, 0.1);
            examples[i].label = 2;
        }
    }

    printf("Starting training with %d samples...\n\n", TRAINING_SAMPLES);

    // Entraîner le réseau
    train(net, examples, TRAINING_SAMPLES, epochs, learning_rate);

    // Test sur les lettres originales
    printf("\n=== Test Results ===\n");
    double hidden[16];
    double output[NUM_LETTERS];

    printf("Testing letter A:\n");
    print_letter(letter_A);
    forward(net, letter_A, hidden, output);
    printf("Output probabilities: A=%.4f, B=%.4f, C=%.4f\n\n", output[0], output[1], output[2]);

    printf("Testing letter B:\n");
    print_letter(letter_B);
    forward(net, letter_B, hidden, output);
    printf("Output probabilities: A=%.4f, B=%.4f, C=%.4f\n\n", output[0], output[1], output[2]);

    printf("Testing letter C:\n");
    print_letter(letter_C);
    forward(net, letter_C, hidden, output);
    printf("Output probabilities: A=%.4f, B=%.4f, C=%.4f\n\n", output[0], output[1], output[2]);

    // Libérer la mémoire
    for (int i = 0; i < TRAINING_SAMPLES; i++) {
        free(examples[i].pixels);
    }
    free(examples);
    free_network(net);

    printf("Training completed!\n");
    return 0;
}