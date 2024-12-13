# Compilateur C
CC = gcc

# Options du compilateur
CFLAGS = -Wall -Wextra -Werror

# Nom de l'exécutable final
TARGET = prog

# Répertoire des sources
SRC_DIR = src

# Répertoire objets
SRC_OBJ = tmp

# Liste des fichiers sources et objets
SOURCES = main.c file_handler.c deduplication.c backup_manager.c utilities.c
OBJECTS = $(patsubst %.c,$(SRC_OBJ)/%.o,$(SOURCES))

# Règle par défaut
all: $(TARGET)

# Règle pour l'exécutable
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ -lssl -lcrypto

# Règle générique pour les fichiers objets
$(SRC_OBJ)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Règle pour nettoyer les fichiers objets et l'exécutable
clean:
	rm -f $(SRC_OBJ)/*.o $(TARGET)
