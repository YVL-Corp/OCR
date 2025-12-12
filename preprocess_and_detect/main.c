#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <math.h>

// Inclusions des modules locaux
#include "processing.h"   
#include "extraction.h"   
#include "image_export.h" 

// --- UTILITAIRE : SUPPRESSION DU CANAL ALPHA (TRANSPARENCE) ---
GdkPixbuf *ensure_rgb_no_alpha(GdkPixbuf *src) {
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    GdkPixbuf *dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    gdk_pixbuf_fill(dst, 0xFFFFFFFF); // Fond blanc
    gdk_pixbuf_composite(src, dst, 0, 0, w, h, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);
    return dst;
}

// --- GESTION DE L'AFFICHAGE (VISUEL) ---
G_MODULE_EXPORT gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    // Fond gris clair
    int area_w = gtk_widget_get_allocated_width(widget);
    int area_h = gtk_widget_get_allocated_height(widget);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

    if (data->processed_pixbuf) {
        int img_w = gdk_pixbuf_get_width(data->processed_pixbuf);
        int img_h = gdk_pixbuf_get_height(data->processed_pixbuf);

        // 1. Calcul de l'échelle (Fit)
        double scale_x = (double)area_w / img_w;
        double scale_y = (double)area_h / img_h;
        double scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (scale > 1.0) scale = 0.95; 

        // 2. Centrage
        cairo_translate(cr, area_w / 2.0, area_h / 2.0);

        // 3. Rotation Visuelle (Slider manuel uniquement)
        cairo_rotate(cr, data->rotation_angle * (G_PI / 180.0));

        // 4. Échelle
        cairo_scale(cr, scale, scale);

        // 5. Dessin centré
        cairo_translate(cr, -img_w / 2.0, -img_h / 2.0);
        gdk_cairo_set_source_pixbuf(cr, data->processed_pixbuf, 0, 0);
        cairo_paint(cr);
    } else {
        // Texte "No Image"
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 20);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "No Image Loaded", &extents);
        cairo_move_to(cr, (area_w - extents.width)/2, (area_h + extents.height)/2);
        cairo_show_text(cr, "No Image Loaded");
    }

    return FALSE;
}

// --- ÉTAPE 1 : CHOISIR FICHIER ---
G_MODULE_EXPORT void on_btn_open_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkWidget *toplevel = gtk_widget_get_toplevel(data->drawing_area);
    
    dialog = gtk_file_chooser_dialog_new("Open Image", GTK_WINDOW(toplevel), action, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_print("\n|-- Loading image: %s --|\n", filename);

        if (data->original_pixbuf) g_object_unref(data->original_pixbuf);
        if (data->processed_pixbuf) g_object_unref(data->processed_pixbuf);

        GError *error = NULL;
        data->original_pixbuf = gdk_pixbuf_new_from_file(filename, &error);

        if (!data->original_pixbuf) {
            g_printerr("Error loading image: %s\n", error->message);
            g_error_free(error);
        } else {
            data->processed_pixbuf = gdk_pixbuf_copy(data->original_pixbuf);
            data->rotation_angle = 0.0;
            gtk_widget_queue_draw(data->drawing_area);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// --- ÉTAPE 2 : PREPROCESS (Filtres + Rotation Physique) ---
G_MODULE_EXPORT void on_btn_auto_rotate_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    if (!data->original_pixbuf) {
        g_print("Error: Please load an image first.\n");
        return;
    }

    g_print("\n|---------- Running Preprocessing... ----------|\n");

    g_print("  > Applying Otsu Binarization...\n");
    apply_bw_filter(data); 

    g_print("  > Detecting Skew Angle...\n");
    auto_rotate(data); 

    if (fabs(data->rotation_angle) > 0.1) {
        g_print("  > Correcting Skew: %.2f degrees...\n", data->rotation_angle);
        
        GdkPixbuf *rotated = create_rotated_pixbuf(data->processed_pixbuf, data->rotation_angle);
        
        if (rotated) {
            g_object_unref(data->processed_pixbuf);
            data->processed_pixbuf = rotated;
            data->rotation_angle = 0.0; 
        }
    } else {
        g_print("  > No significant rotation needed.\n");
    }

    gtk_widget_queue_draw(data->drawing_area);
    g_print("|---------- Preprocessing Completed. ----------|\n");
}

// --- ÉTAPE 3 : EXTRACT (Découpage & Reporting) ---
G_MODULE_EXPORT void on_btn_export_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    if (!data->processed_pixbuf) {
        g_print("Error: No image to extract.\n");
        return;
    }

    g_print("\n|----------- Running Extraction... -----------|\n");

    // 1. Aplatir l'image
    GdkPixbuf *final_pixbuf;
    if (fabs(data->rotation_angle) > 0.1) {
        g_print("  > Applying manual rotation (%.2f deg)...\n", data->rotation_angle);
        GdkPixbuf *rotated = create_rotated_pixbuf(data->processed_pixbuf, data->rotation_angle);
        final_pixbuf = ensure_rgb_no_alpha(rotated);
        g_object_unref(rotated);
    } else {
        final_pixbuf = ensure_rgb_no_alpha(data->processed_pixbuf);
    }

    // 2. Détection
    g_print("  > Analyzing Grid Structure...\n");
    PageLayout *layout = detect_layout_from_pixbuf(final_pixbuf);

    if (layout) {
        // --- RAPPORT DÉTAILLÉ (RESTAURÉ) ---
        g_print("\n||========== DETECTION REPORT ==========||\n");
        
        g_print("  [GRID]\n");
        g_print("    Position       : x=%d, y=%d (W=%d, H=%d)\n", 
                layout->grid_x, layout->grid_y, layout->grid_width, layout->grid_height);
        g_print("    Structure      : %d Rows x %d Cols\n", layout->rows, layout->cols);
        
        if (layout->grid_cells) {
            Box first = layout->grid_cells[0];
            Box last = layout->grid_cells[(layout->rows * layout->cols) - 1];
            g_print("    First Cell [0,0]: x=%d, y=%d, w=%d, h=%d\n", first.x, first.y, first.width, first.height);
            g_print("    Last Cell       : x=%d, y=%d, w=%d, h=%d\n", last.x, last.y, last.width, last.height);
        }

        g_print("\n  [WORD LIST]\n");
        if (layout->has_wordlist) {
            g_print("    Status         : DETECTED\n");
            g_print("    Position       : x=%d (Zone Width: %d px)\n", layout->list_x, layout->list_width);
            g_print("    Words Count    : %d\n", layout->word_count);
            
            // Détail des mots pour vérification
            for(int i = 0; i < layout->word_count; i++) {
                Box w = layout->words[i];
                g_print("      -> Word %02d   : y=%d, h=%d px (w=%d px)\n", 
                        i+1, w.y, w.height, w.width);
            }
        } else {
            g_print("    Status         : NOT DETECTED (or merged)\n");
        }
        g_print("||======================================||\n\n");

        // 3. Export
        g_print("  > Exporting letters to './output/'...\n");
        export_layout_to_files(final_pixbuf, layout, "output");
        
        free_page_layout(layout);
        g_print("|------------ Extraction Completed. ------------|\n");
    } else {
        g_printerr("Error: Grid detection failed.\n");
    }

    g_object_unref(final_pixbuf);
}

