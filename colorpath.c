#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

const char resetColor[] = "\x1b[0m";
const size_t resetColorLen = sizeof(resetColor) - 1;

const char pathString[] = "\x1b[0m/\x1b[0;36m";
const size_t pathStringLen = sizeof(pathString) - 1;

const char Nbranch[] = "\x1b[0;32m";
const char Obranch[] = "\x1b[0;34m";
const size_t branchLen = sizeof(Obranch) - 1;

const char homePathString[] = "\x1b[36m~\x1b[0m";
const size_t homePathStringLen = sizeof(homePathString) - 1;

const int RECENT_FETCH = 60;

int getCurrentGitBranchOrCommit(const char* path, char* result) {
    char headFilePath[1024];
    snprintf(headFilePath, sizeof(headFilePath), "%s/.git/HEAD", path);
    FILE* file = fopen(headFilePath, "r");
    if (file == NULL) {
        return 0;
    }

    char line[256];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }
    fclose(file);
    int factor;

    // Check the last git fetch time
    struct stat fileInfo;
    char fetchHeadPath[1024];
    snprintf(fetchHeadPath, sizeof(fetchHeadPath), "%s/.git/FETCH_HEAD", path);
    if (stat(fetchHeadPath, &fileInfo) == 0) {
        time_t currentTime;
        time(&currentTime);
        double seconds = difftime(currentTime, fileInfo.st_mtime);

        if (fileInfo.st_size > 0 && seconds < RECENT_FETCH) {
            factor = 1;
        } else {
            factor = -1;
        }
    } else {
        factor = 1;
    }

    // Check if line starts with "ref: "
    if (strncmp(line, "ref: ", 5) == 0) {

        // Extract branch name
        char* branch = strrchr(line, '/') + 1; // Find the last '/' to get the branch name

        int length = strlen(branch) - 1; // remove newline
        memcpy(result, branch, length);
        return factor * length;
    } else {
        memcpy(result, line, 8);
        return factor * 8;
    }
}

void colorizePath(const char* inputPath, char* outputPath, unsigned int i) {
    unsigned int o = i;
    int dirstart = 0;
    char gitBranch[64];
    int lastGit = 0;

    for (; inputPath[i] != '\0'; ++i) {
        if (inputPath[i] == '/') {

            char substring[128];
            strncpy(substring, inputPath, i);
            substring[i] = '\0';
            int gitLength = getCurrentGitBranchOrCommit(substring, gitBranch);
            if (gitLength) {
                lastGit = gitLength;
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
    int gitLength = getCurrentGitBranchOrCommit(substring, gitBranch);
    if (gitLength) {
        lastGit = gitLength;
        outputPath[o-dirstart-5] = '1';
    }

    if (lastGit > 0) {
        outputPath[o++] = ' ';
        memcpy(outputPath + o, Nbranch, branchLen);
        o += branchLen;
        memcpy(outputPath + o, gitBranch, lastGit);
        o += lastGit;
    } else if (lastGit < 0) {
        outputPath[o++] = ' ';
        uint absLastGit = abs(lastGit);
        memcpy(outputPath + o, Obranch, branchLen);
        o += branchLen;
        memcpy(outputPath + o, gitBranch, absLastGit);
        o += absLastGit;
    }

    memcpy(outputPath + o, resetColor, resetColorLen);
    o += resetColorLen;
    outputPath[o] = '\0';
}

int main() {
    char* pwd = getenv("PWD");
    if (pwd == NULL) {
        exit(1);
    }
    char* home = getenv("HOME");
    if (home == NULL) {
        exit(1);
    }

    char outputPath[1024];
    unsigned int offset = 0;
    if (strncmp(pwd, home, strlen(home)) == 0) {
        offset = strlen(home);
        sprintf(outputPath, homePathString);
    }
    colorizePath(pwd, outputPath, offset);
    printf(outputPath);
    return 0;
}
