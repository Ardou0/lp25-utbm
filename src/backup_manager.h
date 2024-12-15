#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>

/**
 * @brief Génère un nom de fichier de sauvegarde basé sur la date et l'heure actuelles.
 *
 * @param buffer Un pointeur vers un tampon où le nom de fichier généré sera stocké.
 * @param size La taille du tampon.
 */
void generate_backup_name(char *buffer, size_t size);

/**
 * @brief Crée un nouveau backup incrémental d'un répertoire source vers un répertoire de sauvegarde.
 *
 * @param source_dir Le chemin du répertoire source à sauvegarder.
 * @param backup_dir Le chemin du répertoire de sauvegarde où les fichiers seront copiés.
 */
void create_backup(const char *source_dir, const char *backup_dir);

/**
 * @brief Restaure une sauvegarde à partir d'un identifiant de sauvegarde vers un répertoire de restauration.
 *
 * @param backup_id L'identifiant de la sauvegarde à restaurer.
 * @param restore_dir Le chemin du répertoire où les fichiers de sauvegarde seront restaurés.
 */
void restore_backup(const char *backup_id, const char *restore_dir);

/**
 * @brief Enregistre un tableau de chunks dédupliqués dans un fichier.
 *
 * @param output_filename Le nom du fichier où les chunks seront enregistrés.
 * @param chunks Un tableau de pointeurs vers les chunks à enregistrer.
 * @param chunk_count Le nombre de chunks dans le tableau.
 */
void write_backup_file(const char *output_filename, Chunk **chunks, int chunk_count);

/**
 * @brief Effectue une sauvegarde d'un fichier dédupliqué.
 *
 * @param filename Le nom du fichier à sauvegarder.
 * @param source_path Le chemin du répertoire source contenant le fichier.
 * @param full_backup_path Le chemin complet où le fichier de sauvegarde sera enregistré.
 */
void backup_file(const char *filename,const char *source_path, char *full_backup_path);

/**
 * @brief Restaure un fichier de sauvegarde en utilisant un tableau de chunks.
 *
 * @param output_filename Le nom du fichier de sauvegarde à restaurer.
 * @param chunks Un tableau de pointeurs vers les chunks à restaurer.
 * @param chunk_count Le nombre de chunks dans le tableau.
 */
void write_restored_file(const char *output_filename, Chunk **chunks, int chunk_count);

/**
 * @brief Liste les différentes sauvegardes présentes dans le répertoire de destination.
 *
 * @param backup_dir Le chemin du répertoire contenant les sauvegardes.
 */
void list_backups(const char *backup_dir);

#endif // BACKUP_MANAGER_H