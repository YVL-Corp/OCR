#ifndef NETWORK_IO_H
#define NETWORK_IO_H

#include "neural_network.h"

// Sauvegarde le réseau de neurones dans un fichier binaire
int save_network(Network *net, const char *filename);

// Charge un réseau de neurones depuis un fichier binaire
Network* load_network(const char *filename);

#endif // NETWORK_IO_H