#include "image_export.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

// Seuil de noir (toujours assez strict pour les ponts gris)
#define BLACK_PIXEL_THRESHOLD 400 
#define MIN_BLOB_AREA 10

// Estimation : Une lettre standard fait environ 70% de sa hauteur en largeur.
// Cela nous aide à deviner combien de lettres sont collées.
#define EXPECTED_LETTER_RATIO 0.70

typedef struct {
    int x, y, width, height;
    int area;
} Blob;

static void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) mkdir(path, 0700);
}

static void save_subimage(GdkPixbuf *source, int x, int y, int w, int h, const char *filepath) {
    int img_w = gdk_pixbuf_get_width(source);
    int img_h = gdk_pixbuf_get_height(source);
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x + w > img_w) w = img_w - x;
    if (y + h > img_h) h = img_h - y;
    if (w <= 0 || h <= 0) return;

    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(source, x, y, w, h);
    gdk_pixbuf_save(sub, filepath, "png", NULL, NULL);
    g_object_unref(sub);
}

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

static int compare_blobs(const void *a, const void *b) {
    return ((Blob*)a)->x - ((Blob*)b)->x;
}

// Fonction pour trouver le meilleur endroit où couper (là où il y a le moins de noir)
static int find_best_split_col(int *histo, int start_x, int end_x) {
    int min_val = INT_MAX;
    int best_x = (start_x + end_x) / 2; // Par défaut : le milieu

    for (int x = start_x; x < end_x; x++) {
        // On cherche la colonne avec le moins de pixels noirs (le "pont" le plus fin)
        if (histo[x] < min_val) {
            min_val = histo[x];
            best_x = x;
        } 
        // Si égalité, on préfère être proche du milieu théorique
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

    // 1. Détection des Blobs (Flood Fill)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!visited[y * w + x]) {
                guchar *p = pixels + y * rs + x * nc;
                if ((p[0] + p[1] + p[2]) < BLACK_PIXEL_THRESHOLD) {
                    int min_x = x, max_x = x, min_y = y, max_y = y, area = 0;
                    flood_fill(pixels, visited, w, h, rs, nc, x, y, &min_x, &max_x, &min_y, &max_y, &area);
                    
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

    // 2. Traitement des Blobs (Découpe intelligente)
    if (count > 0) {
        qsort(blobs, count, sizeof(Blob), compare_blobs);
        
        char path[512];
        int letter_idx = 0;

        for (int i = 0; i < count; i++) {
            Blob b = blobs[i];

            // A. CALCUL DE L'HISTOGRAMME LOCAL DU BLOB
            // On en a besoin pour savoir OÙ couper intelligemment
            int *blob_histo = (int*)calloc(b.width, sizeof(int));
            for (int by = 0; by < b.height; by++) {
                for (int bx = 0; bx < b.width; bx++) {
                    // Attention aux coordonnées : relatives au blob, puis au mot
                    int gx = b.x + bx; 
                    int gy = b.y + by;
                    guchar *p = pixels + gy * rs + gx * nc;
                    if ((p[0] + p[1] + p[2]) < BLACK_PIXEL_THRESHOLD) {
                        blob_histo[bx]++;
                    }
                }
            }

            // B. ESTIMATION DU NOMBRE DE LETTRES
            // Ratio = Largeur / (Hauteur * 0.7)
            // ex: "LAA" -> Width 50, Height 20. Ratio = 50 / 14 = 3.5 -> 3 lettres.
            float expected_w = b.height * EXPECTED_LETTER_RATIO;
            int num_letters = (int)((b.width / expected_w) + 0.5); 
            if (num_letters < 1) num_letters = 1;

            // C. DÉCOUPE RÉCURSIVE
            int current_x = 0;
            int chunk_size = b.width / num_letters;

            for (int k = 1; k < num_letters; k++) {
                // On cherche le meilleur point de coupe autour de la position théorique
                // Fenêtre de recherche : +/- 30% de la largeur de chunk
                int ideal_cut = k * chunk_size;
                int search_start = ideal_cut - (chunk_size / 3);
                int search_end = ideal_cut + (chunk_size / 3);
                
                if (search_start < current_x) search_start = current_x + 1;
                if (search_end >= b.width) search_end = b.width - 1;

                // Trouve l'endroit le plus fin (minima de l'histo)
                int cut_x = find_best_split_col(blob_histo, search_start, search_end);

                // Sauvegarde du morceau
                snprintf(path, 512, "%s/letter_%d.png", word_folder, letter_idx++);
                save_subimage(source, word.x + b.x + current_x, word.y + b.y, cut_x - current_x, b.height, path);
                
                current_x = cut_x;
            }
            
            // Sauvegarde du dernier morceau (ou du seul morceau si pas de découpe)
            snprintf(path, 512, "%s/letter_%d.png", word_folder, letter_idx++);
            save_subimage(source, word.x + b.x + current_x, word.y + b.y, b.width - current_x, b.height, path);

            free(blob_histo);
        }
    }

    free(blobs);
    free(visited);
    g_object_unref(sub);
}

void export_layout_to_files(GdkPixbuf *pixbuf, PageLayout *layout, const char *output_folder) {
    if (!layout) return;
    char path[512];
    create_directory(output_folder);

    // --- EXPORT GRILLE ---
    snprintf(path, 512, "%s/grid", output_folder); 
    create_directory(path);
    for (int r = 0; r < layout->rows; r++) {
        for (int c = 0; c < layout->cols; c++) {
            Box cell = layout->grid_cells[r * layout->cols + c];
            snprintf(path, 512, "%s/grid/%d_%d.png", output_folder, c, r);
            save_subimage(pixbuf, cell.x, cell.y, cell.width, cell.height, path);
        }
    }

    // --- EXPORT MOTS ---
    if (layout->has_wordlist) {
        snprintf(path, 512, "%s/words", output_folder); 
        create_directory(path);
        for (int i = 0; i < layout->word_count; i++) {
            snprintf(path, 512, "%s/words/word_%d", output_folder, i); 
            create_directory(path);
            segment_and_save_word(pixbuf, layout->words[i], path);
        }
    }
}
