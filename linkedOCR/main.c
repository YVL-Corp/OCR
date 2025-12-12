#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>

// --- MODULES ---
#include "processing.h"
#include "extraction.h"
#include "image_export.h"
#include "neuralnetwork/nn_module.h"
#include "solver/solver.h"

// --- CONFIG ---
#define OUTPUT_DIR "output"
#define MODEL_PATH "neuralnetwork/model3.bin"

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

// ============================================================
// ==================== LOGIQUE INTERNE (SANS POPUP) ==========
// ============================================================

// --- STEP 2 LOGIC ---
gboolean run_step2_preprocess(struct PreProcessData *data) {
    if (!data->original_pixbuf) return FALSE;

    g_print("\n-------------------------------------\n");
    g_print("---         2. PREPROCESS         ---\n");
    g_print("-------------------------------------\n");
    g_print("  > Applying Otsu Binarization...\n");
    apply_bw_filter(data); 
    g_print("  > Detecting Skew Angle...\n");
    auto_rotate(data); 

    if (fabs(data->rotation_angle) > 0.1) {
        g_print("  > Auto-correction: %.2f degrees\n", data->rotation_angle);
        GdkPixbuf *rotated = create_rotated_pixbuf(data->processed_pixbuf, data->rotation_angle);
        if (rotated) {
            g_object_unref(data->processed_pixbuf);
            data->processed_pixbuf = rotated;
            data->rotation_angle = 0.0; 
        }
    } else {
        g_print("  > Image is already straight.\n");
    }
    gtk_widget_queue_draw(data->drawing_area);
    g_print("| [2] DONE.\n");
    return TRUE;
}

// --- STEP 3 LOGIC (AVEC REPORTING DÉTAILLÉ) ---
gboolean run_step3_extract(struct PreProcessData *data) {
    if (!data->processed_pixbuf) return FALSE;

    g_print("\n-------------------------------------\n");
    g_print("---         3. EXTRACTION         ---\n");
    g_print("-------------------------------------\n");

    GdkPixbuf *final_pixbuf;
    if (fabs(data->rotation_angle) > 0.1) {
        GdkPixbuf *rotated = create_rotated_pixbuf(data->processed_pixbuf, data->rotation_angle);
        final_pixbuf = ensure_rgb_no_alpha(rotated);
        g_object_unref(rotated);
    } else {
        final_pixbuf = ensure_rgb_no_alpha(data->processed_pixbuf);
    }

    g_print("  > Analyzing Grid Structure...\n");
    PageLayout *layout = detect_layout_from_pixbuf(final_pixbuf);

    if (layout) {
        // --- RAPPORT DÉTAILLÉ RESTAURÉ ---
        g_print("\n  === DETECTION REPORT ===\n");
        
        g_print("  [GRID]\n");
        g_print("    Position       : x=%d, y=%d (W=%d, H=%d)\n", 
                layout->grid_x, layout->grid_y, layout->grid_width, layout->grid_height);
        g_print("    Structure      : %d Rows x %d Cols\n", layout->rows, layout->cols);
        
        // Affichage des coins pour vérification
        if (layout->grid_cells) {
            Box first = layout->grid_cells[0];
            Box last = layout->grid_cells[(layout->rows * layout->cols) - 1];
            g_print("    First Cell [0,0]: x=%d, y=%d, w=%d, h=%d\n", 
                    first.x, first.y, first.width, first.height);
            g_print("    Last Cell       : x=%d, y=%d, w=%d, h=%d\n", 
                    last.x, last.y, last.width, last.height);
        }

        g_print("\n  [WORD LIST]\n");
        if (layout->has_wordlist) {
            g_print("    Status         : DETECTED\n");
            g_print("    Position       : x=%d (Zone Width: %d px)\n", 
                    layout->list_x, layout->list_width);
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
        g_print("  ========================\n\n");

        g_print("  > Exporting letters to directory '%s'...\n", OUTPUT_DIR);
        export_layout_to_files(final_pixbuf, layout, OUTPUT_DIR);
        
        free_page_layout(layout);
        g_print("| [3] DONE.\n");
        g_object_unref(final_pixbuf);
        return TRUE;
    } else {
        g_printerr("! Error: Grid detection failed.\n");
        g_object_unref(final_pixbuf);
        return FALSE;
    }
}

// --- STEP 4 LOGIC ---
gboolean run_step4_neural(void) {
    g_print("\n| [4] NEURAL NET : Recognizing Characters...\n");
    struct stat st = {0};
    if (stat(OUTPUT_DIR, &st) == -1) return FALSE;

    int res = nn_run_recognition(OUTPUT_DIR, MODEL_PATH);
    if (res == 0) {
        g_print("| [4] DONE. Files generated.\n");
        return TRUE;
    }
    return FALSE;
}

// --- STEP 5 LOGIC ---
gboolean run_step5_solve(void) {
    g_print("\n| [5] SOLVER : Searching Words...\n");
    char grid_path[1024];
    char words_path[1024];
    snprintf(grid_path, sizeof(grid_path), "%s/grid.txt", OUTPUT_DIR);
    snprintf(words_path, sizeof(words_path), "%s/words.txt", OUTPUT_DIR);

    struct stat st = {0};
    if (stat(grid_path, &st) == -1) return FALSE;

    int res = solve_puzzle(grid_path, words_path);
    if (res == 0) {
        g_print("| [5] DONE.\n");
        return TRUE;
    }
    return FALSE;
}


// ============================================================
// ==================== BOUTONS (HANDLERS) ====================
// ============================================================

// --- 1. LOAD IMAGE ---
G_MODULE_EXPORT void on_btn_open_clicked(GtkButton *button, gpointer user_data)
{
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
            
            GtkWidget *msg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Image Loaded!");
            gtk_dialog_run(GTK_DIALOG(msg));
            gtk_widget_destroy(msg);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// --- 2. PREPROCESS (BUTTON) ---
G_MODULE_EXPORT void on_btn_auto_rotate_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    if (run_step2_preprocess(data)) {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 2 Complete!");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error: Load image first.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    }
}

// --- 3. EXTRACT (BUTTON) ---
G_MODULE_EXPORT void on_btn_export_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    if (run_step3_extract(data)) {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 3 Complete!\nFiles saved.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Detection Failed.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    }
}

// --- 4. NEURAL (BUTTON) ---
G_MODULE_EXPORT void on_btn_neural_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    if (run_step4_neural()) {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 4 Complete!");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "OCR Failed. Check step 3.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    }
}

