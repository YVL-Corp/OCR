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
// 400 is strict enough to ignore light gray noise between letters.
#define BLACK_PIXEL_THRESHOLD 400 

// Minimum area (pixels^2) to consider a blob a valid letter.
#define MIN_BLOB_AREA 10

// Expected width-to-height ratio of a standard letter.
// Used to detect if a blob contains multiple merged letters (e.g., "LAA").
#define EXPECTED_LETTER_RATIO 0.70

// Final output image size (20x20 pixels) for the Neural Network.
#define OUTPUT_SIZE 30

// Padding applied to letters extracted from the word list.
// Since list letters are detected via flood-fill (tight fit), we add padding
// so they visually match the grid letters (which have natural cell margins).
// 4px padding -> Letter will be max 12px high inside the 20px box.
#define LIST_PADDING 4 

// Internal structure to represent a detected connected component
typedef struct {
    int x, y, width, height;
    int area;
} Blob;

// --- HELPER FUNCTIONS ---

// Creates a directory if it does not exist.
static void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        // 0700: Read/Write/Execute permissions for owner
        mkdir(path, 0700);
    }
}

// Comparator for sorting Blobs from left to right (based on X coordinate).
static int compare_blobs(const void *a, const void *b) {
    return ((Blob*)a)->x - ((Blob*)b)->x;
}

// -------------------------------------------------------------
// CORE SAVING FUNCTION: Center, Scale & Save
// -------------------------------------------------------------
// Extracts a region, scales it to fit into (OUTPUT_SIZE - padding),
// centers it on a white 20x20 canvas, and saves it as BMP.
static void save_subimage(GdkPixbuf *source, int x, int y, int w, int h, const char *filepath, int padding) {
    int img_w = gdk_pixbuf_get_width(source);
    int img_h = gdk_pixbuf_get_height(source);
    
    // Safety checks: Ensure coordinates are within image bounds
    if (x < 0) x = 0; 
    if (y < 0) y = 0;
    if (x + w > img_w) w = img_w - x;
    if (y + h > img_h) h = img_h - y;
    
    if (w <= 0 || h <= 0) return;

    // 1. Extract the raw letter image (original variable size)
    GdkPixbuf *extracted = gdk_pixbuf_new_subpixbuf(source, x, y, w, h);
    
    // 2. Create a white 20x20 canvas
    // GDK_COLORSPACE_RGB, No Alpha, 8 bits per channel
    GdkPixbuf *canvas = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, OUTPUT_SIZE, OUTPUT_SIZE);
    gdk_pixbuf_fill(canvas, 0xFFFFFFFF); // Fill with white

    // 3. Calculate scaling factor to respect the padding
    // The "useful" area is (20 - 2 * padding)
    int target_size = OUTPUT_SIZE - (padding * 2);
    if (target_size < 1) target_size = 1;

    // Calculate scale to fit width OR height, preserving aspect ratio
    double scale_w = (double)target_size / w;
    double scale_h = (double)target_size / h;
    double scale = (scale_w < scale_h) ? scale_w : scale_h;
    
    // Calculate new dimensions
    int new_w = (int)(w * scale);
    int new_h = (int)(h * scale);
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;

    // 4. Calculate offsets to center the image on the 20x20 canvas
    int offset_x = (OUTPUT_SIZE - new_w) / 2;
    int offset_y = (OUTPUT_SIZE - new_h) / 2;

    // 5. Composite the scaled letter onto the white canvas
    gdk_pixbuf_scale(extracted, canvas,
                     offset_x, offset_y,
                     new_w, new_h,
                     offset_x, offset_y,
                     scale, scale,
                     GDK_INTERP_BILINEAR);

    // 6. Save to disk as BMP
    if (!gdk_pixbuf_save(canvas, filepath, "bmp", NULL, NULL)) {
        fprintf(stderr, "Error saving image: %s\n", filepath);
    }
    
    // Cleanup
    g_object_unref(canvas);
    g_object_unref(extracted);
}

// -------------------------------------------------------------
// SEGMENTATION LOGIC
// -------------------------------------------------------------

// Recursive Flood Fill algorithm to find connected components (blobs)
static void flood_fill(guchar *pixels, bool *visited, int w, int h, int rs, int nc, 
                       int x, int y, int *min_x, int *max_x, int *min_y, int *max_y, int *area) {
    // Boundary checks
    if (x < 0 || x >= w || y < 0 || y >= h || visited[y * w + x]) return;

    // Color check (is it black ink?)
    guchar *p = pixels + y * rs + x * nc;
    if ((p[0] + p[1] + p[2]) >= BLACK_PIXEL_THRESHOLD) return; // It's white/light gray

    // Mark as visited and update blob statistics
    visited[y * w + x] = true;
    (*area)++;

    if (x < *min_x) *min_x = x;
    if (x > *max_x) *max_x = x;
    if (y < *min_y) *min_y = y;
    if (y > *max_y) *max_y = y;

    // Recurse neighbors (4-connectivity)
    flood_fill(pixels, visited, w, h, rs, nc, x + 1, y, min_x, max_x, min_y, max_y, area);
    flood_fill(pixels, visited, w, h, rs, nc, x - 1, y, min_x, max_x, min_y, max_y, area);
    flood_fill(pixels, visited, w, h, rs, nc, x, y + 1, min_x, max_x, min_y, max_y, area);
    flood_fill(pixels, visited, w, h, rs, nc, x, y - 1, min_x, max_x, min_y, max_y, area);
}

