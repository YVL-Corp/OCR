#include "network_io.h"
#include <stdio.h>
#include <stdlib.h>

int save_network(Network *net, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
        return 0;
    }

    // Écrire les dimensions du réseau
    fwrite(&net->input_size, sizeof(int), 1, file);
    fwrite(&net->hidden_size, sizeof(int), 1, file);
    fwrite(&net->output_size, sizeof(int), 1, file);

    // Écrire les poids input -> hidden
    for (int i = 0; i < net->input_size; i++) {
        fwrite(net->weights_input_hidden[i], sizeof(double), net->hidden_size, file);
    }

    // Écrire les poids hidden -> output
    for (int i = 0; i < net->hidden_size; i++) {
        fwrite(net->weights_hidden_output[i], sizeof(double), net->output_size, file);
    }

    // Écrire les biais
    fwrite(net->bias_hidden, sizeof(double), net->hidden_size, file);
    fwrite(net->bias_output, sizeof(double), net->output_size, file);

    fclose(file);
    printf("Network saved to '%s'\n", filename);
    return 1;
}

Network* load_network(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s' for reading\n", filename);
        return NULL;
    }

    // Lire les dimensions
    int input_size, hidden_size, output_size;
    if (fread(&input_size, sizeof(int), 1, file) != 1 ||
        fread(&hidden_size, sizeof(int), 1, file) != 1 ||
        fread(&output_size, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read network dimensions\n");
        fclose(file);
        return NULL;
    }

    // Créer le réseau avec ces dimensions
    Network *net = create_network(input_size, hidden_size, output_size);
    if (!net) {
        fclose(file);
        return NULL;
    }

    // Lire les poids input -> hidden
    for (int i = 0; i < net->input_size; i++) {
        if (fread(net->weights_input_hidden[i], sizeof(double), net->hidden_size, file) != net->hidden_size) {
            fprintf(stderr, "Error: Failed to read input->hidden weights\n");
            free_network(net);
            fclose(file);
            return NULL;
        }
    }

    // Lire les poids hidden -> output
    for (int i = 0; i < net->hidden_size; i++) {
        if (fread(net->weights_hidden_output[i], sizeof(double), net->output_size, file) != net->output_size) {
            fprintf(stderr, "Error: Failed to read hidden->output weights\n");
            free_network(net);
            fclose(file);
            return NULL;
        }
    }

    // Lire les biais
    if (fread(net->bias_hidden, sizeof(double), net->hidden_size, file) != net->hidden_size ||
        fread(net->bias_output, sizeof(double), net->output_size, file) != net->output_size) {
        fprintf(stderr, "Error: Failed to read biases\n");
        free_network(net);
        fclose(file);
        return NULL;
    }

    fclose(file);
    printf("Network loaded from '%s'\n", filename);
    return net;
}