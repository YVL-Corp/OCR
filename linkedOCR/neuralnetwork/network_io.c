#include "network_io.h"
#include <stdio.h>
#include <stdlib.h>

int save_network(Network *net, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
        return 0;
    }

    // CONVERSION CRITIQUE : On cast en int pour rester compatible avec le format de Tristan
    // (Même si on utilise size_t en mémoire, on écrit 4 octets sur le disque)
    int input = (int)net->input_size;
    int hidden = (int)net->hidden_size;
    int output = (int)net->output_size;

    fwrite(&input, sizeof(int), 1, file);
    fwrite(&hidden, sizeof(int), 1, file);
    fwrite(&output, sizeof(int), 1, file);

    // Les poids sont des doubles, ça ne change pas (8 octets partout)
    for (size_t i = 0; i < net->input_size; i++) {
        fwrite(net->weights_input_hidden[i], sizeof(double), net->hidden_size, file);
    }

    for (size_t i = 0; i < net->hidden_size; i++) {
        fwrite(net->weights_hidden_output[i], sizeof(double), net->output_size, file);
    }

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

    // LECTURE COMPATIBLE : On lit des int (4 octets) comme dans le fichier model3.bin
    int input, hidden, output;
    
    if (fread(&input, sizeof(int), 1, file) != 1 ||
        fread(&hidden, sizeof(int), 1, file) != 1 ||
        fread(&output, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read network dimensions\n");
        fclose(file);
        return NULL;
    }

    // On crée le réseau propre avec des size_t
    Network *net = create_network((size_t)input, (size_t)hidden, (size_t)output);
    if (!net) {
        fclose(file);
        return NULL;
    }

    // Lecture des poids
    for (size_t i = 0; i < net->input_size; i++) {
        if (fread(net->weights_input_hidden[i], sizeof(double), net->hidden_size, file) != net->hidden_size) {
            fprintf(stderr, "Error: Failed to read input->hidden weights\n");
            free_network(net);
            fclose(file);
            return NULL;
        }
    }

    for (size_t i = 0; i < net->hidden_size; i++) {
        if (fread(net->weights_hidden_output[i], sizeof(double), net->output_size, file) != net->output_size) {
            fprintf(stderr, "Error: Failed to read hidden->output weights\n");
            free_network(net);
            fclose(file);
            return NULL;
        }
    }

    if (fread(net->bias_hidden, sizeof(double), net->hidden_size, file) != net->hidden_size ||
        fread(net->bias_output, sizeof(double), net->output_size, file) != net->output_size) {
        fprintf(stderr, "Error: Failed to read biases\n");
        free_network(net);
        fclose(file);
        return NULL;
    }

    fclose(file);
    printf("Network loaded from '%s' (Architecture: %d -> %d -> %d)\n", 
           filename, input, hidden, output);
    return net;
}
