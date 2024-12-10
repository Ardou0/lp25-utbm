#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stdio.h>
#include <openssl/md5.h>

// Structure pour une ligne du fichier log
typedef struct log_element{
    const char *path; // Chemin du fichier/dossier
    unsigned char md5[MD5_DIGEST_LENGTH]; // MD5 du fichier dédupliqué
    char *date; // Date de dernière modification
    struct log_element *next;
    struct log_element *prev;
} log_element;

// Structure pour une liste de log représentant le contenu du fichier backup_log
typedef struct {
    log_element *head; // Début de la liste de log 
    log_element *tail; // Fin de la liste de log
} log_t;

// Structure pour un élément de la liste chaînée de fichiers
typedef struct file_element {
    char *path; // Chemin du fichier
    struct file_element *next;
} file_element;

// Structure pour une liste chaînée de fichiers
typedef struct {
    file_element *head; // Début de la liste de fichiers
    file_element *tail; // Fin de la liste de fichiers
} file_list_t;


/**
 * @brief Lit le fichier de sauvegarde et remplit une liste chaînée de logs.
 *
 * @param logfile Le chemin du fichier de sauvegarde à lire.
 * @return log_t Une structure log_t contenant la liste chaînée des logs lus depuis le fichier.
 */
log_t read_backup_log(const char *logfile);
/**
 * @brief Met à jour le fichier de sauvegarde avec les logs fournis.
 *
 * @param logfile Le chemin du fichier de sauvegarde à mettre à jour.
 * @param logs Un pointeur vers une structure log_t contenant la liste chaînée des logs à écrire dans le fichier.
 */
void update_backup_log(const char *logfile, log_t *logs);
/**
 * @brief Écrit un élément de log dans le fichier de sauvegarde.
 *
 * @param elt Un pointeur vers l'élément de log à écrire.
 * @param logfile Un pointeur vers le fichier de sauvegarde où écrire l'élément de log.
 */
void write_log_element(log_element *elt, FILE *logfile);
/**
 * @brief Liste les fichiers dans un répertoire et les ajoute à une liste chaînée.
 *
 * @param path Le chemin du répertoire à lister.
 * @param file_list Un pointeur vers une structure file_list_t où les chemins de fichiers seront ajoutés.
 * @param recursive Un indicateur pour déterminer si la fonction doit lister les fichiers de manière récursive.
 */
void list_files(const char *path, file_list_t *file_list, int recursive);
/**
 * @brief Libère la mémoire allouée pour une liste chaînée d'index de fichiers.
 *
 * @param file_list Un pointeur vers la structure file_list_t à libérer.
 */
void free_file_list(file_list_t *file_list);
/**
 * @brief Libère la mémoire allouée pour une liste chaînée de logs.
 *
 * @param file_list Un pointeur vers la structure log_t à libérer.
 */
void free_log_list(log_t *logs);
/**
 * @brief Copie un fichier d'un emplacement source vers un emplacement de destination.
 *
 * @param src Le chemin du fichier source à copier.
 * @param dest Le chemin du fichier de destination où copier le fichier source.
 */
void copy_file(const char *src, const char *dest);

#endif // FILE_HANDLER_H
