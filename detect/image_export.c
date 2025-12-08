#include "image_export.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

// --- CONFIGURATION ---

// Threshold to determine if a pixel is black (0-765 sum of RGB).
#define BLACK_PIXEL_THRESHOLD 400 

// Minimum area (pixels^2) to consider a blob a valid letter.
#define MIN_BLOB_AREA 10

// Expected width-to-height ratio of a standard letter.
#define EXPECTED_LETTER_RATIO 0.70

// Final output image size (30x30 pixels as per your last file).
#define OUTPUT_SIZE 30

// Padding for Word List letters (to match visual size of grid letters).
#define LIST_PADDING 7 

// NEW: Zoom factor for Grid letters.
// We crop 2 pixels from each side of the grid cell source to "zoom in" on the letter.
#define GRID_CROP 2

// Internal structure to represent a detected connected component
typedef struct {
    int x, y, width, height;
    int area;
} Blob;

// --- HELPER FUNCTIONS ---

static void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

static int compare_blobs(const void *a, const void *b) {
    return ((Blob*)a)->x - ((Blob*)b)->x;
}

// -------------------------------------------------------------
// CORE SAVING FUNCTION
// -------------------------------------------------------------
static void save_subimage(GdkPixbuf *source, int x, int y, int w, int h, const char *filepath, int padding) {
    int img_w = gdk_pixbuf_get_width(source);
    int img_h = gdk_pixbuf_get_height(source);
    
    // Safety checks
    if (x < 0) x = 0; 
    if (y < 0) y = 0;
    if (x + w > img_w) w = img_w - x;
    if (y + h > img_h) h = img_h - y;
    
    if (w <= 0 || h <= 0) return;

    GdkPixbuf *extracted = gdk_pixbuf_new_subpixbuf(source, x, y, w, h);
    
    // Create white canvas
    GdkPixbuf *canvas = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, OUTPUT_SIZE, OUTPUT_SIZE);
    gdk_pixbuf_fill(canvas, 0xFFFFFFFF);

    // Calculate target size (Output size minus padding)
    int target_size = OUTPUT_SIZE - (padding * 2);
    if (target_size < 1) target_size = 1;

    // Calculate scaling
    double scale_w = (double)target_size / w;
    double scale_h = (double)target_size / h;
    double scale = (scale_w < scale_h) ? scale_w : scale_h;
    
    int new_w = (int)(w * scale);
    int new_h = (int)(h * scale);
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;

    // Centering offsets
    int offset_x = (OUTPUT_SIZE - new_w) / 2;
    int offset_y = (OUTPUT_SIZE - new_h) / 2;

    gdk_pixbuf_scale(extracted, canvas,
                     offset_x, offset_y,
                     new_w, new_h,
                     offset_x, offset_y,
                     scale, scale,
                     GDK_INTERP_BILINEAR);

    if (!gdk_pixbuf_save(canvas, filepath, "bmp", NULL, NULL)) {
        fprintf(stderr, "Error saving image: %s\n", filepath);
    }
    
    g_object_unref(canvas);
    g_object_unref(extracted);
}

// -------------------------------------------------------------
// SEGMENTATION LOGIC
// -------------------------------------------------------------

static void flood_fill(guchar *pixels, bool *visited, int w, int h, int rs, int nc, 
                       int x, int y, int *min_x, int *max_x, int *min_y, int *max_y, int *area) {
    if (x < 0 || x >= w || y < 0 || y >= h || visited[y * w + x]) return;

    guchar *p = pixels + y * rs + x * nc;
    if ((p[0] + p[1] + p[2]) >= BLACK_PIXEL_THRESHOLD) return;

    visited[y * w + x] = true;
    (*area)++;

    if (x < *min_x) *min_x = x;
    if (x > *max_x) *max_x = x;
    if (y < *min_y) *min_y = y;
    if (y > *max_y) *max_y = y;

    flood_fill(pixels, visited, w, h, rs, nc, x + 1, y, min_x, max_x, min_y, max_y, area);
    flood_fill(pixels, visited, w, h, rs, nc, x - 1, y, min_x, max_x, min_y, max_y, area);
    flood_fill(pixels, visited, w, h, rs, nc, x, y + 1, min_x, max_x, min_y, max_y, area);
    flood_fill(pixels, visited, w, h, rs, nc, x, y - 1, min_x, max_x, min_y, max_y, area);
}

static int find_best_split_col(int *histo, int start_x, int end_x) {
    int min_val = INT_MAX;
    int best_x = (start_x + end_x) / 2;

    for (int x = start_x; x < end_x; x++) {
        if (histo[x] < min_val) {
            min_val = histo[x];
            best_x = x;
        } 
        else if (histo[x] == min_val) {
            int mid = (start_x + end_x) / 2;
            if (abs(x - mid) < abs(best_x - mid)) {
                best_x = x;
            }
        }
    }
    return best_x;
}

