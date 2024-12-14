#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include "utilities.h"
#include "file_handler.h"
#include "deduplication.h"

// Fonction permettant de lire un élément du fichier .backup_log
log_t read_backup_log(const char *logfile) {
    log_t logs = { .head = NULL, .tail = NULL };
    FILE *file = fopen(logfile, "r");
    if (!file) {
        perror("Error opening backup log");
        return logs;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        log_element *new_elt = malloc(sizeof(log_element));
        if (!new_elt) {
            perror("Memory allocation failed");
            fclose(file);
            return logs;
        }

        char md5_hex[2 * MD5_DIGEST_LENGTH + 1];
        new_elt->path = strdup(strtok(line, ","));
        strncpy(md5_hex, strtok(NULL, ","), sizeof(md5_hex) - 1);
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sscanf(&md5_hex[i * 2], "%02hhx", &new_elt->md5[i]);
        }
        strcpy(new_elt->date, strtok(NULL, ","));
        new_elt->next = NULL;
        new_elt->prev = logs.tail;

        if (logs.tail) {
            logs.tail->next = new_elt;
        } else {
            logs.head = new_elt;
        }
        logs.tail = new_elt;
    }

    fclose(file);
    return logs;
}

// Fonction permettant de mettre à jour une ligne du fichier .backup_log
void update_backup_log(const char *logfile, log_t *logs) {
    FILE *file = fopen(logfile, "w");
    if (!file) {
        perror("Error opening backup log for writing");
        return;
    }

    log_element *current = logs->head;
    while (current) {
        write_log_element(current, file);
        current = current->next;
    }

    fclose(file);
}

// Fonction permettant d'écrire un élément log dans le fichier .backup_log
void write_log_element(log_element* element, FILE* log_file) {
    if (element == NULL || log_file == NULL) {
        return;
    }

    // Vérifiez que les chaînes de caractères sont correctement initialisées
    if (element->path == NULL || element->date == NULL || element->md5 == NULL) {
        fprintf(stderr, "Invalid log element\n");
        return;
    }

    // Écrire les éléments dans le fichier de log
    fprintf(log_file, "%s,", element->path);
    fprintf(log_file, "%s,", element->date);
    fprintf(log_file, "%s\n", element->md5);
}

// Fonction pour lister les fichiers dans un répertoire et les ajouter à une liste chaînée
void list_files(const char *path, file_list_t *file_list, int recursive) {
    // Buffer pour le chemin
    char *path_buffer = strdup(path);
    if (!path_buffer) {
        perror("Error allocating memory");
        return;
    }

    // Si le path contient un '/' à la fin, le retirer
    remove_trailing_slash(path_buffer);

    struct dirent *entry;
    DIR *dir = opendir(path_buffer);

    if (!dir) {
        perror("Error opening directory");
        free(path_buffer);
        return;
    }

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char *full_path = build_full_path(path, entry->d_name);
            if (!full_path) {
                perror("Error building full path");
                continue;
            }

            struct stat path_stat;
            if (stat(full_path, &path_stat) == -1) {
                perror("Error getting file status");
                free(full_path);
                continue;
            }

            if (S_ISDIR(path_stat.st_mode) && recursive) {
                // Si c'est un dossier, appeler récursivement la fonction
                list_files(full_path, file_list, recursive);
            } else {
                // Si c'est un fichier, l'ajouter à l'index des fichiers
                file_element *new_elt = malloc(sizeof(file_element));
                if (!new_elt) {
                    perror("Memory allocation failed");
                    free(full_path);
                    continue;
                }
                //new_elt->path = entry->d_name;
                new_elt->path = strdup(entry->d_name);
                new_elt->next = NULL;

                if (file_list->tail) {
                    file_list->tail->next = new_elt;
                } else {
                    file_list->head = new_elt;
                }
                file_list->tail = new_elt;
            }
            free(full_path);
        }
    }

    closedir(dir);
    free(path_buffer);
}

// Fonction pour libérer une liste d'index de fichiers
void free_file_list(file_list_t *file_list) {
    file_element *current = file_list->head;
    while (current) {
        file_element *next = current->next;
        free(current->path);
        free(current);
        current = next;
    }
    file_list->head = NULL;
    file_list->tail = NULL;
}

// Fonction pour libérer une liste de logs
void free_log_list(log_t *logs) {
    log_element *current = logs->head;
    while (current) {
        log_element *next = current->next;

        // Libérer la mémoire allouée pour les champs dynamiques
        if (current->path) {
            free((void *)current->path); // Appel void pour éviter tout warning
        }

        // Libérer la mémoire allouée pour l'élément lui-même
        free(current);

        current = next;
    }

    // Réinitialiser les pointeurs de la liste
    logs->head = NULL;
    logs->tail = NULL;
}

// Fonction pour copier un fichier
void copy_file(const char *src, const char *dest) {
    FILE *source = fopen(src, "rb");
    if (!source) {
        perror("Error opening source file");
        return;
    }

    FILE *destination = fopen(dest, "wb");
    if (!destination) {
        perror("Error opening destination file");
        fclose(source);
        return;
    }

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes, destination);
    }

    fclose(source);
    fclose(destination);
}
