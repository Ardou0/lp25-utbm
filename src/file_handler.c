#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/md5.h>
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
        new_elt->date = strdup(strtok(NULL, ","));
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
void write_log_element(log_element *elt, FILE *file) {
    fprintf(file, "%s,", elt->path);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        fprintf(file, "%02x", elt->md5[i]);
    }
    fprintf(file, ",%s\n", elt->date);
}

// Fonction pour lister les fichiers dans un répertoire
void list_files(const char *path) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (!dir) {
        perror("Error opening directory");
        return;
    }

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            printf("%s/%s\n", path, entry->d_name);
        }
    }

    closedir(dir);
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
