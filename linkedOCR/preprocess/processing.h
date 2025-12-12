#ifndef PROCESSING_H
#define PROCESSING_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

// Déclarations anticipées (pour éviter les erreurs d'inclusion croisée)
struct PageLayout;
struct FoundLine;

struct PreProcessData
{
    GtkWidget *drawing_area;
    GtkWidget *scale_rotate;
    
    GdkPixbuf *original_pixbuf;
    GdkPixbuf *processed_pixbuf;
    
    double rotation_angle;

    // --- NOUVEAU : Données pour le dessin final (Trait vert sur les mots) ---
    struct PageLayout *layout;  // Pour connaître la position des cases en pixels
    struct FoundLine *lines;    // Liste des lignes (début/fin) trouvées par le solver
    int line_count;             // Nombre de lignes à dessiner
};

int find_otsu_threshold(GdkPixbuf *pixbuf);
void apply_bw_filter(struct PreProcessData *data);
void auto_rotate(struct PreProcessData *data);
GdkPixbuf *create_rotated_pixbuf(GdkPixbuf *src, double angle_deg);

#endif
