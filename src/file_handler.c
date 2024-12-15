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

    char line[2048];
    while (fgets(line, sizeof(line), file)) {
        log_element *new_elt = malloc(sizeof(log_element));
        if (!new_elt) {
            perror("Memory allocation failed");
            fclose(file);
            return logs;
        }

        char *path = strtok(line, ",");
        char *date = strtok(NULL, ",");
        char *md5_hex = strtok(NULL, ",");

        if (!path || !date || !md5_hex) {
            fprintf(stderr, "Error parsing line: %s\n", line);
            free(new_elt);
            continue;
        }

        new_elt->path = strdup(path);
        if (!new_elt->path) {
            perror("Memory allocation failed");
            free(new_elt);
            continue;
        }

        strcpy(new_elt->date, date);

        strcpy((char*)new_elt->md5, md5_hex);

        new_elt->next = NULL;
        new_elt->prev = logs.tail;

        if (logs.tail) {
            logs.tail->next = new_elt;
        }
        else {
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
void write_log_element(log_element *element, FILE *log_file) {
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
    fprintf(log_file, "%s,\n", element->md5);
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
            }
            else {
                // Si c'est un fichier, l'ajouter à l'index des fichiers
                file_element *new_elt = malloc(sizeof(file_element));
                if (!new_elt) {
                    perror("Memory allocation failed");
                    free(full_path);
                    continue;
                }
                //new_elt->path = entry->d_name;
                new_elt->path = strdup(full_path);
                new_elt->next = NULL;

                if (file_list->tail) {
                    file_list->tail->next = new_elt;
                }
                else {
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
            free((char*)current->path); // Appel void pour éviter tout warning
        }
        // Ne pas libérer current->md5 et current->date car ils ne sont pas alloués dynamiquement

        // Libérer la mémoire allouée pour l'élément lui-même
        free(current);

        current = next;
    }

    // Réinitialiser les pointeurs de la liste
    logs->head = NULL;
    logs->tail = NULL;
}



// Fonction pour copier un fichier
int copy_file(const char *source_path, const char *dest_path) {
    // Ouvre le fichier source en mode lecture binaire
    struct stat st;
    if (stat(source_path, &st) == 0) {
        //printf("Taille du fichier source : %ld octets\n", st.st_size);
    }
    else {
        perror("Impossible d'obtenir des informations sur le fichier source");
        return -1;
    }
    FILE *source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        perror("Erreur lors de l'ouverture du fichier source");
        return -1;
    }

    // Ouvre le fichier de destination en mode écriture binaire
    FILE *dest_file = fopen(dest_path, "wb");
    if (dest_file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de destination");
        fclose(source_file);
        return -1;
    }

    // Copie les données par blocs
    char buffer[4096]; // Taille du tampon pour la copie
    size_t bytes_read;
    size_t total_bytes_copied = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            perror("Erreur lors de l'écriture dans le fichier de destination");
            fclose(source_file);
            fclose(dest_file);
            return -1;
        }
        total_bytes_copied += bytes_read;
    }

    if (ferror(source_file)) {
        perror("Erreur lors de la lecture du fichier source");
        fclose(source_file);
        fclose(dest_file);
        return -1;
    }

    // Ferme les fichiers
    fclose(source_file);
    fclose(dest_file);

    return 0; // Succès
}
