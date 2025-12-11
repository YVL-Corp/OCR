# Nom de l'exécutable
TARGET = ocr_solver

CC = gcc

# Flags de compilation
# -Ipreprocess -Idetect : Permet d'inclure "processing.h" ou "extraction.h" sans préciser le chemin
# pkg-config : Configure GTK 3
CFLAGS = -Wall -Wextra -g \
         $(shell pkg-config --cflags gtk+-3.0) \
         -Ipreprocess -Idetect

# Flags d'édition de liens (Linker)
# -rdynamic : Nécessaire pour que GTK trouve les signaux (on_btn_...)
# -lm : Bibliothèque mathématique
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -rdynamic -lm

# Liste des sources (racine + sous-dossiers)
SRCS = main.c \
       preprocess/processing.c \
       detect/extraction.c \
       detect/image_export.c

# Génération des noms d'objets (.o)
OBJS = $(SRCS:.c=.o)

# Règle par défaut
all: $(TARGET)

# Édition de liens
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compilation des fichiers .c
# Cette règle générique fonctionne aussi pour les fichiers dans les sous-dossiers
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf output

# Recompiler proprement
re: clean all


# Règle pour lancer proprement sans les warnings GTK/dconf
run: $(TARGET)
	GSETTINGS_BACKEND=memory ./$(TARGET)

.PHONY: all clean re run
