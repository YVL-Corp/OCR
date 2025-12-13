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

// --- ALGORITHME DE RECHERCHE INTELLIGENT ---
Position* solver(char** grid, int rows, int cols, const char word[])
{
    // On va stocker ici le meilleur résultat trouvé (si imparfait)
    Position* best_match = NULL;

    int len = strlen(word);
    if (len == 0) return NULL;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            
            for (int d = 0; d < 8; d++) {
                
                int r = i, c = j, k;
                int errors = 0;

                for (k = 0; k < len; k++) {
                    if (r < 0 || r >= rows || c < 0 || c >= cols) 
                        break;
                    
                    if (grid[r][c] != word[k]) {
                        errors++;
                        if (errors > 1) break; // Trop d'erreurs, on coupe
                    }

                    r += directions[d][0];
                    c += directions[d][1];
                }

                // Si on a trouvé une correspondance valide (0 ou 1 erreur)
                if (k == len && errors <= 1) {
                    
                    // CAS 1 : Match PARFAIT (0 erreur)
                    // C'est le Graal, on prend tout de suite et on s'arrête.
                    if (errors == 0) {
                        // Si on avait déjà alloué de la mémoire pour un match imparfait, on la réutilise
                        if (best_match == NULL) {
                            best_match = (Position*)malloc(sizeof(Position) * 2);
                            if (best_match == NULL) err(1, "malloc failed");
                        }

                        best_match[0].x = j; 
                        best_match[0].y = i;
                        best_match[1].x = c - directions[d][1];
                        best_match[1].y = r - directions[d][0];
                        
                        return best_match; // RETOUR IMMÉDIAT
                    }

                    // CAS 2 : Match IMPARFAIT (1 erreur)
                    // On ne le prend que si on n'a rien trouvé avant.
                    // Et surtout : ON CONTINUE DE CHERCHER au cas où une version parfaite existe plus loin.
                    if (best_match == NULL) {
                        best_match = (Position*)malloc(sizeof(Position) * 2);
                        if (best_match == NULL) err(1, "malloc failed");

                        best_match[0].x = j; 
                        best_match[0].y = i;
                        best_match[1].x = c - directions[d][1];
                        best_match[1].y = r - directions[d][0];
                    }
                }
            }
        }
    }
    
    // Si on a fini de parcourir toute la grille sans trouver de match parfait (0 erreur),
    // on retourne le match imparfait (1 erreur) qu'on a trouvé (ou NULL si rien trouvé).
    return best_match;
}

// --- LECTURE DYNAMIQUE DE LA GRILLE ---
char** ReadGridFromFile(const char* filename, int* rows, int* cols) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) return NULL;

    char **grid = NULL;
    char buffer[2048]; 
    *rows = 0;
    *cols = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        int len = strlen(buffer);
        if (len == 0) continue; 

        if (*rows == 0) {
            *cols = len;
        } 
        
        grid = realloc(grid, (*rows + 1) * sizeof(char*));
        if (!grid) err(1, "realloc failed");

        grid[*rows] = strdup(buffer);
        (*rows)++;
    }

    fclose(file);
    return grid;
}

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

// --- FONCTION PRINCIPALE ---
int solve_puzzle(const char *grid_file, const char *words_file, FoundLine **found_lines, int *lines_count) {
    printf("\n--- SOLVER MODULE ---\n");

    int rows = 0, cols = 0;
    char** grid = ReadGridFromFile(grid_file, &rows, &cols);

    if (grid == NULL || rows == 0) {
        fprintf(stderr, "Error loading grid from %s\n", grid_file);
        return 1;
    }

    // Mise en majuscule
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) grid[i][j] = toupper((unsigned char)grid[i][j]);
    }

    int word_count = 0;
    char** words = ReadWordsFromFile(words_file, &word_count);

    if (words == NULL) {
        fprintf(stderr, "Error loading words from %s\n", words_file);
        for (int i = 0; i < rows; i++) free(grid[i]);
        free(grid);
        return 1;
    }

    printf("Grid size: %dx%d | Words to find: %d\n", cols, rows, word_count);
    printf("--------------------------------\n");

    // Allocation des lignes trouvées pour l'interface
    *found_lines = malloc(sizeof(FoundLine) * word_count);
    *lines_count = 0;

    for (int i = 0; i < word_count; i++) {
        char* search_word = strdup(words[i]);
        for(int c=0; search_word[c]; c++) search_word[c] = toupper((unsigned char)search_word[c]);

        Position* pos = solver(grid, rows, cols, search_word);

        if (pos != NULL) {
            printf("\033[0;32mFOUND: %-15s (%d,%d) -> (%d,%d)\033[0m\n", 
                   words[i], pos[0].x, pos[0].y, pos[1].x, pos[1].y);
            
            (*found_lines)[*lines_count].start_col = pos[0].x;
            (*found_lines)[*lines_count].start_row = pos[0].y;
            (*found_lines)[*lines_count].end_col   = pos[1].x;
            (*found_lines)[*lines_count].end_row   = pos[1].y;
            (*lines_count)++;

            free(pos);
        } else {
            printf("\033[0;31mMISSING: %s\033[0m\n", words[i]);
        }
        free(search_word);
        free(words[i]);
    }

    // Nettoyage
    free(words);
    for (int i = 0; i < rows; i++) free(grid[i]);
    free(grid);

    return 0;
}
