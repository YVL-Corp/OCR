#ifndef EXTRACTION_H_
#define EXTRACTION_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

/**
 * Structure representing a rectangular area (a word or a grid cell).
 */
typedef struct {
    int x, y, width, height;
} Box;

/**
 * Main structure representing the analyzed page layout.
 * It decouples detection logic from export/drawing logic.
 */
typedef struct PageLayout {
    // --- Grid Information ---
    int grid_x, grid_y, grid_width, grid_height;
    int rows, cols;
    
    // Array storing the exact coordinates of EACH cell in the grid
    Box *grid_cells; 
    
    // --- Word List Information ---
    int has_wordlist;
    int list_x, list_y, list_width, list_height;
    
    // Array storing the detected words in the list
    Box *words;    
    int word_count;
    
} PageLayout;

/**
 * Main detection function.
 * Analyzes the pixbuf using XY-Cuts and returns a PageLayout structure.
 */
PageLayout* detect_layout_from_pixbuf(GdkPixbuf *pixbuf);

/**
 * Frees all memory associated with a PageLayout.
 */
void free_page_layout(PageLayout *layout);

#endif
