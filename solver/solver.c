#include <stdio.h>
#include <string.h>
#include "solver.h"
#include <err.h>

int directions[8][2] = {
    // vérification des directions
    // droite, bas-droite, bas, bas-gauche, gauche, haut-gauche, haut, haut-droite
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

    static Position positions[2]; // p
    int len = strlen(word); // calcul de la longueur du mot pour pouvoir vérifier chaque lettre

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {

            // verification de la première lettre du mot trouvé dans la grille
            if (grid[i][j] == word[0]) {
                for (int d = 0; d < 8; d++) {
                    // vérification des lettres suivantes du mot pour toutes les directions
                    int k;
                    for (k = 1; k < len; k++) {
                        // calcul des nouvelles positions en fonction de la direction
                        int newX = i + k * directions[d][0];
                        int newY = j + k * directions[d][1];
                        // vérification du dépassement des bordures de la grille et si la lettre est la bonne
                        if (newX < 0 || newX >= ROWS || newY < 0 || newY >= COLS || grid[newX][newY] != word[k]) {
                            break;
                        }
                    }
                    // si toutes les lettres du mot ont été trouvées on renvoie les positions des premieres et dernières lettres
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

    // si le mot n'est pas trouvé retourner des positions invalides pour le debut et la fin du mot
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

    char buffer[COLS + 8]; // buffer pour lire chaque ligne, +8 pour gérer les caractères de fin de ligne
    for (int i = 0; i < ROWS; i++) {
        if (fgets(buffer, sizeof(buffer), file) == NULL) {
            break; 
        }

        // supprime \n et \r
        buffer[strcspn(buffer, "\r\n")] = '\0';

        // Copie les caractères dans la grille
        for (int j = 0; j < COLS; j++) {
            if (buffer[j] == '\0') {
                // si la ligne est trop courte, on remplit le reste avec des espaces
                grid[i][j] = ' ';
            } else {
                grid[i][j] = buffer[j];
            }
        }
    }

    fclose(file);
}


int main(size_t argc, char** argv) {
    // vérification du nombre d'arguments
    if (argc != 3) {
        errx(1, "arguments error");
    }

    // lecture de la grille depuis le fichier
    char grid[ROWS][COLS];
    ReadGridFromFile(argv[1], grid);

    char *temp = argv[2];
    size_t length = strlen(temp);

    // vérification de la longueur du mot
    if (length == 0) {
        errx(1, "no word");
    }

    char word[length + 1];
    strcpy(word, argv[2]);

    // conversion du mot en majuscules
    for (size_t i = 0; i < length; i++) {
        if (word[i] >= 'a' && word[i] <= 'z') {
            word[i] = word[i] - 32;
        }
    }

    Position* positions = solver(grid, word);
    if (positions[0].x != -1) {
        printf("(%d, %d)(%d, %d)\n",positions[0].x, positions[0].y, positions[1].x, positions[1].y);
    } else {
        printf("Not found\n");
    }
    return 0;
}


