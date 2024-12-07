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

void deduplicate_file(FILE *file, Chunk *chunks, Md5Entry *hash_table) {
    unsigned char buffer[CHUNK_SIZE];
    int chunk_index = 0;

    while (!feof(file)) {
        size_t bytes_read = fread(buffer, 1, CHUNK_SIZE, file);
        if (bytes_read == 0) break;

        unsigned char md5[MD5_DIGEST_LENGTH];
        compute_md5(buffer, bytes_read, md5);

        int existing_index = find_md5(hash_table, md5);
        if (existing_index == -1) {
            add_md5(hash_table, md5, chunk_index);
            chunks[chunk_index].data = malloc(bytes_read);
            memcpy(chunks[chunk_index].data, buffer, bytes_read);
            memcpy(chunks[chunk_index].md5, md5, MD5_DIGEST_LENGTH);
            chunk_index++;
        }
    }
}

void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count) {
    *chunk_count = 0;
    size_t chunk_capacity = 10;
    *chunks = malloc(chunk_capacity * sizeof(Chunk));

    unsigned char md5[MD5_DIGEST_LENGTH];

    while (fread(md5, 1, MD5_DIGEST_LENGTH, file) == MD5_DIGEST_LENGTH) {
        size_t data_size;
        fread(&data_size, sizeof(size_t), 1, file);

        if ((size_t)*chunk_count >= chunk_capacity) {
            chunk_capacity *= 2;
            *chunks = realloc(*chunks, chunk_capacity * sizeof(Chunk));
        }

        Chunk *chunk = &(*chunks)[*chunk_count];
        chunk->data = malloc(data_size);
        fread(chunk->data, 1, data_size, file);
        memcpy(chunk->md5, md5, MD5_DIGEST_LENGTH);
        (*chunk_count)++;
    }
}