// --- ÉTAPE 4 : NEURAL NETWORK ---
G_MODULE_EXPORT void on_btn_neural_clicked(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    g_print("\n|--------- Neural Network Recognition... ---------|\n");
    g_print("  [TODO] Not implemented yet.\n");
}

// --- ÉTAPE 5 : SOLVE ---
G_MODULE_EXPORT void on_btn_solve_clicked(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    g_print("\n|--------- Solving Puzzle... ---------|\n");
    g_print("  [TODO] Not implemented yet.\n");
}

// --- RUN ALL ---
G_MODULE_EXPORT void on_btn_run_all_clicked(GtkButton *button, gpointer user_data)
{
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    if (!data->original_pixbuf) {
        g_print("Error: Load an image first.\n");
        return;
    }

    g_print("\n||======================================||\n");
    g_print("      RUNNING FULL OCR PIPELINE\n");
    g_print("||======================================||\n");
    
    on_btn_auto_rotate_clicked(NULL, user_data);
    while (gtk_events_pending()) gtk_main_iteration(); 

    on_btn_export_clicked(NULL, user_data);
    while (gtk_events_pending()) gtk_main_iteration();

    on_btn_neural_clicked(NULL, user_data);
    while (gtk_events_pending()) gtk_main_iteration();

    on_btn_solve_clicked(NULL, user_data);
    while (gtk_events_pending()) gtk_main_iteration();

    g_print("\n||====================================||\n");
    g_print("         PIPELINE FINISHED\n");
    g_print("||====================================||\n");
}

// --- MAIN ENTRY POINT ---
G_MODULE_EXPORT void on_main_window_destroy() { gtk_main_quit(); }

int main(int argc, char *argv[])
{
    GtkBuilder *builder;
    GtkWidget *window;
    GError *error = NULL;

    gtk_init(&argc, &argv);

    struct PreProcessData *data = g_slice_new(struct PreProcessData);
    data->original_pixbuf = NULL;
    data->processed_pixbuf = NULL;
    data->rotation_angle = 0.0;
    data->scale_rotate = NULL;

    builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, "main.ui", &error) == 0) {
        g_printerr("Error loading UI: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    data->drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "drawing_area"));
    
    gtk_builder_connect_signals(builder, data);
    g_object_unref(builder);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
