#ifndef PROCESSING_H
#define PROCESSING_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

struct PreProcessData
{
    GtkWidget *drawing_area;
    GtkWidget *scale_rotate;
    GdkPixbuf *original_pixbuf;
    GdkPixbuf *processed_pixbuf;
    double rotation_angle;
};

int find_otsu_threshold(GdkPixbuf *pixbuf);
void apply_bw_filter(struct PreProcessData *data);
void auto_rotate(struct PreProcessData *data);
GdkPixbuf *create_rotated_pixbuf(GdkPixbuf *src, double angle_deg);

#endif
