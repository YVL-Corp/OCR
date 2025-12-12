#include "nn_module.h" 
#include "neural_network.h"
#include "image_loader.h"
#include "network_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib.h> // Nécessaire pour guint/gchar, etc.

// --- Constantes du Réseau ---
#define HIDDEN_SIZE 64
#define NUM_CLASSES 26
#define MAX_IMAGES_PER_LETTER 3000

// --- Fonctions Helper Statiques ---

// Similaire à la fonction shuffle_dataset du main original, pour mélanger un dataset ImageData
static void shuffle_dataset(ImageData *images, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        ImageData temp = images[i];
        images[i] = images[j];
        images[j] = temp;
    }
}

// --- Logique Coeur: Prédiction de la Grille (Crée grid.txt) ---

static int core_predict_grid(Network *net, const char *grid_folder, const char *output_file) {
    GridLetter *letters;
    int num_letters = load_grid_images(grid_folder, &letters);
    
    if (num_letters == 0) { 
        return 0; 
    }

    printf("  > Processing grid (%d letters)...\n", num_letters);
    
    // Allocation des buffers de prédiction
    double *hidden = malloc(net->hidden_size * sizeof(double));
    double *output = malloc(net->output_size * sizeof(double));

    int min_x = letters[0].x, max_x = letters[0].x;
    int min_y = letters[0].y, max_y = letters[0].y;

    // Boucle de prédiction
    for (int i = 0; i < num_letters; i++) {
        forward(net, letters[i].pixels, hidden, output);
        int predicted = 0;
        double max_prob = output[0];
        
        for (size_t j = 1; j < net->output_size; j++) {
            if (output[j] > max_prob) { 
                max_prob = output[j]; 
                predicted = (int)j; 
            }
        }
        letters[i].predicted_letter = 'A' + predicted;

        // Mise à jour des bornes de la grille
        if (letters[i].x < min_x) min_x = letters[i].x;
        if (letters[i].x > max_x) max_x = letters[i].x;
        if (letters[i].y < min_y) min_y = letters[i].y;
        if (letters[i].y > max_y) max_y = letters[i].y;
    }

    // Reconstruction 2D de la grille
    int width = max_x - min_x + 1;
    int height = max_y - min_y + 1;
    
    char **grid = malloc(height * sizeof(char*));
    for(int i=0; i<height; i++) {
        grid[i] = malloc((width + 1) * sizeof(char));
        memset(grid[i], ' ', width);
        grid[i][width] = '\0';
    }

    for (int i = 0; i < num_letters; i++) {
        int x = letters[i].x - min_x;
        int y = letters[i].y - min_y;
        if (x >= 0 && x < width && y >= 0 && y < height)
            grid[y][x] = letters[i].predicted_letter;
    }

    // --- AFFICHAGE PROPRE DE LA GRILLE DANS LA CONSOLE ---
    printf("\n  [Grid OCR Result]\n");
    
    // Calcul de la largeur totale de la ligne d'affichage 
    int display_width = width * 2 + 2; 

    // Bordure supérieure
    printf("  +");
    for(int j=0; j<display_width - 2; j++) printf("-");
    printf("+\n");
    
    // Contenu de la grille
    FILE *f = fopen(output_file, "w");
    if (f) {
        for(int i=0; i<height; i++) {
            // Affichage dans la console (avec espaces)
            printf("  | ");
            for(int j=0; j<width; j++) {
                printf("%c ", grid[i][j]);
            }
            printf("|\n");
            
            // Écriture dans le fichier (sans espaces)
            fprintf(f, "%s\n", grid[i]);
            
            free(grid[i]); // Libération de la mémoire de la ligne
        }
        fclose(f);
        
        // Bordure inférieure
        printf("  +");
        for(int j=0; j<display_width - 2; j++) printf("-");
        printf("+\n");
        
        printf("  ✓ Grid saved to %s\n", output_file);
    } else {
        fprintf(stderr, "Error saving grid to %s\n", output_file);
    }
    // ----------------------------------------------------
    
    free(grid); // Libère le tableau de pointeurs
    free(hidden); free(output);
    free_grid_letters(letters, num_letters);
    return 1;
}

// --- Logique Coeur: Prédiction des Mots (Crée words.txt) ---

static int core_predict_words(Network *net, const char *words_folder, const char *output_file) {
    Word *words_list;
    // load_and_predict_words est une fonction fournie (implémentée dans image_loader.c)
    int num_words = load_and_predict_words(words_folder, net, &words_list);
    
    if (num_words == 0) {
        return 0;
    }

    FILE *f = fopen(output_file, "w");
    if (f) {
        printf("  > Decrypted %d words:\n", num_words);
        for (int i = 0; i < num_words; i++) {
            printf("    - %s\n", words_list[i].word);
            fprintf(f, "%s\n", words_list[i].word);
        }
        fclose(f);
        printf("  ✓ Words list saved to %s\n", output_file);
    } else {
        perror("Error saving words file");
    }

    free_words(words_list, num_words);
    return 1;
}

// --- Fonction Exportée du Module ---

int nn_run_recognition(const char *root_folder, const char *model_file) {
    
    printf("\n=== NEURAL NETWORK MODULE (NN) ===\n");
    printf("Processing images in: %s\n", root_folder);
    printf("Using model: %s\n", model_file);

    // Charger le modèle (utilise une logique d'I/O compatible pour model3.bin)
    Network *net = load_network(model_file);
    if (!net) {
        fprintf(stderr, "CRITICAL: Failed to load model %s.\n", model_file);
        return 1;
    }

    // Construction des chemins d'entrée/sortie
    char grid_path[1024];
    char words_path[1024];
    char grid_out[1024];
    char words_out[1024];

    snprintf(grid_path, sizeof(grid_path), "%s/grid", root_folder);
    snprintf(words_path, sizeof(words_path), "%s/words", root_folder);
    snprintf(grid_out, sizeof(grid_out), "%s/grid.txt", root_folder);
    snprintf(words_out, sizeof(words_out), "%s/words.txt", root_folder);

    // 1. Reconnaissance de la Grille
    printf("\n[1/2] Grid Recognition:\n");
    core_predict_grid(net, grid_path, grid_out);

    // 2. Reconnaissance des Mots
    printf("\n[2/2] Words Recognition:\n");
    core_predict_words(net, words_path, words_out);
    
    free_network(net);
    return 0;
}

