#ifndef PROCESSING_H
#define PROCESSING_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

struct PageLayout;
struct FoundLine;

struct PreProcessData
{
    GtkWidget *drawing_area;
    GtkWidget *scale_rotate;
    
    GdkPixbuf *original_pixbuf;
    GdkPixbuf *processed_pixbuf;
    
    double rotation_angle;

    struct PageLayout *layout;
    struct FoundLine *lines;
    int line_count;
};

// calculates the optimal binarization threshold using otsu's method
int find_otsu_threshold(GdkPixbuf *pixbuf);

// applies a grayscale and threshold filter (binarization)
void apply_bw_filter(struct PreProcessData *data);

// automatically detects and corrects the skew angle of the image
void auto_rotate(struct PreProcessData *data);

// create the rotated pixbuf and export it
GdkPixbuf *create_rotated_pixbuf(GdkPixbuf *src, double angle_deg);

#endif
