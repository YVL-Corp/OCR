#include "extraction.h"
#include <stdio.h>
#include <stdlib.h>

// --- Thresholds ---
#define BLACK_THRESHOLD 750             // Sum of R+G+B to be considered "black"
#define BLOB_THRESHOLD 10               // Min pixel count to detect a "blob" in Pass 1
#define GRID_LINE_THRESHOLD_PERCENT 0.7 // % of grid width/height a line must fill
                                        // to be considered a "grid line" (not text)

// --- Private Function Prototypes ---
static void free_bar_list(BarList *list);
static BarList* analyze_bars(int *histogram, int length, int threshold);
static Bar find_widest_bar(BarList *list);
static void print_grid_bounding_box(int x0, int y0, int x1, int y1);
static void find_grid_cells(BarList *h_bars, BarList *v_bars);


// Safely frees the memory allocated for a BarList.
static void free_bar_list(BarList *list) {
    if (list) {
        free(list->bars);
        free(list);
    }
}

// Analyzes a 1D histogram array to find continuous bars of
// values above a given threshold.
// This runs in two passes: first to count bars for allocation,
// second to fill the bar data.
static BarList* analyze_bars(int *histogram, int length, int threshold) {
    int count = 0;
    int in_bar = 0;

    // Pass 1: Count the bars to allocate the exact amount of memory
    for (int i = 0; i < length; i++) {
        if (histogram[i] > threshold) {
            if (!in_bar) {
                in_bar = 1;
                count++; // Only count the beginning of a new bar
            }
        } else {
            if (in_bar) {
                in_bar = 0;
            }
        }
    }
    
    // If no bars found, return an empty list
    if (count == 0) {
        return (BarList*)calloc(1, sizeof(BarList));
    }

    // Allocate memory for the list and the array of bars
    BarList *list = (BarList*)malloc(sizeof(BarList));
    if (!list) return NULL;
    list->bars = (Bar*)malloc(count * sizeof(Bar));
    if (!list->bars) { free(list); return NULL; }
    list->count = count;
    
    // Pass 2: Fill the BarList with start/end coordinates
    in_bar = 0;
    int current_bar_index = -1;
    int bar_start = 0;
    for (int i = 0; i < length; i++) {
        if (histogram[i] > threshold) {
            if (!in_bar) {
                // We are entering a new bar
                in_bar = 1;
                bar_start = i;
                current_bar_index++;
            }
        } else {
            if (in_bar) {
                // We are exiting a bar
                in_bar = 0;
                int bar_end = i - 1;
                list->bars[current_bar_index].start = bar_start;
                list->bars[current_bar_index].end = bar_end;
                list->bars[current_bar_index].thickness = (bar_end - bar_start) + 1;
            }
        }
    }
    // Handle edge case: bar touches the end of the image
    if (in_bar) {
        int bar_end = length - 1;
        list->bars[current_bar_index].start = bar_start;
        list->bars[current_bar_index].end = bar_end;
        list->bars[current_bar_index].thickness = (bar_end - bar_start) + 1;
    }
    return list;
}

// Finds the bar with the largest 'thickness' in a BarList.
// Used to identify the grid from other "blobs" (like a word list).
static Bar find_widest_bar(BarList *list) {
    Bar widest = {0, 0, 0};
    if (!list || list->count == 0) {
        return widest;
    }
    
    widest = list->bars[0];
    for (int i = 1; i < list->count; i++) {
        if (list->bars[i].thickness > widest.thickness) {
            widest = list->bars[i];
        }
    }
    return widest;
}

// Prints the final detected bounding box of the grid.
static void print_grid_bounding_box(int x0, int y0, int x1, int y1) {
    printf("\n--- Grid Position Detection ---\n");
    printf("GRID BOUNDING BOX (X0, Y0) -> (X1, Y1) : (%d, %d) -> (%d, %d)\n", x0, y0, x1, y1);
    printf("Width: %d px, Height: %d px\n", (x1-x0)+1, (y1-y0)+1);
}

// Deduces and prints the coordinates of each cell (the empty spaces)
// based on the gaps *between* the internal grid bars.
static void find_grid_cells(BarList *h_bars, BarList *v_bars) {
    printf("\n--- Grid Cell Detection ---\n");

    // We need N+1 bars to define N cells
    int num_cell_rows = h_bars->count - 1;
    int num_cell_cols = v_bars->count - 1;
    
    if (num_cell_rows <= 0 || num_cell_cols <= 0) {
        printf("Not enough bars found to deduce cells!\n");
        return;
    }

    printf("Detected Grid: %d rows x %d columns\n", num_cell_rows, num_cell_cols);

    // Iterate over the gaps between horizontal bars
    for (int i = 0; i < num_cell_rows; i++) {
        int y0 = h_bars->bars[i].end + 1;
        int y1 = h_bars->bars[i+1].start - 1;

        // Iterate over the gaps between vertical bars
        for (int j = 0; j < num_cell_cols; j++) {
            int x0 = v_bars->bars[j].end + 1;
            int x1 = v_bars->bars[j+1].start - 1;
            
            printf("  Cell (%d, %d) -> (X: %d to %d, Y: %d to %d)\n", 
                   i, j, x0, x1, y0, y1);
        }
    }
}

