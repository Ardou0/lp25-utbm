#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <dirent.h>
#include <arpa/inet.h> // For htonl and ntohl

unsigned int hash_md5(unsigned char* md5) {
    unsigned int hash = 0;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        hash = (hash << 5) + hash + md5[i];
    }
    return hash % HASH_TABLE_SIZE;
}

void bytes_to_hex(const unsigned char* bytes, size_t len, unsigned char* hex) {
    const char* hex_chars = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex[2 * i] = hex_chars[(bytes[i] >> 4) & 0xF];
        hex[2 * i + 1] = hex_chars[bytes[i] & 0xF];
    }
    hex[2 * len] = '\0';
}

void compute_md5(void* data, size_t len, unsigned char* md5_hex_out) {
    unsigned char md5_out[MD5_DIGEST_LENGTH];
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        fprintf(stderr, "Failed to create EVP_MD_CTX\n");
        exit(EXIT_FAILURE);
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1 ||
        EVP_DigestUpdate(mdctx, data, len) != 1 ||
        EVP_DigestFinal_ex(mdctx, md5_out, NULL) != 1) {
        fprintf(stderr, "Failed to compute MD5 hash\n");
        EVP_MD_CTX_free(mdctx);
        exit(EXIT_FAILURE);
    }

    EVP_MD_CTX_free(mdctx);

    // Convertir le hachage en chaîne hexadécimale
    bytes_to_hex(md5_out, MD5_DIGEST_LENGTH, md5_hex_out);
}

int find_md5(Md5Entry* hash_table[HASH_TABLE_SIZE  * 2 + 1], unsigned char* md5) {
    unsigned int index = hash_md5(md5);
    while (hash_table[index]->index != -1) {
        if (memcmp(hash_table[index]->md5, md5, MD5_DIGEST_LENGTH  * 2 + 1) == 0) {
            return hash_table[index]->index;
        }
        index = (index + 1) % HASH_TABLE_SIZE;
    }
    return -1;
}
//sefsefsefsefsef
void add_md5(Md5Entry* hash_table[HASH_TABLE_SIZE * 2 + 1], unsigned char* md5, int index) {
    unsigned int hash = hash_md5(md5);
    while (hash_table[hash]->index != -1) {
        hash = (hash + 1) % HASH_TABLE_SIZE;
    }
    memcpy(hash_table[hash]->md5, md5, MD5_DIGEST_LENGTH);
    hash_table[hash]->index = index;
}

void init_hash_table(Md5Entry* hash_table[HASH_TABLE_SIZE  * 2 + 1]) {
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_table[i] = malloc(sizeof(Md5Entry));
        if (hash_table[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory for hash_table entry\n");
            exit(EXIT_FAILURE);
        }
        memset(hash_table[i]->md5, 0, MD5_DIGEST_LENGTH  * 2 + 1); // Initialize md5 to zeros
        hash_table[i]->index = -1; // Initialize index to -1 (or any default value)
    }
}

void clean_hash_table(Md5Entry* hash_table[HASH_TABLE_SIZE  * 2 + 1]) {
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (hash_table[i]) {
            free(hash_table[i]);
        }
    }
}


