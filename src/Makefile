# ==========================================
#      OCR PROJECT - FINAL MAKEFILE
# ==========================================

# 1. EXECUTABLES
# L'interface graphique (Solver final)
TARGET = ocr_solver
# L'entrainement (Console uniquement)
TRAIN_TARGET = ocr_trainer

# 2. COMPILATION
CC = gcc
# Flags: -I permet d'inclure les headers des sous-dossiers sans chemin relatif complexe
CFLAGS = -Wall -Wextra -O3 -g \
         $(shell pkg-config --cflags gtk+-3.0) \
         -Ipreprocess -Idetect -Ineuralnetwork -Isolver -lm

LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -rdynamic -lm

# 3. SOURCES
# --- Sources pour l'interface graphique (Main Project) ---
SRCS = main.c \
       preprocess/processing.c \
       detect/extraction.c \
       detect/image_export.c \
       neuralnetwork/neural_network.c \
       neuralnetwork/image_loader.c \
       neuralnetwork/network_io.c \
       neuralnetwork/nn_module.c \
       solver/solver.c

# --- Sources pour l'entrainement (Selon Tristan) ---
# Attention : On utilise main_letters.c qui est dans neuralnetwork/
TRAIN_SRCS = neuralnetwork/main_letters.c \
             neuralnetwork/neural_network.c \
             neuralnetwork/image_loader.c \
             neuralnetwork/network_io.c

# Objets pour le projet principal
OBJS = $(SRCS:.c=.o)

# ==========================================
#                 RÈGLES
# ==========================================

all: $(TARGET)

# --- Build de l'interface graphique ---
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "---------------------------------------"
	@echo " [GUI] Build Successful: ./$(TARGET)"
	@echo "---------------------------------------"

# --- Build du Trainer (ocr_trainer) ---
# On compile directement les sources sans passer par des .o intermédiaires pour simplifier
$(TRAIN_TARGET):
	$(CC) $(CFLAGS) $(TRAIN_SRCS) -o $(TRAIN_TARGET) $(LDFLAGS)
	@echo "---------------------------------------"
	@echo " [TRAINER] Build Successful: ./$(TRAIN_TARGET)"
	@echo "---------------------------------------"

# --- Règle générique (.c -> .o) ---
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- COMMANDES UTILITAIRES ---

# La commande pour la soutenance !
# Lance la compilation du trainer PUIS l'exécution avec les arguments de Tristan
# Arguments : <Dossier Dataset> <Epochs> <Learning Rate> <Fichier Sortie>
train: $(TRAIN_TARGET)
	@echo "========================================"
	@echo "   STARTING TRAINING DEMO               "
	@echo "========================================"
	./$(TRAIN_TARGET) train dataset/ 1000 0.01 model.bin

clean:
	rm -f $(OBJS) $(TARGET) $(TRAIN_TARGET)
	rm -rf output

re: clean all

run: $(TARGET)
	GSETTINGS_BACKEND=memory ./$(TARGET)

.PHONY: all clean re run train
