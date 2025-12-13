#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <err.h>    
#include <ctype.h>  
#include <stddef.h> 
#include "solver.h" 

// Directions
int directions[8][2] = {
    {0, 1}, {1, 1}, {1, 0}, {1, -1},
    {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}
};

// --- 1. CONFIGURATION DES ÉQUIVALENCES ---
typedef struct {
    const char *a;
    const char *b;
} OcrEquivalence;

// Liste des paires interchangeables
OcrEquivalence EQUIVALENCES[] = {
    {"VV", "W"},  // VV <-> W
    {"Q", "O"},   // Q  <-> O
    {"C", "G"},   // C  <-> G
    {"M", "N"}    // M  <-> N

};;

// --- UTILITAIRE : REMPLACEMENT ---
char *str_replace(const char *src, const char *orig, const char *rep) {
    char *p = strstr(src, orig);
    if (!p) return NULL; 

    size_t len_src = strlen(src);
    size_t len_orig = strlen(orig);
    size_t len_rep = strlen(rep);
    
    size_t new_len = (p - src) + len_rep + (len_src - (p - src) - len_orig) + 1;
    char *new_str = malloc(new_len);
    if (!new_str) return NULL;

    strncpy(new_str, src, p - src);
    new_str[p - src] = '\0';
    strcat(new_str, rep);
    strcat(new_str, p + len_orig);
    
    return new_str;
}

// solver alogrithm
Position* solver(char** grid, int rows, int cols, const char word[])
{
    Position* best_match = NULL;
    int len = strlen(word);
    if (len == 0) return NULL;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            for (int d = 0; d < 8; d++) {
                int r = i, c = j, k;
                int errors = 0;
		//check if the word was found
                for (k = 0; k < len; k++) {
                    if (r < 0 || r >= rows || c < 0 || c >= cols) break;
                    
                    if (grid[r][c] != word[k]) {
                        errors++;
                        if (errors > 1) break;
                    }
                    r += directions[d][0];
                    c += directions[d][1];
                }

		//search if an error occured or stop if the word was found
                if (k == len && errors <= 1) {
                    if (errors == 0) { 
                        if (best_match == NULL) {
                            best_match = (Position*)malloc(sizeof(Position) * 2);
                            if (!best_match) err(1, "malloc failed");
                        }
                        best_match[0].x = j; best_match[0].y = i;
                        best_match[1].x = c - directions[d][1];
                        best_match[1].y = r - directions[d][0];
                        return best_match; 
                    }
                    if (best_match == NULL) { 
                        best_match = (Position*)malloc(sizeof(Position) * 2);
                        if (!best_match) err(1, "malloc failed");
                        best_match[0].x = j; best_match[0].y = i;
                        best_match[1].x = c - directions[d][1];
                        best_match[1].y = r - directions[d][0];
                    }
                }
            }
        }
    }
    return best_match;
}
//Read a txt to complete a binary board
char** ReadGridFromFile(const char* filename, int* rows, int* cols) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) return NULL;
    char **grid = NULL;
    char buffer[2048]; //buffer to complete with an high capacity
    *rows = 0; *cols = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        int len = strlen(buffer);
        if (len == 0) continue; 
        if (*rows == 0) *cols = len;
        grid = realloc(grid, (*rows + 1) * sizeof(char*));
        if (!grid) err(1, "realloc failed");
        grid[*rows] = strdup(buffer);
        (*rows)++;
    }
    fclose(file);
    return grid;
}
// read the word txt file to complete the words board
char** ReadWordsFromFile(const char* filename, int* count) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    char** words = NULL;
    char buffer[256];
    *count = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strlen(buffer) > 0) {
            words = realloc(words, sizeof(char*) * (*count + 1));
            words[*count] = strdup(buffer);
            (*count)++;
        }
    }
    fclose(file);
    return words;
}


