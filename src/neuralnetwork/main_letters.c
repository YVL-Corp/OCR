#include "neural_network.h"
#include "image_loader.h"
#include "network_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>

#define HIDDEN_SIZE 128
#define NUM_CLASSES 26
#define MAX_IMAGES_PER_LETTER 500

void shuffle_dataset(ImageData *images, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        ImageData temp = images[i];
        images[i] = images[j];
        images[j] = temp;
    }
}

void train_network(Network *net, ImageData *images, int num_images, int epochs, float learning_rate) {
    TrainingExample *examples = malloc(num_images * sizeof(TrainingExample));
    for (int i = 0; i < num_images; i++) {
        examples[i].pixels = images[i].pixels;
        examples[i].label = images[i].label;
    }

    printf("\nStarting training...\n");
    printf("Images: %d, Epochs: %d, Learning rate: %.4f\n\n", num_images, epochs, learning_rate);

    train(net, examples, num_images, epochs, learning_rate);

    free(examples);
}

void test_network(Network *net, ImageData *images, int num_images, int num_tests) {
    printf("\n=== Testing Network ===\n");
    
    double *hidden = malloc(net->hidden_size * sizeof(double));
    double *output = malloc(net->output_size * sizeof(double));
    int correct = 0;

    for (int i = 0; i < num_tests && i < num_images; i++) {
        int test_idx = rand() % num_images;
        
        printf("\nTest %d - Expected: %c\n", i + 1, 'A' + images[test_idx].label);
        print_image(images[test_idx].pixels);
        
        forward(net, images[test_idx].pixels, hidden, output);
        
        // Trouver la prédiction
        int predicted = 0;
        double max_prob = output[0];
        for (int j = 1; j < net->output_size; j++) {
            if (output[j] > max_prob) {
                max_prob = output[j];
                predicted = j;
            }
        }
        
        printf("Predicted: %c (Confidence: %.2f%%)\n", 'A' + predicted, max_prob * 100);
        
        if (predicted == images[test_idx].label) {
            printf("✓ CORRECT\n");
            correct++;
        } else {
            printf("✗ INCORRECT\n");
        }
    }

    printf("\n========================================\n");
    printf("Test Accuracy: %d/%d (%.2f%%)\n", correct, num_tests, (double)correct / num_tests * 100.0);
    printf("========================================\n");

    free(hidden);
    free(output);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  Train:      %s train <dataset_folder> <epochs> <learning_rate> <output_file.bin>\n", argv[0]);
        fprintf(stderr, "  Continue:   %s continue <dataset_folder> <input_file.bin> <epochs> <learning_rate> <output_file.bin>\n", argv[0]);
        fprintf(stderr, "  Test:       %s test <dataset_folder> <model_file.bin> [num_tests]\n", argv[0]);
        fprintf(stderr, "  Solve:      %s solve <grid_folder> <words_folder> <model_file.bin> <output_folder>\n", argv[0]);
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  %s train ./dataset 1000 0.05 model.bin\n", argv[0]);
        fprintf(stderr, "  %s continue ./dataset model.bin 500 0.01 model_improved.bin\n", argv[0]);
        fprintf(stderr, "  %s test ./dataset model.bin 20\n", argv[0]);
        fprintf(stderr, "  %s solve ./grid_images ./words_folder model.bin ./output\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    const char *mode = argv[1];

    if (strcmp(mode, "train") == 0) {
        // ========== MODE ENTRAÎNEMENT ==========
        if (argc != 6) {
            fprintf(stderr, "Error: train mode requires 4 arguments\n");
            fprintf(stderr, "Usage: %s train <dataset_folder> <epochs> <learning_rate> <output_file.bin>\n", argv[0]);
            return 1;
        }

        const char *dataset_path = argv[2];
        int epochs = atoi(argv[3]);
        float learning_rate = atof(argv[4]);
        const char *output_file = argv[5];

        if (epochs <= 0) {
            fprintf(stderr, "Error: epochs must be positive\n");
            return 1;
        }

        if (learning_rate <= 0 || learning_rate > 1.0) {
            fprintf(stderr, "Error: learning_rate must be between 0 and 1\n");
            return 1;
        }

        printf("=== Neural Network Training Mode ===\n");
        printf("Dataset: %s\n", dataset_path);
        printf("Output file: %s\n", output_file);
        printf("Max images per letter: %d\n\n", MAX_IMAGES_PER_LETTER);

        // Charger les images
        ImageData *images;
        int num_images = load_dataset(dataset_path, &images, MAX_IMAGES_PER_LETTER);

        if (num_images == 0) {
            fprintf(stderr, "Error: No images loaded\n");
            return 1;
        }

        printf("\nTotal images loaded: %d\n", num_images);

        // Mélanger le dataset
        printf("Shuffling dataset...\n");
        shuffle_dataset(images, num_images);

        // Créer le réseau
        printf("Creating network: %d -> %d -> %d\n", PIXEL_COUNT, HIDDEN_SIZE, NUM_CLASSES);
        Network *net = create_network(PIXEL_COUNT, HIDDEN_SIZE, NUM_CLASSES);

        // Entraîner
        train_network(net, images, num_images, epochs, learning_rate);

        // Sauvegarder
        if (save_network(net, output_file)) {
            printf("\n✓ Model saved successfully!\n");
        } else {
            fprintf(stderr, "\n✗ Failed to save model\n");
        }

        // Tester
        test_network(net, images, num_images, 10);

        // Libérer la mémoire
        free_images(images, num_images);
        free_network(net);

    } else if (strcmp(mode, "continue") == 0) {
        // ========== MODE CONTINUATION ==========
        if (argc != 7) {
            fprintf(stderr, "Error: continue mode requires 5 arguments\n");
            fprintf(stderr, "Usage: %s continue <dataset_folder> <input_file.bin> <epochs> <learning_rate> <output_file.bin>\n", argv[0]);
            return 1;
        }

        const char *dataset_path = argv[2];
        const char *input_file = argv[3];
        int epochs = atoi(argv[4]);
        float learning_rate = atof(argv[5]);
        const char *output_file = argv[6];

        if (epochs <= 0) {
            fprintf(stderr, "Error: epochs must be positive\n");
            return 1;
        }

        if (learning_rate <= 0 || learning_rate > 1.0) {
            fprintf(stderr, "Error: learning_rate must be between 0 and 1\n");
            return 1;
        }

        printf("=== Neural Network Continue Training Mode ===\n");
        printf("Dataset: %s\n", dataset_path);
        printf("Input model: %s\n", input_file);
        printf("Output model: %s\n", output_file);
        printf("Additional epochs: %d\n", epochs);
        printf("Learning rate: %.4f\n\n", learning_rate);

        // Charger le modèle existant
        Network *net = load_network(input_file);
        if (!net) {
            fprintf(stderr, "Error: Failed to load model\n");
            return 1;
        }

        printf("Model loaded successfully!\n");
        printf("Network architecture: %ld -> %ld -> %ld\n\n", 
               net->input_size, net->hidden_size, net->output_size);

        // Charger les images
        ImageData *images;
        int num_images = load_dataset(dataset_path, &images, MAX_IMAGES_PER_LETTER);

        if (num_images == 0) {
            fprintf(stderr, "Error: No images loaded\n");
            free_network(net);
            return 1;
        }

        printf("Total images loaded: %d\n", num_images);

        // Mélanger le dataset
        printf("Shuffling dataset...\n");
        shuffle_dataset(images, num_images);

        // Continuer l'entraînement
        train_network(net, images, num_images, epochs, learning_rate);

        // Sauvegarder le modèle amélioré
        if (save_network(net, output_file)) {
            printf("\n✓ Improved model saved successfully!\n");
        } else {
            fprintf(stderr, "\n✗ Failed to save model\n");
        }

        // Tester
        test_network(net, images, num_images, 10);

        // Libérer la mémoire
        free_images(images, num_images);
        free_network(net);

    } else if (strcmp(mode, "test") == 0) {
        // ========== MODE TEST ==========
        if (argc < 4) {
            fprintf(stderr, "Error: test mode requires at least 2 arguments\n");
            fprintf(stderr, "Usage: %s test <dataset_folder> <model_file.bin> [num_tests]\n", argv[0]);
            return 1;
        }

        const char *dataset_path = argv[2];
        const char *model_file = argv[3];
        int num_tests = (argc >= 5) ? atoi(argv[4]) : 10;

        printf("=== Neural Network Test Mode ===\n");
        printf("Dataset: %s\n", dataset_path);
        printf("Model file: %s\n", model_file);
        printf("Number of tests: %d\n\n", num_tests);

        // Charger le modèle
        Network *net = load_network(model_file);
        if (!net) {
            fprintf(stderr, "Error: Failed to load model\n");
            return 1;
        }

        printf("Network architecture: %ld -> %ld -> %ld\n\n", 
               net->input_size, net->hidden_size, net->output_size);

        // Charger les images
        ImageData *images;
        int num_images = load_dataset(dataset_path, &images, MAX_IMAGES_PER_LETTER);

        if (num_images == 0) {
            fprintf(stderr, "Error: No images loaded\n");
            free_network(net);
            return 1;
        }

        printf("Total images loaded: %d\n", num_images);

        // Tester
        test_network(net, images, num_images, num_tests);

        // Libérer la mémoire
        free_images(images, num_images);
        free_network(net);

    } else if (strcmp(mode, "predict") == 0) {
        // ========== MODE PRÉDICTION GRILLE ==========
        if (argc != 5) {
            fprintf(stderr, "Error: predict mode requires 3 arguments\n");
            fprintf(stderr, "Usage: %s predict <grid_folder> <model_file.bin> <output_file.txt>\n", argv[0]);
            return 1;
        }

        const char *grid_folder = argv[2];
        const char *model_file = argv[3];
        const char *output_file = argv[4];

        printf("=== Neural Network Grid Prediction Mode ===\n");
        printf("Grid folder: %s\n", grid_folder);
        printf("Model file: %s\n", model_file);
        printf("Output file: %s\n\n", output_file);

        // Charger le modèle
        Network *net = load_network(model_file);
        if (!net) {
            fprintf(stderr, "Error: Failed to load model\n");
            return 1;
        }

        // Charger les images de la grille
        GridLetter *letters;
        int num_letters = load_grid_images(grid_folder, &letters);

        if (num_letters == 0) {
            fprintf(stderr, "Error: No grid images loaded\n");
            free_network(net);
            return 1;
        }

        printf("Processing %d letters...\n", num_letters);

        // Prédire chaque lettre
        double *hidden = malloc(net->hidden_size * sizeof(double));
        double *output = malloc(net->output_size * sizeof(double));

        for (int i = 0; i < num_letters; i++) {
            forward(net, letters[i].pixels, hidden, output);
            
            // Trouver la prédiction
            int predicted = 0;
            double max_prob = output[0];
            for (int j = 1; j < net->output_size; j++) {
                if (output[j] > max_prob) {
                    max_prob = output[j];
                    predicted = j;
                }
            }
            
            letters[i].predicted_letter = 'A' + predicted;
            
            if ((i + 1) % 50 == 0) {
                printf("  Processed %d/%d letters\n", i + 1, num_letters);
            }
        }

        free(hidden);
        free(output);

        printf("All letters predicted!\n\n");

        // Déterminer les dimensions de la grille (min et max)
        int min_x = letters[0].x, max_x = letters[0].x;
        int min_y = letters[0].y, max_y = letters[0].y;
        
        for (int i = 1; i < num_letters; i++) {
            if (letters[i].x < min_x) min_x = letters[i].x;
            if (letters[i].x > max_x) max_x = letters[i].x;
            if (letters[i].y < min_y) min_y = letters[i].y;
            if (letters[i].y > max_y) max_y = letters[i].y;
        }

        int grid_width = max_x - min_x + 1;
        int grid_height = max_y - min_y + 1;

        printf("Grid coordinates: X=[%d..%d], Y=[%d..%d]\n", min_x, max_x, min_y, max_y);
        printf("Grid dimensions: %d x %d\n", grid_width, grid_height);

        // Créer la grille
        char **grid = malloc(grid_height * sizeof(char*));
        for (int i = 0; i < grid_height; i++) {
            grid[i] = malloc(grid_width * sizeof(char));
            for (int j = 0; j < grid_width; j++) {
                grid[i][j] = ' '; // Espaces par défaut
            }
        }

        // Remplir la grille
        for (int i = 0; i < num_letters; i++) {
            int x = letters[i].x - min_x;  // Normaliser par rapport au minimum
            int y = letters[i].y - min_y;  // Normaliser par rapport au minimum
            if (x >= 0 && x < grid_width && y >= 0 && y < grid_height) {
                grid[y][x] = letters[i].predicted_letter;
            }
        }

        // Afficher la grille
        printf("\nReconstructed grid:\n");
        printf("===================\n");
        for (int y = 0; y < grid_height; y++) {
            for (int x = 0; x < grid_width; x++) {
                printf("%c ", grid[y][x]);
            }
            printf("\n");
        }
        printf("===================\n\n");

        // Sauvegarder dans un fichier
        FILE *file = fopen(output_file, "w");
        if (!file) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
        } else {
            for (int y = 0; y < grid_height; y++) {
                for (int x = 0; x < grid_width; x++) {
                    fprintf(file, "%c", grid[y][x]);
                }
                fprintf(file, "\n");
            }
            fclose(file);
            printf("✓ Grid saved to '%s'\n", output_file);
        }

        // Libérer la mémoire
        for (int i = 0; i < grid_height; i++) {
            free(grid[i]);
        }
        free(grid);
        free_grid_letters(letters, num_letters);
        free_network(net);

    } else if (strcmp(mode, "words") == 0) {
        // ========== MODE DÉCHIFFRAGE DE MOTS ==========
        if (argc != 5) {
            fprintf(stderr, "Error: words mode requires 3 arguments\n");
            fprintf(stderr, "Usage: %s words <words_folder> <model_file.bin> <output_file.txt>\n", argv[0]);
            fprintf(stderr, "\nExpected structure:\n");
            fprintf(stderr, "  words_folder/\n");
            fprintf(stderr, "    word1/\n");
            fprintf(stderr, "      0.bmp (première lettre)\n");
            fprintf(stderr, "      1.bmp (deuxième lettre)\n");
            fprintf(stderr, "      2.bmp\n");
            fprintf(stderr, "      ...\n");
            fprintf(stderr, "    word2/\n");
            fprintf(stderr, "      0.bmp\n");
            fprintf(stderr, "      1.bmp\n");
            fprintf(stderr, "      ...\n");
            return 1;
        }

        const char *words_folder = argv[2];
        const char *model_file = argv[3];
        const char *output_file = argv[4];

        printf("=== Neural Network Word Decryption Mode ===\n");
        printf("Words folder: %s\n", words_folder);
        printf("Model file: %s\n", model_file);
        printf("Output file: %s\n\n", output_file);

        // Charger le modèle
        Network *net = load_network(model_file);
        if (!net) {
            fprintf(stderr, "Error: Failed to load model\n");
            return 1;
        }

        // Prédire tous les mots
        printf("Decrypting words...\n");
        Word *words;
        int num_words = load_and_predict_words(words_folder, net, &words);

        if (num_words == 0) {
            fprintf(stderr, "Error: No words found\n");
            free_network(net);
            return 1;
        }

        printf("\n========================================\n");
        printf("Decrypted %d words:\n", num_words);
        printf("========================================\n");

        for (int i = 0; i < num_words; i++) {
            printf("%2d. %s (%d letters)\n", i + 1, words[i].word, words[i].length);
        }

        printf("========================================\n\n");

        // Sauvegarder dans un fichier
        FILE *file = fopen(output_file, "w");
        if (!file) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
        } else {
            fprintf(file, "Words to find:\n");
            fprintf(file, "==============\n\n");
            for (int i = 0; i < num_words; i++) {
                fprintf(file, "%s\n", words[i].word);
            }
            fclose(file);
            printf("✓ Words saved to '%s'\n", output_file);
        }

        // Libérer la mémoire
        free_words(words, num_words);
        free_network(net);

    } else {
        fprintf(stderr, "Error: Unknown mode '%s'\n", mode);
        fprintf(stderr, "Valid modes: train, continue, test, predict, words\n");
        return 1;
    }

    printf("\nDone!\n");
    return 0;
}