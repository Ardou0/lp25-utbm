#include "utilities.h"
#include "deduplication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// Fonction pour vérifier si un répertoire en local est accessible
int is_directory_accessible(const char *path) {
    struct stat sb;
    // Vérifie si le chemin existe et est un répertoire
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        // Vérifie les permissions de lecture et écriture
        return access(path, R_OK | W_OK) == 0;
    }
    return 0;
}

// Fonction pour construire le chemin complet d'un fichier
char *build_full_path(const char *dir, const char *relative_path) {
    // Calculer la longueur totale du chemin complet
    size_t source_dir_len = strlen(dir);
    size_t relative_path_len = strlen(relative_path);
    size_t total_len = source_dir_len + relative_path_len + 2; // +1 pour le '/' et +1 pour le '\0'

    // Allouer de la mémoire pour le chemin complet
    char *full_path = (char*)malloc(total_len);
    if (full_path == NULL) {
        perror("Erreur lors de l'allocation de mémoire");
        return NULL;
    }

    // Construire le chemin complet
    snprintf(full_path, total_len, "%s/%s", dir, relative_path);


    return full_path;
}

// Fonction pour obtenir la dernière date de modification d'un fichier
char *get_last_modification_date(const char *filepath) {
    struct stat fileStat;
    if (stat(filepath, &fileStat) < 0) {
        perror("Erreur lors de l'obtention des informations du fichier");
        return NULL;
    }

    // Convertir le temps de modification en une structure tm
    struct tm *timeinfo = localtime(&fileStat.st_mtime);
    if (timeinfo == NULL) {
        perror("Erreur lors de la conversion du temps");
        return NULL;
    }

    // Allouer de la mémoire pour la chaîne de caractères de la date
    // Le format est "\0", ce qui fait 17 caractères
    char *date_str = (char*)malloc((strlen("YYYY-MM-DD-hh:mm") + 1)  *sizeof(char));
    if (date_str == NULL) {
        perror("Erreur lors de l'allocation de mémoire");
        return NULL;
    }
    // Formater la date dans le format YYYY-MM-DD-hh:mm
    if (strftime(date_str, 17, "%Y-%m-%d-%H:%M", timeinfo) == 0) {
        perror("Erreur lors du formatage de la date");
        free(date_str);
        return NULL;
    }

    return date_str;
}

//calcule le md5 d'un fichier
void get_md5(const char *filepath, unsigned char *md5_hex) {

    // Open the file to read its data
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Erreur lors de l'ouverture du fichier pour le calcul du MD5");
        return;
    }

    // Read the file into memory and calculate the MD5
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    void *buffer = malloc(file_size);
    if (!buffer) {
        fclose(file);
        perror("Erreur d'allocation mémoire pour le calcul du MD5");
        return;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        free(buffer);
        fclose(file);
        perror("Erreur lors de la lecture du fichier pour le calcul du MD5");
        return;
    }
    fclose(file);
    // Calculate the MD5 using compute_md5
    compute_md5(buffer, file_size, md5_hex);
    free(buffer);
}


// Fonction pour convertir une chaîne de caractères en structure tm
int parse_date(const char *date_str, struct tm *tm_info) {
    if (strlen(date_str) != 16) {
        return -1; // Échec de la conversion
    }

    if (sscanf(date_str, "%4d-%2d-%2d-%2d:%2d",
        &tm_info->tm_year, &tm_info->tm_mon, &tm_info->tm_mday,
        &tm_info->tm_hour, &tm_info->tm_min) != 5) {
        return -1; // Échec de la conversion
    }

    tm_info->tm_year -= 1900; // tm_year est le nombre d'années depuis 1900
    tm_info->tm_mon -= 1;     // tm_mon est le mois de l'année (0-11)
    tm_info->tm_sec = 0;      // Initialiser les secondes à 0
    tm_info->tm_isdst = -1;   // Indiquer que l'information sur l'heure d'été n'est pas disponible

    return 0;
}

