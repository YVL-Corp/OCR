#include <stdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "extraction.h" // Include our detection module

// Main entry point for the program.
// Loads an image from the command line argument and passes it to the
// grid detector.
int main(int argc, char **argv) {
    // 1. Check for command line argument
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image_path>\n", argv[0]);
        return 1;
    }

    // 2. Load the image using GdkPixbuf
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(argv[1], &error);

    // 3. Handle loading errors
    if (error != NULL) {
        fprintf(stderr, "Unable to load image %s: %s\n", 
                argv[1], error->message);
        g_error_free(error);
        return 1;
    }
    
    printf("Analyzing image: %s (%d x %d)\n", 
           argv[1], 
           gdk_pixbuf_get_width(pixbuf), 
           gdk_pixbuf_get_height(pixbuf));
    
    // 4. Call the main detection function from our module
    detect_grid_from_pixbuf(pixbuf);
    
    // 5. Clean up
    g_object_unref(pixbuf); 

    return 0;
}
