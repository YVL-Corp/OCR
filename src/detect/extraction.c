#include "extraction.h"
#include <stdio.h>
#include <stdlib.h>

// --- Configuration Thresholds ---
#define BLACK_THRESHOLD 700             // Sum of RGB below this is considered "black"
#define BLOB_MIN_PIXELS 5               // Minimum size to consider a blob
#define MERGE_THRESHOLD_X 45            // Gap size to merge horizontal blocks
#define MERGE_THRESHOLD_Y 8             // Gap size to merge vertical lines (text lines)
#define GRID_LINE_THRESHOLD_PERCENT 0.5 // Sensitivity for grid line detection
#define MIN_LIST_WIDTH_RATIO 0.10       // List must be at least 10% of grid width
#define TEXT_LINE_MIN_HEIGHT 8          // Minimum height for a line of text

// Horizontal gap to split two words on the same line (e.g., "DESK   THE")
#define WORD_SPLIT_GAP 15 

// --- Internal Structures for Histogram Analysis ---
typedef struct { int start; int end; int thickness; } Bar;
typedef struct { Bar *bars; int count; } BarList;

// --- Helper Functions ---

/**
 * Frees a BarList structure.
 */
static void free_bar_list(BarList *list) {
    if (list) { free(list->bars); free(list); }
}

/**
 * Comparator for sorting bars by thickness (descending).
 */
static int compare_bars(const void *a, const void *b) {
    return ((Bar*)b)->thickness - ((Bar*)a)->thickness;
}

/**
 * Converts a raw histogram into a list of "Bars" (segments of activity).
 */
static BarList* analyze_bars(int *histogram, int length, int threshold) {
    int count = 0, in_bar = 0;
    
    // 1. Count segments
    for (int i = 0; i < length; i++) {
        if (histogram[i] > threshold) { 
            if (!in_bar) { in_bar = 1; count++; } 
        } else { 
            in_bar = 0; 
        }
    }
    
    if (count == 0) return (BarList*)calloc(1, sizeof(BarList));

    // 2. Populate segments
    BarList *list = (BarList*)malloc(sizeof(BarList));
    list->bars = (Bar*)malloc(count * sizeof(Bar));
    list->count = count;
    
    in_bar = 0; 
    int idx = -1, start = 0;
    
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
    // Close last bar if valid
    if (in_bar) {
        list->bars[idx].start = start;
        list->bars[idx].end = length - 1;
        list->bars[idx].thickness = length - start;
    }
    return list;
}

/**
 * Merges bars that are close to each other (closer than merge_gap).
 */
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
            // Merge
            curr->end = next->end;
            curr->thickness = (curr->end - curr->start) + 1;
        } else {
            // Add new
            m_idx++; 
            merged->bars[m_idx] = *next;
        }
    }
    merged->count = m_idx + 1;
    free(original->bars); 
    free(original);
    return merged;
}

/**
 * Detects words inside the identified list area.
 * Handles multi-column lists by checking horizontal gaps.
 */
