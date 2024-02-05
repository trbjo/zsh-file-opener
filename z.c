#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_HASH 8179
#define MAX_ENTRIES 10000


const char resetColor[] = "\x1b[0m";
const size_t resetColorLen = sizeof(resetColor) - 1;

const char pathString[] = "/\x1b[36m";
const size_t pathStringLen = sizeof(pathString) - 1;

const char gitString[] = "\x1b[1m";
const size_t gitStringLen = sizeof(gitString) - 1;

const char homePathString[] = "\x1b[36m~\x1b[0m";
const size_t homePathStringLen = sizeof(homePathString) - 1;


int directoryHasGit(const char* path) {
    struct stat sb;
    char gitPath[1024];
    snprintf(gitPath, sizeof(gitPath), "%s/.git", path);
    return stat(gitPath, &sb) == 0 && S_ISDIR(sb.st_mode);
}


unsigned long hashFunction(const char* str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash % MAX_HASH;
}


typedef struct Node {
    unsigned long key;
    char *path;
    char *prettyPath;
    int prettyPathLen;
    struct Node *prev, *next;
    struct Node *lruNext, *lruPrev; // For LRU cache's ordering
    struct Node *parent;
} Node;


Node* createNode(unsigned long hashIndex, char* path, char* prettyPath, Node *parent) {
    Node* node = (Node*) malloc(sizeof(Node));
    node->path = strdup(path);

    node->prettyPath = strdup(prettyPath);
    node->prettyPathLen = strlen(prettyPath);

    node->key = hashIndex;

    node->prev = node->next = NULL;
    node->parent = parent;

    return node;
}

typedef struct {
    Node **array;
    unsigned long capacity;
    int size;
    Node *head, *tail;
    Node *root;
} LRUCache;



LRUCache* createCache(unsigned long capacity) {
    LRUCache *cache = (LRUCache*) malloc(sizeof(LRUCache));
    memset(cache, 0, sizeof(LRUCache));

    cache->capacity = capacity;
    cache->size = 0;

    cache->array = (Node**) malloc(sizeof(Node*) * capacity);
    memset(cache->array, 0, sizeof(Node*) * capacity);

    for (int i = 0; i < capacity; i++) {
        cache->array[i] = NULL;
    }
    cache->head = cache->tail = NULL;
    return cache;
}


void addToFront(LRUCache *cache, Node *node) {
    node->lruNext = cache->head;
    node->lruPrev = NULL;
    if (cache->head != NULL) {
        cache->head->lruPrev = node;
    }
    cache->head = node;
    if (cache->tail == NULL) {
        cache->tail = node;
    }
    cache->size++;
}

void removeFromList(LRUCache *cache, Node *node) {
    if (node->lruPrev != NULL) {
        node->lruPrev->lruNext = node->lruNext;
    } else {
        cache->head = node->lruNext;
    }
    if (node->lruNext != NULL) {
        node->lruNext->lruPrev = node->lruPrev;
    } else {
        cache->tail = node->lruPrev;
    }
    node->lruNext = NULL;
    node->lruPrev = NULL;
    cache->size--;
}

void evictIfNecessary(LRUCache *cache) {
    if (cache->size < cache->capacity) {
        return;
    }
    Node *last = cache->tail;
    removeFromList(cache, last);

    unsigned long hashIndex = last->key;
    if (last->prev != NULL) {
        last->prev->next = last->next;
    } else {
        cache->array[hashIndex] = last->next;
    }
    if (last->next != NULL) {
        last->next->prev = last->prev;
    }

    free(last->path);
    free(last->prettyPath);
    free(last);
}

void put(LRUCache *cache, unsigned long hashIndex, Node* newNode) {
    newNode->next = cache->array[hashIndex];
    if (cache->array[hashIndex] != NULL) {
        cache->array[hashIndex]->prev = newNode;
    }
    cache->array[hashIndex] = newNode;

    addToFront(cache, newNode);
    evictIfNecessary(cache);
}


Node* get(LRUCache *cache, unsigned long hashIndex, char *path) {
    Node *node = cache->array[hashIndex];
    while (node != NULL) {
        if (strcmp(node->path, path) == 0) {
            removeFromList(cache, node);
            addToFront(cache, node);
            return node;
        }
        node = node->next;
    }
    return NULL;
}



Node* formatNode(LRUCache *cache, unsigned long homeHash, char* path) {
    if (!path) return NULL;
    unsigned long hashIndex = hashFunction(path);

    Node* node = get(cache, hashIndex, path);
    if (node != NULL) {
        return node; // The path is already processed and in the cache
    }

    char* lastSlash = strrchr(path, '/');

    // root special case
    if (lastSlash == NULL) {
        node = createNode(hashIndex, path, "", NULL);
        put(cache, hashIndex, node);
        return node;
    }

    char parentPath[256];
    char prettyPath[512];
    unsigned int idx = 0;

    strncpy(parentPath, path, lastSlash - path);
    parentPath[lastSlash - path] = '\0';
    Node* parentNode = formatNode(cache, homeHash, parentPath);

    if (homeHash == hashIndex) {
        if (directoryHasGit(path)) {
            memcpy(prettyPath+idx, gitString, gitStringLen);
            idx += gitStringLen;
        }

        memcpy(prettyPath+idx, homePathString, homePathStringLen);
        idx+=homePathStringLen;

    } else {

        memcpy(prettyPath+idx, parentNode->prettyPath, parentNode->prettyPathLen);
        idx+=parentNode->prettyPathLen;

        memcpy(prettyPath+idx, pathString, pathStringLen);
        idx+=pathStringLen;

        if (directoryHasGit(path)) {
            memcpy(prettyPath+idx, gitString, gitStringLen);
            idx += gitStringLen;
        }

        int lastSlashLength = strlen(lastSlash+1);
        memcpy(prettyPath+idx, lastSlash+1, lastSlashLength);
        idx+=lastSlashLength;

    }

    memcpy(prettyPath+idx, resetColor, resetColorLen);
    idx+=resetColorLen;
    prettyPath[idx] = '\0';

    node = createNode(hashIndex, path, prettyPath, parentNode);
    put(cache, hashIndex, node);

    return node;
}


typedef struct Entry {
    char path[512];
    double score;
} Entry;

int compareEntries(const void* a, const void* b) {
    Entry *entryA = (Entry *)a;
    Entry *entryB = (Entry *)b;
    return (entryB->score - entryA->score); // For descending order
}

int main() {
    char *zPath = getenv("ZSHZ_LOCATION");
    if (zPath == NULL) {
        printf("pleas set ZSHZ_LOCATION in you env\n");
        exit(1);
    }
    char *home = getenv("HOME");
    if (home == NULL) {
        exit(1);
    }

    unsigned long homeHash = hashFunction(home);
    LRUCache* cache = createCache(MAX_HASH);

    FILE *file = fopen(zPath, "r");
    char line[1024];
    Entry entries[MAX_ENTRIES];
    int count = 0;

    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    while (fgets(line, sizeof(line), file) && count < MAX_ENTRIES) {
        char *token = strtok(line, "|");
        if (token != NULL) {
            struct stat sb;

            if (stat(token, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
                continue;
            }

            Node* result = formatNode(cache, homeHash, token);

            strcpy(entries[count].path, result->prettyPath);
            token = strtok(NULL, "|");
        }
        if (token != NULL) {
            entries[count].score = atof(token);
        }
        count++;
    }

    fclose(file);

    qsort(entries, count, sizeof(Entry), compareEntries);

    printf("%s", resetColor);
    for (int i = 0; i < count; i++) {
        printf("%s\n", entries[i].path);
    }

    return 0;
}
