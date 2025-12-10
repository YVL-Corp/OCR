#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <err.h>
#include <ctype.h> 
#include "solver.h" 

// Directions de recherche (identiques)
int directions[8][2] = {
    {0, 1}, 
    {1, 1}, 
    {1, 0}, 
    {1, -1},
    {0, -1},
    {-1, -1},
    {-1, 0},
    {-1, 1}
};

// Search a word in the grid 
Position* solver(char** grid, int rows, int cols, const char word[])
{
    
    Position* positions = (Position*)malloc(sizeof(Position) * 2);
    if (positions == NULL)
        err(1, "malloc failed");

    int len = strlen(word);
    
    // Exit if the word lengh is zero
    if (len == 0) {
        free(positions);
        return NULL;
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            
            // search in the different directions
            for (int d = 0; d < 8; d++) {
                
                int errors = 0; // error counter
                int k;
                
                // if the first letter is not matching, start with one error
                if (grid[i][j] != word[0]) {
                    errors++;
                }

                // Check the rest of the letters
                for (k = 1; k < len; k++) {
                    
                    // New coordinates
                    int newX = i + k * directions[d][0];
                    int newY = j + k * directions[d][1];
                    
                    // check bounds
                    if (newX < 0 || newX >= rows || newY < 0 || newY >= cols) {
                        break; 
                    }
                
                    if (grid[newX][newY] != word[k]) {
                        errors++; 
                        
                        if (errors > 1) {
                            break; 
                        }
                    }
                }
                
                if (k == len && errors <= 1) {
                    
                    positions[0].x = i;
                    positions[0].y = j;
                    positions[1].x = i + (len - 1) * directions[d][0];
                    positions[1].y = j + (len - 1) * directions[d][1];
                    
                    return positions; 
                }
            }
        }
    }
    
    // free allocated memory if not found
    free(positions);
    return NULL; 
}


// Function to read the grid from a file
char** ReadGridFromFile(const char* filename, int* rows, int* cols) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier");
        *rows = 0;
        *cols = 0;
        return NULL;
    }

    char **grid = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    *rows = 0;
    *cols = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        size_t current_len = strlen(line);

        if (*rows == 0 || current_len > (size_t)*cols) {
            *cols = (int)current_len;
        }

        grid = (char**)realloc(grid, (*rows + 1) * sizeof(char*));
        if (grid == NULL) err(1, "realloc failed for grid rows");

        grid[*rows] = (char*)malloc((*cols + 1) * sizeof(char));
        if (grid[*rows] == NULL) err(1, "malloc failed for grid row columns");
        
        strcpy(grid[*rows], line);

        for (int j = (int)current_len; j < *cols; j++) {
            grid[*rows][j] = ' ';
        }
        grid[*rows][*cols] = '\0';

        (*rows)++;
    }

    free(line);
    fclose(file);

    if (*rows == 0 && grid != NULL) {
        free(grid);
        grid = NULL;
    } else if (*rows > 0 && *cols == 0) {
        for(int i = 0; i < *rows; i++) free(grid[i]);
        free(grid);
        grid = NULL;
        *rows = 0;
    }
    return grid;
}


int main(size_t argc, char** argv) {
	// check arguments
    if (argc != 3) {
        errx(1, "Usage: %s <grid_file> <word_to_find>", argv[0]);
    }

    int rows = 0;
    int cols = 0;

    // Read the grid from the file
    char** grid = ReadGridFromFile(argv[1], &rows, &cols);

    if (grid == NULL || rows == 0 || cols == 0) {
        fprintf(stderr, "Could not load a valid grid from file %s\n", argv[1]);
        return 1;
    }

    char *temp = argv[2];
    size_t length = strlen(temp);

    if (length == 0) {
        errx(1, "no word to search");
    }

	// Allocate memory for the word to search
    char* word = (char*)malloc(length + 1);
    if (word == NULL)
        err(1, "malloc failed");

    strcpy(word, argv[2]);

    // Convert the word to uppercase
    for (size_t i = 0; i < length; i++) {
        word[i] = (char)toupper((unsigned char)word[i]);
    }
    
    // Convert the grid to uppercase 
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            grid[i][j] = (char)toupper((unsigned char)grid[i][j]);
        }
    }

    Position* positions = solver(grid, rows, cols, word);
    
    if (positions != NULL) {
        printf("(%d, %d)(%d, %d)\n", positions[0].x, positions[0].y,
                                      positions[1].x, positions[1].y);
        free(positions);
    } else {
        printf("Not found\n");
    }

    // Free allocated memory
    if (grid != NULL) {
        for (int i = 0; i < rows; i++) {
            free(grid[i]);
        }
        free(grid);
    }
    free(word);
    
    return 0;
}