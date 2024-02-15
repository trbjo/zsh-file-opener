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
#include <ctype.h>
#include <sys/stat.h>

#define MAX_ARGS 256
#define MAX_EXT_LENGTH 10
#define MAX_PATH_LENGTH 1024
#define MAX_INPUTS 1024
#define ERROR_RETURN 128

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


const char *get_file_extension(const char *fullname) {
    const char *filename_with_slash = strrchr(fullname, '/');
    if (!filename_with_slash) return "";
    int length = strlen(filename_with_slash);
    if (length < 2) {
        return "";
    }

    char *filename = strdup(filename_with_slash + 1); // remove possibly leading .
    if (!filename) return ""; // Check strdup succeeded

    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) { // Check dot exists and is not the first character
        free(filename);
        return "";
    }

    char *extension = strdup(dot + 1); // Duplicate the extension part
    free(filename); // Now we can free filename safely

    if (!extension) return ""; // Check strdup succeeded
    return extension;
}


// Function to convert a string to lower case
char* str_to_lower(const char* str) {
    if (str == NULL) return NULL;
    char* lower_str = strdup(str);
    for (int i = 0; lower_str[i]; i++) {
        lower_str[i] = tolower(lower_str[i]);
    }
    return lower_str;
}

// Function to check if the extension is in the list
int is_extension_in_list(const char *ext, const char *list) {
    if (ext == NULL || list == NULL) return 0;

    // Convert ext to lower case
    char *lower_ext = str_to_lower(ext);
    if (lower_ext == NULL) return 0;

    // Duplicate list for strtok
    char *ext_dup = strdup(list);
    if (ext_dup == NULL) {
        free(lower_ext);
        return 0;
    }

    char *token = strtok(ext_dup, ",");
    while (token != NULL) {
        // Convert token to lower case
        char *lower_token = str_to_lower(token);
        if (lower_token == NULL) continue;

        if (strcmp("*", lower_token) == 0 || strcmp(lower_ext, lower_token) == 0) {
            free(lower_ext);
            free(lower_token);
            free(ext_dup);
            return 1;
        }

        free(lower_token);
        token = strtok(NULL, ",");
    }

    free(lower_ext);
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
    char **criteria; // The criteria for swaymsg
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


int run_sway(char **cmd_args) {
    if (getenv("WAYLAND_DISPLAY") == NULL) {
        return 1;
    }

    int arg_count;
    for (arg_count = 0; cmd_args[arg_count] != NULL; arg_count++); // Count the number of opener arguments

    char *swaymsg[arg_count + 3]; // +1 for "swaymsg" and +1 for NULL terminator
    swaymsg[0] = "swaymsg";
    swaymsg[1] = "-q";

    for (int i = 0; i < arg_count; i++) {
        swaymsg[i + 2] = cmd_args[i];
    }
    swaymsg[arg_count + 2] = NULL; // Correct index for NULL terminator
    return run_cmd(swaymsg, 1); // Assume this function runs the command
}

void launch_opener_with_files(char **command, char **file_paths, int wait) {
    int arg_count;
    for (arg_count = 0; command[arg_count] != NULL; arg_count++); // Count the number of opener arguments

    // Count the number of file paths
    int file_count;
    for (file_count = 0; file_paths[file_count] != NULL; file_count++);

    // char **args = malloc((arg_count + file_count + 1) * sizeof(char*)); // +1 for the NULL terminator
    char *args[MAX_ARGS + 1];

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


void replace_percent20_with_space(char *str) {
    char *read_ptr = str;
    char *write_ptr = str;

    // Loop through the string
    while (*read_ptr != '\0') {
        if (strncmp(read_ptr, "%20", 3) == 0) {  // Check for "%20"
            *write_ptr++ = ' '; // Replace with space
            read_ptr += 3; // Move past "%20"
        } else {
            *write_ptr++ = *read_ptr++; // Copy other characters
        }
    }
    *write_ptr = '\0'; // Null-terminate the modified string
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


int is_directory(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        // perror("stat"); // Handle error, e.g., file doesn't exist or no access
        return -1; // Indicate error
    }

    return S_ISDIR(path_stat.st_mode);
}

int main(int argc, char **argv) {
    int attach_mode = 0;
    int only_files = 0;
    int error_return = 0;

    if (getenv("WAYLAND_DISPLAY") == NULL) {
        attach_mode = 1;
    }

    // Check if the first argument is --attach
    if (argc > 1 && strcmp(argv[1], "--attach") == 0) {
        attach_mode = 1;
        // Shift the argv array and decrease argc
        argc--;
        argv++;
    }

    if (argc > 1 && strcmp(argv[1], "--only-files") == 0) {
        only_files = 1;
        // Shift the argv array and decrease argc
        argc--;
        argv++;
    }

    if (argc > 1 && strcmp(argv[1], "--attach") == 0) {
        only_files = 1;
        // Shift the argv array and decrease argc
        argc--;
        argv++;
    }

    if (argc > 1 && strcmp(argv[1], "--only-files") == 0) {
        only_files = 1;
        // Shift the argv array and decrease argc
        argc--;
        argv++;
    }

    if (!attach_mode) {
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

    if (total_count == 0)
        return ERROR_RETURN;

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
    char *multimedia_criteria[] = {"[app_id=^mpv$]", "focus", NULL};
    Opener multimedia_opener = {multimedia_command, multimedia_criteria};
    char *multimedia_files[total_count];

    const char *book_formats = getenv("_FILE_OPENER_BOOK_FORMATS") ? : "";
    char *book_command[] = {"/usr/bin/zathura", NULL};
    char *book_criteria[] = {"[app_id=^org.pwmt.zathura$]", "focus", NULL};
    Opener book_opener = {book_command, book_criteria};
    char *book_files[total_count];

    const char *picture_formats = getenv("_FILE_OPENER_PICTURE_FORMATS") ? : "";
    char *picture_command[] = {"eog", NULL};
    char *picture_criteria[] = {"[app_id=^eog$]", "focus", NULL};
    Opener picture_opener = {picture_command, picture_criteria};
    char *picture_files[total_count];

    const char *libreoffice_formats = getenv("_FILE_OPENER_LIBREOFFICE_FORMATS") ? : "";
    char *libreoffice_command[] = {"/usr/bin/libreoffice", "--norestore", NULL};
    char *libreoffice_criteria[] = {"[app_id=^libreoffice$]", "focus", NULL};
    Opener libreoffice_opener = {libreoffice_command, libreoffice_criteria};
    char *libreoffice_files[total_count];

    const char *web_formats = getenv("_FILE_OPENER_WEB_FORMATS") ? : "";
    char *firefox_command[] = {"firefox", NULL};
    char *firefox_criteria[] = {"app_id=^firefox$","focus", NULL};
    Opener firefox_opener = {firefox_command, firefox_criteria};
    char *web_files[total_count];
    char *url_files[total_count];

    const char *archive_formats = getenv("_FILE_OPENER_ARCHIVE_FORMATS") ? : "";

    char *editor_command[3];  // Fixed size array, large enough to hold all possible arguments
    char *editor_criteria[] = {"[app_id=^sublime_text$]","focus", NULL};
    if (getenv("WAYLAND_DISPLAY") == NULL) {
        editor_command[0] = "nvim";
        editor_command[1] = NULL;
    } else {
        int argcount = 2 + attach_mode;
        editor_command[0] = "/opt/sublime_text/sublime_text";
        editor_command[1] = attach_mode ? "--wait" : NULL;
        editor_command[2] = NULL;
    }

    Opener sublime_opener = {editor_command, editor_criteria};
    char *other_files[total_count];


    char *magnet_files[total_count];

    int multimedia_count = 0, book_count = 0, picture_count = 0, other_count = 0, web_count = 0, url_count = 0, disabled_count = 0, magnet_count = 0, libreoffice_count = 0;

    for (int i = 0; i < total_count; i++) {
        char *input = inputs[i];

        if (!input || input[0] == '\0') {
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

        if (strncmp(input, "file://", 7) == 0) {
            input += 7;  // Skip the "file://" part
            char *host_end = strchr(input, '/');
            if (host_end) {
                // If there's a host part in the URL, skip it. Otherwise, input is already the path
                input = host_end;
            }
            replace_percent20_with_space(input); // Replace "%20" with spaces for file paths
        }

        input = build_absolute_path_or_url(input);

        if(!input) {
            continue;
        }

        if (only_files && is_directory(input) == 1) {
            disabled_files[disabled_count++] = input;
            continue;
        }

        const char *ext = get_file_extension(input);


        if (is_extension_in_list(ext, multimedia_formats)) {
            multimedia_files[multimedia_count++] = input;
        } else if(is_extension_in_list(ext, book_formats)) {
            book_files[book_count++] = input;
        } else if(is_extension_in_list(ext, web_formats)) {
            web_files[web_count++] = input;
        } else if(is_extension_in_list(ext, picture_formats)) {
            picture_files[picture_count++] = input;
        } else if(is_extension_in_list(ext, libreoffice_formats)) {
            libreoffice_files[libreoffice_count++] = input;
        } else if(is_extension_in_list(ext, exclude_suffixes)) {
            disabled_files[disabled_count++] = input;
        } else {
            other_files[other_count++] = input;
        }
    }

    for (int i = 0; i < disabled_count; i++) {
        fprintf(stderr, "%s\n", disabled_files[i]);
    }

    char *preopenenv = NULL;
    preopenenv = getenv("FILE_OPENER_PRE_CALLBACK");
    if (preopenenv && !disabled_count) {
        char* preopencmd[MAX_ARGS] = {0};

        char* token = strtok(preopenenv, " ");
        for (size_t i=0; i<MAX_ARGS+1 && token != NULL; i++) {
            preopencmd[i] = strdup(token);
            token = strtok(NULL, " ");
        }
        run_sway(preopencmd);
    }


    if (!attach_mode) {
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
        launch_opener_with_files(torrent_command, magnet_files, attach_mode);
    }


    multimedia_files[multimedia_count] = NULL;
    if (multimedia_count > 0) {
        if (run_sway(multimedia_opener.criteria)) {
            const char *socket_path = "/tmp/mpvsocket";
            for (int i = 0; i < multimedia_count; i++) {
                mpv_message(socket_path, multimedia_files[i]);
            }
        } else {
            launch_opener_with_files(multimedia_opener.command, multimedia_files, attach_mode);
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

            char criteria[44+strlen(book_basename)];
            snprintf(criteria, sizeof(criteria), "[app_id=\"^org.pwmt.zathura$\" title=\"^%s \"]", book_basename);
            char *launch[] = {(char *)criteria, "focus", NULL};
            if (!run_sway(launch)) {
                char *book_a[] = {book, NULL};
                launch_opener_with_files(book_opener.command, book_a, attach_mode);
            }
        }
    }
    for (int i = 0; i < book_count; i++) {
        free(book_files[i]);
    }

    web_files[web_count] = NULL;
    if (web_count > 0) {
        run_sway(firefox_opener.criteria);
        launch_opener_with_files(firefox_opener.command, web_files, attach_mode);
    }
    for (int i = 0; i < web_count; i++) {
        free(web_files[i]);
    }

    url_files[url_count] = NULL;
    if (url_count > 0) {
        run_sway(firefox_opener.criteria);
        launch_opener_with_files(firefox_opener.command, url_files, attach_mode);
    }
    for (int i = 0; i < url_count; i++) {
        free(url_files[i]);
    }

    picture_files[picture_count] = NULL;
    if (picture_count > 0) launch_opener_with_files(picture_opener.command, picture_files, attach_mode);
    for (int i = 0; i < picture_count; i++) {
        free(picture_files[i]);
    }

    libreoffice_files[libreoffice_count] = NULL;
    if (libreoffice_count > 0) launch_opener_with_files(libreoffice_opener.command, libreoffice_files, attach_mode);
    for (int i = 0; i < libreoffice_count; i++) {
        free(libreoffice_files[i]);
    }

    other_files[other_count] = NULL;
    if (other_count > 0) {
        if (!run_sway(sublime_opener.criteria)) {
            const char *json_string = "{\"app_id\": \"sublime_text\", \"rules\": [\"move to workspace 2\", \"focus\", \"mark --add ctrl+2__dynamic_focus__\"]}";
            char *launch[] = {"-t" "send_tick", (char *)json_string, NULL};
            run_sway(launch);
        }
        launch_opener_with_files(sublime_opener.command, other_files, attach_mode);
    }
    for (int i = 0; i < other_count; i++) {
        free(other_files[i]);
    }

    for (int i = 0; i < total_count; i++) {
        free(inputs[i]);
    }

    free(inputs);

    const char *callback = NULL;
    callback = getenv("FILE_OPENER_CALLBACK");
    if (attach_mode && callback) {
        char callbackmsg[128];
        snprintf(callbackmsg, sizeof(callbackmsg), "[con_id=%s]", callback);
        char *callbackargs[] = {callbackmsg, "focus", NULL};
        run_sway(callbackargs);
    }

    return disabled_count + error_return;
}
