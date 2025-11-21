#include <gtk/gtk.h>
#include <cairo.h>

#include "processing.h"

// exports the processed image
G_MODULE_EXPORT void on_btn_export_clicked(GtkButton *button, gpointer user_data)
{
    // silence the "unused parameter" warning
    (void)button;

    // get our shared data struct
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    // check if there's actually an image to export
    if (!data->processed_pixbuf)
    {
        g_print("No processed image to export.\n");
        return;
    }

    // create the actually rotated pixbuf
    // This function is now in processing.c
    GdkPixbuf *rotated_pixbuf = create_rotated_pixbuf(
        data->processed_pixbuf,
        data->rotation_angle);

    if (!rotated_pixbuf)
    {
        g_print("Error during the creation of the rotated image.\n");
        return;
    }

    // open a "save as" dialog
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save the corrected image",
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->drawing_area))),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    // suggest a default filename
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "corrected_image.png");

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        GError *error = NULL;

        // save as a png to disk
        if (!gdk_pixbuf_save(rotated_pixbuf, filename, "png", &error, NULL))
        {
            g_printerr("Error while saving: %s\n", error->message);
            g_error_free(error);
        }
        else
        {
            g_print("Image successfully exported to %s\n", filename);
        }

        g_free(filename);
    }

    // cleanup dialog and pixbuf
    gtk_widget_destroy(dialog);
    g_object_unref(rotated_pixbuf);
}

// loads an image and applies the b&w filter
G_MODULE_EXPORT void on_btn_open_clicked(GtkButton *button, gpointer user_data)
{
    // get our shared data struct
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    // set up the file chooser dialog
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new(
        "Open an image",
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
        action,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Open",
        GTK_RESPONSE_ACCEPT,
        NULL);

    // show the dialog
    res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT)
    {
        // get the chosen filename
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        // free the old pixbuf if one exists
        if (data->original_pixbuf)
        {
            g_object_unref(data->original_pixbuf);
        }

        GError *error = NULL;

        // load the new one
        data->original_pixbuf = gdk_pixbuf_new_from_file(filename, &error);

        if (error)
        {
            g_printerr("Error loading the image: %s\n", error->message);
            g_error_free(error);
        }
        else
        {
            // apply the b&w filter (this function is in processing.c)
            apply_bw_filter(data);
            // tell the drawing area to redraw
            gtk_widget_queue_draw(data->drawing_area);
        }

        // free the filename string
        g_free(filename);
    }

    // destroy the dialog
    gtk_widget_destroy(dialog);
}

// "value-changed" signal for the rotation slider
G_MODULE_EXPORT void on_scale_rotate_value_changed(GtkRange *range, gpointer user_data)
{
    // get our shared data struct
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    // update the angle in our data struct
    data->rotation_angle = gtk_range_get_value(range);

    // tell the drawing area to redraw
    gtk_widget_queue_draw(data->drawing_area);
}

// "clicked" signal for the auto-rotate button
G_MODULE_EXPORT void on_btn_auto_rotate_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    struct PreProcessData *data = (struct PreProcessData *)user_data;
    
    auto_rotate(data);
}

// "draw" signal, executes when the drawing area needs to be redrawn
G_MODULE_EXPORT gboolean on_drawing_area_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    // get our shared data struct
    struct PreProcessData *data = (struct PreProcessData *)user_data;

    // safety check: no image, nothing to draw
    if (!data->processed_pixbuf)
    {
        return FALSE;
    }

    // image dimensions
    int pix_width = gdk_pixbuf_get_width(data->processed_pixbuf);
    int pix_height = gdk_pixbuf_get_height(data->processed_pixbuf);

    // drawing area dimensions
    int area_width = gtk_widget_get_allocated_width(widget);
    int area_height = gtk_widget_get_allocated_height(widget);

    // move origin to the center of the drawing area
    cairo_translate(cr, area_width / 2.0, area_height / 2.0);

    // rotate the coordinate system
    cairo_rotate(cr, data->rotation_angle * G_PI / 180.0);

    // translate back to center the *image* on the origin
    cairo_translate(cr, -pix_width / 2.0, -pix_height / 2.0);

    // set our processed image as the source
    gdk_cairo_set_source_pixbuf(cr, data->processed_pixbuf, 0, 0);

    // paint it
    cairo_paint(cr);

    return TRUE;
}

// main window "destroy" signal (quits app)
G_MODULE_EXPORT void on_main_window_destroy()
{
    gtk_main_quit();
}

int main(int argc, char *argv[])
{
    GtkBuilder *builder;
    GtkWidget *window;

    // init gtk
    gtk_init(&argc, &argv);

    // init our data struct
    struct PreProcessData *data = g_slice_new(struct PreProcessData);
    data->original_pixbuf = NULL;
    data->processed_pixbuf = NULL;
    data->rotation_angle = 0.0;

    // load the .ui file made with Glade (https://glade.gnome.org/)
    builder = gtk_builder_new_from_file("main.ui");

    // get the widgets we need
    window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    data->drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "drawing_area"));
    data->scale_rotate = GTK_WIDGET(gtk_builder_get_object(builder, "scale_rotate"));

    // connect signals to our data struct
    gtk_builder_connect_signals(builder, data);

    // free the builder
    g_object_unref(builder);

    // show the window
    gtk_widget_show_all(window);

    // gtk main loop
    gtk_main();

    // cleanup
    if (data->original_pixbuf)
    {
        g_object_unref(data->original_pixbuf);
    }

    if (data->processed_pixbuf)
    {
        g_object_unref(data->processed_pixbuf);
    }

    // free the struct itself
    g_slice_free(struct PreProcessData, data);

    return 0;
}
