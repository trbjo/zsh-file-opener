#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

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

int main() {
    char inputPath[1024];
    if (getcwd(inputPath, sizeof(inputPath)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    char* home = getenv("HOME");
    if (home == NULL) {
        abort();
    }

    char outputPath[1024];
    unsigned int offset = 0;
    if (strncmp(inputPath, home, strlen(home)) == 0) {
        offset = strlen(home);
        sprintf(outputPath, homePathString);
    }
    colorizePath(inputPath, outputPath, offset);
    printf(outputPath);
    return 0;
}
