#include <stdio.h>
#include <string.h>
#include "solver.h"
#include <err.h>

int directions[8][2] = {
	// check the directions
	// right, bottom right, bottom, bottom left, left, top left, top, top right
	{0, 1},   
	{1, 1},   
	{1, 0},   
	{1, -1},  
	{0, -1},  
	{-1, -1}, 
	{-1, 0},  
	{-1, 1}   
};

Position* solver(char grid[ROWS][COLS], char word[])
{

    static Position positions[2]; 
    int len = strlen(word); // calculate the length of the word to verify each lette   
	for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {

            //check the first lette
	    if (grid[i][j] == word[0]) {
                for (int d = 0; d < 8; d++) {
                    // check the next letters for each direction
                    int k;
                    for (k = 1; k < len; k++) {
                        // calculate each position with direction
                        int newX = i + k * directions[d][0];
                        int newY = j + k * directions[d][1];
                        // check for the right letter and overflow
                        if (newX < 0 || newX >= ROWS || newY < 0 ||
					newY >= COLS || grid[newX][newY] != word[k]) {
                            break;
                        }
                    }
                    // if every letter is found, return the position of the first and last letters
                    if (k == len) {
                        positions[0].x = i;
                        positions[0].y = j;
                        positions[1].x = i + (len - 1) * directions[d][0];
                        positions[1].y = j + (len - 1) * directions[d][1];
                        return positions;
                    }
                }
            }
        }
	}
	// if the word isnt found, return the invalid coords of the first and last letter 
	positions[0].x = -1;
	positions[0].y = -1;
	positions[1].x = -1;
	positions[1].y = -1;
	return positions;
}


void ReadGridFromFile(const char* filename, char grid[ROWS][COLS]) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier");
        return;
    }

    char buffer[COLS + 8]; //buffer to read each line, +8 to try on the letters of the end of the line
	for (int i = 0; i < ROWS; i++) {
        if (fgets(buffer, sizeof(buffer), file) == NULL) {
            break; 
        }
	// delete \n and \r
        buffer[strcspn(buffer, "\r\n")] = '\0';

	// copy the letters in the grid
        for (int j = 0; j < COLS; j++) {
            if (buffer[j] == '\0') {
		// if the line is too short, fill the rest with space
		 grid[i][j] = ' ';
            } else {
                grid[i][j] = buffer[j];
            }
        }
    }

    fclose(file);
}


int main(size_t argc, char** argv) {
	// check the number of argument    
	if (argc != 3) {
		errx(1, "arguments error");
	}

	// reqd the grid from the file
	char grid[ROWS][COLS];
	ReadGridFromFile(argv[1], grid);

	char *temp = argv[2];
	size_t length = strlen(temp);

	// check the length of the word
	if (length == 0) {
		errx(1, "no word");
	}

	char word[length + 1];
	strcpy(word, argv[2]);

	// convert letters to upper
	for (size_t i = 0; i < length; i++) {
		if (word[i] >= 'a' && word[i] <= 'z') {
			word[i] = word[i] - 32;
		}
	}

	Position* positions = solver(grid, word);
	if (positions[0].x != -1) {
		printf("(%d, %d)(%d, %d)\n",positions[0].x, positions[0].y,
				positions[1].x, positions[1].y);
	} else {
		printf("Not found\n");
	}
	return 0;
}