// Finds the best X-coordinate to split a wide blob (e.g., "AA").
// It looks for the column with the minimum amount of black pixels (thinnest connection).
static int find_best_split_col(int *histo, int start_x, int end_x) {
    int min_val = INT_MAX;
    int best_x = (start_x + end_x) / 2; // Default to middle

    for (int x = start_x; x < end_x; x++) {
        // Find the "thinnest" part of the letter connection
        if (histo[x] < min_val) {
            min_val = histo[x];
            best_x = x;
        } 
        // Tie-breaker: choose the one closest to the center
        else if (histo[x] == min_val) {
            int mid = (start_x + end_x) / 2;
            if (abs(x - mid) < abs(best_x - mid)) {
                best_x = x;
            }
        }
    }
    return best_x;
}

// Main logic to process a word image from the list.
// 1. Detects blobs (letters).
// 2. Splits merged blobs (like "LAA") based on aspect ratio.
// 3. Saves individual letters.
static void segment_and_save_word(GdkPixbuf *source, Box word, const char *word_folder) {
    int w = word.width;
    int h = word.height;
    
    // Extract the word image for processing
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(source, word.x, word.y, w, h);
    guchar *pixels = gdk_pixbuf_get_pixels(sub);
    int rs = gdk_pixbuf_get_rowstride(sub);
    int nc = gdk_pixbuf_get_n_channels(sub);

    bool *visited = (bool*)calloc(w * h, sizeof(bool));
    int capacity = 20;
    Blob *blobs = (Blob*)malloc(capacity * sizeof(Blob));
    int count = 0;

    // 1. Blob Detection Loop
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!visited[y * w + x]) {
                guchar *p = pixels + y * rs + x * nc;
                // If pixel is black
                if ((p[0] + p[1] + p[2]) < BLACK_PIXEL_THRESHOLD) {
                    
                    // Initialize new blob
                    int min_x = x, max_x = x, min_y = y, max_y = y, area = 0;
                    
                    // Flood fill to get the full component
                    flood_fill(pixels, visited, w, h, rs, nc, x, y, 
                               &min_x, &max_x, &min_y, &max_y, &area);
                    
                    // Filter out tiny noise
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

    // 2. Process Detected Blobs
    if (count > 0) {
        // Sort letters from left to right
        qsort(blobs, count, sizeof(Blob), compare_blobs);
        
        char path[512];
        int letter_idx = 0;

        for (int i = 0; i < count; i++) {
            Blob b = blobs[i];

            // Calculate local histogram for smart splitting
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

            // Estimate number of letters in this blob
            // Example: Width 50, Height 20 -> Ratio 2.5 -> Likely 3 letters
            float expected_w = b.height * EXPECTED_LETTER_RATIO;
            int num_letters = (int)((b.width / expected_w) + 0.5); 
            if (num_letters < 1) num_letters = 1;

            // Split and Save
            int current_x = 0;
            int chunk_size = b.width / num_letters;

            for (int k = 1; k < num_letters; k++) {
                // Determine search area for the split
                int ideal_cut = k * chunk_size;
                int search_start = ideal_cut - (chunk_size / 3);
                int search_end = ideal_cut + (chunk_size / 3);
                
                if (search_start < current_x) search_start = current_x + 1;
                if (search_end >= b.width) search_end = b.width - 1;

                // Find the best column to cut (minimum ink)
                int cut_x = find_best_split_col(blob_histo, search_start, search_end);

                // Save segment
                snprintf(path, 512, "%s/letter_%d.bmp", word_folder, letter_idx++);
                // NOTE: We use LIST_PADDING here to shrink these tight letters
                save_subimage(source, word.x + b.x + current_x, word.y + b.y, 
                              cut_x - current_x, b.height, path, LIST_PADDING);
                
                current_x = cut_x;
            }
            
            // Save remaining part
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
            
            // Filename format: x_y.bmp
            snprintf(path, 512, "%s/grid/%d_%d.bmp", output_folder, c, r);
            
            // NOTE: Padding = 0 because grid cells already contain white space
            save_subimage(pixbuf, cell.x, cell.y, cell.width, cell.height, path, 0);
        }
    }

    // --- EXPORT WORD LIST ---
    if (layout->has_wordlist) {
        printf("Exporting word list...\n");
        snprintf(path, 512, "%s/words", output_folder); 
        create_directory(path);
        
        for (int i = 0; i < layout->word_count; i++) {
            // Create folder for each word
            snprintf(path, 512, "%s/words/word_%d", output_folder, i); 
            create_directory(path);
            
            // Analyze word, split letters, apply LIST_PADDING, and save
            segment_and_save_word(pixbuf, layout->words[i], path);
        }
    }
}
