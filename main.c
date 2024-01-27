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
#include <sys/socket.h>
#include <sys/un.h>


#define MAX_ARGS 256 // Define a maximum number of arguments
#define MAX_EXT_LENGTH 10
#define MAX_PATH_LENGTH 1024
#define MAX_INPUTS 1024

void mpv_message(const char *socket_path, const char *path) {
    char message[1024];
    snprintf(message, sizeof(message), "loadfile \"%s\" append\n", path);

    // Create a socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket error");
        exit(1);
    }

    // Set up the address structure
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    // Connect to the socket
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error");
        close(sock);  // Ensure the socket is properly closed
        exit(1);
    }

    // Write the message to the socket
    if (write(sock, message, strlen(message)) == -1) {
        perror("write error");
        close(sock);  // Ensure the socket is properly closed
        exit(1);
    }

    // Close the socket
    close(sock);
}

char* get_basename(char *path) {
    // Find the last occurrence of '/'
    char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        // Return the substring after the last '/'
        return last_slash + 1;
    } else {
        // The path doesn't contain '/', return the original path
        return path;
    }
}

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

        if(strcmp("*", token) == 0) {
            free(ext_dup);
            return 1;
        }

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
    if (strncmp(path, "https://", 8) == 0 || strncmp(path, "http://", 7) == 0 || strncmp(path, "magnet:", 7) == 0 || strncmp(path, "file://", 7) == 0) {
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


int run_cmd(char **cmd_args, int wait) {
    signal(SIGCHLD, wait ? SIG_DFL: SIG_IGN);

    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd_args[0], cmd_args);
        printf("Unknown command: %s\n", cmd_args[0]);
        exit(1);
    } else if (pid > 0) {
        if (wait) {
            int status;
            waitpid(pid, &status, 0); // Wait for the child process to finish
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                // printf("Child exited with status %d\n", exit_status);
                return (exit_status == 0) ? 1 : 0; // Return 1 on success, 0 otherwise
            } else {
                // printf("Child did not exit normally\n");
                return 0; // Return 0 for abnormal exit
            }
        }
    } else {
        perror("fork failed for opener");
        exit(1);
    }
    return 1;
}

