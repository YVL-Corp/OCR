#include "image_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

double* load_and_convert_image(const char *filepath) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filepath, &error);
    
    if (!pixbuf) {
        fprintf(stderr, "Error loading image %s: %s\n", filepath, error->message);
        g_error_free(error);
        return NULL;
    }

    // Redimensionner
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, IMAGE_SIZE, IMAGE_SIZE, GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);
    
    if (!scaled) {
        fprintf(stderr, "Error scaling image: %s\n", filepath);
        return NULL;
    }

    // Obtenir les données de l'image
    int n_channels = gdk_pixbuf_get_n_channels(scaled);
    int rowstride = gdk_pixbuf_get_rowstride(scaled);
    guchar *pixels_data = gdk_pixbuf_get_pixels(scaled);

    // Convertir en array de doubles (0 ou 1)
    double *pixels = malloc(PIXEL_COUNT * sizeof(double));
    
    for (int y = 0; y < IMAGE_SIZE; y++) {
        for (int x = 0; x < IMAGE_SIZE; x++) {
            guchar *pixel = pixels_data + y * rowstride + x * n_channels;
            
            // Calculer la luminosité
            int luminance;
            if (n_channels >= 3) {
                luminance = (pixel[0] + pixel[1] + pixel[2]) / 3;
            } else {
                luminance = pixel[0];
            }
            
            // Seuil à 128: pixels sombres = 1, pixels clairs = 0
            pixels[y * IMAGE_SIZE + x] = luminance < 128 ? 1.0 : 0.0;
        }
    }

    g_object_unref(scaled);
    return pixels;
}

static gboolean is_valid_image(const char *filename) {
    int len = strlen(filename);
    if (len < 5) return FALSE;
    
    const char *ext = filename + len - 4;
    return (strcmp(ext, ".png") == 0 || 
            strcmp(ext, ".bmp") == 0 || 
            strcmp(ext, ".jpg") == 0);
}

int load_images_from_folder(const char *folder_path, int label, ImageData **images, int *current_count, int max_images) {
    GError *error = NULL;
    GDir *dir = g_dir_open(folder_path, 0, &error);
    
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open folder '%s': %s\n", folder_path, error->message);
        g_error_free(error);
        return 0;
    }

    // Collecter tous les fichiers valides
    GPtrArray *valid_files = g_ptr_array_new_with_free_func(g_free);
    const gchar *filename;
    
    while ((filename = g_dir_read_name(dir)) != NULL) {
        if (is_valid_image(filename)) {
            g_ptr_array_add(valid_files, g_strdup(filename));
        }
    }
    g_dir_close(dir);
    
    guint total_files = valid_files->len;
    if (total_files == 0) {
        g_ptr_array_free(valid_files, TRUE);
        return 0;
    }
    
    // Mélanger les fichiers
    int *indices = malloc(total_files * sizeof(int));
    for (guint i = 0; i < total_files; i++) {
        indices[i] = i;
    }
    
    // Shuffle
    for (int i = total_files - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }
    
    // Charger les images
    int loaded = 0;
    int max_to_load = ((int)total_files < max_images) ? (int)total_files : max_images;
    
    for (int i = 0; i < max_to_load; i++) {
        filename = g_ptr_array_index(valid_files, indices[i]);
        gchar *filepath = g_build_filename(folder_path, filename, NULL);

        double *pixels = load_and_convert_image(filepath);
        g_free(filepath);
        
        if (!pixels) continue;

        (*images)[*current_count].pixels = pixels;
        (*images)[*current_count].label = label;
        (*current_count)++;
        loaded++;
    }
    
    free(indices);
    g_ptr_array_free(valid_files, TRUE);
    return loaded;
}

int load_dataset(const char *root_path, ImageData **images, int max_per_letter) {
    GError *error = NULL;
    GDir *dir = g_dir_open(root_path, 0, &error);
    
    if (!dir) {
        fprintf(stderr, "Error: Cannot open root folder '%s': %s\n", root_path, error->message);
        g_error_free(error);
        return 0;
    }

    // Allouer mémoire (26 lettres max)
    *images = malloc(26 * max_per_letter * sizeof(ImageData));
    if (!*images) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        g_dir_close(dir);
        return 0;
    }

    int total_count = 0;
    const gchar *entry_name;
    
    printf("Loading dataset from '%s'...\n", root_path);

    while ((entry_name = g_dir_read_name(dir)) != NULL) {
        if (entry_name[0] == '.') continue;

        gchar *letter_path = g_build_filename(root_path, entry_name, NULL);

        if (!g_file_test(letter_path, G_FILE_TEST_IS_DIR)) {
            g_free(letter_path);
            continue;
        }

        // Déterminer le label à partir du nom du dossier
        int label = -1;
        if (entry_name[0] >= 'A' && entry_name[0] <= 'Z') {
            label = entry_name[0] - 'A';
        } else if (entry_name[0] >= 'a' && entry_name[0] <= 'z') {
            label = entry_name[0] - 'a';
        }

        if (label < 0 || label >= 26) {
            g_free(letter_path);
            continue;
        }

        printf("  Loading letter '%c' (label %d)...", 'A' + label, label);
        fflush(stdout);

        int loaded = load_images_from_folder(letter_path, label, images, &total_count, max_per_letter);
        printf(" %d images loaded\n", loaded);
        
        g_free(letter_path);
    }

    g_dir_close(dir);
    
    if (total_count > 0) {
        *images = realloc(*images, total_count * sizeof(ImageData));
    }
    
    return total_count;
}

