#include "neural_network.h"

void create_test_xor() {
    printf("=== Test XOR : -A.B + A.-B ===\n\n");
    
    // Create a small network : 2 inputs → 3 hidden → 1 output
    Network *net = create_network(2, 3, 1);
    initialize_weights(net);
    
    // Dataset XOR : 4 exemples
    TrainingExample xor_data[4];
    
    // A=0, B=0 → Output=0
    xor_data[0].pixels = malloc(2 * sizeof(double));
    xor_data[0].pixels[0] = 0.0;
    xor_data[0].pixels[1] = 0.0;
    xor_data[0].label = 0;
    
    // A=0, B=1 → Output=1
    xor_data[1].pixels = malloc(2 * sizeof(double));
    xor_data[1].pixels[0] = 0.0;
    xor_data[1].pixels[1] = 1.0;
    xor_data[1].label = 1;
    
    // A=1, B=0 → Output=1
    xor_data[2].pixels = malloc(2 * sizeof(double));
    xor_data[2].pixels[0] = 1.0;
    xor_data[2].pixels[1] = 0.0;
    xor_data[2].label = 1;
    
    // A=1, B=1 → Output=0
    xor_data[3].pixels = malloc(2 * sizeof(double));
    xor_data[3].pixels[0] = 1.0;
    xor_data[3].pixels[1] = 1.0;
    xor_data[3].label = 0;
    
    // Train
    printf("Entraînement sur XOR...\n");
    for (int epoch = 0; epoch < 5000; epoch++) {
        for (int i = 0; i < 4; i++) {
            double *target = calloc(1, sizeof(double));
            target[0] = xor_data[i].label;
            backpropagate(net, xor_data[i].pixels, target, 0.5);
            free(target);
        }
        
        if ((epoch + 1) % 1000 == 0) {
            printf("Epoch %d/5000\n", epoch + 1);
        }
    }
    
    // Test
    printf("\n=== Résultats ===\n");
    double *hidden = malloc(3 * sizeof(double));
    double *output = malloc(1 * sizeof(double));
    
    printf("A | B | XOR | Prédiction\n");
    printf("--+---+-----+-----------\n");
    
    for (int i = 0; i < 4; i++) {
        forward(net, xor_data[i].pixels, hidden, output);
        int A = (int)xor_data[i].pixels[0];
        int B = (int)xor_data[i].pixels[1];
        int expected = xor_data[i].label;
        
        printf("%d | %d |  %d  | %.4f (%s)\n", 
               A, B, expected, output[0],
               (output[0] > 0.5) == expected ? "✓" : "✗");
    }
    
    // Free allocated memory
    for (int i = 0; i < 4; i++) {
        free(xor_data[i].pixels);
    }
    free(hidden);
    free(output);
    free_network(net);
}

// Version générique pour n'importe quelle opération binaire
void train_binary_operation(const char *name, int truth_table[4]) {
    printf("\n=== Test %s ===\n", name);
    
    Network *net = create_network(2, 4, 1);
    initialize_weights(net);
    
    TrainingExample data[4];
    double inputs[4][2] = {{0,0}, {0,1}, {1,0}, {1,1}};
    
    for (int i = 0; i < 4; i++) {
        data[i].pixels = malloc(2 * sizeof(double));
        data[i].pixels[0] = inputs[i][0];
        data[i].pixels[1] = inputs[i][1];
        data[i].label = truth_table[i];
    }
    
    // Entraînement
    for (int epoch = 0; epoch < 3000; epoch++) {
        for (int i = 0; i < 4; i++) {
            double *target = calloc(1, sizeof(double));
            target[0] = data[i].label;
            backpropagate(net, data[i].pixels, target, 0.5);
            free(target);
        }
    }
    
    // Test
    double *hidden = malloc(4 * sizeof(double));
    double *output = malloc(1 * sizeof(double));
    
    printf("A | B | %s\n", name);
    printf("--+---+-----\n");
    
    for (int i = 0; i < 4; i++) {
        forward(net, data[i].pixels, hidden, output);
        printf("%d | %d | %.2f\n", 
               (int)data[i].pixels[0], 
               (int)data[i].pixels[1], 
               output[0]);
    }
    
    for (int i = 0; i < 4; i++) free(data[i].pixels);
    free(hidden);
    free(output);
    free_network(net);
}

int main() 
{

    // Test XOR : -A.B + A.-B

    printf("Création du test XOR...\n");
    create_test_xor();
    printf("Fin du test XOR.\n");

    // Test AND : A.B
    printf("\n=== Test AND : A.B ===\n");
    int and_table[4] = {0, 0, 0, 1};  // 0,0→0  0,1→0  1,0→0  1,1→1
    train_binary_operation("AND", and_table);
    printf("Fin du test AND.\n");
    
    // Test OR : A+B
    int or_table[4] = {0, 1, 1, 1};   // 0,0→0  0,1→1  1,0→1  1,1→1
    train_binary_operation("OR", or_table);
    
    // Test NAND : -(A.B)
    int nand_table[4] = {1, 1, 1, 0}; // 0,0→1  0,1→1  1,0→1  1,1→0
    train_binary_operation("NAND", nand_table);
    
    // Test XNOR (A == B) : -A.-B + A.B
    int xnor_table[4] = {1, 0, 0, 1}; // 0,0→1  0,1→0  1,0→0  1,1→1
    train_binary_operation("XNOR (-A.-B + A.B)", xnor_table);

    // ALWAYS free memory
    free(hidden);
    free(output);
    free_network(net);

    
    return 0;
}