// Main detection logic. Runs a 3-pass analysis.
void detect_grid_from_pixbuf(GdkPixbuf *pixbuf) {
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    // --- PASS 1: Global X-Axis Analysis ---
    // Find all vertical "blobs" in the image (grid, word list, etc.)
    printf("--- PASS 1: Global X-Axis Analysis ---\n");
    int *global_histo_x = (int *)calloc(width, sizeof(int));
    for (int y = 0; y < height; y++) {
        guchar *row = pixels + y * rowstride;
        for (int x = 0; x < width; x++) {
            guchar *p = row + x * n_channels;
            if ((p[0] + p[1] + p[2]) < BLACK_THRESHOLD) {
                global_histo_x[x]++;
            }
        }
    }
    BarList *vertical_blobs = analyze_bars(global_histo_x, width, BLOB_THRESHOLD);
    
    // Assume the grid is the widest blob
    Bar grid_blob = find_widest_bar(vertical_blobs);
    
    if (grid_blob.thickness == 0) {
        fprintf(stderr, "Error: Could not find grid blob.\n");
        free(global_histo_x);
        free_bar_list(vertical_blobs);
        return;
    }
    
    int grid_X0 = grid_blob.start;
    int grid_X1 = grid_blob.end;
    int grid_width = (grid_X1 - grid_X0) + 1;
    
    printf("Grid blob found: X from %d to %d (width %d px)\n", grid_X0, grid_X1, grid_width);
    free(global_histo_x);
    free_bar_list(vertical_blobs);


    // --- PASS 2: Y-Axis Analysis (Grid Area) ---
    // Find horizontal grid lines, but *only* within the grid's X-bounds.
    printf("--- PASS 2: Y-Axis Analysis (Grid Area) ---\n");
    int *internal_histo_y = (int *)calloc(height, sizeof(int));
    for (int y = 0; y < height; y++) {
        guchar *row = pixels + y * rowstride;
        for (int x = grid_X0; x <= grid_X1; x++) {
            guchar *p = row + x * n_channels;
            if ((p[0] + p[1] + p[2]) < BLACK_THRESHOLD) {
                internal_histo_y[y]++;
            }
        }
    }
    
    // Use a dynamic threshold to distinguish real grid lines from text lines
    int h_grid_threshold = (int)(grid_width * GRID_LINE_THRESHOLD_PERCENT);
    printf("Dynamic Y-axis threshold (bars): %d\n", h_grid_threshold);
    
    BarList *internal_h_bars = analyze_bars(internal_histo_y, height, h_grid_threshold);
    free(internal_histo_y);
    
    if (internal_h_bars->count == 0) {
        fprintf(stderr, "Error: Could not find internal horizontal bars.\n");
        free_bar_list(internal_h_bars);
        return;
    }

    // Get the grid's Y-bounds from the first and last horizontal bar
    int grid_Y0 = internal_h_bars->bars[0].start;
    int grid_Y1 = internal_h_bars->bars[internal_h_bars->count - 1].end;
    int grid_height = (grid_Y1 - grid_Y0) + 1;
    
    
    // --- PASS 3: X-Axis Analysis (Grid Area) ---
    // Find vertical grid lines, but *only* within the grid's X and Y bounds.
    printf("--- PASS 3: X-Axis Analysis (Grid Area) ---\n");
    int *internal_histo_x = (int *)calloc(width, sizeof(int));
    for (int y = grid_Y0; y <= grid_Y1; y++) { 
        guchar *row = pixels + y * rowstride;
        for (int x = grid_X0; x <= grid_X1; x++) {
            guchar *p = row + x * n_channels;
            if ((p[0] + p[1] + p[2]) < BLACK_THRESHOLD) {
                internal_histo_x[x]++; 
            }
        }
    }
    
    // Use a dynamic threshold based on grid height
    int v_grid_threshold = (int)(grid_height * GRID_LINE_THRESHOLD_PERCENT);
    printf("Dynamic X-axis threshold (bars): %d\n", v_grid_threshold);

    BarList *internal_v_bars = analyze_bars(internal_histo_x, width, v_grid_threshold);
    free(internal_histo_x);

    if (internal_v_bars->count == 0) {
        fprintf(stderr, "Error: Could not find internal vertical bars.\n");
        free_bar_list(internal_h_bars);
        free_bar_list(internal_v_bars);
        return;
    }

    // RESULTS
    // Print the internal bars found
    printf("\n--- Internal Horizontal Bars (Y) ---\n");
    for (int i = 0; i < internal_h_bars->count; i++) {
        Bar *b = &internal_h_bars->bars[i];
        printf("HORIZONTAL BAR [%d]: Start_Y=%d, End_Y=%d, Thickness=%d px\n",
               i, b->start, b->end, b->thickness);
    }
    printf("\n--- Internal Vertical Bars (X) ---\n");
    for (int i = 0; i < internal_v_bars->count; i++) {
        Bar *b = &internal_v_bars->bars[i];
        printf("VERTICAL BAR [%d]: Start_X=%d, End_X=%d, Thickness=%d px\n",
               i, b->start, b->end, b->thickness);
    }

    // Print the final bounding box and cell coordinates
    print_grid_bounding_box(grid_X0, grid_Y0, grid_X1, grid_Y1);
    find_grid_cells(internal_h_bars, internal_v_bars);

    // Final cleanup
    free_bar_list(internal_h_bars);
    free_bar_list(internal_v_bars);
}
