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

    // Récupère l'heure actuelle
    clock_gettime(CLOCK_REALTIME, &ts);
    // Convertir en heure locale
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

    char* full_backup_path = build_full_path(backup_dir_copy, backup_name);
    if (!full_backup_path) {
        perror("Failed to build full path for backup");
        free(source_dir_copy);
        free(backup_dir_copy);
        return;
    }
    printf("Chemin complet de la sauvegarde : %s\n", full_backup_path);

    // Vérifier s'il existe une sauvegarde précédente
    if (is_directory_empty(backup_dir_copy) == 1) {
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
        file_element* temporary = tablist.head;

        // Liste de log représentant le contenu du fichier backup_log
        log_t tablog = { .head = NULL, .tail = NULL };

        FILE* log_file = NULL;
        // Créer le fichier .backup_log

        char* log_file_path = build_full_path(backup_dir_copy, ".backup_log");
        log_file = fopen(log_file_path, "w");
        if (!log_file) {
            perror("Erreur lors de la création du fichier .backup_log");
            free(source_dir_copy);
            free(backup_dir_copy);
            return;
        }
        // Élément de la liste chaînée de fichiers

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
            char* file_dest_path = remove_source_dir(source_dir_copy, temporary->path);
            unsigned char md5_hex[MD5_DIGEST_LENGTH * 2 + 1] = { 0 };
            get_md5(temporary->path, md5_hex);
            char* log_element_path = build_full_path(backup_name, file_dest_path);


            new_elt->path = log_element_path;
            if (!new_elt->path) {
                perror("Failed to build full path for backup");
                free(new_elt);
                free(file_dest_path);
                free(source_dir_copy);
                free(backup_dir_copy);
                return;
            }
            char* last_date = get_last_modification_date(temporary->path);
            strcpy(new_elt->date, last_date);
            strcpy((char*)new_elt->md5, (char*)md5_hex);
            new_elt->next = NULL;
            new_elt->prev = tablog.tail;

            if (tablog.tail) {
                tablog.tail->next = new_elt;
            }
            else {
                tablog.head = new_elt;
            }
            tablog.tail = new_elt;

            backup_file(file_dest_path, source_dir_copy, full_backup_path);

            temporary = temporary->next;
            free(file_dest_path);
            free(last_date);
        }



        // Pointeur sur la liste tablog, pour tout écrire dans backup.log
        log_element* search_elt = tablog.head;
        while (search_elt != NULL) {

            //fprintf(log_file, "%s,%s,%s\n", search_elt->path, search_elt->date, search_elt->md5);

            write_log_element(search_elt, log_file);
            search_elt = search_elt->next;
        }
        printf("Sauvegarde effectuée. Tous les fichiers sont sauvegardés et l'index est dans .backup_log.\n");
        fclose(log_file);
        free(log_file_path);

        free_log_list(&tablog);
        free_file_list(&tablist);
    }
    else {
        printf("Une sauvegarde a été trouvé. Création d'un nouveau répertoire.\n");
        char* backup_log_path = build_full_path(backup_dir_copy, ".backup_log");
        char* last_backup_directory = get_latest_backup_dir(backup_dir_copy);
        printf("%s\n", last_backup_directory);

        log_t backup_log = read_backup_log(backup_log_path);


        log_t save_log = { .head = NULL, .tail = NULL };

        file_list_t tablist = { .head = NULL, .tail = NULL };
        list_files(source_dir_copy, &tablist, 1);
        file_element* temporary = tablist.head;

        char* last_backup_directory_path = build_full_path(last_backup_directory, ".backup_log");
        char* last_backup_directory_full_path = build_full_path(backup_dir_copy, last_backup_directory_path);
        printf("%s\n", last_backup_directory_full_path);
        if (copy_file(backup_log_path, last_backup_directory_full_path) != 0) {
            printf("Le fichier des logs n'a pas été dupliquer");
        };

        FILE* log_file = NULL;
        // Créer le fichier .backup_log
        log_file = fopen(backup_log_path, "w");
        if (!log_file) {
            perror("Erreur lors de la création du fichier .backup_log");
            free(source_dir_copy);
            free(backup_dir_copy);
            return;
        }

        while (temporary != NULL) {
            log_element* new_elt = malloc(sizeof(log_element));
            if (!new_elt) {
                perror("Memory allocation failed");
                free(source_dir_copy);
                free(backup_dir_copy);
                return;
            }
            char* file_dest_path = remove_source_dir(source_dir_copy, temporary->path);
            unsigned char md5_hex[MD5_DIGEST_LENGTH * 2 + 1] = { 0 };
            get_md5(temporary->path, md5_hex);
            char* log_element_path = build_full_path(backup_name, file_dest_path);


            new_elt->path = log_element_path;
            if (!new_elt->path) {
                perror("Failed to build full path for backup");
                free(new_elt);
                free(file_dest_path);
                free(source_dir_copy);
                free(backup_dir_copy);
                return;
            }
            char* last_date = get_last_modification_date(temporary->path);
            strcpy(new_elt->date, last_date);

            strcpy((char*)new_elt->md5, (char*)md5_hex);

            new_elt->next = NULL;
            new_elt->prev = save_log.tail;

            if (save_log.tail) {
                save_log.tail->next = new_elt;
            }
            else {
                save_log.head = new_elt;
            }
            save_log.tail = new_elt;
            temporary = temporary->next;
            free(file_dest_path);
            free(last_date);
        }

        log_element* elt_save_log = save_log.head;

        int new_save = 0;

        while (elt_save_log != NULL) {
            char* file_source_path = cut_after_first_slash(elt_save_log->path);
            log_element* elt_backup_log = backup_log.head;
            while (elt_backup_log != NULL) {
                char* file_backup_path = cut_after_first_slash(elt_backup_log->path);
                if (file_backup_path == NULL) {
                    printf("file dest path goes Brrrr");
                    backup_file(elt_save_log->path, source_dir_copy, full_backup_path);
                }
                else {
                    //printf("%s       %s\n", file_backup_path, file_source_path);
                    if (strcmp(file_source_path, file_backup_path) == 0) {
                        if (strcmp((char*)elt_backup_log->md5, (char*)elt_save_log->md5) != 0) {
                            if (new_save == 0) {
                                if (mkdir(full_backup_path, 0755) == -1) {
                                    perror("Erreur lors de la création du répertoire de sauvegarde");
                                    free(source_dir_copy);
                                    free(backup_dir_copy);
                                    return;
                                }
                                new_save = 1;
                            }
                            printf("%s\n", elt_save_log->path);
                            backup_file(file_source_path, source_dir_copy, full_backup_path);
                        }
                        else {
                            free(elt_save_log->path);
                            elt_save_log->path = strdup(elt_backup_log->path);
                            strcpy((char*)elt_save_log->md5, (char*)elt_backup_log->md5);
                            strcpy(elt_save_log->date, elt_backup_log->date);
                        }
                    }
                }
                elt_backup_log = elt_backup_log->next;
                free(file_backup_path);
            }
            free(file_source_path);
            elt_save_log = elt_save_log->next;

            // Penser à copier le fichier backp.log avant de le modifier
        }

        log_element* search_elt = save_log.head;
        while (search_elt != NULL) {
            write_log_element(search_elt, log_file);
            search_elt = search_elt->next;
        }

        fclose(log_file);
        free(last_backup_directory);
        free(backup_log_path);
        free(last_backup_directory_path);
        free(last_backup_directory_full_path);
        free_log_list(&backup_log);
        free_log_list(&save_log);
        free_file_list(&tablist);
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
        size_t size = CHUNK_SIZE;
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
    Chunk** tab_chunk = malloc(num_chunks * sizeof(Chunk));
    if (!tab_chunk) {
        perror("Failed to allocate memory for tab_chunk");
        fclose(source_file);
        free(filename_source_path);
        return;
    }

    Md5Entry* hash_table[HASH_TABLE_SIZE];
    init_hash_table(hash_table);
    deduplicate_file(source_file, tab_chunk, hash_table);

    // Copy the saved file to the backup directory using the chunk array
    char* backup_path = build_full_path(full_backup_path, filename);
    if (!backup_path) {
        perror("Failed to build full path for backup file");
        free(tab_chunk);
        clean_hash_table(hash_table);
        fclose(source_file);
        free(filename_source_path);
        return;
    }

    // Create intermediate directories if they do not exist
    create_intermediate_directories(backup_path);

    write_backup_file(backup_path, tab_chunk, num_chunks);

    // Free allocated memory
    free(filename_source_path);
    free(backup_path);
    for (size_t i = 0; i < num_chunks; i++) {
        free(tab_chunk[i]->data);
        free(tab_chunk[i]);
    }
    free(tab_chunk);
    clean_hash_table(hash_table);
    fclose(source_file);
    printf("Fichier saved\n");
}


// Fonction permettant de lister les différentes sauvegardes présentes dans la destination
/*
void list_backups(const char *backup_dir) {

}
*/