void deduplicate_file(FILE* file, Chunk** chunks, Md5Entry* hash_table[HASH_TABLE_SIZE  * 2 + 1]) {
    int chunk_index = 0;
    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE - 1, file)) > 0) {
        unsigned char md5[MD5_DIGEST_LENGTH * 2 + 1];
        compute_md5(buffer, bytes_read, md5);

        int index = find_md5(hash_table, md5);
        if (index == -1) {
            // Nouveau chunk, ajouter à la table de hachage
            add_md5(hash_table, md5, chunk_index);
            chunks[chunk_index] = malloc(sizeof(Chunk)); // Allocate memory for the chunk
            if (chunks[chunk_index] == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk\n");
                exit(EXIT_FAILURE);
            }
            chunks[chunk_index]->data = malloc(CHUNK_SIZE);
            if (chunks[chunk_index]->data == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk data\n");
                exit(EXIT_FAILURE);
            }
            unsigned char* data = (unsigned char*)chunks[chunk_index]->data;
            data[0] = 1; // Indiquer que c'est un chunk normal
            memcpy(data + 1, buffer, bytes_read);
            // Remplir les octets restants avec des zéros
            chunks[chunk_index]->size = bytes_read + 1;
            chunk_index++;
        }
        else {
            // Chunk déjà présent, créer un sub_chunk de référence
            unsigned char sub_chunk[SUB_CHUNK_SIZE];
            sub_chunk[0] = 0; // Premier bit à 0 pour indiquer un sub_chunk
            memcpy(sub_chunk + 1, &index, sizeof(int)); // Stocker l'index en binaire
            chunks[chunk_index] = malloc(sizeof(Chunk)); // Allocate memory for the chunk
            if (chunks[chunk_index] == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk\n");
                exit(EXIT_FAILURE);
            }
            chunks[chunk_index]->data = malloc(SUB_CHUNK_SIZE);
            if (chunks[chunk_index]->data == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk data\n");
                exit(EXIT_FAILURE);
            }
            memcpy(chunks[chunk_index]->data, sub_chunk, SUB_CHUNK_SIZE);

            chunks[chunk_index]->size = SUB_CHUNK_SIZE;
            // Remplir les octets restants avec des zéros
            chunk_index++;
        }
    }
}

void undeduplicate_file(FILE* file, Chunk** chunks, int* chunk_count) {
    int chunk_index = 0;
    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;
    unsigned char type;

    while ((bytes_read = fread(&type, 1, 1, file)) > 0) {
        printf("buffer[0]: %d\n", type); // Debug message to display buffer[0]
        if (type == 1) {
            // C'est un chunk normal, lire le reste du chunk
            if (fread(buffer, 1, CHUNK_SIZE - 1, file) != CHUNK_SIZE - 1) {
                fprintf(stderr, "Failed to read normal chunk\n");
                exit(EXIT_FAILURE);
            }
            // Copier directement dans le tableau de chunks
            chunks[chunk_index] = malloc(sizeof(Chunk));
            if (chunks[chunk_index] == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk\n");
                exit(EXIT_FAILURE);
            }
            chunks[chunk_index]->data = malloc(CHUNK_SIZE - 1);
            if (chunks[chunk_index]->data == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk data\n");
                exit(EXIT_FAILURE);
            }
            memcpy(chunks[chunk_index]->data, buffer, CHUNK_SIZE - 1);
            chunks[chunk_index]->size = CHUNK_SIZE - 1;
            chunk_index++;
        } else {
            // C'est un sub_chunk, lire le reste du sub_chunk
            if (fread(buffer, 1, SUB_CHUNK_SIZE - 1, file) != SUB_CHUNK_SIZE - 1) {
                fprintf(stderr, "Failed to read sub_chunk\n");
                exit(EXIT_FAILURE);
            }
            // Récupérer l'index du chunk original
            int index;
            // Lire l'index en utilisant ntohl pour convertir en format hôte
            memcpy(&index, buffer, sizeof(int));
            index = ntohl(index);
            printf("Read sub_chunk with index: %d\n", index); // Debug message
            if (index >= chunk_index) {
                fprintf(stderr, "Invalid index in sub_chunk\n");
                exit(EXIT_FAILURE);
            }
            Chunk* original_chunk = chunks[index];
            // Copier les données du chunk original dans le tableau de chunks
            chunks[chunk_index] = malloc(sizeof(Chunk));
            if (chunks[chunk_index] == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk\n");
                exit(EXIT_FAILURE);
            }
            chunks[chunk_index]->data = malloc(original_chunk->size);
            if (chunks[chunk_index]->data == NULL) {
                fprintf(stderr, "Failed to allocate memory for chunk data\n");
                exit(EXIT_FAILURE);
            }
            memcpy(chunks[chunk_index]->data, original_chunk->data, original_chunk->size);
            chunks[chunk_index]->size = original_chunk->size;
            chunk_index++;
        }
        // Réinitialiser le buffer après avoir traité chaque chunk
        memset(buffer, 0, CHUNK_SIZE);
    }

    // Mettre à jour le compteur de chunks
    *chunk_count = chunk_index;
}

