#ifndef UTILITIES_H
#define UTILITIES_H

#include <time.h>

// Fonction pour vérifier si un répertoire en local est accessible
//utile pour backup_manager
int is_directory_accessible(const char *path);

// Fonction pour construire le chemin complet d'un fichier
//utile pour backup_manager
char* build_full_path(const char *dir, const char *relative_path);

// Fonction pour obtenir la dernière date de modification d'un fichier
//utile pour backup_manager
char* get_last_modification_date(const char *filepath);

//calcule le md5 d'un fichier
//utile pour backup_manager
void get_md5(const char *filepath, unsigned char *md5_hex);

// Fonction pour convertir une chaîne de caractères en structure tm
//utile pour backup_manager
int parse_date(const char *date_str, struct tm *tm_info);

// Fonction pour comparer deux dates
//utile pour backup_manager
int compare_dates(const char *date1, const char *date2);

void remove_trailing_slash(char *path);

#endif //UTILITIES_H
