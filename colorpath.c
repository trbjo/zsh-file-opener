#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


#define MAX_PATH_DEPTH 64
#define MAX_STR_LENGTH 1024

const char resetColor[] = "\x1b[0m";
const size_t resetColorLen = sizeof(resetColor) - 1;

const char pathString[] = "\x1b[0m/\x1b[36m";
const size_t pathStringLen = sizeof(pathString) - 1;

const char gitpathString[] = "\x1b[0m/\x1b[1;36m";
const size_t gitpathStringLen = sizeof(gitpathString) - 1;

const char Nbranch[] = "\x1b[0;32m";
const char Obranch[] = "\x1b[0;34m";
const size_t branchLen = sizeof(Obranch) - 1;

const char homePathString[] = "\x1b[36m~";
const size_t homePathStringLen = sizeof(homePathString) - 1;

const int RECENT_FETCH = 60;


static int getCurrentGitBranchOrCommit(const char* path, char* result) {
    char headFilePath[MAX_STR_LENGTH];
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



int main(int argc, char **argv) {
    char* pwd = getenv("PWD");
    if (pwd == NULL) {
        exit(1);
    }
    char* home = getenv("HOME");
    if (home == NULL) {
        exit(1);
    }

    int gitpath = 0;
    if (argc > 1 && strcmp(argv[1], "--git-path") == 0) {
        gitpath = 1;
        argc--;
        argv++;
    }


    unsigned int written = 0;
    char outputPath[1024];

    unsigned int pathOffset = 0;
    int homelength=strlen(home);

    if (strncmp(pwd, home, homelength) == 0) {
        pathOffset += homelength;
        memcpy(outputPath, homePathString, homePathStringLen);
        written+=homePathStringLen;
    }

    size_t hasGit[MAX_PATH_DEPTH];
    for (int i = 0; i < MAX_PATH_DEPTH; i++) {
        hasGit[i] = 0;
    }

    char directories[MAX_PATH_DEPTH][MAX_STR_LENGTH];
    char substring[1024];
    int lastGit = 0;
    int gitLength = 0;
    char gitBranch[64];
    int lastindex = 0;

    size_t depth = 0;
    for (size_t i=pathOffset, j=1; pwd[i] != '\0'; i++, j++) {
        if (pwd[i+1] != '/' && pwd[i+1] != '\0')
            continue;

        memcpy(directories[depth], pwd+i-j+2, j-1);
        directories[depth][j] = '\0';
        j=0; // reset path separator
        memcpy(substring, pwd, i+1);
        substring[i+1] = '\0';
        gitLength = getCurrentGitBranchOrCommit(substring, gitBranch);
        if (gitLength) {
            hasGit[depth] = 1;
            lastindex = i+1;
            lastGit = gitLength;
        }
        depth++;
    }

    if (gitpath) {
        write(1, pwd, lastindex);
        if (lastindex)
            return 0;
        return 1;
    }

    for (size_t j = 0; j < depth; j++) {
        if (hasGit[j]) {
            memcpy(outputPath + written, gitpathString, gitpathStringLen);
            written+=gitpathStringLen;
        } else {
            memcpy(outputPath + written, pathString, pathStringLen);
            written+=pathStringLen;
        }
        written+=snprintf(outputPath + written, MAX_STR_LENGTH - written, directories[j]);

    }

    if (!lastGit) {
        memcpy(outputPath + written, resetColor, resetColorLen);
        written += resetColorLen;
        write(1, outputPath, written);
        return 0;
    }

    uint absLastGit = abs(lastGit);
    outputPath[written++] = ' ';

    if (lastGit > 0) {
        memcpy(outputPath + written, Nbranch, branchLen);
    } else {
        memcpy(outputPath + written, Obranch, branchLen);
    }
    written += branchLen;

    memcpy(outputPath + written, gitBranch, absLastGit);
    written += absLastGit;

    memcpy(outputPath + written, resetColor, resetColorLen);
    written += resetColorLen;

    write(1, outputPath, written);
    return 0;
}
