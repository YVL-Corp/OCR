#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "neural_network.h"

#define IMAGE_SIZE 28 // Attention: MNIST standard est 28x28, vérifie si tu as besoin de 30 ou 28 selon tes données. J'ai remis 30 selon ton ancien code si besoin change ici.
#undef IMAGE_SIZE
#define IMAGE_SIZE 30 // Remis à 30 comme dans ton code original

#define PIXEL_COUNT (IMAGE_SIZE * IMAGE_SIZE)

typedef struct {
    double *pixels;
    int label;
} ImageData;

// Charge une image et la convertit en tableau de 0 et 1
double* load_and_convert_image(const char *filepath);

// Charge toutes les images d'un dossier avec un label donné
int load_images_from_folder(const char *folder_path, int label, ImageData **images, int *current_count, int max_images);

// Charge tout le dataset (tous les dossiers de lettres)
int load_dataset(const char *root_path, ImageData **images, int max_per_letter);

// Libère la mémoire des images
void free_images(ImageData *images, int count);

// Affiche une image en ASCII
void print_image(double *pixels);

// Structure pour stocker une lettre avec ses coordonnées
typedef struct {
    int x;
    int y;
    double *pixels;
    char predicted_letter;
} GridLetter;

// Charge des images au format XX_YY.bmp pour reconstruire une grille
int load_grid_images(const char *folder_path, GridLetter **letters);

// Libère la mémoire des lettres de la grille
void free_grid_letters(GridLetter *letters, int count);

// Structure pour stocker un mot déchiffré
typedef struct {
    char *word;
    int length;
} Word;

// Charge les images d'un mot depuis un dossier (lettres numérotées 0.bmp, 1.bmp, etc.)
char* load_and_predict_word(const char *word_folder, Network *net);

// Charge tous les mots depuis un dossier parent contenant des sous-dossiers
int load_and_predict_words(const char *words_folder, Network *net, Word **words);

// Libère la mémoire des mots
void free_words(Word *words, int count);

#endif // IMAGE_LOADER_H