void launch_opener_with_files(char **command, char **file_paths, int wait) {
    int arg_count;
    for (arg_count = 0; command[arg_count] != NULL; arg_count++); // Count the number of opener arguments

    // Count the number of file paths
    int file_count;
    for (file_count = 0; file_paths[file_count] != NULL; file_count++);

    // char **args = malloc((arg_count + file_count + 1) * sizeof(char*)); // +1 for the NULL terminator
    char *args[MAX_ARGS + 1]; // +1 for the NULL terminator

    for (int i = 0; i < arg_count; i++) {
        args[i] = command[i];
    }

    // Add file paths to args
    for (int i = 0; i < file_count; i++) {
        args[arg_count + i] = file_paths[i];
    }

    args[arg_count + file_count] = NULL; // NULL-terminate the arguments array
    run_cmd(args, wait);
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
        close(dev_null_fd);
    }

    int argv_input_count = 0, stdin_input_count = 0;
    char **argv_inputs = process_argv(argc, argv, &argv_input_count);
    char **stdin_inputs = NULL;
    if (!isatty(STDIN_FILENO)) {
        stdin_inputs = process_stdin(&stdin_input_count);
    }

    int total_count = argv_input_count + stdin_input_count;
    char **inputs = (char **)malloc(total_count * sizeof(char *));
    if (!inputs) {
        perror("Failed to allocate memory for inputs");
        return 1;
    }

    // Copy argv inputs
    for (int i = 0; i < argv_input_count; i++) {
        inputs[i] = argv_inputs[i];
    }

    // Copy stdin inputs
    for (int i = 0; i < stdin_input_count; i++) {
        inputs[argv_input_count + i] = stdin_inputs[i];
    }

    // Define your opener environment variables
    const char *exclude_suffixes = getenv("_FILE_OPENER_EXCLUDE_SUFFIXES") ? : "";
    char *disabled_files[total_count];


    const char *multimedia_formats = getenv("_FILE_OPENER_MULTIMEDIA_FORMATS") ? : "";
    char *multimedia_command[] = {"mpv", NULL};
    Opener multimedia_opener = {multimedia_command, "[app_id=^mpv$]"};
    char *multimedia_files[total_count];

    const char *book_formats = getenv("_FILE_OPENER_BOOK_FORMATS") ? : "";
    char *book_command[] = {"/usr/bin/zathura", NULL};
    Opener book_opener = {book_command, "[app_id=^org.pwmt.zathura$]"};
    char *book_files[total_count];

    const char *picture_formats = getenv("_FILE_OPENER_PICTURE_FORMATS") ? : "";
    char *picture_command[] = {"eog", NULL};
    Opener picture_opener = {picture_command, "[app_id=^eog$]"};
    char *picture_files[total_count];

    const char *libreoffice_formats = getenv("_FILE_OPENER_LIBREOFFICE_FORMATS") ? : "";
    char *libreoffice_command[] = {"/usr/bin/libreoffice", "--norestore", NULL};
    Opener libreoffice_opener = {libreoffice_command, "[app_id=^libreoffice$]"};
    char *libreoffice_files[total_count];

    const char *web_formats = getenv("_FILE_OPENER_WEB_FORMATS") ? : "";
    char *firefox_command[] = {"firefox", NULL};
    Opener firefox_opener = {firefox_command, "[app_id=^firefox$]"};
    char *web_files[total_count];
    char *url_files[total_count];

    const char *archive_formats = getenv("_FILE_OPENER_ARCHIVE_FORMATS") ? : "";


    char *sublime_command[] = {"/opt/sublime_text/sublime_text", NULL};
    Opener sublime_opener = {sublime_command, "[app_id=^sublime_text$]"};
    char *other_files[total_count];


    char *magnet_files[total_count];


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


    int multimedia_count = 0, book_count = 0, picture_count = 0, other_count = 0, web_count = 0, url_count = 0, disabled_count = 0, magnet_count = 0;

    for (int i = 0; i < total_count; i++) {
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
            continue;
        }

        if (strncmp(input, "https://", 8) == 0 || strncmp(input, "http://", 7) == 0) {
            url_files[url_count++] = strdup(input);
            continue;
        }

        if (strncmp(input, "magnet:", 7) == 0) {
            magnet_files[magnet_count++] = strdup(input);
            continue;
        }

        const char *ext = get_file_extension(input);

        if(is_extension_in_list(ext, multimedia_formats)) {
            multimedia_files[multimedia_count++] = input;
        } else if(is_extension_in_list(ext, book_formats)) {
            book_files[book_count++] = input;
        } else if(is_extension_in_list(ext, web_formats)) {
            web_files[web_count++] = input;
        } else if(is_extension_in_list(ext, picture_formats)) {
            picture_files[picture_count++] = input;
        } else {

            if(is_extension_in_list(ext, exclude_suffixes)) {
                disabled_files[disabled_count++] = input;
                if (debug_mode) {
                    fprintf(stderr, "Skipping excluded file type: %s\n", input);
                }
                continue;
            }

            other_files[other_count++] = input;
        }

    }
    for (int i = 0; i < disabled_count; i++) {
        fprintf(stderr, "%s\n", disabled_files[i]);
    }

    if (!debug_mode) {
        int dev_null_fd = open("/dev/null", O_WRONLY);
        if (dev_null_fd == -1) {
            perror("Failed to open /dev/null");
            return 1;
        }
        dup2(dev_null_fd, STDERR_FILENO);
        close(dev_null_fd);
    }

    magnet_files[magnet_count] = NULL;
    if(magnet_count > 0) {
        char *torrent_command[] = {"transmission.sh", NULL};
        launch_opener_with_files(torrent_command, magnet_files, debug_mode);
    }


    multimedia_files[multimedia_count] = NULL;
    if (multimedia_count > 0) {
        char *swaymsg[] = {"swaymsg", multimedia_opener.criteria, "focus", NULL};
        if (run_cmd(swaymsg, 1)) {
            const char *socket_path = "/tmp/mpvsocket";
            for (int i = 0; i < multimedia_count; i++) {
                mpv_message(socket_path, multimedia_files[i]);
            }
        } else {
            launch_opener_with_files(multimedia_opener.command, multimedia_files, debug_mode);
        }
    }

    for (int i = 0; i < multimedia_count; i++) {
        free(multimedia_files[i]);
    }

    book_files[book_count] = NULL;
    if (book_count > 0) {
        for (int i = 0; i < book_count; i++) {
            char *book = book_files[i];
            char *book_basename = get_basename(book);

            char criteria[128] = "";
            snprintf(criteria, 128, "[app_id=\"^org.pwmt.zathura$\" title=\"^%s \"]", book_basename);
            char *swaymsg[] = {"swaymsg", criteria, "focus", NULL};

            if (!run_cmd(swaymsg, 1)) {
                char *book_a[] = {book, NULL};
                launch_opener_with_files(book_opener.command, book_a, debug_mode);
            }
        }
    }
    for (int i = 0; i < book_count; i++) {
        free(book_files[i]);
    }

    web_files[web_count] = NULL;
    if (web_count > 0) {
        char *swaymsg[] = {"swaymsg", firefox_opener.criteria, "focus", NULL};
        run_cmd(swaymsg, 0);
        launch_opener_with_files(firefox_opener.command, web_files, debug_mode);
    }
    for (int i = 0; i < web_count; i++) {
        free(web_files[i]);
    }

    url_files[url_count] = NULL;
    if (url_count > 0) {
        char *swaymsg[] = {"swaymsg", firefox_opener.criteria, "focus", NULL};
        run_cmd(swaymsg, 0);
        launch_opener_with_files(firefox_opener.command, url_files, debug_mode);
    }
    for (int i = 0; i < url_count; i++) {
        free(url_files[i]);
    }

    picture_files[picture_count] = NULL;
    if (picture_count > 0) launch_opener_with_files(picture_opener.command, picture_files, debug_mode);
    for (int i = 0; i < picture_count; i++) {
        free(picture_files[i]);
    }

    other_files[other_count] = NULL;
    if (other_count > 0) {
        char *swaymsg[] = {"swaymsg", sublime_opener.criteria, "focus", NULL};
        if (!run_cmd(swaymsg, 1)) {
            const char *json_string = "{\"app_id\": \"sublime_text\", \"rules\": [\"move to workspace 2\", \"focus\", \"mark --add ctrl+2__dynamic_focus__\"]}";
            char *launch[] = {"swaymsg", "-t" "send_tick", (char *)json_string, NULL};
            run_cmd(launch, 1);
        }
        launch_opener_with_files(sublime_opener.command, other_files, debug_mode);
    }
    for (int i = 0; i < other_count; i++) {
        free(other_files[i]);
    }

    for (int i = 0; i < total_count; i++) {
        free(inputs[i]);
    }
    free(inputs);
    return disabled_count;
}
