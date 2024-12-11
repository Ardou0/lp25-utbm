#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <dirent.h>

unsigned int hash_md5(unsigned char *md5) {
    unsigned int hash = 0;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        hash = (hash << 5) + hash + md5[i];
    }
    return hash % HASH_TABLE_SIZE;
}

void compute_md5(void *data, size_t len, unsigned char *md5_out) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        perror("Failed to create EVP_MD_CTX");
        exit(EXIT_FAILURE);
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1 ||
        EVP_DigestUpdate(mdctx, data, len) != 1 ||
        EVP_DigestFinal_ex(mdctx, md5_out, NULL) != 1) {
        perror("Failed to compute MD5 hash");
        EVP_MD_CTX_free(mdctx);
        exit(EXIT_FAILURE);
    }

    EVP_MD_CTX_free(mdctx);
}

int find_md5(Md5Entry *hash_table, unsigned char *md5) {
    unsigned int index = hash_md5(md5);
    while (hash_table[index].index != -1) {
        if (memcmp(hash_table[index].md5, md5, MD5_DIGEST_LENGTH) == 0) {
            return hash_table[index].index;
        }
        index = (index + 1) % HASH_TABLE_SIZE;
    }
    return -1;
}

void add_md5(Md5Entry *hash_table, unsigned char *md5, int index) {
    unsigned int hash = hash_md5(md5);
    while (hash_table[hash].index != -1) {
        hash = (hash + 1) % HASH_TABLE_SIZE;
    }
    memcpy(hash_table[hash].md5, md5, MD5_DIGEST_LENGTH);
    hash_table[hash].index = index;
}

void init_hash_table(Md5Entry *hash_table) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_table[i].index = -1;
    }
}

void deduplicate_file(FILE *file, Chunk *chunks, Md5Entry *hash_table) {
    int chunk_index = 0;
    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        unsigned char md5[MD5_DIGEST_LENGTH];
        compute_md5(buffer, bytes_read, md5);

        int index = find_md5(hash_table, md5);
        if (index == -1) {
            // Nouveau chunk, ajouter à la table de hachage
            add_md5(hash_table, md5, chunk_index);
            chunks[chunk_index].data = malloc(bytes_read);
            memcpy(chunks[chunk_index].data, buffer, bytes_read);
            memcpy(chunks[chunk_index].md5, md5, MD5_DIGEST_LENGTH);
            chunk_index++;
        } else {
            // Chunk déjà présent, créer un sub_chunk de référence
            unsigned char sub_chunk[SUB_CHUNK_SIZE];
            sub_chunk[0] = 0; // Premier bit à 0 pour indiquer un sub_chunk
            snprintf((char *)sub_chunk + 1, SUB_CHUNK_SIZE - 1, "%d", index);
            chunks[chunk_index].data = malloc(SUB_CHUNK_SIZE);
            memcpy(chunks[chunk_index].data, sub_chunk, SUB_CHUNK_SIZE);
            chunk_index++;
        }
    }
}

void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count) {
    int chunk_index = 0;
    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (buffer[0] == 1) {
            // Chunk normal
            chunks[chunk_index] = malloc(sizeof(Chunk));
            chunks[chunk_index]->data = malloc(CHUNK_SIZE - 1);
            memcpy(chunks[chunk_index]->data, buffer + 1, CHUNK_SIZE - 1);
            chunk_index++;
        } else {
            // Sub_chunk de référence
            char ref_index_str[SUB_CHUNK_SIZE];
            memcpy(ref_index_str, buffer + 1, SUB_CHUNK_SIZE - 1);
            int ref_index = atoi(ref_index_str);

            // Remplacer par les données du chunk référencé
            chunks[chunk_index] = malloc(sizeof(Chunk));
            chunks[chunk_index]->data = malloc(CHUNK_SIZE - 1);
            memcpy(chunks[chunk_index]->data, chunks[ref_index]->data, CHUNK_SIZE - 1);
            chunk_index++;
        }
    }
    *chunk_count = chunk_index;
}
