//
// Created by marou on 15/12/2024.
//

#ifndef UTILITIES_H
#define UTILITIES_H

#include <time.h>

/**
* @brief Vérifie si un répertoire est accessible en lecture et écriture.
*
* @param path Le chemin du répertoire à vérifier.
* @return int 1 si le répertoire est accessible, 0 sinon.
* Vérifie l'existence du répertoire et les permissions R/W.
*/
int is_directory_accessible(const char *path); //utile pour backup_manager

/**
* @brief Construit le chemin complet d'un fichier.
*
* @param dir Le répertoire de base.
* @param relative_path Le chemin relatif à ajouter.
* @return char* Le chemin complet alloué dynamiquement ou NULL en cas d'erreur.
* Concatène le répertoire et le chemin relatif avec '/' entre les deux.
*/
char *build_full_path(const char *dir, const char *relative_path); //utile pour backup_manager

/**
* @brief Obtient la date de dernière modification d'un fichier.
*
* @param filepath Le chemin du fichier.
* @return char* La date au format "YYYY-MM-DD-hh:mm" ou NULL en cas d'erreur.
* La chaîne retournée doit être libérée par l'appelant.
*/
char *get_last_modification_date(const char *filepath); //utile pour backup_manager

/**
* @brief Calcule le MD5 d'un fichier.
*
* @param filepath Le chemin du fichier.
* @param md5_hex Le buffer pour stocker le hash en hexadécimal.
* Le buffer doit être assez grand pour contenir le MD5 en hex + '\0'.
*/
void get_md5(const char *filepath, unsigned char *md5_hex); //utile pour backup_manager

/**
* @brief Parse une chaîne de date vers une structure tm.
*
* @param date_str La date au format "YYYY-MM-DD-hh:mm".
* @param tm_info La structure tm à remplir.
* @return int 0 si succès, -1 si erreur.
*/
int parse_date(const char *date_str, struct tm *tm_info); //utile pour backup_manager

/**
* @brief Compare deux dates.
*
* @param date1 Première date au format "YYYY-MM-DD-hh:mm".
* @param date2 Deuxième date au même format.
* @return int -1 si date1 < date2, 0 si égales, 1 si date1 > date2.
*/
int compare_dates(const char *date1, const char *date2); //utile pour backup_manager

/**
* @brief Supprime le slash final d'un chemin s'il existe.
*
* @param path Le chemin à nettoyer.
* Modifie la chaîne directement.
*/
void remove_trailing_slash(char *path);

/**
* @brief Retire le répertoire source d'un chemin complet.
*
* @param source_dir_copy Le répertoire source à retirer.
* @param full_path Le chemin complet.
* @return char* Le chemin relatif alloué dynamiquement.
*/
char *remove_source_dir(const char *source_dir_copy, const char *full_path);

/**
* @brief Crée les répertoires intermédiaires d'un chemin.
*
* @param path Le chemin complet.
* Crée tous les répertoires manquants du chemin.
*/
void create_intermediate_directories(const char *path);

/**
* @brief Trouve le répertoire de sauvegarde le plus récent.
*
* @param backup_dir_copy Le répertoire des sauvegardes.
* @return char* Le nom de la sauvegarde la plus récente ou NULL.
*/
char *get_latest_backup_dir(const char *backup_dir_copy);

/**
* @brief Vérifie si un répertoire est vide.
*
* @param dir_path Le chemin du répertoire.
* @return int 1 si vide, 0 si non vide, -1 si erreur.
*/
int is_directory_empty(const char *dir_path);

/**
* @brief Extrait la partie après le premier slash.
*
* @param input Le chemin d'entrée.
* @return char* La partie après le premier slash ou une copie de l'entrée.
*/
char *cut_after_first_slash(const char *input);

/**
* @brief Garde uniquement la partie avant le slash.
*
* @param input Le chemin d'entrée.
* @return char* La partie avant le premier slash ou une copie de l'entrée
*/

char* remove_after_slash(const char *input);

#endif //UTILITIES_H