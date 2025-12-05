#include <stdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "extraction.h"
#include "image_export.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image_path>\n", argv[0]);
        return 1;
    }

    GError *err = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(argv[1], &err);
    if (!pixbuf) { fprintf(stderr, "%s\n", err->message); return 1; }

    printf("Image: %s (%dx%d)\n", argv[1], gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf));

    PageLayout *layout = detect_layout_from_pixbuf(pixbuf);
    if (layout) {
        printf("\n=== RAPPORT ===\n");
        printf("GRILLE : %d lignes x %d colonnes\n", layout->rows, layout->cols);
        printf("LISTE  : %d mots trouvés\n", layout->word_count);

        printf("\n=== EXPORT DES IMAGES ===\n");
        export_layout_to_files(pixbuf, layout, "output");
        printf("Terminé. Voir le dossier 'output'.\n");
        
        free_page_layout(layout);
    }
    g_object_unref(pixbuf);
    return 0;
}
