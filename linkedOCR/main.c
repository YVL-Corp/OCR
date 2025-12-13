#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h> // Pour le nettoyage de dossier
#include <unistd.h>

// --- MODULES ---
#include "processing.h"
#include "extraction.h"
#include "image_export.h"
#include "neuralnetwork/nn_module.h"
#include "solver/solver.h"

// --- CONFIG ---
#define OUTPUT_DIR "output"
#define MODEL_PATH "neuralnetwork/model3.bin"

// --- FONCTION DE NETTOYAGE (RM -RF) ---
void recursive_rmdir(const char *path) {
    DIR *d = opendir(path);
    if (!d) return; // Le dossier n'existe pas, tant mieux

    struct dirent *p;
    while ((p = readdir(d))) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;

        char *buf = malloc(strlen(path) + strlen(p->d_name) + 2);
        sprintf(buf, "%s/%s", path, p->d_name);
        
        struct stat statbuf;
        stat(buf, &statbuf);
        
        if (S_ISDIR(statbuf.st_mode)) {
            recursive_rmdir(buf); // Récursion
        } else {
            unlink(buf); // Supprimer fichier
        }
        free(buf);
    }
    closedir(d);
    rmdir(path); // Supprimer le dossier vide
}

void clean_output_directory() {
    g_print("  > Cleaning workspace ('%s')...\n", OUTPUT_DIR);
    recursive_rmdir(OUTPUT_DIR);
}

// --- UTILITAIRE : SUPPRESSION DU CANAL ALPHA ---
GdkPixbuf *ensure_rgb_no_alpha(GdkPixbuf *src) {
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    GdkPixbuf *dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    gdk_pixbuf_fill(dst, 0xFFFFFFFF);
    gdk_pixbuf_composite(src, dst, 0, 0, w, h, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);
    return dst;
}

// --- AFFICHAGE ---
G_MODULE_EXPORT gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    int area_w = gtk_widget_get_allocated_width(widget);
    int area_h = gtk_widget_get_allocated_height(widget);
    
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
    cairo_paint(cr);

    if (data->processed_pixbuf) {
        int img_w = gdk_pixbuf_get_width(data->processed_pixbuf);
        int img_h = gdk_pixbuf_get_height(data->processed_pixbuf);

        double scale_x = (double)area_w / img_w;
        double scale_y = (double)area_h / img_h;
        double scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (scale > 0.95) scale = 0.95; 

        cairo_translate(cr, area_w / 2.0, area_h / 2.0);
        cairo_rotate(cr, data->rotation_angle * (G_PI / 180.0));
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -img_w / 2.0, -img_h / 2.0);
        
        gdk_cairo_set_source_pixbuf(cr, data->processed_pixbuf, 0, 0);
        cairo_paint(cr);

        // Dessiner les solutions (traits verts)
        if (data->layout && data->lines && data->line_count > 0) {
            cairo_set_source_rgba(cr, 0.0, 0.8, 0.0, 0.7);
            cairo_set_line_width(cr, 5.0);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

            for (int i = 0; i < data->line_count; i++) {
                FoundLine line = ((FoundLine*)data->lines)[i];
                int idx_start = line.start_row * data->layout->cols + line.start_col;
                int idx_end   = line.end_row * data->layout->cols + line.end_col;

                if (idx_start < data->layout->rows * data->layout->cols && idx_end < data->layout->rows * data->layout->cols) {
                    Box b1 = data->layout->grid_cells[idx_start];
                    Box b2 = data->layout->grid_cells[idx_end];
                    cairo_move_to(cr, b1.x + b1.width / 2.0, b1.y + b1.height / 2.0);
                    cairo_line_to(cr, b2.x + b2.width / 2.0, b2.y + b2.height / 2.0);
                    cairo_stroke(cr);
                }
            }
        }

    } else {
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 24);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "No Image Loaded", &extents);
        cairo_move_to(cr, (area_w - extents.width)/2, (area_h + extents.height)/2);
        cairo_show_text(cr, "No Image Loaded");
    }
    return FALSE;
}

