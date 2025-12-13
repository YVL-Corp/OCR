#include "network_io.h"
#include <stdio.h>
#include <stdlib.h>

int save_network(Network *net, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
        return 0;
    }

    // Write data of size of 4 bytes
    int input = (int)net->input_size;
    int hidden = (int)net->hidden_size;
    int output = (int)net->output_size;

    fwrite(&input, sizeof(int), 1, file);
    fwrite(&hidden, sizeof(int), 1, file);
    fwrite(&output, sizeof(int), 1, file);


    // Write first layer (input)
    for (size_t i = 0; i < net->input_size; i++) {
        fwrite(net->weights_input_hidden[i], sizeof(double), net->hidden_size, file);
    }

    // Second layer (hidden)
    for (size_t i = 0; i < net->hidden_size; i++) {
        fwrite(net->weights_hidden_output[i], sizeof(double), net->output_size, file);
    }

    // Thrid layer (output)
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

    // Incompatibility problems when reading/writing size_t data so we use int
    int input, hidden, output;
    
    // Check if parse seems correct
    if (fread(&input, sizeof(int), 1, file) != 1 ||
        fread(&hidden, sizeof(int), 1, file) != 1 ||
        fread(&output, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read network dimensions\n");
        fclose(file);
        return NULL;
    }

    // Recreate network with parsed data
    Network *net = create_network((size_t)input, (size_t)hidden, (size_t)output);
    if (!net) {
        fclose(file);
        return NULL;
    }

    // Read weights for input layers
    for (size_t i = 0; i < net->input_size; i++) {
        if (fread(net->weights_input_hidden[i], sizeof(double), net->hidden_size, file) != net->hidden_size) {
            fprintf(stderr, "Error: Failed to read input->hidden weights\n");
            free_network(net);
            fclose(file);
            return NULL;
        }
    }

    // Read weights for hidden layers
    for (size_t i = 0; i < net->hidden_size; i++) {
        if (fread(net->weights_hidden_output[i], sizeof(double), net->output_size, file) != net->output_size) {
            fprintf(stderr, "Error: Failed to read hidden->output weights\n");
            free_network(net);
            fclose(file);
            return NULL;
        }
    }

    // Read weights for output layers
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
