#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "neural_network.h"

#define IMAGE_SIZE 30 

#define PIXEL_COUNT (IMAGE_SIZE * IMAGE_SIZE)

typedef struct {
    double *pixels;
    int label;
} ImageData;

double* load_and_convert_image(const char *filepath);

int load_images_from_folder(const char *folder_path, int label, ImageData **images, int *current_count, int max_images);

int load_dataset(const char *root_path, ImageData **images, int max_per_letter);

void free_images(ImageData *images, int count);

void print_image(double *pixels);

typedef struct {
    int x;
    int y;
    double *pixels;
    char predicted_letter;
} GridLetter;

int load_grid_images(const char *folder_path, GridLetter **letters);

void free_grid_letters(GridLetter *letters, int count);

// Struct for decyphered words
typedef struct {
    char *word;
    int length;
} Word;

char* load_and_predict_word(const char *word_folder, Network *net);

int load_and_predict_words(const char *words_folder, Network *net, Word **words);

void free_words(Word *words, int count);

#endif
