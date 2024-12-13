#include "backup_manager.h"
#include "deduplication.h"
#include "file_handler.h"
#include "utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Générer le nom de sauvegarde avec la date et l'heure actuelle
void generate_backup_name(char* buffer, size_t size) {
    struct timespec ts;
    struct tm* info;
    char date_buffer[64];
    char ms_buffer[16];

    //récupère l'heure actuelle
    clock_gettime(CLOCK_REALTIME, &ts);
    //Convertir en heure locale
    info = localtime(&ts.tv_sec);

    // Formatage de la date (sans millisecondes)
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d-%H:%M:%S", info);
    // Ajouter les millisecondes à la chaîne formatée
    snprintf(ms_buffer, sizeof(ms_buffer), ".%03ld", ts.tv_nsec / 1000000);
    snprintf(buffer, size, "%s%s", date_buffer, ms_buffer);
}

//ajoute au nom du fichier changé: sa date de modification et sa somme md5


// Fonction pour créer une nouvelle sauvegarde complète puis incrémentale
void create_backup(const char* source_dir, const char* backup_dir) {
    char backup_name[128];
    //char log_file_path[1024];
    //char full_backup_path[1024];
    struct stat st;

    // Remove trailing slash from source and backup directories
    char* source_dir_copy = strdup(source_dir);
    char* backup_dir_copy = strdup(backup_dir);
    if (!source_dir_copy || !backup_dir_copy) {
        perror("Memory allocation failed");
        free(source_dir_copy);
        free(backup_dir_copy);
        return;
    }
    remove_trailing_slash(source_dir_copy);
    remove_trailing_slash(backup_dir_copy);

    // Vérifier si le répertoire de sauvegarde est accessible
    if (!is_directory_accessible(backup_dir_copy)) {
        fprintf(stderr, "Le répertoire de sauvegarde '%s' n'est pas accessible ou n'existe pas.\n", backup_dir_copy);
        free(source_dir_copy);
        free(backup_dir_copy);
        return;
    }

    // Vérifier si le répertoire source est accessible
    if (!is_directory_accessible(source_dir_copy)) {
        fprintf(stderr, "Le répertoire source '%s' n'est pas accessible ou n'existe pas.\n", source_dir_copy);
        free(source_dir_copy);
        free(backup_dir_copy);
        return;
    }

    // Générer le nom de la sauvegarde avec la date et l'heure actuelle
    generate_backup_name(backup_name, sizeof(backup_name));
    printf("Nom de la sauvegarde générée : %s\n", backup_name);

    // Créer le nom du chemin complet

    //snprintf(full_backup_path, sizeof(full_backup_path), "%s/%s", backup_dir_copy, backup_name);
    char* full_backup_path = build_full_path(backup_dir_copy, backup_name);
    printf("Chemin complet de la sauvegarde : %s\n", full_backup_path);

    // Vérifier s'il existe une sauvegarde précédente
    if (stat(backup_dir_copy, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Aucune sauvegarde précédente trouvée. Création d'un nouveau répertoire.\n");

        // Créer un répertoire pour la nouvelle sauvegarde
        if (mkdir(full_backup_path, 0755) == -1) {
            perror("Erreur lors de la création du répertoire de sauvegarde");
            free(source_dir_copy);
            free(backup_dir_copy);
            return;
        }

        printf("Répertoire de sauvegarde créé : %s\n", full_backup_path);

        // Liste chaînée de tous les fichiers contenus dans la source
        file_list_t tablist = { .head = NULL, .tail = NULL };
        list_files(source_dir_copy, &tablist, 1);

        // Liste de log représentant le contenu du fichier backup_log
        log_t tablog = { .head = NULL, .tail = NULL };

        // Élément de la liste chaînée de fichiers
        file_element* temporary = tablist.head;

        // Pour chaque fichier dans tablist, on liste ses caractéristiques
        while (temporary != NULL) {
            // new_elt est une ligne du fichier backup.log
            log_element* new_elt = malloc(sizeof(log_element));
            if (!new_elt) {
                perror("Memory allocation failed");
                free(source_dir_copy);
                free(backup_dir_copy);
                return;
            }

            char* file_source_path = build_full_path(source_dir_copy, temporary->path);
            if (!file_source_path) {
                perror("Failed to build full path");
                free(new_elt);
                free(source_dir_copy);
                free(backup_dir_copy);
                return;
            }
            char md5_hex[2 * MD5_DIGEST_LENGTH + 1] = { 0 };

            get_md5(file_source_path, md5_hex);
            new_elt->path = build_full_path(backup_dir_copy, temporary->path);
            if (!new_elt->path) {
                perror("Failed to build full path for backup");
                free(new_elt);
                free(file_source_path);
                free(source_dir_copy);
                free(backup_dir_copy);
                return;
            }
            new_elt->date = get_last_modification_date(file_source_path);
            memcpy(new_elt->md5, md5_hex, strlen(md5_hex) + 1);
            new_elt->next = NULL;
            new_elt->prev = tablog.tail;
            if (tablog.tail) {
                tablog.tail->next = new_elt;
            }
            else {
                tablog.head = new_elt;
            }
            tablog.tail = new_elt;

            backup_file(temporary->path, source_dir_copy, full_backup_path);

            temporary = temporary->next;
            free(file_source_path);
        }

        FILE* log_file = NULL;
        // Créer le fichier .backup_log

        //snprintf(log_file_path, sizeof(log_file_path), "%s/.backup_log", full_backup_path);
        log_file = fopen(build_full_path(full_backup_path, ".backup_log"), "w");
        if (!log_file) {
            perror("Erreur lors de la création du fichier .backup_log");
            free(source_dir_copy);
            free(backup_dir_copy);
            return;
        }

        // Pointeur sur la liste tablog, pour tout écrire dans backup.log
        log_element* search = tablog.head;
        while (search != NULL) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s,%s,%s\n", search->path, search->date, search->md5);
            fwrite(buffer, 1, strlen(buffer), log_file);
            search = search->next;
        }
        printf("Sauvegarde effectuée. Tous les fichiers sont sauvegardés et l'index est dans .backup_log.\n");
        fclose(log_file);

        // Free the log list
        while (tablog.head != NULL) {
            log_element* temp = tablog.head;
            tablog.head = tablog.head->next;
            free((void*)temp->path); // Cast to void* to avoid discarding const qualifier
            free(temp);
        }
    }
    else {
        log_t backup_log = { .head = NULL, .tail = NULL };
        log_t save_log = { .head = NULL, .tail = NULL };
        log_t new_save_log = { .head = NULL, .tail = NULL };

        backup_log = read_backup_log(full_backup_path);
        save_log = read_backup_log(source_dir_copy);
        log_element* elt_backup_log = backup_log.head;
        log_element* elt_save_log = save_log.head;
        log_element* elt_new = new_save_log.head;
        while (elt_backup_log != NULL) {
            while (elt_save_log != NULL) {
                if (strcmp(elt_backup_log->path, elt_save_log->path) == 0) {
                    int result = compare_dates(elt_backup_log->date, elt_save_log->date);
                    if (result == 0) {
                        elt_new = elt_backup_log;
                        elt_new = elt_new->next;
                    }
                    if (result < 0) {
                        // Mettre à jour le fichier (trouver comment faire)
                        elt_new = elt_backup_log;
                        elt_new = elt_new->next;
                    }
                }
                elt_save_log = elt_save_log->next;
            }
            elt_backup_log = elt_backup_log->next;

            // Penser à copier le fichier backp.log avant de le modifier
        }
    }

    free(full_backup_path);
    free(source_dir_copy);
    free(backup_dir_copy);
}

