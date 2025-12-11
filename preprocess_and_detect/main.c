#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>

// Inclusions des modules
#include "processing.h"   // Module Preprocess
#include "extraction.h"   // Module Detect
#include "image_export.h" // Module Export

// --- UTILITAIRE (Pour éviter les images vides) ---
GdkPixbuf *ensure_rgb_no_alpha(GdkPixbuf *src) {
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    GdkPixbuf *dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    gdk_pixbuf_fill(dst, 0xFFFFFFFF);
    gdk_pixbuf_composite(src, dst, 0, 0, w, h, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);
    return dst;
}

// --- GESTION DU BOUTON EXPORT ---
G_MODULE_EXPORT void on_btn_export_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;

    struct PreProcessData *data = (struct PreProcessData *)user_data;

    // 1. Vérifier qu'une image est chargée
    if (!data->processed_pixbuf) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, 
            GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, 
            "Aucune image à traiter.\nVeuillez ouvrir une image d'abord.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    g_print("\n----------------------------------------\n");
    g_print("--- DÉBUT DU TRAITEMENT OCR ---\n");
    g_print("----------------------------------------\n");

    // 2. Générer l'image finale ROTATÉE
    GdkPixbuf *rotated_with_alpha = create_rotated_pixbuf(
        data->processed_pixbuf,
        data->rotation_angle);

    if (!rotated_with_alpha) {
        g_printerr("Erreur : Impossible de générer l'image rotatée.\n");
        return;
    }

    // 3. APLATIR L'IMAGE (Supprimer transparence)
    GdkPixbuf *final_pixbuf = ensure_rgb_no_alpha(rotated_with_alpha);
    g_object_unref(rotated_with_alpha);

    // 4. Lancer la Détection (extraction.c)
    g_print(" > Analyse de la structure (Grille & Mots)...\n");
    PageLayout *layout = detect_layout_from_pixbuf(final_pixbuf);

    if (layout) {
        // --- RAPPORT DÉTAILLÉ DANS LA CONSOLE ---
        g_print("\n=== RAPPORT DÉTECTION ===\n");
        
        g_print("[GRILLE]\n");
        g_print("  Position Globale : x=%d, y=%d (L=%d, H=%d)\n", 
                layout->grid_x, layout->grid_y, layout->grid_width, layout->grid_height);
        g_print("  Structure        : %d Lignes x %d Colonnes\n", layout->rows, layout->cols);
        
        // Vérification des coins (pour voir si le calage est bon)
        if (layout->grid_cells) {
            Box first = layout->grid_cells[0];
            Box last = layout->grid_cells[(layout->rows * layout->cols) - 1];
            g_print("  Case [0,0]       : x=%d, y=%d, w=%d, h=%d\n", first.x, first.y, first.width, first.height);
            g_print("  Case [Fin,Fin]   : x=%d, y=%d, w=%d, h=%d\n", last.x, last.y, last.width, last.height);
        }

        g_print("\n[LISTE DE MOTS]\n");
        if (layout->has_wordlist) {
            g_print("  Status           : DÉTECTÉE\n");
            g_print("  Position         : x=%d (Largeur zone: %d px)\n", layout->list_x, layout->list_width);
            g_print("  Nombre de mots   : %d\n", layout->word_count);
            
            // Afficher les détails des mots pour vérification
            for(int i = 0; i < layout->word_count; i++) {
                Box w = layout->words[i];
                g_print("    -> Mot %02d      : y=%d, h=%d px (Largeur: %d px)\n", 
                        i+1, w.y, w.height, w.width);
            }
        } else {
            g_print("  Status           : NON DÉTECTÉE (ou fusionnée)\n");
        }
        g_print("=========================\n");

        // 5. Lancer l'Export (image_export.c)
        g_print(" > Découpage et sauvegarde dans './output/'...\n");
        export_layout_to_files(final_pixbuf, layout, "output");
        
        g_print("--- TERMINÉ AVEC SUCCÈS ---\n");

        GtkWidget *dialog = gtk_message_dialog_new(NULL, 
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, 
            "Traitement terminé !\n"
            "Lettres extraites dans le dossier 'output'.\n\n"
            "Grille: %dx%d | Mots: %d", 
            layout->rows, layout->cols, layout->word_count);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        free_page_layout(layout);
    } 
    else {
        g_printerr("Erreur : La structure de la grille n'a pas été détectée.\n");
        GtkWidget *dialog = gtk_message_dialog_new(NULL, 
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
            "Échec de la détection.\nVérifiez l'orientation de l'image.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    g_object_unref(final_pixbuf);
}

// --- CALLBACKS GUI (Venant de preprocess/main.c) ---

G_MODULE_EXPORT void on_btn_open_clicked(GtkButton *button, gpointer user_data)
{
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    GtkWidget *dialog;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Ouvrir une image",
                                         GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Annuler", GTK_RESPONSE_CANCEL,
                                         "_Ouvrir", GTK_RESPONSE_ACCEPT,
                                         NULL);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        if (data->original_pixbuf) g_object_unref(data->original_pixbuf);

        GError *error = NULL;
        data->original_pixbuf = gdk_pixbuf_new_from_file(filename, &error);

        if (error) {
            g_printerr("Erreur chargement: %s\n", error->message);
            g_error_free(error);
        } else {
            // Afficher l'original sans filtre
            if (data->processed_pixbuf) g_object_unref(data->processed_pixbuf);
            data->processed_pixbuf = gdk_pixbuf_copy(data->original_pixbuf);
            
            // Reset rotation
            data->rotation_angle = 0.0;
            if (data->scale_rotate) gtk_range_set_value(GTK_RANGE(data->scale_rotate), 0);

            gtk_widget_queue_draw(data->drawing_area);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

G_MODULE_EXPORT void on_scale_rotate_value_changed(GtkRange *range, gpointer user_data)
{
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    data->rotation_angle = gtk_range_get_value(range);
    gtk_widget_queue_draw(data->drawing_area);
}

G_MODULE_EXPORT void on_btn_auto_rotate_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    
    if (!data->original_pixbuf) return;

    g_print("--- AUTO CORRECTION ---\n");
    g_print("1. Application du filtre Otsu...\n");
    apply_bw_filter(data); 
    
    g_print("2. Détection de l'angle et rotation...\n");
    auto_rotate(data);

    if (data->scale_rotate) {
        gtk_range_set_value(GTK_RANGE(data->scale_rotate), data->rotation_angle);
    }
    gtk_widget_queue_draw(data->drawing_area);
}

G_MODULE_EXPORT gboolean on_drawing_area_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    if (!data->processed_pixbuf) return FALSE;

    int pix_w = gdk_pixbuf_get_width(data->processed_pixbuf);
    int pix_h = gdk_pixbuf_get_height(data->processed_pixbuf);
    int area_w = gtk_widget_get_allocated_width(widget);
    int area_h = gtk_widget_get_allocated_height(widget);

    cairo_translate(cr, area_w / 2.0, area_h / 2.0);
    cairo_rotate(cr, data->rotation_angle * G_PI / 180.0);
    cairo_translate(cr, -pix_w / 2.0, -pix_h / 2.0);
    
    gdk_cairo_set_source_pixbuf(cr, data->processed_pixbuf, 0, 0);
    cairo_paint(cr);

    return TRUE;
}

G_MODULE_EXPORT void on_main_window_destroy()
{
    gtk_main_quit();
}

int main(int argc, char *argv[])
{
    GtkBuilder *builder;
    GtkWidget *window;

    gtk_init(&argc, &argv);

    struct PreProcessData *data = g_slice_new(struct PreProcessData);
    data->original_pixbuf = NULL;
    data->processed_pixbuf = NULL;
    data->rotation_angle = 0.0;

    builder = gtk_builder_new_from_file("preprocess/main.ui");

    if (!builder) {
        g_printerr("Erreur: Impossible de charger preprocess/main.ui\n");
        return 1;
    }

    window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    data->drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "drawing_area"));
    data->scale_rotate = GTK_WIDGET(gtk_builder_get_object(builder, "scale_rotate"));

    gtk_builder_connect_signals(builder, data);
    g_object_unref(builder);

    gtk_widget_show_all(window);
    gtk_main();

    if (data->original_pixbuf) g_object_unref(data->original_pixbuf);
    if (data->processed_pixbuf) g_object_unref(data->processed_pixbuf);
    g_slice_free(struct PreProcessData, data);

    return 0;
}
