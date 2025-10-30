#include "processing.h"
#include <math.h>
#include <string.h>
#include <cairo.h>

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