void free_images(ImageData *images, int count) {
    for (int i = 0; i < count; i++) {
        free(images[i].pixels);
    }
    free(images);
}

void print_image(double *pixels) {
    for (int i = 0; i < IMAGE_SIZE; i++) {
        for (int j = 0; j < IMAGE_SIZE; j++) {
            printf("%s", pixels[i * IMAGE_SIZE + j] > 0.5 ? "█" : " ");
        }
        printf("\n");
    }
    printf("\n");
}

static gboolean parse_grid_filename(const char *filename, int *x, int *y) {
    // Format attendu: XX_YY.bmp
    int len = strlen(filename);
    if (len < 7 || strcmp(filename + len - 4, ".bmp") != 0) return FALSE;
    
    const char *underscore = strchr(filename, '_');
    if (!underscore || underscore == filename || underscore >= filename + len - 4) return FALSE;
    
    char x_str[16], y_str[16];
    int x_len = underscore - filename;
    int y_len = (filename + len - 4) - (underscore + 1);
    
    if (x_len >= 16 || y_len >= 16 || x_len < 1 || y_len < 1) return FALSE;
    
    // CORRECTION ICI : Utilisation de memcpy pour éviter le warning -Wstringop-truncation
    memcpy(x_str, filename, x_len);
    x_str[x_len] = '\0';
    
    memcpy(y_str, underscore + 1, y_len);
    y_str[y_len] = '\0';
    
    for (int i = 0; i < x_len; i++) if (x_str[i] < '0' || x_str[i] > '9') return FALSE;
    for (int i = 0; i < y_len; i++) if (y_str[i] < '0' || y_str[i] > '9') return FALSE;
    
    *x = atoi(x_str);
    *y = atoi(y_str);
    
    return TRUE;
}

int load_grid_images(const char *folder_path, GridLetter **letters) {
    GError *error = NULL;
    GDir *dir = g_dir_open(folder_path, 0, &error);
    
    if (!dir) {
        fprintf(stderr, "Error: Cannot open folder '%s': %s\n", folder_path, error->message);
        g_error_free(error);
        return 0;
    }

    int count = 0;
    const gchar *filename;
    
    while ((filename = g_dir_read_name(dir)) != NULL) {
        int x, y;
        if (parse_grid_filename(filename, &x, &y)) count++;
    }
    
    if (count == 0) {
        g_dir_close(dir);
        return 0;
    }
    
    *letters = malloc(count * sizeof(GridLetter));
    
    g_dir_rewind(dir);
    int loaded = 0;
    
    while ((filename = g_dir_read_name(dir)) != NULL) {
        int x, y;
        if (!parse_grid_filename(filename, &x, &y)) continue;
        
        gchar *filepath = g_build_filename(folder_path, filename, NULL);
        double *pixels = load_and_convert_image(filepath);
        g_free(filepath);
        
        if (!pixels) continue;
        
        (*letters)[loaded].x = x;
        (*letters)[loaded].y = y;
        (*letters)[loaded].pixels = pixels;
        (*letters)[loaded].predicted_letter = '?';
        loaded++;
    }
    
    g_dir_close(dir);
    printf("Loaded %d grid images\n", loaded);
    return loaded;
}

void free_grid_letters(GridLetter *letters, int count) {
    for (int i = 0; i < count; i++) {
        free(letters[i].pixels);
    }
    free(letters);
}

typedef struct {
    char *filename;
    int number;
} NumberedFile;

static int compare_numbered_files(const void *a, const void *b) {
    const NumberedFile * const *pa = a;
    const NumberedFile * const *pb = b;
    return ((*pa)->number) - ((*pb)->number);
}

static gboolean parse_number_filename(const char *filename, int *number) {
    int len = strlen(filename);
    gboolean is_png = (len >= 4 && strcmp(filename + len - 4, ".png") == 0);
    gboolean is_bmp = (len >= 4 && strcmp(filename + len - 4, ".bmp") == 0);
    
    if (!is_png && !is_bmp) return FALSE;
    
    int ext_len = 4;
    
    if (len > 7 && strncmp(filename, "letter_", 7) == 0) {
        char num_str[16];
        int num_len = len - 7 - ext_len;
        
        if (num_len >= 16 || num_len < 1) return FALSE;
        
        // CORRECTION ICI : memcpy au lieu de strncpy
        memcpy(num_str, filename + 7, num_len);
        num_str[num_len] = '\0';
        
        for (int i = 0; i < num_len; i++) if (num_str[i] < '0' || num_str[i] > '9') return FALSE;
        
        *number = atoi(num_str);
        return TRUE;
    }
    else {
        // Cas fichiers "N.bmp"
        char num_str[16];
        int num_len = len - ext_len;
        if (num_len >= 16 || num_len < 1) return FALSE;
        
        // CORRECTION ICI : memcpy au lieu de strncpy
        memcpy(num_str, filename, num_len);
        num_str[num_len] = '\0';
        
        for (int i = 0; i < num_len; i++) if (num_str[i] < '0' || num_str[i] > '9') return FALSE;
         *number = atoi(num_str);
         return TRUE;
    }
    
    return FALSE;
}

