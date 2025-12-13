#ifndef NETWORK_IO_H
#define NETWORK_IO_H

#include "neural_network.h"

int save_network(Network *net, const char *filename);

Network* load_network(const char *filename);

#endif
