//
// Created by marou on 15/12/2024.
//

#ifndef DEDUPLICATION_H
#define DEDUPLICATION_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <dirent.h>

// Constantes pour la gestion des chunks
/** @brief Taille d'un chunk normal (4096 octets) */
#define CHUNK_SIZE 4096
/** @brief Taille d'un chunk de référence */
#define SUB_CHUNK_SIZE 100
/** @brief Taille de la table de hachage pour la déduplication*/
/** dont on a déjà calculé le MD5 pour effectuer les comparaisons*/
#define HASH_TABLE_SIZE 1000

/**
 * @brief Structure représentant un chunk de données.
 */
typedef struct {
    size_t size;  // Taille du chunk
    void *data;   // Données du chunk
} Chunk;

/**
 * @brief Structure pour stocker un MD5 et son index dans la table de hachage.
 */
typedef struct {
    unsigned char md5[MD5_DIGEST_LENGTH * 2 + 1]; // Hash MD5 en format hexadécimal
    int index;    // Index dans le tableau de chunks
} Md5Entry;

/**
 * @brief Calcule le hash pour l'indexation dans la table.
 *
 * @param md5 Le hash MD5 à transformer en index.
 * @return unsigned int L'index dans la table de hachage.
 */
unsigned int hash_md5(unsigned char *md5);

/**
 * @brief Calcule le MD5 d'un bloc de données.
 *
 * @param data Les données à hasher.
 * @param len La taille des données.
 * @param md5_out Le buffer pour stocker le hash (doit être alloué).
 */
void compute_md5(void *data, size_t len, unsigned char *md5_out);

/**
 * @brief Cherche un MD5 dans la table de hachage.
 *
 * @param hash_table La table de hachage.
 * @param md5 Le MD5 à rechercher.
 * @return int L'index du chunk trouvé ou -1 si non trouvé.
 */
int find_md5(Md5Entry *hash_table[HASH_TABLE_SIZE], unsigned char *md5);

/**
 * @brief Ajoute un MD5 dans la table de hachage.
 *
 * @param hash_table La table de hachage.
 * @param md5 Le MD5 à ajouter.
 * @param index L'index du chunk correspondant.
 */
void add_md5(Md5Entry *hash_table[HASH_TABLE_SIZE], unsigned char *md5, int index);

/**
 * @brief Initialise la table de hachage.
 *
 * @param hash_table La table à initialiser.
 */
void init_hash_table(Md5Entry *hash_table[HASH_TABLE_SIZE]);

/**
 * @brief Libère la mémoire de la table de hachage.
 *
 * @param hash_table La table à nettoyer.
 */
void clean_hash_table(Md5Entry *hash_table[HASH_TABLE_SIZE]);

/**
 * @brief Déduplique un fichier en chunks.
 *
 * @param file Le fichier à dédupliquer.
 * @param chunks Le tableau de chunks résultant.
 * @param hash_table La table de hachage pour la déduplication.
 */
void deduplicate_file(FILE *file, Chunk **chunks, Md5Entry *hash_table[HASH_TABLE_SIZE]);

/**
 * @brief Reconstruit un fichier à partir de ses chunks dédupliqués.
 *
 * @param file Le fichier source contenant les chunks.
 * @param chunks Le tableau pour stocker les chunks reconstruits.
 * @param chunk_count Le nombre de chunks lus.
 */
void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count);

#endif // DEDUPLICATION_H