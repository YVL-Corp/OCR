#ifndef EXTRACTION_H_
#define EXTRACTION_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

// Structure représentant une zone rectangulaire (mot ou case)
typedef struct {
    int x, y, width, height;
} Box;

// Structure principale
typedef struct {
    // --- Infos Grille ---
    int grid_x, grid_y, grid_width, grid_height;
    int rows, cols;
    
    // NOUVEAU : Tableau stockant la position exacte de CHAQUE case
    // Taille = rows * cols. Accès : grid_cells[row * cols + col]
    Box *grid_cells; 
    
    // --- Infos Liste de mots ---
    int has_wordlist;
    int list_x, list_y, list_width, list_height;
    Box *words;    // Tableau des mots détectés
    int word_count;
    
} PageLayout;

PageLayout* detect_layout_from_pixbuf(GdkPixbuf *pixbuf);
void free_page_layout(PageLayout *layout);

#endif
