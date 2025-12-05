#include "extraction.h"
#include <stdio.h>
#include <stdlib.h>

// --- Seuils ---
#define BLACK_THRESHOLD 700             
#define BLOB_MIN_PIXELS 5               
#define MERGE_THRESHOLD_X 45            
#define MERGE_THRESHOLD_Y 8             
#define GRID_LINE_THRESHOLD_PERCENT 0.5 
#define MIN_LIST_WIDTH_RATIO 0.10       
#define TEXT_LINE_MIN_HEIGHT 8          

// --- Structures Internes ---
typedef struct { int start; int end; int thickness; } Bar;
typedef struct { Bar *bars; int count; } BarList;

// --- Fonctions Utilitaires ---
static void free_bar_list(BarList *list) {
    if (list) { free(list->bars); free(list); }
}

static int compare_bars(const void *a, const void *b) {
    return ((Bar*)b)->thickness - ((Bar*)a)->thickness;
}

static BarList* analyze_bars(int *histogram, int length, int threshold) {
    int count = 0, in_bar = 0;
    for (int i = 0; i < length; i++) {
        if (histogram[i] > threshold) { if (!in_bar) { in_bar = 1; count++; } }
        else { in_bar = 0; }
    }
    if (count == 0) return (BarList*)calloc(1, sizeof(BarList));

    BarList *list = (BarList*)malloc(sizeof(BarList));
    list->bars = (Bar*)malloc(count * sizeof(Bar));
    list->count = count;
    
    in_bar = 0; int idx = -1, start = 0;
    for (int i = 0; i < length; i++) {
        if (histogram[i] > threshold) {
            if (!in_bar) { in_bar = 1; start = i; idx++; }
        } else {
            if (in_bar) {
                in_bar = 0;
                list->bars[idx].start = start;
                list->bars[idx].end = i - 1;
                list->bars[idx].thickness = i - start;
            }
        }
    }
    if (in_bar) {
        list->bars[idx].start = start;
        list->bars[idx].end = length - 1;
        list->bars[idx].thickness = length - start;
    }
    return list;
}

static BarList* merge_bar_list(BarList *original, int merge_gap) {
    if (!original || original->count == 0) return original;
    BarList *merged = (BarList*)malloc(sizeof(BarList));
    merged->bars = (Bar*)malloc(original->count * sizeof(Bar));
    
    int m_idx = 0;
    merged->bars[0] = original->bars[0];

    for (int i = 1; i < original->count; i++) {
        Bar *curr = &merged->bars[m_idx];
        Bar *next = &original->bars[i];

        if ((next->start - curr->end) <= merge_gap) {
            curr->end = next->end;
            curr->thickness = (curr->end - curr->start) + 1;
        } else {
            m_idx++; merged->bars[m_idx] = *next;
        }
    }
    merged->count = m_idx + 1;
    free(original->bars); free(original);
    return merged;
}

static void detect_words_in_list(GdkPixbuf *pixbuf, PageLayout *layout) {
    if (!layout->has_wordlist) return;
    int h = gdk_pixbuf_get_height(pixbuf);
    int rs = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int nc = gdk_pixbuf_get_n_channels(pixbuf);

    int *histo = (int *)calloc(h, sizeof(int));
    int end_x = layout->list_x + layout->list_width;
    if (end_x >= gdk_pixbuf_get_width(pixbuf)) end_x = gdk_pixbuf_get_width(pixbuf) - 1;

    for (int y = 0; y < h; y++) {
        guchar *row = pixels + y * rs;
        int blacks = 0;
        for (int x = layout->list_x; x <= end_x; x++) {
            if ((row[x*nc] + row[x*nc+1] + row[x*nc+2]) < BLACK_THRESHOLD) blacks++;
        }
        if (blacks > 2) histo[y] = blacks;
    }

    BarList *raw = analyze_bars(histo, h, 0);
    BarList *words = merge_bar_list(raw, MERGE_THRESHOLD_Y);

    if (words->count > 0) {
        int valid = 0;
        for(int i=0; i<words->count; i++) if(words->bars[i].thickness >= TEXT_LINE_MIN_HEIGHT) valid++;
        
        layout->word_count = valid;
        layout->words = (Box*)malloc(valid * sizeof(Box));
        int idx = 0;
        for(int i=0; i<words->count; i++) {
            if(words->bars[i].thickness >= TEXT_LINE_MIN_HEIGHT) {
                layout->words[idx].x = layout->list_x;
                layout->words[idx].width = layout->list_width;
                layout->words[idx].y = words->bars[i].start;
                layout->words[idx].height = words->bars[i].thickness;
                idx++;
            }
        }
    }
    free(histo); free_bar_list(words);
}