static void segment_and_save_word(GdkPixbuf *source, Box word, const char *word_folder) {
    int w = word.width;
    int h = word.height;
    
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(source, word.x, word.y, w, h);
    guchar *pixels = gdk_pixbuf_get_pixels(sub);
    int rs = gdk_pixbuf_get_rowstride(sub);
    int nc = gdk_pixbuf_get_n_channels(sub);

    bool *visited = (bool*)calloc(w * h, sizeof(bool));
    int capacity = 20;
    Blob *blobs = (Blob*)malloc(capacity * sizeof(Blob));
    int count = 0;

    // Blob Detection
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!visited[y * w + x]) {
                guchar *p = pixels + y * rs + x * nc;
                if ((p[0] + p[1] + p[2]) < BLACK_PIXEL_THRESHOLD) {
                    
                    int min_x = x, max_x = x, min_y = y, max_y = y, area = 0;
                    flood_fill(pixels, visited, w, h, rs, nc, x, y, 
                               &min_x, &max_x, &min_y, &max_y, &area);
                    
                    if (area >= MIN_BLOB_AREA) {
                        if (count >= capacity) {
                            capacity *= 2;
                            blobs = (Blob*)realloc(blobs, capacity * sizeof(Blob));
                        }
                        blobs[count].x = min_x;
                        blobs[count].y = min_y;
                        blobs[count].width = (max_x - min_x) + 1;
                        blobs[count].height = (max_y - min_y) + 1;
                        blobs[count].area = area;
                        count++;
                    }
                }
            }
        }
    }

    // Process Blobs
    if (count > 0) {
        qsort(blobs, count, sizeof(Blob), compare_blobs);
        
        char path[512];
        int letter_idx = 0;

        for (int i = 0; i < count; i++) {
            Blob b = blobs[i];

            int *blob_histo = (int*)calloc(b.width, sizeof(int));
            for (int by = 0; by < b.height; by++) {
                for (int bx = 0; bx < b.width; bx++) {
                    int gx = b.x + bx; 
                    int gy = b.y + by;
                    guchar *p = pixels + gy * rs + gx * nc;
                    if ((p[0] + p[1] + p[2]) < BLACK_PIXEL_THRESHOLD) {
                        blob_histo[bx]++;
                    }
                }
            }

            float expected_w = b.height * EXPECTED_LETTER_RATIO;
            int num_letters = (int)((b.width / expected_w) + 0.5); 
            if (num_letters < 1) num_letters = 1;

            int current_x = 0;
            int chunk_size = b.width / num_letters;

            for (int k = 1; k < num_letters; k++) {
                int ideal_cut = k * chunk_size;
                int search_start = ideal_cut - (chunk_size / 3);
                int search_end = ideal_cut + (chunk_size / 3);
                
                if (search_start < current_x) search_start = current_x + 1;
                if (search_end >= b.width) search_end = b.width - 1;

                int cut_x = find_best_split_col(blob_histo, search_start, search_end);

                snprintf(path, 512, "%s/letter_%d.bmp", word_folder, letter_idx++);
                save_subimage(source, word.x + b.x + current_x, word.y + b.y, 
                              cut_x - current_x, b.height, path, LIST_PADDING);
                
                current_x = cut_x;
            }
            
            snprintf(path, 512, "%s/letter_%d.bmp", word_folder, letter_idx++);
            save_subimage(source, word.x + b.x + current_x, word.y + b.y, 
                          b.width - current_x, b.height, path, LIST_PADDING);

            free(blob_histo);
        }
    }

    free(blobs);
    free(visited);
    g_object_unref(sub);
}

// -------------------------------------------------------------
// PUBLIC FUNCTION
// -------------------------------------------------------------

void export_layout_to_files(GdkPixbuf *pixbuf, PageLayout *layout, const char *output_folder) {
    if (!layout) return;
    char path[512];
    create_directory(output_folder);

    // --- EXPORT GRID CELLS ---
    printf("Exporting grid cells...\n");
    snprintf(path, 512, "%s/grid", output_folder); 
    create_directory(path);
    
    for (int r = 0; r < layout->rows; r++) {
        for (int c = 0; c < layout->cols; c++) {
            Box cell = layout->grid_cells[r * layout->cols + c];
            
            snprintf(path, 512, "%s/grid/%d_%d.bmp", output_folder, c, r);
            
            // APPLY ZOOM: Crop the source cell by GRID_CROP (2px) on all sides.
            // This removes borders/whitespace, making the letter appear bigger in the output.
            save_subimage(pixbuf, 
                          cell.x + GRID_CROP, 
                          cell.y + GRID_CROP, 
                          cell.width - (GRID_CROP * 2), 
                          cell.height - (GRID_CROP * 2), 
                          path, 
                          0); // Padding is 0 because we handle spacing via cropping
        }
    }

    // --- EXPORT WORD LIST ---
    if (layout->has_wordlist) {
        printf("Exporting word list...\n");
        snprintf(path, 512, "%s/words", output_folder); 
        create_directory(path);
        
        for (int i = 0; i < layout->word_count; i++) {
            snprintf(path, 512, "%s/words/word_%d", output_folder, i); 
            create_directory(path);
            segment_and_save_word(pixbuf, layout->words[i], path);
        }
    }
}