// --- 5. SOLVE (BUTTON) ---
G_MODULE_EXPORT void on_btn_solve_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    if (run_step5_solve()) {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Step 5 Complete!\nPuzzle Solved.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Solver Failed.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    }
}

// --- RUN ALL (CHAINED WITHOUT INTERMEDIATE POPUPS) ---
G_MODULE_EXPORT void on_btn_run_all_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    if (!data->original_pixbuf) {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Please load an image first (Step 1).");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return;
    }

    g_print("\n#############################################\n");
    g_print("###       STARTING FULL OCR PIPELINE      ###\n");
    g_print("#############################################\n");

    if (!run_step2_preprocess(data)) return;
    while (gtk_events_pending()) gtk_main_iteration();

    if (!run_step3_extract(data)) return;
    while (gtk_events_pending()) gtk_main_iteration();

    if (!run_step4_neural()) return;
    while (gtk_events_pending()) gtk_main_iteration();

    if (!run_step5_solve()) return;
    while (gtk_events_pending()) gtk_main_iteration();

    g_print("\n#############################################\n");
    g_print("###            PIPELINE FINISHED          ###\n");
    g_print("#############################################\n");

    // SEULE POPUP FINALE
    GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, 
        "Pipeline Finished Successfully!\nCheck console for the solution grid.");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

// --- BOILERPLATE ---
G_MODULE_EXPORT void on_scale_rotate_value_changed(GtkRange *range, gpointer user_data) {
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    data->rotation_angle = gtk_range_get_value(range);
    gtk_widget_queue_draw(data->drawing_area);
}

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

    if(data->original_pixbuf) g_object_unref(data->original_pixbuf);
    if(data->processed_pixbuf) g_object_unref(data->processed_pixbuf);
    g_slice_free(struct PreProcessData, data);

    return 0;
}
