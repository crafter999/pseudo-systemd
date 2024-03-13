#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#ifdef __APPLE__
    #define SYSTEMD_UNIT_PATH "/tmp/systemd"
#elif __linux__
    #define SYSTEMD_UNIT_PATH "/etc/systemd/system/"
#endif

typedef struct Service {
    char *name;
    char *workingDirectory;
    char *execStart;
    char *arguments;
    char *environment;
} Service;

char *commands[] = {
    "start",
    "stop",
    "restart",
    "status",
    "enable"
};

char** get_service_files(int* num_files) {
    *num_files = 0; // init
    struct dirent *de;
    DIR *dr = opendir(SYSTEMD_UNIT_PATH);
    char** files = NULL;

    if (dr == NULL) {
        printf("Could not open directory");
        return NULL;
    }

    while ((de = readdir(dr)) != NULL) {
        if (strstr(de->d_name, ".service") != NULL) {
            files = realloc(files, (*num_files + 1) * sizeof(char*));
            files[*num_files] = strdup(de->d_name);
            (*num_files)++;
        }
    }

    closedir(dr);
    return files;
}

bool is_valid_service_file(const char* file) {
    FILE *f = fopen(file, "r");
    if (f == NULL) {
        return false;
    }

    char file_content[1000];
    fread(file_content, 1000, 1, f);
    fclose(f);

    if (strstr(file_content, "[Unit]") == NULL) {
        return false;
    }

    if (strstr(file_content, "[Service]") == NULL) {
        return false;
    }

    if (strstr(file_content, "[Install]") == NULL) {
        return false;
    }

    return true;
}

char **get_lines_from_file(const char *filename, int *num_lines) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    char **lines = NULL;
    *num_lines = 0;
    char buffer[1000];
    while (fgets(buffer, 1000, file) != NULL) {
        lines = realloc(lines, (*num_lines + 1) * sizeof(char *));
        lines[*num_lines] = strdup(buffer);
        (*num_lines)++;
    }

    fclose(file);
    return lines;
}

void write_pid_to_file(const char* name, int pid) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/tmp/PSYSD_%s", name);
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Could not open file\n");
        return;
    }
    fprintf(f, "%d", pid);
    fclose(f);
}

int start_background_process(Service s) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "fork failed\n");
        return 1;
    }

    if (pid > 0) {
        // In the parent process, just return 0
        return 0;
    }

    // In the child process, start a new session
    if (setsid() < 0) {
        fprintf(stderr, "setsid failed\n");
        return 2;
    }
    
    write_pid_to_file(s.name, getpid());
    
    printf("Changing working directory to %s\n", s.workingDirectory);
    if (chdir(s.workingDirectory) < 0) {
        perror("chdir failed");
        return 3;
    }

    if (s.environment != NULL) {
        printf("Setting environment variable: %s\n", s.environment);
        putenv(s.environment);
    }

    // Replace the current process image with the given program
    char *args[] = {s.execStart, s.arguments, NULL};
    execvp(args[0], args);

    // execlp will only return if an error occurred
    perror("execlp failed");
    return 4;
}

int stop_background_proccess(const char* name) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/tmp/PSYSD_%s", name);
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "Could not open file\n");
        return 1;
    }
    int pid;
    fscanf(f, "%d", &pid);
    fclose(f);

    if (kill(pid, SIGTERM) < 0) {
        fprintf(stderr, "kill failed\n");
        return 2;
    }

    if (unlink(filename) < 0) {
        fprintf(stderr, "unlink failed\n");
        return 3;
    }

    return 0;
}

Service parse_service_from_lines(char** lines, int num_lines, char* name) {
    char* workingDirectory = NULL;
    char* execStart = NULL;
    char* arguments = NULL;
    char* environment = NULL;

    for (int i = 0; i < num_lines; i++){
        if (strstr(lines[i], "WorkingDirectory") != NULL){
            workingDirectory = lines[i] + 17;
            // trim any new line characters
            workingDirectory[strcspn(workingDirectory, "\n")] = 0;        
        }
        if (strstr(lines[i], "ExecStart") != NULL){
            char* x = strchr(lines[i], '=');
            char* y = strchr(x, ' ');

            if (x != NULL && y != NULL) {
                *y = '\0'; // Null terminate at the space to split the string into two
                execStart = x + 1;
                arguments = y + 1;
            }


            execStart = x + 1;
            arguments = y + 1;

            arguments[strcspn(arguments, "\n")] = 0; // trim
        }

        if (strstr(lines[i], "Environment") != NULL){
            environment = lines[i] + 12;
            environment[strcspn(environment, "\n")] = 0; // trim
        }
    }
    Service s = {
        .name = name,
        .workingDirectory = workingDirectory,
        .execStart = execStart,
        .arguments = arguments,
        .environment = environment
    };

    return s;
}

int main(int argc, char *argv[]){
    if (argc <2){
        return 0;
    }

    // daemon-reload
    if (argc == 2){
        if (strcmp(argv[1], "daemon-reload") != 0){
            return 1;
        }
    }

    // start, stop, restart, status
    if (argc == 3){
        int status = 0;
        for (int i = 0; i < 4; i++){
            if (strcmp(argv[1], commands[i]) != 0){
                status = 1;
                break;
            }
        }
        if (status == 0){
            return 2;
        }
    }

    // get service file
    char* serviceFile = NULL;
    int num_files;
    char** files = get_service_files(&num_files);
    if (files == NULL){
        printf("Could not get service files\n");
        return 3;
    }
    for (int i = 0; i < num_files; i++) {
        if (strstr(files[i], argv[2]) != NULL){
            serviceFile = files[i];
            break;
        }
    }
    if (serviceFile == NULL){
        printf("Service file not found\n");
        return 4;
    }

    // read service file
    char filename[100];
    sprintf(filename, "%s/%s", SYSTEMD_UNIT_PATH, serviceFile);
    int num_lines;
    char** lines = get_lines_from_file(filename, &num_lines);
    if (lines == NULL){
        printf("Could not read service file\n");
        return 5;
    }

    // check if service file is valid
    if (!is_valid_service_file(filename)){
        printf("Service file is not valid\n");
        return 6;
    }

    // parse service file
    Service s = parse_service_from_lines(lines, num_lines, argv[2]);
    printf("WorkingDirectory: %s\n", s.workingDirectory);
    printf("ExecStart: %s\n", s.execStart);
    printf("Arguments: %s\n", s.arguments);
    printf("Environment: %s\n", s.environment);

    // execute command
    if (strcmp(argv[1], "start") == 0){
        printf("Starting service %s\n", argv[2]);
        int status = start_background_process(s);
        if (status != 0){
            printf("Error starting service\n");
            return 7;
        }
    } else if (strcmp(argv[1], "stop") == 0){
        printf("Stopping service %s\n", argv[2]);
        int s = stop_background_proccess(argv[2]);
        if (s != 0){
            printf("Error stopping service\n");
            return 8;
        }
    } else if (strcmp(argv[1], "restart") == 0){
        printf("Not implemented\n");
    } else if (strcmp(argv[1], "status") == 0){
        printf("Not implemented\n");
    }

    return 0;
}