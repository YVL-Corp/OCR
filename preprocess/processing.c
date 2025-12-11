#include "processing.h"
#include <math.h>
#include <string.h>
#include <cairo.h>
#include <stdint.h>

// calculates the optimal binarization threshold using Otsu's method
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

// apply a simple contrast/brightness adjustment on a pixbuf in-place
// new_gray = clamp(alpha * (gray - 128) + 128 + beta)
// alpha > 1.0  => more contrast
// alpha < 1.0  => less contrast
// beta  > 0    => brighter
// beta  < 0    => darker
void enhance_contrast(GdkPixbuf *pixbuf, double alpha, int beta)
{
    if (!pixbuf)
        return;

    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            guchar *p = pixels + y * rowstride + x * n_channels;

            // compute current gray level
            double gray = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];

            // apply contrast / brightness
            double new_gray = alpha * (gray - 128.0) + 128.0 + beta;

            // clamp to [0, 255]
            if (new_gray < 0.0)
            {
                new_gray = 0.0;
            }

            if (new_gray > 255.0)
            {
                new_gray = 255.0;
            }

            guchar g = (guchar)new_gray;

            // write back as a gray RGB pixel
            p[0] = p[1] = p[2] = g;
        }
    }
}

void remove_isolated_noise(GdkPixbuf *pixbuf)
{
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    guchar *copy = g_memdup2(pixels, rowstride * height);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            guchar *p = copy + y * rowstride + x * n_channels;

            if (p[0] == 0)
            {
                int black_neighbors = 0;

                for (int j = -1; j <= 1; j++)
                {
                    for (int i = -1; i <= 1; i++)
                    {
                        if (i == 0 && j == 0)
                            continue;

                        guchar *q = copy + (y + j) * rowstride + (x + i) * n_channels;

                        if (q[0] == 0)
                            black_neighbors++;
                    }
                }
                if (black_neighbors <= 1)
                {
                    guchar *dst = pixels + y * rowstride + x * n_channels;
                    dst[0] = dst[1] = dst[2] = 255;
                }
            }
        }
    }

    g_free(copy);
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

    // make a copy of the original to work on
    data->processed_pixbuf = gdk_pixbuf_copy(data->original_pixbuf);

    // reinforce contrast on the grayscale image
    // enhance_contrast(data->processed_pixbuf, 1.3, 0);

    // calculate the dynamic threshold based on the original image
    int threshold = find_otsu_threshold(data->original_pixbuf);
    g_print("Otsu's optimal threshold: %d\n", threshold);

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
            guchar *p = pixels + y * rowstride + x * n_channels;

            // convert to grayscale using standard luminosity formula
            guchar gray = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];

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

    remove_isolated_noise(data->processed_pixbuf);
}

// create the rotated pixbuf and export it
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

// helper to calculate variance of an array
double calculate_variance(long *data, int n)
{
    double sum = 0;
    double sum_sq = 0;

    for (int i = 0; i < n; i++)
    {
        sum += data[i];
        sum_sq += data[i] * data[i];
    }

    double mean = sum / n;

    return (sum_sq / n) - (mean * mean);
}

// detects skew angle using projection profile
double detect_skew_angle(GdkPixbuf *pixbuf)
{
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    double best_angle = 0.0;
    double max_variance = -1.0;

    // check angles from -45 to 45 degrees
    for (double angle = -45.0; angle <= 45.0; angle += 0.5)
    {
        double rad = angle * G_PI / 180.0;
        double c = cos(rad);
        double s = sin(rad);

        // calculate new height bounds to allocate histogram
        int diag = (int)sqrt(width * width + height * height);
        int bin_size = diag * 2; // safety margin, center at diag
        long *histogram = g_malloc0(bin_size * sizeof(long));

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                guchar *p = pixels + y * rowstride + x * n_channels;
                // check if pixel is black (text)
                // we assume binarized image: 0 is black, 255 is white
                if (p[0] == 0)
                {
                    // project to y axis of rotated image
                    int y_prime = (int)(-x * s + y * c);

                    // shift to array index
                    int idx = y_prime + diag;

                    if (idx >= 0 && idx < bin_size)
                    {
                        histogram[idx]++;
                    }
                }
            }
        }

        double variance = calculate_variance(histogram, bin_size);

        if (variance > max_variance)
        {
            max_variance = variance;
            best_angle = angle;
        }

        g_free(histogram);
    }

    return best_angle;
}

void auto_rotate(struct PreProcessData *data)
{
    if (!data->processed_pixbuf)
    {
        return;
    }

    g_print("Detecting skew angle...\n");
    double angle = detect_skew_angle(data->processed_pixbuf);
    g_print("Detected skew angle: %.2f degrees\n", angle);

    data->rotation_angle = -angle;

    // update the slider
    if (data->scale_rotate)
    {
        gtk_range_set_value(GTK_RANGE(data->scale_rotate), -angle);
    }

    // redraw
    gtk_widget_queue_draw(data->drawing_area);
}
