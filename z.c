#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_ENTRIES 10000

const char resetColor[] = "\x1b[0m";
const size_t resetColorLen = sizeof(resetColor) - 1;

const char pathString[] = "\x1b[0m/\x1b[0;36m";
const size_t pathStringLen = sizeof(pathString) - 1;

const char homePathString[] = "\x1b[36m~\x1b[0m";
const size_t homePathStringLen = sizeof(homePathString) - 1;

int directoryHasGit(const char* path) {
    struct stat sb;
    char gitPath[1024];
    snprintf(gitPath, sizeof(gitPath), "%s/.git", path);
    return stat(gitPath, &sb) == 0 && S_ISDIR(sb.st_mode);
}

void colorizePath(const char* inputPath, char* outputPath, unsigned int i) {
    unsigned int o = i;
    int dirstart = 0;

    for (; inputPath[i] != '\0'; ++i) {
        if (inputPath[i] == '/') {

            char substring[128];
            strncpy(substring, inputPath, i);
            substring[i] = '\0';
            if (directoryHasGit(substring)) {
                outputPath[o-dirstart-5] = '1';
            }
            memcpy(outputPath + o, pathString, pathStringLen);
            o += pathStringLen;

            dirstart = 0;
        } else {
            dirstart++;
            outputPath[o++] = inputPath[i];
        }
    }

    char substring[128];
    strncpy(substring, inputPath, i);
    substring[i] = '\0';
    if (directoryHasGit(substring)) {
        outputPath[o-dirstart-5] = '1';
    }

    memcpy(outputPath + o, resetColor, resetColorLen);
    o += resetColorLen;
    outputPath[o] = '\0';
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

    FILE *file = fopen(zPath, "r");
    char line[256];
    Entry entries[MAX_ENTRIES]; // Assuming we have at most 100 entries
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

            // Replace $HOME with ~
            char outputPath[1024];
            unsigned int offset = 0;
            if (strncmp(token, home, strlen(home)) == 0) {
                offset = strlen(home);
                sprintf(outputPath, homePathString);
            }
            colorizePath(token, outputPath, offset);

            strcpy(entries[count].path, outputPath);
            token = strtok(NULL, "|");
        }
        if (token != NULL) {
            entries[count].score = atof(token);
        }
        count++;
    }

    fclose(file);

    // Sorting the entries by score in descending order
    qsort(entries, count, sizeof(Entry), compareEntries);

    // Printing the sorted entries
    for (int i = 0; i < count; i++) {
        printf("%s\n", entries[i].path);
    }

    return 0;
}