int compare_dates(const char *date1, const char *date2) {
    struct tm tm1, tm2;
    time_t time1, time2;

    if (parse_date(date1, &tm1) != 0 || parse_date(date2, &tm2) != 0) {
        fprintf(stderr, "Erreur de format de date\n");
        return -1;
    }

    time1 = mktime(&tm1);
    time2 = mktime(&tm2);

    if (time1 < time2) {
        return -1;
    } else if (time1 > time2) {
        return 1;
    } else {
        return 0;
    }
}


void remove_trailing_slash(char *path) {
    size_t len = strlen(path);
    if (len > 0 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}


char *remove_source_dir(const char *source_dir_copy, const char *full_path) {
    // Trouver la longueur de source_dir_copy
    size_t source_dir_len = strlen(source_dir_copy);

    // Vérifier si full_path commence par source_dir_copy
    if (strncmp(full_path, source_dir_copy, source_dir_len) == 0) {
        // Vérifier si le caractère suivant est un '/'
        size_t start_pos = source_dir_len;
        if (full_path[start_pos] == '/') {
            start_pos++;
        }

        // Allouer de la mémoire pour la nouvelle chaîne sans source_dir_copy
        char *new_path = malloc(strlen(full_path) - start_pos + 1);
        if (new_path == NULL) {
            perror("Erreur lors de l'allocation de mémoire");
            exit(EXIT_FAILURE);
        }

        // Copier la partie restante du chemin après source_dir_copy
        strcpy(new_path, full_path + start_pos);

        return new_path;
    }
    else {
        // Si source_dir_copy n'est pas trouvé au début de full_path, retourner NULL ou gérer l'erreur
        fprintf(stderr, "Le chemin ne commence pas par le dossier source\n");
        return strdup(full_path);
    }
}

void create_intermediate_directories(const char *path) {
    char *path_copy = strdup(path);
    if (!path_copy) {
        perror("Failed to duplicate path");
        return;
    }

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0'; // Remove the filename part
    }

    char *token = strtok(path_copy, "/");
    char current_path[1024] = "";

    while (token != NULL) {
        strcat(current_path, token);
        strcat(current_path, "/");

        struct stat st = { 0 };
        if (stat(current_path, &st) == -1) {
            mkdir(current_path, 0700);
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
}

time_t get_modification_timestamp(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}


char *get_latest_backup_dir(const char *backup_dir_copy) {
    DIR *dir = opendir(backup_dir_copy);
    if (!dir) {
        perror("Failed to open directory");
        return NULL;
    }

    struct dirent *entry;
    char *latest_dir = NULL;
    time_t latest_time = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char *full_path = malloc(strlen(backup_dir_copy) + strlen(entry->d_name) + 2);
            if (!full_path) {
                perror("Memory allocation failed");
                closedir(dir);
                return NULL;
            }
            sprintf(full_path, "%s/%s", backup_dir_copy, entry->d_name);

            time_t mod_time = get_modification_timestamp(full_path);
            if (mod_time > latest_time) {
                latest_time = mod_time;
                if (latest_dir) {
                    free(latest_dir);
                }
                latest_dir = full_path;
            }
            else {
                free(full_path);
            }
        }
    }

    closedir(dir);
    char *tmp = remove_source_dir(backup_dir_copy, latest_dir);
    free(latest_dir);
    return tmp;
}

int is_directory_empty(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Failed to open directory");
        return -1; // Erreur lors de l'ouverture du répertoire
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
            break;
        }
    }

    closedir(dir);
    return count == 0;
}

char *cut_after_first_slash(const char *input) {
    // Trouver le premier '/'
    char *first_slash = strchr(input, '/');
    if (!first_slash) {
        // Si aucun '/' n'est trouvé, retourner une copie de la chaîne d'origine
        return strdup(input);
    }

    // Calculer la longueur de la partie restante
    size_t length = strlen(first_slash + 1);

    // Allouer de la mémoire pour la nouvelle chaîne
    char *result = malloc(length + 1);
    if (!result) {
        perror("Memory allocation failed");
        return NULL;
    }

    // Copier la partie restante de la chaîne
    strcpy(result, first_slash + 1);


    return result;
}

void remove_after_slash(const char *input, char *output) {
    const char *slash_pos = strchr(input, '/');
    if (slash_pos) {
        size_t length = slash_pos - input;
        strncpy(output, input, length);
        output[length] = '\0';
    } else {
        strcpy(output, input);
    }
}