//search all the words in the boards
int solve_puzzle(const char *grid_file, const char *words_file, FoundLine **found_lines, int *lines_count) {
    printf("\n--- SOLVER MODULE ---\n");

    int rows = 0, cols = 0; //set up rows and cols
    char** grid = ReadGridFromFile(grid_file, &rows, &cols);
    if (!grid || rows == 0) {
        fprintf(stderr, "Error loading grid from %s\n", grid_file);
        return 1;
    }

    for (int i = 0; i < rows; i++) 
        for (int j = 0; j < cols; j++) grid[i][j] = toupper((unsigned char)grid[i][j]);

    //read words
    int word_count = 0;
    char** words = ReadWordsFromFile(words_file, &word_count);
    if (!words) {
        fprintf(stderr, "Error loading words from %s\n", words_file);
        for (int i = 0; i < rows; i++) free(grid[i]);
        free(grid);
        return 1;
    }

    // displaying bonus
    printf("Grid size: %dx%d | Words to find: %d\n", cols, rows, word_count);
    printf("--------------------------------\n");

    *found_lines = malloc(sizeof(FoundLine) * word_count);
    *lines_count = 0;
    
    int num_fixes = sizeof(EQUIVALENCES) / sizeof(EQUIVALENCES[0]);

    for (int i = 0; i < word_count; i++) {
        char* search_word = strdup(words[i]);
        for(int c=0; search_word[c]; c++) search_word[c] = toupper((unsigned char)search_word[c]);

        // basic search
        Position* pos = solver(grid, rows, cols, search_word);

        // check the different errors cases
        if (pos == NULL) {
            for (int r = 0; r < num_fixes; r++) {
                char *fixed_word = NULL;

                
                fixed_word = str_replace(search_word, EQUIVALENCES[r].a, EQUIVALENCES[r].b);
                if (fixed_word) {
                    // display patterns on the consol
                    printf("  [Try Fix %s->%s] '%s' -> '%s'...\n", 
                           EQUIVALENCES[r].a, EQUIVALENCES[r].b, search_word, fixed_word);
                    
                    pos = solver(grid, rows, cols, fixed_word);
                    if (pos != NULL) {
                        printf("\033[0;33mFOUND (Fixed): %-15s (%d,%d) -> (%d,%d)\033[0m\n", 
                               fixed_word, pos[0].x, pos[0].y, pos[1].x, pos[1].y);
                        free(fixed_word);
                        break; 
                    }
                    free(fixed_word);
                }

      
                fixed_word = str_replace(search_word, EQUIVALENCES[r].b, EQUIVALENCES[r].a);
                if (fixed_word) {
                    // MODIFICATION ICI : Affichage dynamique des patterns
                    printf("  [Try Fix %s->%s] '%s' -> '%s'...\n", 
                           EQUIVALENCES[r].b, EQUIVALENCES[r].a, search_word, fixed_word);
                    
                    pos = solver(grid, rows, cols, fixed_word);
                    if (pos != NULL) {
                        printf("\033[0;33mFOUND (Fixed): %-15s (%d,%d) -> (%d,%d)\033[0m\n", 
                               fixed_word, pos[0].x, pos[0].y, pos[1].x, pos[1].y);
                        free(fixed_word);
                        break; 
                    }
                    free(fixed_word);
                }
            }
        } else {
            // found normally
            printf("\033[0;32mFOUND: %-15s (%d,%d) -> (%d,%d)\033[0m\n", 
                   search_word, pos[0].x, pos[0].y, pos[1].x, pos[1].y);
        }

        // saving results
        if (pos != NULL) {
            (*found_lines)[*lines_count].start_col = pos[0].x;
            (*found_lines)[*lines_count].start_row = pos[0].y;
            (*found_lines)[*lines_count].end_col   = pos[1].x;
            (*found_lines)[*lines_count].end_row   = pos[1].y;
            (*lines_count)++;
            free(pos);
        } else {
            printf("\033[0;31mMISSING: %s\033[0m\n", search_word);
        }
        //free the board and words
        free(search_word);
        free(words[i]);
    }

    free(words);
    for (int i = 0; i < rows; i++) free(grid[i]);
    free(grid);

    return 0;
}
