#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>


#define MAX_EXT_LENGTH 10
#define MAX_PATH_LENGTH 1024
#define MAX_INPUTS 1024

// Function to get the file extension
const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

// Function to check if the extension is in the list
int is_extension_in_list(const char *ext, const char *list) {
    char *ext_dup = strdup(list);
    char *token = strtok(ext_dup, ",");
    while(token != NULL) {
        if(strcmp(ext, token) == 0) {
            free(ext_dup);
            return 1;
        }
        token = strtok(NULL, ",");
    }
    free(ext_dup);
    return 0;
}


// Function to expand tilde to HOME
char* expand_tilde(const char *path) {
    if(path[0] == '~') {
        const char *home_dir = NULL;

        if(path[1] == '/' || path[1] == '\0') {
            // If the path is just "~" or "~/...", use the HOME environment variable
            home_dir = getenv("HOME");
            if (!home_dir) {
                struct passwd *pwd = getpwuid(getuid());
                if (pwd) {
                    home_dir = pwd->pw_dir;
                } else {
                    perror("Couldn't find home directory");
                    return NULL;
                }
            }
        } else {
            // If the path is "~username/...", get the home directory of "username"
            const char *username = path + 1;
            const char *slash = strchr(username, '/');
            if (slash) {
                char *user_only = strndup(username, slash - username);
                struct passwd *pwd = getpwnam(user_only);
                free(user_only);
                if (pwd) {
                    home_dir = pwd->pw_dir;
                } else {
                    perror("Couldn't find user's home directory");
                    return NULL;
                }
            }
        }

        if (home_dir) {
            char *expanded_path = malloc(MAX_PATH_LENGTH);
            if (path[1] == '/' || path[1] == '\0') {
                snprintf(expanded_path, MAX_PATH_LENGTH, "%s%s", home_dir, path + 1);
            } else {
                snprintf(expanded_path, MAX_PATH_LENGTH, "%s%s", home_dir, strchr(path, '/'));
            }
            return expanded_path;
        }
    }
    return strdup(path); // No tilde, just return a copy of the path
}


// Modified function to build absolute path or URL
char* build_absolute_path_or_url(const char *path) {
    // Check if path is a URL
    if (strncmp(path, "https://", 8) == 0 || strncmp(path, "http://", 7) == 0 || strncmp(path, "file://", 7) == 0) {
        char *url = strdup(path);
        return url; // Return the URL as-is
    }

    char *expanded_path = expand_tilde(path);
    if (!expanded_path) {
        return NULL;
    }

    char *abs_path = malloc(MAX_PATH_LENGTH);
    if(expanded_path[0] == '/') {
        strcpy(abs_path, expanded_path);
    } else {
        char cwd[MAX_PATH_LENGTH];
        if(getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(abs_path, MAX_PATH_LENGTH, "%s/%s", cwd, expanded_path);
        } else {
            perror("getcwd() error");
            free(expanded_path);
            return NULL;
        }
    }

    free(expanded_path);
    return abs_path;
}

typedef struct {
    char **command; // The command and its arguments
    char *criteria; // The criteria for swaymsg
} Opener;


void launch_opener_with_files(Opener opener, char **file_paths) {

    // Construct the swaymsg argument string
    char sway_arg[128]; // Ensure this is large enough for the expected strings
    snprintf(sway_arg, sizeof(sway_arg), "%s focus", opener.criteria);

    char *sway[] = {"swaymsg", sway_arg, NULL};

    // Launch swaymsg to focus the app based on the criteria
    pid_t pid_sway = fork();
    if (pid_sway == 0) {
        execvp(sway[0], sway);
        printf("Unknown command: %s\n", sway[0]);
        exit(1);
    } else if (pid_sway > 0) {
        wait(NULL);  // Optionally wait for swaymsg to complete
    } else {
        perror("fork failed for swaymsg");
        exit(1);
    }





    // Prepare to launch the actual opener
    int arg_count;
    for (arg_count = 0; opener.command[arg_count] != NULL; arg_count++); // Count the number of opener arguments

    // Count the number of file paths
    int file_count;
    for (file_count = 0; file_paths[file_count] != NULL; file_count++);

    char **args = malloc((arg_count + file_count + 1) * sizeof(char*)); // +1 for the NULL terminator

    // Copy opener arguments to args
    for (int i = 0; i < arg_count; i++) {
        args[i] = opener.command[i];
    }

    // Add file paths to args
    for (int i = 0; i < file_count; i++) {
        args[arg_count + i] = file_paths[i];
    }

    args[arg_count + file_count] = NULL; // NULL-terminate the arguments array

    // Launch the actual opener
    pid_t pid_opener = fork();
    if (pid_opener == 0) {
        execvp(opener.command[0], args);
        printf("Unknown command: %s\n", opener.command[0]);
        exit(1);
    } else if (pid_opener > 0) {
        // Optionally, handle or ignore the SIGCHLD signal here
    } else {
        perror("fork failed for opener");
        exit(1);
    }

    free(args); // Free the arguments array

}

