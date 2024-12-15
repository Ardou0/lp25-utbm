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
#include <math.h>

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

    // Créer le nom du chemin complet

    char* full_backup_path = build_full_path(backup_dir_copy, backup_name);
    if (!full_backup_path) {
        perror("Failed to build full path for backup");
        free(source_dir_copy);
        free(backup_dir_copy);
        return;
    }

    // Vérifier s'il existe une sauvegarde précédente
    if (is_directory_empty(backup_dir_copy) == 1) {

        // Créer un répertoire pour la nouvelle sauvegarde
        if (mkdir(full_backup_path, 0755) == -1) {
            perror("Erreur lors de la création du répertoire de sauvegarde");
            free(source_dir_copy);
            free(backup_dir_copy);
            return;
        }

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
        fclose(log_file);
        char* full_backup_log_path = build_full_path(full_backup_path, ".backup_log");
        if (copy_file(log_file_path, full_backup_log_path) != 0) {
            printf("Le fichier des logs n'a pas été dupliquer");
        };
        free(log_file_path);
        free(full_backup_log_path);


        free_log_list(&tablog);
        free_file_list(&tablist);
    }
    else {
        char* backup_log_path = build_full_path(backup_dir_copy, ".backup_log");
        char* last_backup_directory = get_latest_backup_dir(backup_dir_copy);


        log_t backup_log = read_backup_log(backup_log_path);


        log_t save_log = { .head = NULL, .tail = NULL };

        file_list_t tablist = { .head = NULL, .tail = NULL };
        list_files(source_dir_copy, &tablist, 1);
        file_element* temporary = tablist.head;

        char* last_backup_directory_path = build_full_path(last_backup_directory, ".backup_log");
        char* last_backup_directory_full_path = build_full_path(backup_dir_copy, last_backup_directory_path);

        if (copy_file(backup_log_path, last_backup_directory_full_path) != 0) {
            printf("Le fichier des logs n'a pas été copier");
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
            int find_equivalent = 0;
            while (elt_backup_log != NULL && find_equivalent == 0) {
                char* file_backup_path = cut_after_first_slash(elt_backup_log->path);

                //printf("%s       %s\n", file_backup_path, file_source_path);
                if (strcmp(file_source_path, file_backup_path) == 0) {
                    find_equivalent = 1;
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
                        backup_file(file_source_path, source_dir_copy, full_backup_path);
                    }
                    else {
                        free(elt_save_log->path);
                        elt_save_log->path = strdup(elt_backup_log->path);
                        strcpy((char*)elt_save_log->md5, (char*)elt_backup_log->md5);
                        strcpy(elt_save_log->date, elt_backup_log->date);
                    }
                }

                elt_backup_log = elt_backup_log->next;
                free(file_backup_path);
            }

            if (find_equivalent == 0) {
                if (new_save == 0) {
                    if (mkdir(full_backup_path, 0755) == -1) {
                        perror("Erreur lors de la création du répertoire de sauvegarde");
                        free(source_dir_copy);
                        free(backup_dir_copy);
                        return;
                    }
                    new_save = 1;
                }
                backup_file(file_source_path, source_dir_copy, full_backup_path);
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

        char* full_backup_log_path = build_full_path(full_backup_path, ".backup_log");
        if (copy_file(backup_log_path, full_backup_log_path) != 0) {
            printf("Le fichier des logs n'a pas été copier");
        };
        free(full_backup_log_path);
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
    FILE* file = fopen(output_filename, "w+b");
    if (file == NULL) {
        perror("Failed to open output file");
        return;
    }

    for (int i = 0; i < chunk_count; i++) {
        unsigned char* data = (unsigned char*)chunks[i]->data;
        size_t size = chunks[i]->size; // Utiliser la taille stockée dans le chunk
        // Écrire la taille du chunk dans le fichier
        if (fwrite(data, 1, size, file) != size) {
            perror("Failed to write chunk data to file");
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

    printf("|%s  =>  Saved\n", filename_source_path);
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
}

//
void restore_backup(const char* backup_id, const char* restore_dir) {
    // Vérifier l'accessibilité du répertoire comme dans create_backup 
    char* backup_id_copy = strdup(backup_id);
    char* restore_dir_copy = strdup(restore_dir);
    remove_trailing_slash(backup_id_copy);
    remove_trailing_slash(restore_dir_copy);
    if (!is_directory_accessible(restore_dir)) {
        fprintf(stderr, "Le répertoire de restauration '%s' n'est pas accessible.\n", restore_dir);
        return;
    }

    // Lire le backup_log comme dans create_backup
    char* backup_log_path = build_full_path(backup_id_copy, ".backup_log");
    if (!backup_log_path) {

        fprintf(stderr, "Erreur lors de la construction du chemin du fichier log\n");
        return;

    }
    log_t backup_log = read_backup_log(backup_log_path);
    log_element* current = backup_log.head;

    while (current) {
        // Construire les chemins comme dans backup_file
        char* file_path = cut_after_first_slash(current->path);
        char* dir_backup = remove_after_slash(backup_id_copy);
        char* source_path = build_full_path(dir_backup, current->path);
        char* dest_path = build_full_path(restore_dir_copy, file_path);
        if (!source_path || !dest_path) {
            free(source_path);
            free(dest_path);
            continue;
        }

        // Créer les répertoires intermédiaires comme dans backup_file
        create_intermediate_directories(dest_path);

        // Ouvrir le fichier source en binaire comme dans backup_file
        FILE* source_file = fopen(source_path, "rb");
        if (!source_file) {
            perror("Failed to open source file");
            free(source_path);
            free(dest_path);
            continue;
        }

        // Calculer la taille comme dans backup_file
        fseek(source_file, 0, SEEK_END);
        size_t file_size = ftell(source_file);
        rewind(source_file);
        size_t num_chunks = (size_t)ceil((double)file_size / CHUNK_SIZE);

        // Allouer les chunks comme dans backup_file
        Chunk** chunks = malloc(num_chunks * sizeof(Chunk*));
        if (!chunks) {
            perror("Failed to allocate memory for chunks");
            fclose(source_file);
            free(source_path);
            free(dest_path);
            continue;
        }

        // Initialiser la table de hash comme dans backup_file
        Md5Entry* hash_table[HASH_TABLE_SIZE];
        init_hash_table(hash_table);

        // Restaurer le fichier
        int chunk_count = 0;
        undeduplicate_file(source_file, chunks, &chunk_count);
        write_restored_file(dest_path, chunks, chunk_count);

        // Nettoyage comme dans backup_file
        for (size_t i = 0; i < num_chunks; i++) {
            if (chunks[i]) {
                free(chunks[i]->data);
                free(chunks[i]);
            }
        }
        free(chunks);
        clean_hash_table(hash_table);
        fclose(source_file);
        free(source_path);
        free(dir_backup);
        free(dest_path);
        free(file_path);
        current = current->next;
    }

    // Libérer la mémoire comme dans create_backup
    free(backup_log_path);
    free_log_list(&backup_log);
    free(backup_id_copy);
    free(restore_dir_copy);
}
void write_restored_file(const char* output_filename, Chunk** chunks, int chunk_count) {
    FILE* file = fopen(output_filename, "wb");
    if (file == NULL) {
        perror("Failed to open output file");
        return;
    }

    for (int i = 0; i < chunk_count; i++) {
        if (chunks[i] && chunks[i]->data && chunks[i]->size) {
            // Convert size to an integer safely
            size_t size = chunks[i]->size;
            // Write chunk data to the file
            if (fwrite(chunks[i]->data, 1, size, file) != size) {
                perror("Failed to write chunk data");
                fclose(file);
                return;
            }
        }
    }

    fclose(file);
    printf("|%s  =>   Restored\n", output_filename);
}



void list_backups(const char* backup_dir) {
    file_list_t files = { .head = NULL, .tail = NULL };
    list_files(backup_dir, &files, 0);
    file_element* current = files.head;

    int backup_file = 0;
    int backup_folder = 1;

    while (current) {
        char* file_name = remove_source_dir(backup_dir, current->path);
        if (strcmp(file_name, ".backup_log") == 0) {
            backup_file = 1;
        }
        else {
            if (!is_directory_accessible(current->path)) {
                backup_folder = 0;
            }
        }
        free(file_name);

        current = current->next;
    }

    // Reset current to the head of the list
    current = files.head;
    int i = 1;

    if (backup_file && backup_folder) {
        while (current) {
            char* file_name = remove_source_dir(backup_dir, current->path);
            if (strcmp(file_name, ".backup_log") != 0) {
                printf("%d# | %s\n", i, file_name);
                i++;
            }
            free(file_name);
            current = current->next;
        }
    }
    else {
        printf("|Source is not a valid backup folder: %s.\n", backup_dir);
    }

    free_file_list(&files);
}