#ifndef NN_MODULE_H
#define NN_MODULE_H

// Fonction principale pour lancer la reconnaissance optique de caractères (OCR)
// Lit les dossiers d'images et génère grid.txt et words.txt
// Retourne 0 si succès, 1 si erreur
int nn_run_recognition(const char *root_folder, const char *model_file);

#endif // NN_MODULE_H