static void detect_words_in_list(GdkPixbuf *pixbuf, PageLayout *layout) {
    if (!layout->has_wordlist) return;
    
    int h = gdk_pixbuf_get_height(pixbuf);
    int rs = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int nc = gdk_pixbuf_get_n_channels(pixbuf);

    // 1. Vertical Analysis: Find lines of text
    int *histo_y = (int *)calloc(h, sizeof(int));
    int end_x_list = layout->list_x + layout->list_width;
    if (end_x_list >= gdk_pixbuf_get_width(pixbuf)) end_x_list = gdk_pixbuf_get_width(pixbuf) - 1;

    for (int y = 0; y < h; y++) {
        guchar *row = pixels + y * rs;
        int blacks = 0;
        for (int x = layout->list_x; x <= end_x_list; x++) {
            if ((row[x*nc] + row[x*nc+1] + row[x*nc+2]) < BLACK_THRESHOLD) blacks++;
        }
        if (blacks > 2) histo_y[y] = blacks;
    }

    BarList *lines = merge_bar_list(analyze_bars(histo_y, h, 0), MERGE_THRESHOLD_Y);
    free(histo_y);

    if (lines->count == 0) {
        free_bar_list(lines);
        return;
    }

    // 2. Horizontal Analysis per line: Split words separated by gaps
    int capacity = lines->count * 2; 
    layout->words = (Box*)malloc(capacity * sizeof(Box));
    layout->word_count = 0;

    for (int i = 0; i < lines->count; i++) {
        Bar line = lines->bars[i];
        if (line.thickness < TEXT_LINE_MIN_HEIGHT) continue;

        // Build horizontal histogram for this specific text line
        int list_w = layout->list_width;
        int *histo_x = (int*)calloc(list_w, sizeof(int));
        
        for (int y = line.start; y <= line.end; y++) {
            guchar *row = pixels + y * rs;
            for (int x = 0; x < list_w; x++) {
                int global_x = layout->list_x + x;
                if (global_x >= gdk_pixbuf_get_width(pixbuf)) break;
                
                guchar *p = row + global_x * nc;
                if ((p[0] + p[1] + p[2]) < BLACK_THRESHOLD) {
                    histo_x[x]++;
                }
            }
        }

        // Split line if significant gaps are found
        BarList *words_in_line = merge_bar_list(analyze_bars(histo_x, list_w, 0), WORD_SPLIT_GAP);
        
        // Add detected word blocks
        for (int j = 0; j < words_in_line->count; j++) {
            if (layout->word_count >= capacity) {
                capacity *= 2;
                layout->words = realloc(layout->words, capacity * sizeof(Box));
            }

            Box *w = &layout->words[layout->word_count];
            w->x = layout->list_x + words_in_line->bars[j].start;
            w->y = line.start;
            w->width = words_in_line->bars[j].thickness;
            w->height = line.thickness;
            
            layout->word_count++;
        }

        free(histo_x);
        free_bar_list(words_in_line);
    }

    free_bar_list(lines);
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

    // --- PASS 1: Global X Projection (Find Grid vs WordList) ---
    int *gx = (int*)calloc(w, sizeof(int));
    for(int y=0; y<h; y++) {
        guchar *row = pixels + y*rs;
        for(int x=0; x<w; x++) if((row[x*nc]+row[x*nc+1]+row[x*nc+2]) < BLACK_THRESHOLD) gx[x]++;
    }
    
    BarList *xb = merge_bar_list(analyze_bars(gx, w, BLOB_MIN_PIXELS), MERGE_THRESHOLD_X);
    free(gx);
    
    if (xb->count == 0) { free_bar_list(xb); return layout; }
    
    // Sort bars to find the biggest ones (assuming biggest is grid)
    qsort(xb->bars, xb->count, sizeof(Bar), compare_bars);
    
    // 1st biggest block is the GRID
    layout->grid_x = xb->bars[0].start;
    layout->grid_width = xb->bars[0].thickness;
    
    // 2nd biggest block (if significant) is the WORD LIST
    if (xb->count > 1 && xb->bars[1].thickness > layout->grid_width * MIN_LIST_WIDTH_RATIO) {
        layout->has_wordlist = 1;
        layout->list_x = xb->bars[1].start;
        layout->list_width = xb->bars[1].thickness;
    }
    free_bar_list(xb);

    // --- PASS 2: Grid Rows Detection (Y Projection) ---
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

    // --- PASS 3: Grid Columns Detection (X Projection inside grid) ---
    int *gix = (int*)calloc(w, sizeof(int));
    for(int y=layout->grid_y; y<layout->grid_y+layout->grid_height; y++) {
        guchar *row = pixels + y*rs;
        for(int x=layout->grid_x; x<layout->grid_x+layout->grid_width; x++)
            if((row[x*nc]+row[x*nc+1]+row[x*nc+2]) < BLACK_THRESHOLD) gix[x]++;
    }
    BarList *xib = analyze_bars(gix, w, (int)(layout->grid_height * GRID_LINE_THRESHOLD_PERCENT));
    free(gix);

    layout->cols = xib->count - 1;

    // --- CELL RECONSTRUCTION ---
    // Intersect rows and cols to define grid cells
    if (layout->rows > 0 && layout->cols > 0) {
        layout->grid_cells = (Box*)malloc(layout->rows * layout->cols * sizeof(Box));
        
        for (int r = 0; r < layout->rows; r++) {
            int y0 = yb->bars[r].end + 1;
            int y1 = yb->bars[r+1].start - 1;
            int h_cell = y1 - y0 + 1;

            for (int c = 0; c < layout->cols; c++) {
                int x0 = xib->bars[c].end + 1;
                int x1 = xib->bars[c+1].start - 1;
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

    // --- WORD LIST ANALYSIS ---
    detect_words_in_list(pixbuf, layout);
    
    return layout;
}
