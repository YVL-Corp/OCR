#ifndef EXTRACTION_H_
#define EXTRACTION_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

// Represents a single continuous bar (horizontal or vertical)
// found in an image histogram.
typedef struct {
    int start;      // Start coordinate (x or y)
    int end;        // End coordinate (x or y)
    int thickness;  // Length of the bar in pixels
} Bar;

// A dynamic list to hold an array of Bar structs.
typedef struct {
    Bar *bars;      // Pointer to the array of bars
    int count;      // Number of bars in the array
} BarList;

// Main function to analyze a GdkPixbuf, detect the primary grid,
// and find its internal cells.
void detect_grid_from_pixbuf(GdkPixbuf *pixbuf);

#endif // EXTRACTION_H_
