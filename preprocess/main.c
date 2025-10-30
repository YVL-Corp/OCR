#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

#include <math.h> // for cos, sin, fabs

// our app's shared state
struct PreProcessData
{
    GtkWidget *drawing_area;
    GdkPixbuf *original_pixbuf;
    GdkPixbuf *processed_pixbuf;
    double rotation_angle;
};

int find_otsu_threshold(GdkPixbuf *pixbuf)
{
    // build a grayscale histogram (256 bins)
    long histogram[256] = {0};

    // get image info
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    // image dimensions
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    long total_pixels = width * height;

    // number of color channels (usually 3 for RGB, 4 for RGBA)
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    // distance in bytes to get to the next row (stride)
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    // first pass: populate the histogram
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            guchar *p = pixels + y * rowstride + x * n_channels;
            // standard luminosity formula
            guchar gray = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
            histogram[gray]++;
        }
    }

    // Otsu's algorithm to find the optimal threshold
    float sum = 0;

    for (int i = 0; i < 256; i++)
    {
        // total sum of all pixel intensities
        sum += i * histogram[i];
    }

    // background sum, background weight & foreground weight
    float sumB = 0; 
    long wB = 0;
    long wF = 0;

    // max inter-class variance
    float varMax = 0;
    
    // the threshold that gives varMax
    int optimal_threshold = 0; 

    // loop through all possible thresholds (0-255)
    for (int t = 0; t < 256; t++)
    {
        // add pixels at this level to background
        wB += histogram[t];

        if (wB == 0)
        {
            // skip if no pixels in background
            continue;
        }

        // foreground is the rest
        wF = total_pixels - wB;

        if (wF == 0)
        {
            // all pixels are in background, we're done
            break;
        }

        sumB += (float)(t * histogram[t]);

        // mean intensity of background & foreground
        float mB = sumB / wB;
        float mF = (sum - sumB) / wF;

        // calculate inter-class variance
        float varBetween = (float)wB * (float)wF * (mB - mF) * (mB - mF);

        // check if this is the best variance we've found so far
        if (varBetween > varMax)
        {
            varMax = varBetween;
            optimal_threshold = t;
        }
    }

    return optimal_threshold;
}

// applies a grayscale and threshold filter (binarization)
void apply_bw_filter(struct PreProcessData *data)
{
    // safety check, no image loaded
    if (!data->original_pixbuf)
    {
        return;
    }

    // free the old buffer if we have one
    if (data->processed_pixbuf)
    {
        g_object_unref(data->processed_pixbuf);
    }

    // calculate the dynamic threshold based on the original image
    int threshold = find_otsu_threshold(data->original_pixbuf);
    g_print("Otsu's optimal threshold: %d\n", threshold);

    // make a copy of the original to work on
    data->processed_pixbuf = gdk_pixbuf_copy(data->original_pixbuf);

    // get image info
    guchar *pixels = gdk_pixbuf_get_pixels(data->processed_pixbuf);

    // image dimensions
    int width = gdk_pixbuf_get_width(data->processed_pixbuf);
    int height = gdk_pixbuf_get_height(data->processed_pixbuf);

    // number of color channels (usually 3 for RGB, 4 for RGBA)
    int n_channels = gdk_pixbuf_get_n_channels(data->processed_pixbuf);

    // distance in bytes to get to the next row (stride)
    int rowstride = gdk_pixbuf_get_rowstride(data->processed_pixbuf);

    // loop over each row (top to bottom)
    for (int y = 0; y < height; y++)
    {
        // loop over each pixel in that row (left to right)
        for (int x = 0; x < width; x++)
        {
            /**
             * pointer to the current pixel (x, y)
             *
             * - [pixels] : start from the top-left (0,0)
             * - [+ y * rowstride] : jump down 'y' rows
             * - [+ x * n_channels] : move 'x' pixels to the right
             *
             * p[0] = R, p[1] = G, p[2] = B
             */
            guchar *p = pixels + y * rowstride + x * n_channels;

            // convert to grayscale using standard luminosity formula
            // formula source: https://www.w3.org/TR/AERT/#color-contrast
            guchar gray = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];

            // median threshold to decide if a pixel is light or dark
            // for the 2nd presentation, this will be dynamic (Otsu's method)

            if (gray > threshold)
            {
                // if the pixel is lighter than the threshold, make it white
                p[0] = p[1] = p[2] = 255;
            }
            else
            {
                // otherwise, make it black
                p[0] = p[1] = p[2] = 0;
            }
        }
    }
}

GdkPixbuf *create_rotated_pixbuf(GdkPixbuf *src, double angle_deg)
{
    if (!src)
    {
        return NULL;
    }

    // get original w/h
    int old_width = gdk_pixbuf_get_width(src);
    int old_height = gdk_pixbuf_get_height(src);

    // convert angle from degrees to radians
    double angle_rad = angle_deg * G_PI / 180.0;

    // pre-calculate cos and sin for efficiency
    double c = cos(angle_rad);
    double s = sin(angle_rad);

    // calculate the new bounding box size using trig projections
    int new_width = (int)(fabs(old_width * c) + fabs(old_height * s));
    int new_height = (int)(fabs(old_width * s) + fabs(old_height * c));

    // create an in-memory cairo surface (off-screen buffer)
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
    cairo_t *cr = cairo_create(surface);

    // fill the background with white so the rotated corners aren't black
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    // move origin to the center of the new surface
    cairo_translate(cr, new_width / 2.0, new_height / 2.0);

    // rotate the coordinate system
    cairo_rotate(cr, angle_rad);

    // translate back by half the *old* image size to center it
    cairo_translate(cr, -old_width / 2.0, -old_height / 2.0);

    // draw the source image onto the rotated surface
    gdk_cairo_set_source_pixbuf(cr, src, 0, 0);
    cairo_paint(cr);

    // create a new GdkPixbuf from our cairo surface
    GdkPixbuf *rotated_pixbuf = gdk_pixbuf_get_from_surface(
        surface, 0, 0, new_width, new_height);

    // cleanup to avoid mem leaks
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return rotated_pixbuf;
}

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
        g_print("Aucune image traitée à exporter.\n");
        return;
    }

    // create the actually rotated pixbuf
    GdkPixbuf *rotated_pixbuf = create_rotated_pixbuf(
        data->processed_pixbuf,
        data->rotation_angle);

    if (!rotated_pixbuf)
    {
        g_print("Erreur durant la création de l'image tournée.\n");
        return;
    }

    // open a "save as" dialog
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Enregistrer l'image corrigée",
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->drawing_area))),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    // suggest a default filename
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "image_corrigee.png");

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        GError *error = NULL;

        // save as a png to disk
        if (!gdk_pixbuf_save(rotated_pixbuf, filename, "png", &error, NULL))
        {
            g_printerr("Erreur lors de la sauvegarde : %s\n", error->message);
            g_error_free(error);
        }
        else
        {
            g_print("Image exportée avec succès vers %s\n", filename);
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
        "Ouvrir une image",
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
            g_printerr("Erreur lors du chargement de l'image: %s\n", error->message);
            g_error_free(error);
        }
        else
        {
            // apply the b&w filter
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