char* load_and_predict_word(const char *word_folder, Network *net) {
    GError *error = NULL;
    GDir *dir = g_dir_open(word_folder, 0, &error);
    
    if (!dir) {
        fprintf(stderr, "Error: Cannot open folder '%s': %s\n", word_folder, error->message);
        g_error_free(error);
        return NULL;
    }

    GPtrArray *files_array = g_ptr_array_new();
    const gchar *filename;
    
    while ((filename = g_dir_read_name(dir)) != NULL) {
        int number;
        if (parse_number_filename(filename, &number)) {
            NumberedFile *nf = malloc(sizeof(NumberedFile));
            nf->filename = g_strdup(filename);
            nf->number = number;
            g_ptr_array_add(files_array, nf);
        }
    }
    g_dir_close(dir);
    
    if (files_array->len == 0) {
        g_ptr_array_free(files_array, TRUE);
        return NULL;
    }
    
    qsort(files_array->pdata, files_array->len, sizeof(gpointer), compare_numbered_files);
    
    char *word = malloc((files_array->len + 1) * sizeof(char));
    
    double *hidden = malloc(net->hidden_size * sizeof(double));
    double *output = malloc(net->output_size * sizeof(double));
    
    // Correction boucle guint
    for (guint i = 0; i < files_array->len; i++) {
        NumberedFile *nf = g_ptr_array_index(files_array, i);
        gchar *filepath = g_build_filename(word_folder, nf->filename, NULL);
        
        double *pixels = load_and_convert_image(filepath);
        g_free(filepath);
        
        if (!pixels) {
            word[i] = '?';
            continue;
        }
        
        forward(net, pixels, hidden, output);
        
        int predicted = 0;
        double max_prob = output[0];
        
        // Correction boucle size_t
        for (size_t j = 1; j < net->output_size; j++) {
            if (output[j] > max_prob) {
                max_prob = output[j];
                predicted = (int)j;
            }
        }
        
        word[i] = 'A' + predicted;
        free(pixels);
    }
    
    word[files_array->len] = '\0';
    
    free(hidden);
    free(output);
    
    // Correction boucle guint
    for (guint i = 0; i < files_array->len; i++) {
        NumberedFile *nf = g_ptr_array_index(files_array, i);
        g_free(nf->filename);
        free(nf);
    }
    g_ptr_array_free(files_array, TRUE);
    
    return word;
}

int load_and_predict_words(const char *words_folder, Network *net, Word **words) {
    GError *error = NULL;
    GDir *dir = g_dir_open(words_folder, 0, &error);
    
    if (!dir) {
        fprintf(stderr, "Error: Cannot open folder '%s': %s\n", words_folder, error->message);
        g_error_free(error);
        return 0;
    }

    GPtrArray *folders = g_ptr_array_new_with_free_func(g_free);
    const gchar *entry_name;
    
    while ((entry_name = g_dir_read_name(dir)) != NULL) {
        if (entry_name[0] == '.') continue;
        
        gchar *folder_path = g_build_filename(words_folder, entry_name, NULL);
        
        if (g_file_test(folder_path, G_FILE_TEST_IS_DIR)) {
            g_ptr_array_add(folders, folder_path);
        } else {
            g_free(folder_path);
        }
    }
    g_dir_close(dir);
    
    guint num_words = folders->len;
    if (num_words == 0) {
        g_ptr_array_free(folders, TRUE);
        return 0;
    }
    
    g_ptr_array_sort(folders, (GCompareFunc)strcmp);
    
    *words = malloc(num_words * sizeof(Word));
    
    // Correction boucle guint
    for (guint i = 0; i < num_words; i++) {
        const char *folder_path = g_ptr_array_index(folders, i);
        const char *folder_name = strrchr(folder_path, '/');
        if (!folder_name) folder_name = strrchr(folder_path, '\\');
        folder_name = folder_name ? folder_name + 1 : folder_path;
        
        printf("  Processing word folder '%s'...", folder_name);
        fflush(stdout);
        
        char *word = load_and_predict_word(folder_path, net);
        
        if (word) {
            (*words)[i].word = word;
            (*words)[i].length = strlen(word);
            printf(" '%s'\n", word);
        } else {
            (*words)[i].word = strdup("ERROR");
            (*words)[i].length = 5;
            printf(" FAILED\n");
        }
    }
    
    g_ptr_array_free(folders, TRUE);
    return (int)num_words;
}

void free_words(Word *words, int count) {
    for (int i = 0; i < count; i++) {
        free(words[i].word);
    }
    free(words);
}