// Fonction permettant d'enregistrer dans un fichier le tableau de chunk dédupliqué
void write_backup_file(const char* output_filename, Chunk** chunks, int chunk_count) {
    FILE* file = fopen(output_filename, "wb");
    if (file == NULL) {
        perror("Failed to open output file");
        return;
    }

    for (int i = 0; i < chunk_count; i++) {
        unsigned char* data = (unsigned char*)chunks[i]->data;
        size_t size = 0;
        if (data[0] == 1) {
            size = CHUNK_SIZE;
        }
        else if (data[0] == 0) {
            size = SUB_CHUNK_SIZE;
        }
        else {
            fprintf(stderr, "Invalid chunk type at index %d\n", i);
            continue;
        }

        if (fwrite(data, 1, size, file) != size) {
            perror("Failed to write chunk to file");
            fclose(file);
            return;
        }
    }

    fclose(file);
}

// Fonction implémentant la logique pour la sauvegarde d'un fichier
void backup_file(const char* filename, const char* source_path, char* full_backup_path) {
    // Deduplication from the source directory

    char* filename_source_path = build_full_path(source_path, filename);
    if (!filename_source_path) {
        perror("Failed to build full path for source file");
        return;
    }
    FILE* source_file = fopen(filename_source_path, "rb");
    if (!source_file) {
        perror("Failed to open source file");
        free(filename_source_path);
        return;
    }

    // Determine the file size to allocate sufficient memory for tab_chunk
    fseek(source_file, 0, SEEK_END);
    size_t file_size = ftell(source_file);
    rewind(source_file);

    // Calculate the number of chunks
    size_t num_chunks = (file_size + 4095) / 4096; // Round up to the nearest chunk

    // Allocate memory for tab_chunk
    Chunk** tab_chunk = malloc(num_chunks * sizeof(Chunk*));
    if (!tab_chunk) {
        perror("Failed to allocate memory for tab_chunk");
        fclose(source_file);
        free(filename_source_path);
        return;
    }

    Md5Entry* hash_table = malloc(num_chunks * sizeof(Md5Entry));
    if (!hash_table) {
        perror("Failed to allocate memory for hash_table");
        free(tab_chunk);
        fclose(source_file);
        free(filename_source_path);
        return;
    }
    init_hash_table(hash_table, num_chunks);
    deduplicate_file(source_file, tab_chunk, hash_table);

    // Copy the saved file to the backup directory using the chunk array
    char* backup_path = build_full_path(full_backup_path, filename);
    if (!backup_path) {
        perror("Failed to build full path for backup file");
        free(tab_chunk);
        free(hash_table);
        fclose(source_file);
        free(filename_source_path);
        return;
    }

    write_backup_file(backup_path, tab_chunk, num_chunks);

    // Free allocated memory
    free(filename_source_path);
    free(backup_path);
    free(tab_chunk);
    free(hash_table);
    fclose(source_file);
}


// Fonction permettant la restauration du fichier backup via le tableau de chunk
//void write_restored_file(const char* output_filename, Chunk* chunks, int chunk_count) {
    /*
    */
    //}

    // Fonction pour restaurer une sauvegarde
    //void restore_backup(const char* backup_id, const char* restore_dir) {
        /* @param: backup_id est le chemin vers le répertoire de la sauvegarde que l'on veut restaurer
        *          restore_dir est le répertoire de destination de la restauration
        */
        //}

        // Fonction permettant de lister les différentes sauvegardes présentes dans la destination
        /*
        void list_backups(const char* backup_dir) {

        }
        */