// --- RESET HELPERS ---
void clear_solution_data(struct PreProcessData *data) {
    if (data->lines) { free(data->lines); data->lines = NULL; }
    data->line_count = 0;
    if (data->layout) { free_page_layout(data->layout); data->layout = NULL; }
}

// ============================================================
// ==================== LOGIQUE ÉTAPES ========================
// ============================================================

gboolean run_step2_preprocess(struct PreProcessData *data) {
    if (!data->original_pixbuf) return FALSE;
    clear_solution_data(data);

    g_print("\n--- [2] PREPROCESS ---\n");
    apply_bw_filter(data); 
    auto_rotate(data); 

    if (fabs(data->rotation_angle) > 0.1) {
        GdkPixbuf *rotated = create_rotated_pixbuf(data->processed_pixbuf, data->rotation_angle);
        if (rotated) {
            g_object_unref(data->processed_pixbuf);
            data->processed_pixbuf = rotated;
            data->rotation_angle = 0.0; 
        }
    }
    gtk_widget_queue_draw(data->drawing_area);
    g_print("| [2] DONE.\n");
    return TRUE;
}

gboolean run_step3_extract(struct PreProcessData *data) {
    if (!data->processed_pixbuf) return FALSE;
    clear_solution_data(data); 

    g_print("\n--- [3] EXTRACTION ---\n");

    if (fabs(data->rotation_angle) > 0.1) {
        GdkPixbuf *rotated = create_rotated_pixbuf(data->processed_pixbuf, data->rotation_angle);
        if (rotated) {
            g_object_unref(data->processed_pixbuf);
            data->processed_pixbuf = rotated;
            data->rotation_angle = 0.0;
            if (data->scale_rotate) gtk_range_set_value(GTK_RANGE(data->scale_rotate), 0.0);
        }
    }

    GdkPixbuf *final_pixbuf = ensure_rgb_no_alpha(data->processed_pixbuf);
    data->layout = detect_layout_from_pixbuf(final_pixbuf);

    if (data->layout) {
        g_print("  > Grid Detected: %dx%d\n", data->layout->cols, data->layout->rows);
        
        // --- NETTOYAGE CRITIQUE AVANT EXPORT ---
        // On s'assure que le dossier est propre pour éviter le mélange de lettres
        // (Note: clean_output_directory vide tout, donc on recrée dans image_export.c)
        
        export_layout_to_files(final_pixbuf, data->layout, OUTPUT_DIR);
        g_print("| [3] DONE.\n");
        g_object_unref(final_pixbuf);
        return TRUE;
    } else {
        g_printerr("! Error: Grid detection failed.\n");
        g_object_unref(final_pixbuf);
        return FALSE;
    }
}

gboolean run_step4_neural(void) {
    g_print("\n--- [4] NEURAL NET ---\n");
    struct stat st = {0};
    if (stat(OUTPUT_DIR, &st) == -1) return FALSE;
    int res = nn_run_recognition(OUTPUT_DIR, MODEL_PATH);
    return (res == 0);
}

gboolean run_step5_solve(struct PreProcessData *data) {
    g_print("\n--- [5] SOLVER ---\n");
    char grid_path[1024], words_path[1024];
    snprintf(grid_path, 1024, "%s/grid.txt", OUTPUT_DIR);
    snprintf(words_path, 1024, "%s/words.txt", OUTPUT_DIR);

    int res = solve_puzzle(grid_path, words_path, (FoundLine**)&data->lines, &data->line_count);
    
    if (res == 0) {
        g_print("| [5] DONE. Found %d words.\n", data->line_count);
        gtk_widget_queue_draw(data->drawing_area);
        return TRUE;
    }
    return FALSE;
}

// ============================================================
// ==================== HANDLERS ==============================
// ============================================================

