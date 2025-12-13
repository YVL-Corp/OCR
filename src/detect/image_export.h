#ifndef IMAGE_EXPORT_H_
#define IMAGE_EXPORT_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "extraction.h"

/**
 * Exports the detected layout (grid and words) to the specified output folder.
 * Creates subdirectories and BMP files for the neural network.
 */
void export_layout_to_files(GdkPixbuf *pixbuf, PageLayout *layout, const char *output_folder);

#endif
