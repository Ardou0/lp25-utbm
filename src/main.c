#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file_handler.h"
#include "deduplication.h"
#include "backup_manager.h"

// Modes possibles
typedef enum { NONE, BACKUP, RESTORE, LIST_BACKUPS } ProgramMode;

// Fonction pour afficher l'usage du programme
void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  --backup                Create a backup (cannot be used with --restore or --list-backups)\n");
    printf("  --restore               Restore a backup (cannot be used with --backup or --list-backups)\n");
    printf("  --list-backups          List available backups (cannot be used with --backup or --restore)\n");
    printf("  --dry-run               Test backup or restore without performing actual operations\n");
    printf("  --dest [PATH]           Specify the destination path\n");
    printf("  --source [PATH]         Specify the source path\n");
    printf("  -v, --verbose           Display verbose output\n");
    printf("  -h, --help              Display this help message\n");
}

int main(int argc, char *argv[]) {
    // Variables pour stocker les options
    ProgramMode mode = NONE;
    int dry_run = 0;
    int verbose = 0;
    char *dest_path = NULL;
    char *source_path = NULL;

    // Définition des options longues
    static struct option long_options[] = {
        {"backup", no_argument, 0, 0},
        {"restore", no_argument, 0, 0},
        {"list-backups", no_argument, 0, 0},
        {"dry-run", no_argument, 0, 0},
        {"dest", required_argument, 0, 0},
        {"source", required_argument, 0, 0},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    // Analyse des options de la ligne de commande
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hv", long_options, &option_index);

        if (c == -1) break; // Fin des options

        switch (c) {
        case 0: // Options longues
            if (strcmp("backup", long_options[option_index].name) == 0) {
                if (mode != NONE) {
                    fprintf(stderr, "Error: Only one of --backup, --restore, or --list-backups can be used.\n");
                    return EXIT_FAILURE;
                }
                mode = BACKUP;
            } else if (strcmp("restore", long_options[option_index].name) == 0) {
                if (mode != NONE) {
                    fprintf(stderr, "Error: Only one of --backup, --restore, or --list-backups can be used.\n");
                    return EXIT_FAILURE;
                }
                mode = RESTORE;
            } else if (strcmp("list-backups", long_options[option_index].name) == 0) {
                if (mode != NONE) {
                    fprintf(stderr, "Error: Only one of --backup, --restore, or --list-backups can be used.\n");
                    return EXIT_FAILURE;
                }
                mode = LIST_BACKUPS;
            } else if (strcmp("dry-run", long_options[option_index].name) == 0) {
                dry_run = 1;
            } else if (strcmp("dest", long_options[option_index].name) == 0) {
                dest_path = optarg;
            } else if (strcmp("source", long_options[option_index].name) == 0) {
                source_path = optarg;
            }
            break;
        case 'h': // Option -h ou --help
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'v': // Option -v ou --verbose
            verbose = 1;
            break;
        case '?': // Option non reconnue
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // Validation des arguments obligatoires
    if (mode == NONE) {
        fprintf(stderr, "Error: One of --backup, --restore, or --list-backups must be specified.\n");
        return EXIT_FAILURE;
    }

    if (mode == BACKUP) {
        if (source_path == NULL || dest_path == NULL) {
            fprintf(stderr, "Error: --source and --dest are required for --backup.\n");
            return EXIT_FAILURE;
        }
        if (verbose) printf("Starting backup from '%s' to '%s'\n", source_path, dest_path);
        // Appel à la fonction de sauvegarde (à implémenter)
    } else if (mode == RESTORE) {
        if (source_path == NULL || dest_path == NULL) {
            fprintf(stderr, "Error: --source and --dest are required for --restore.\n");
            return EXIT_FAILURE;
        }
        if (verbose) printf("Starting restore from '%s' to '%s'\n", source_path, dest_path);
        // Appel à la fonction de restauration (à implémenter)
    } else if (mode == LIST_BACKUPS) {
        if (dest_path != NULL) {
            if (verbose) printf("Listing backups...\n");
            file_list_t files = { .head = NULL, .tail = NULL };
            list_files(dest_path, &files, 0);
            file_element *current = files.head;
            while (current) {
                printf("%s\n", current->path);
                current = current->next;
            }
            free_file_list(&files);
        } else {
            fprintf(stderr, "Error: --dest is required for --list-backups.\n");
            return EXIT_FAILURE;
        }

        // Appel à la fonction de listing des sauvegardes (à implémenter)
    }

    if(dry_run) {
        printf("Need to be implemented");
    }

    if (verbose) printf("Operation completed successfully.\n");
    return EXIT_SUCCESS;
}