// --- STEP 1 : OPEN (AVEC NETTOYAGE) ---
G_MODULE_EXPORT void on_btn_open_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    GtkWidget *toplevel = gtk_widget_get_toplevel(data->drawing_area);
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Image", GTK_WINDOW(toplevel), GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_print("\n| [1] LOAD : %s\n", filename);

        // --- NETTOYAGE DU WORKSPACE ---
        clean_output_directory();
        clear_solution_data(data);
        if (data->original_pixbuf) g_object_unref(data->original_pixbuf);
        if (data->processed_pixbuf) g_object_unref(data->processed_pixbuf);

        GError *err = NULL;
        data->original_pixbuf = gdk_pixbuf_new_from_file(filename, &err);

        if (!data->original_pixbuf) {
            g_printerr("Error: %s\n", err->message);
            g_error_free(err);
        } else {
            data->processed_pixbuf = gdk_pixbuf_copy(data->original_pixbuf);
            data->rotation_angle = 0.0;
            gtk_widget_queue_draw(data->drawing_area);
            GtkWidget *msg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Image Loaded!\nWorkspace cleaned.");
            gtk_dialog_run(GTK_DIALOG(msg)); gtk_widget_destroy(msg);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// --- AUTRES BOUTONS ---
G_MODULE_EXPORT void on_btn_auto_rotate_clicked(GtkButton *b, gpointer d) {
    (void)b;
    if(run_step2_preprocess((struct PreProcessData*)d)) {
        GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 2 Complete!");
        gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
    }
}

G_MODULE_EXPORT void on_btn_export_clicked(GtkButton *b, gpointer d) {
    (void)b;
    if(run_step3_extract((struct PreProcessData*)d)) {
        GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 3 Complete!");
        gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
    }
}

G_MODULE_EXPORT void on_btn_neural_clicked(GtkButton *b, gpointer d) {
    (void)b; (void)d;
    if(run_step4_neural()) {
        GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 4 Complete!");
        gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
    }
}

G_MODULE_EXPORT void on_btn_solve_clicked(GtkButton *b, gpointer d) {
    (void)b;
    if(run_step5_solve((struct PreProcessData*)d)) {
        GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 5 Complete!\nLook at the image!");
        gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
    }
}

G_MODULE_EXPORT void on_btn_run_all_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    if (!data->original_pixbuf) return;

    g_print("\n=== RUNNING ALL ===\n");
    if(!run_step2_preprocess(data)) return;
    while (gtk_events_pending()) gtk_main_iteration();
    
    if(!run_step3_extract(data)) return;
    while (gtk_events_pending()) gtk_main_iteration();
    
    if(!run_step4_neural()) return;
    while (gtk_events_pending()) gtk_main_iteration();
    
    if(!run_step5_solve(data)) return;
    while (gtk_events_pending()) gtk_main_iteration();

    GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Pipeline Finished!\nSolution drawn on image.");
    gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
}

G_MODULE_EXPORT void on_scale_rotate_value_changed(GtkRange *range, gpointer user_data) {
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    data->rotation_angle = gtk_range_get_value(range);
    gtk_widget_queue_draw(data->drawing_area);
}

// --- FERMETURE DE LA FENÊTRE (NETTOYAGE FINAL) ---
G_MODULE_EXPORT void on_main_window_destroy() { 
    clean_output_directory(); // On laisse propre en partant
    gtk_main_quit(); 
}

int main(int argc, char *argv[]) {
    GtkBuilder *builder;
    GtkWidget *window;
    GError *error = NULL;
    gtk_init(&argc, &argv);

    struct PreProcessData *data = g_slice_new0(struct PreProcessData);
    
    builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, "main.ui", &error) == 0) return 1;

    window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    data->drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "drawing_area"));
    data->scale_rotate = GTK_WIDGET(gtk_builder_get_object(builder, "scale_rotate"));

    gtk_builder_connect_signals(builder, data);
    g_object_unref(builder);
    gtk_widget_show_all(window);
    gtk_main();
    
    if(data->original_pixbuf) g_object_unref(data->original_pixbuf);
    if(data->processed_pixbuf) g_object_unref(data->processed_pixbuf);
    clear_solution_data(data);
    g_slice_free(struct PreProcessData, data);
    return 0;
}