void free_page_layout(PageLayout *layout) {
    if (layout) { 
        if (layout->words) free(layout->words); 
        if (layout->grid_cells) free(layout->grid_cells);
        free(layout); 
    }
}

PageLayout* detect_layout_from_pixbuf(GdkPixbuf *pixbuf) {
    PageLayout *layout = (PageLayout*)calloc(1, sizeof(PageLayout));
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int rs = gdk_pixbuf_get_rowstride(pixbuf);
    int nc = gdk_pixbuf_get_n_channels(pixbuf);

    // --- PASS 1: Global X ---
    int *gx = (int*)calloc(w, sizeof(int));
    for(int y=0; y<h; y++) {
        guchar *row = pixels + y*rs;
        for(int x=0; x<w; x++) if((row[x*nc]+row[x*nc+1]+row[x*nc+2]) < BLACK_THRESHOLD) gx[x]++;
    }
    BarList *xb = merge_bar_list(analyze_bars(gx, w, BLOB_MIN_PIXELS), MERGE_THRESHOLD_X);
    free(gx);
    
    if (xb->count == 0) { free_bar_list(xb); return layout; }
    qsort(xb->bars, xb->count, sizeof(Bar), compare_bars);
    
    layout->grid_x = xb->bars[0].start;
    layout->grid_width = xb->bars[0].thickness;
    if (xb->count > 1 && xb->bars[1].thickness > layout->grid_width * MIN_LIST_WIDTH_RATIO) {
        layout->has_wordlist = 1;
        layout->list_x = xb->bars[1].start;
        layout->list_width = xb->bars[1].thickness;
    }
    free_bar_list(xb);

    // --- PASS 2: Grid Rows (Y) ---
    int *gy = (int*)calloc(h, sizeof(int));
    for(int y=0; y<h; y++) {
        guchar *row = pixels + y*rs;
        for(int x=layout->grid_x; x<layout->grid_x+layout->grid_width; x++)
            if((row[x*nc]+row[x*nc+1]+row[x*nc+2]) < BLACK_THRESHOLD) gy[y]++;
    }
    BarList *yb = analyze_bars(gy, h, (int)(layout->grid_width * GRID_LINE_THRESHOLD_PERCENT));
    free(gy);

    layout->grid_y = yb->bars[0].start;
    layout->grid_height = (yb->bars[yb->count-1].end - layout->grid_y) + 1;
    layout->rows = yb->count - 1;

    // --- PASS 3: Grid Cols (X) ---
    int *gix = (int*)calloc(w, sizeof(int));
    for(int y=layout->grid_y; y<layout->grid_y+layout->grid_height; y++) {
        guchar *row = pixels + y*rs;
        for(int x=layout->grid_x; x<layout->grid_x+layout->grid_width; x++)
            if((row[x*nc]+row[x*nc+1]+row[x*nc+2]) < BLACK_THRESHOLD) gix[x]++;
    }
    BarList *xib = analyze_bars(gix, w, (int)(layout->grid_height * GRID_LINE_THRESHOLD_PERCENT));
    free(gix);

    layout->cols = xib->count - 1;

    // --- CONSTRUCTION PRÉCISE DES CELLULES ---
    // On utilise les barres détectées pour définir les frontières exactes
    if (layout->rows > 0 && layout->cols > 0) {
        layout->grid_cells = (Box*)malloc(layout->rows * layout->cols * sizeof(Box));
        
        for (int r = 0; r < layout->rows; r++) {
            // La case est située ENTRE la barre r et la barre r+1
            int y0 = yb->bars[r].end + 1; // Juste après la ligne du haut
            int y1 = yb->bars[r+1].start - 1; // Juste avant la ligne du bas
            int h_cell = y1 - y0 + 1;

            for (int c = 0; c < layout->cols; c++) {
                int x0 = xib->bars[c].end + 1; // Juste après la ligne de gauche
                int x1 = xib->bars[c+1].start - 1; // Juste avant la ligne de droite
                int w_cell = x1 - x0 + 1;

                int index = r * layout->cols + c;
                layout->grid_cells[index].x = x0;
                layout->grid_cells[index].y = y0;
                layout->grid_cells[index].width = w_cell;
                layout->grid_cells[index].height = h_cell;
            }
        }
    }

    free_bar_list(yb);
    free_bar_list(xib);

    detect_words_in_list(pixbuf, layout);
    return layout;
}