char** process_argv(int argc, char **argv, int *count);
char** process_stdin(int *count);


char** process_argv(int argc, char **argv, int *count) {
    char **inputs = (char**)malloc(MAX_INPUTS * sizeof(char*));
    *count = 0; // Initialize count to 0

    // Start from i = 1 to skip the program name
    for (int i = 1; i < argc && *count < MAX_INPUTS; i++) {
        inputs[*count] = strdup(argv[i]); // Duplicate and store the input
        (*count)++;
    }

    return inputs;
}

char** process_stdin(int *count) {
    char **inputs = (char**)malloc(MAX_INPUTS * sizeof(char*));
    *count = 0; // Initialize count to 0

    char line[MAX_PATH_LENGTH];
    while (fgets(line, sizeof(line), stdin) && *count < MAX_INPUTS) {
        line[strcspn(line, "\n")] = 0; // Remove newline character
        inputs[*count] = strdup(line); // Duplicate and store the input
        (*count)++;
    }

    return inputs;
}

int main(int argc, char **argv) {
    signal(SIGCHLD, SIG_IGN); // Ignore SIGCHLD
    int debug_mode = 0;

    // Check if the first argument is --debug
    if (argc > 1 && strcmp(argv[1], "--debug") == 0) {
        debug_mode = 1;
        // Shift the argv array and decrease argc
        argc--;
        argv++;
    } else {
        // Redirect stdout and stderr to /dev/null
        int dev_null_fd = open("/dev/null", O_WRONLY);
        if (dev_null_fd == -1) {
            perror("Failed to open /dev/null");
            return 1;
        }
        dup2(dev_null_fd, STDOUT_FILENO);
        dup2(dev_null_fd, STDERR_FILENO);
        close(dev_null_fd);
    }

    // if(argc < 2) {
        // printf("Usage: %s <file1> <file2> ...\n", argv[0]);
        // return 1;
    // }

    char **inputs;
    int input_count = 0;
    if (isatty(STDIN_FILENO)) {
        inputs = process_argv(argc, argv, &input_count);
    } else {
        inputs = process_stdin(&input_count);
    }


    // Define your opener environment variables
    const char *exclude_suffixes = getenv("_FILE_OPENER_EXCLUDE_SUFFIXES") ? : "";


    const char *multimedia_formats = getenv("_FILE_OPENER_MULTIMEDIA_FORMATS") ? : "";
    char *multimedia_command[] = {"mpv", NULL};
    Opener multimedia_opener = {multimedia_command, "[app_id=^mpv$]"};
    char *multimedia_files[argc];

    const char *book_formats = getenv("_FILE_OPENER_BOOK_FORMATS") ? : "";
    char *book_command[] = {"zathura", NULL};
    Opener book_opener = {book_command, "[app_id=^org.pwmt.zathura$]"};
    char *book_files[argc];

    const char *picture_formats = getenv("_FILE_OPENER_PICTURE_FORMATS") ? : "";
    char *picture_command[] = {"eog", NULL};
    Opener picture_opener = {picture_command, "[app_id=^eog$]"};
    char *picture_files[argc];

    const char *libreoffice_formats = getenv("_FILE_OPENER_LIBREOFFICE_FORMATS") ? : "";
    char *libreoffice_command[] = {"/usr/bin/libreoffice", "--norestore", NULL};
    Opener libreoffice_opener = {libreoffice_command, "[app_id=^libreoffice$]"};
    char *libreoffice_files[argc];

    const char *web_formats = getenv("_FILE_OPENER_WEB_FORMATS") ? : "";
    char *firefox_command[] = {"firefox", NULL};
    Opener firefox_opener = {firefox_command, "[app_id=^firefox$]"};
    char *web_files[argc];
    char *url_files[argc];

    const char *archive_formats = getenv("_FILE_OPENER_ARCHIVE_FORMATS") ? : "";


    char *sublime_command[] = {"/opt/sublime_text/sublime_text", NULL};
    Opener sublime_opener = {sublime_command, "[app_id=^sublime_text$]"};
    char *other_files[argc];




    // Handle the EDITOR environment variable
    char *editor_env = getenv("EDITOR");
    char *editor_opener[10]; // Assuming the max number of arguments won't exceed 10
    int editor_arg_count = 0;
    if (editor_env != NULL) {
        char *editor_env_dup = strdup(editor_env); // Duplicate the string for strtok
        char *token = strtok(editor_env_dup, " ");
        while (token != NULL && editor_arg_count < 9) {
            editor_opener[editor_arg_count++] = token;
            token = strtok(NULL, " ");
        }
        editor_opener[editor_arg_count] = NULL; // NULL-terminate the array
    } else {
        printf("No default editor set. Cannot open text files.\n");
    }


    int multimedia_count = 0, book_count = 0, picture_count = 0, other_count = 0, web_count = 0, url_count = 0;

    for (int i = 0; i < input_count; i++) {
        if (!inputs[i] || inputs[i][0] == '\0') {
            continue;
        }

        char *input = build_absolute_path_or_url(inputs[i]);

        if (debug_mode) {
            fprintf(stdout, "file: '%s', input: '%s'\n", input, inputs[i]);
        }

        if(!input) {
            continue;
        }


        if (strncmp(input, "file://", 7) == 0) {
            // Handle file:// URLs
            char *local_path = input + 7; // Skip the "file://" part
            char *host_end = strchr(local_path, '/'); // Find the start of the actual path
            if (host_end) {
                // If there's a host part, skip it. Otherwise, local_path is already the path
                local_path = host_end;
            }
            const char *ext = get_file_extension(local_path);
            other_files[other_count++] = strdup(local_path); // Duplicate the path if you're going to free input later

        } else if (strncmp(input, "https://", 8) == 0 || strncmp(input, "http://", 7) == 0) {
            url_files[url_count++] = strdup(input); // Duplicate the URL if you're going to free input later
        } else {


            const char *ext = get_file_extension(input);
            if(is_extension_in_list(ext, exclude_suffixes)) {

                if (debug_mode) {
                    fprintf(stderr, "Skipping excluded file type: %s\n", input);
                }

                free(input);
                continue; // Skip this file if the extension is in the exclude list
            }

            // Sort files into corresponding arrays based on the file extension
            if(is_extension_in_list(ext, multimedia_formats)) {
                multimedia_files[multimedia_count++] = input;
            } else if(is_extension_in_list(ext, book_formats)) {
                book_files[book_count++] = input;
            } else if(is_extension_in_list(ext, web_formats)) {
                web_files[web_count++] = input;
            } else if(is_extension_in_list(ext, picture_formats)) {
                picture_files[picture_count++] = input;
            } //... (Handle other file types)
            else {
                other_files[other_count++] = input;
            }
        }

    }

    multimedia_files[multimedia_count] = NULL;
    if (multimedia_count > 0) launch_opener_with_files(multimedia_opener, multimedia_files);

    book_files[book_count] = NULL;
    if (book_count > 0) launch_opener_with_files(book_opener, book_files);

    web_files[web_count] = NULL;
    if (web_count > 0) launch_opener_with_files(firefox_opener, web_files);

    url_files[url_count] = NULL;
    if (url_count > 0) launch_opener_with_files(firefox_opener, url_files);

    picture_files[picture_count] = NULL;
    if (picture_count > 0) launch_opener_with_files(picture_opener, picture_files);

    other_files[other_count] = NULL;
    if (other_count > 0) launch_opener_with_files(sublime_opener, other_files); // Assuming editor_opener for files not matched by any type

    // Free the allocated file paths
    for (int i = 0; i < other_count; i++) {
        free(other_files[i]);
    }


    // Free the allocated memory for inputs
    for (int i = 0; i < input_count; i++) {
        free(inputs[i]);
    }
    free(inputs);

    //... (Free other allocated file paths)

    return 0;
}
