# Nom de l'exécutable final
TARGET = ocr_solver

# Compilateur
CC = gcc

# Flags de compilation
# -I... : Permet de faire #include "fichier.h" sans se soucier du dossier
# -O3   : Optimisation maximale (Important pour le Neural Network)
# -g    : Symboles de débogage (utile si ça crash)
CFLAGS = -Wall -Wextra -O3 -g \
         $(shell pkg-config --cflags gtk+-3.0) \
         -Ipreprocess -Idetect -Ineuralnetwork -Isolver

# Flags d'édition de liens
# -rdynamic : Pour les signaux GTK
# -lm       : Pour les maths (pow, exp, sqrt...)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -rdynamic -lm

# Liste de TOUS les fichiers sources du projet
SRCS = main.c \
       preprocess/processing.c \
       detect/extraction.c \
       detect/image_export.c \
       neuralnetwork/neural_network.c \
       neuralnetwork/image_loader.c \
       neuralnetwork/network_io.c \
       neuralnetwork/nn_module.c \
       solver/solver.c

# Génération automatique de la liste des objets (.o)
OBJS = $(SRCS:.c=.o)

# Règle principale
all: $(TARGET)

# Édition de liens (Création de l'exécutable)
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "---------------------------------------"
	@echo "Build Successful! Run with: make run"
	@echo "---------------------------------------"

# Règle générique de compilation (.c -> .o)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage des fichiers temporaires
clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf output

# Recompiler depuis zéro
re: clean all

# Lancement propre (évite les warnings dconf/GTK)
run: $(TARGET)
	GSETTINGS_BACKEND=memory ./$(TARGET)

.PHONY: all clean re run
