#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <wait.h>
#include <fcntl.h>

#pragma ide diagnostic ignored "EndlessLoop"
#define MAX_LINE 80

#define CREATE_FLAGS (O_WRONLY | O_TRUNC | O_CREAT )
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define CREATE_APPEND_FLAGS (O_WRONLY | O_APPEND | O_CREAT )

typedef struct Process Process;
struct Process {
    int id;
    char command[MAX_LINE];
    pid_t pid;
    Process *next;
};

Process *runningHead;
Process *finishedHead;
int numOfProcesses;

typedef struct Bookmark Bookmark;
struct Bookmark {
    char command[MAX_LINE];
    Bookmark *next;
};

Bookmark *bookmarkHead;

void setup(char *inputBuffer, char **args, int *background, int *argc) {
    int length, i, start;

    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);
    start = -1;
    if (length == 0)
        exit(0);

    if ((length < 0) && (errno != EINTR)) {
        perror("error reading the command");
        exit(-1);
    }

    for (i = 0; i < length; i++) {
        switch (inputBuffer[i]) {
            case ' ':
            case '\t' :
                if (start != -1) {
                    args[*argc] = &inputBuffer[start];
                    (*argc)++;
                }
                inputBuffer[i] = '\0';
                start = -1;
                break;

            case '\n':
                if (start != -1) {
                    args[*argc] = &inputBuffer[start];
                    (*argc)++;
                }
                inputBuffer[i] = '\0';
                args[*argc] = NULL;
                break;

            default :
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&') {
                    *background = 1;
                    inputBuffer[i - 1] = '\0';
                }
        }
    }

    args[*argc] = NULL;
}

Process *push_process(char *command, pid_t pid) {
    Process *newProcess = (Process *) malloc(sizeof(Process));
    newProcess->id = numOfProcesses++;
    strcpy(newProcess->command, command);
    newProcess->next = NULL;
    newProcess->pid = pid;

    if (runningHead == NULL) {
        runningHead = newProcess;
    } else {
        Process *iter = runningHead;
        while (iter->next != NULL) {
            iter = iter->next;
        }
        iter->next = newProcess;
    }
    return newProcess;
}

void finish_process(pid_t pid) {
    Process *prev, *process = runningHead;

    while (process != NULL && process->pid != pid) {
        prev = process;
        process = process->next;
    }

    if (process == NULL) {
        fprintf(stderr, "Error: Process is already finished.\n");
        return;
    }

    if (process == runningHead) {
        runningHead = runningHead->next;
    } else {
        prev->next = process->next;
    }
    process->next = NULL;

    if (finishedHead == NULL) {
        finishedHead = process;
    } else {
        Process *iter;
        for (iter = finishedHead; iter->next != NULL; iter = iter->next);
        iter->next = process;
    }
}

void print_process_list(Process *head) {
    Process *temp;
    for (temp = head; temp != NULL; temp = temp->next) {
        printf("\t\t[%d] %s (Pid=%d)\n", temp->id, temp->command, temp->pid);
    }
}

void ps_all() {
    printf("\tRunning:\n");
    print_process_list(runningHead);
    printf("\tFinished:\n");
    print_process_list(finishedHead);
}

void push_bookmark(char *name) {
    Bookmark *current = NULL;
    current = (Bookmark *) malloc(sizeof(Bookmark));
    strcpy(current->command, name);
    current->next = NULL;

    if (bookmarkHead == NULL) {
        bookmarkHead = current;
    } else {
        Bookmark *iter = bookmarkHead;
        while (iter->next != NULL) {
            iter = iter->next;
        }
        iter->next = current;
    }
}

void pop_bookmark(int n) {
    Bookmark *current = bookmarkHead;
    Bookmark *temp = NULL;
    int i;
    for (i = 0; i < n - 1; i++) {
        if (current->next == NULL) {
            fprintf(stderr, "Error: Invalid value\n");
        }
        current = current->next;
    }
    if (n == 0) {
        temp = bookmarkHead;
        bookmarkHead = bookmarkHead->next;
        temp->next = NULL;
    } else {
        temp = current->next;
        current->next = temp->next;
        temp->next = NULL;
    }
    free(temp);
}

void print_bookmarks() {
    Bookmark *iter = bookmarkHead;
    int j = 0;
    while (iter != NULL) {
        printf("\t%d\t\"%s\"\n", j, iter->command);
        iter = iter->next;
        j++;
    }
}

char *find_in_path(char **args) {
    DIR *dir;
    struct dirent *dirent;
    char *pathList = getenv("PATH");
    char *path = strtok(pathList, ":");
    while (path != NULL) {
        dir = opendir(path);
        if (dir != NULL) {
            while (1) {
                dirent = readdir(dir);
                if (dirent == NULL) {
                    break;
                }
                if (dirent->d_type != DT_DIR && strcmp(dirent->d_name, args[0]) == 0) {
                    strcat(path, "/");
                    strcat(path, dirent->d_name);
                    return path;
                }
            }
        }
        path = strtok(NULL, ":");
    }

    return NULL;
}

int execute(char **args) {
    char *path = find_in_path(args);
    if (path == NULL) {
        perror("Execution error");
        return -1;
    }
    execv(path, args);
    return 0;
}

void bookmark(char **args, int argc) {
    argc--;
    int i;
    char commandStr[MAX_LINE] = "";
    for (i = 0; i < argc; i++) {
        strcat(commandStr, args[i]);
        if (i < argc - 1) {
            strcat(commandStr, " ");
        }
    }
    long length = strlen(args[argc - 1]);
    if (args[0][0] == '"' && (args[argc - 1][length - 1] == '"')) {
        memmove(commandStr, commandStr + 1, strlen(commandStr));
        commandStr[strlen(commandStr) - 1] = '\0';
        push_bookmark(commandStr);
    } else if (strstr(args[0], "-l")) {// Sort commandStr -l
        print_bookmarks();
    } else if (strstr(args[0], "-i")) {// Execution commandStr -k
        if (args[1] == NULL) {
            fprintf(stderr, "Error: No bookmark provided.\n");
            return;
        }
        Bookmark *bookmark = bookmarkHead;
        int j;
        for (j = 0; bookmark != NULL; j++) {
            if (atoi(args[1]) == j) {
                break;
            }
            bookmark = bookmark->next;
        }

        if (bookmark == NULL) {
            fprintf(stderr, "Error: Bookmark not found.\n");
            return;
        }

        int k = 0;
        char *p = strtok(bookmark->command, " ");
        char *execArgs[MAX_LINE];

        while (p != NULL) {
            execArgs[k++] = p;
            p = strtok(NULL, " ");
        }
        execute(execArgs);
    } else if (strstr(args[0], "-d")) {// Delete commandStr -d
        pop_bookmark(atoi(args[1]));
    } else { //Wrong argument
        fprintf(stderr, "Error: Wrong usage\n");
    }
}

int is_directory(char *subdirPath) {
    DIR *subDir = opendir(subdirPath);
    if (subDir != NULL) {
        closedir(subDir);
        return 1;
    } else {
        return 0;
    }
}

void search_inside_file(struct dirent *dirent, char *subdirPath, char *keyword) {
    char temp[PATH_MAX];
    int line_num = 1;
    int find_result = 0;
    char filePath[PATH_MAX];
    sprintf(filePath, "%s/%s", subdirPath, dirent->d_name);
    FILE *file = fopen(filePath, "r");
    while (fgets(temp, PATH_MAX, file) != NULL) {

        if ((strstr(temp, keyword)) != NULL) {

            printf("%d:\t./%s -> %s", line_num, dirent->d_name, temp);

            find_result++;
        }
        line_num++;
    }
    if (find_result == 0) {
        return;
    }
    printf("\n");
    // Close the file if still open.
    if (file) {
        fclose(file);
    }
}

int is_right_extension(char *direntNameInFunction) {
    // printf("%s", direntNameInFunction);
    int length = strlen(direntNameInFunction);

    if (length != 0) {
        if (strcmp(&direntNameInFunction[length - 2], ".c") == 0 ||
            strcmp(&direntNameInFunction[length - 2], ".h") == 0 ||
            strcmp(&direntNameInFunction[length - 2], ".C") == 0 ||
            strcmp(&direntNameInFunction[length - 2], ".H") == 0) {
            return 1;
        }
    } else {
        return 0;
    }
    return 0;
}

void search_directory(struct dirent *dirent, DIR *dir, char *startPath, char *keyword, int recursive) {
    dir = opendir(startPath);
    dirent = readdir(dir);
    while (dirent != NULL) {
        if (!((strcoll(dirent->d_name, ".") == 0 || strcoll(dirent->d_name, "..") == 0))) {
            char direntPath[PATH_MAX];
            sprintf(direntPath, "%s/%s", startPath, dirent->d_name);
            //   printf("%s",direntName);
            if (is_directory(direntPath) == 1 && recursive) {
                char tempPath[PATH_MAX];
                sprintf(tempPath, "%s", direntPath);
                search_directory(dirent, dir, tempPath, keyword, 1);
            } else {
                if (is_right_extension(dirent->d_name) == 1) {
                    search_inside_file(dirent, startPath, keyword);
                }
            }
        }
        //move to next directory entry for dataset
        dirent = readdir(dir);
    }

}

void search(char **args, int argc) {
    if (argc == 1) {
        fprintf(stderr, "Error: No keyword argument given.\n");
        return;
    }
    char **keywordArgs;
    int keywordSize;
    int recursive = 0;
    char keyword[MAX_LINE] = "";
    if (strcmp(args[1], "-r") == 0) {
        recursive = 1;
        if (args[2] == NULL) {
            fprintf(stderr, "Error: No keyword argument given.\n");
            return;
        }
        keywordArgs = args + 2;
        keywordSize = argc - 2;
    } else {
        keywordArgs = args + 1;
        keywordSize = argc - 1;
    }
    int length = strlen(keywordArgs[keywordSize - 1]);
    if (keywordArgs[0][0] == '"' && (keywordArgs[keywordSize - 1][length - 1] == '"')) {
        int i;
        for (i = 0; i < keywordSize; i++) {
            strcat(keyword, keywordArgs[i]);
            if (i < keywordSize - 1) {
                strcat(keyword, " ");
            }
        }
    } else {
        fprintf(stderr, "Error: Invalid keyword, try using with quotes.\n");
        return;
    }
    memmove(keyword, keyword + 1, strlen(keyword));
    keyword[strlen(keyword) - 1] = '\0';
    char currentWorkingDir[PATH_MAX];
    if (getcwd(currentWorkingDir, sizeof(currentWorkingDir)) == NULL) {
        fprintf(stderr, "Error: Can't get current working directory.\n");
        return;
    }
    DIR *dir;
    struct dirent *dirent;
    dir = opendir(currentWorkingDir);
    dirent = readdir(dir);
    search_directory(dirent, dir, currentWorkingDir, keyword, recursive);
}

void operate(char **args, int argc, int background) {
    char *program = args[0];
    if (strcmp(program, "exit") == 0) {
        exit(0);
    }

    int outputFile;
    int inputFile;
    if (argc > 2) {
        int isOutputRedirection = 0;
        char *fileName;
        if (strcmp(args[argc - 2], ">") == 0) {
            fileName = args[argc - 1];
            outputFile = open(fileName, CREATE_FLAGS, CREATE_MODE);
            isOutputRedirection = 1;
        } else if (strcmp(args[argc - 2], ">>") == 0) {
            fileName = args[argc - 1];
            outputFile = open(fileName, CREATE_APPEND_FLAGS, CREATE_MODE);
            isOutputRedirection = 1;
        } else if (strcmp(args[argc - 2], "<") == 0) {
            fileName = args[argc - 1];
            inputFile = open(fileName, CREATE_FLAGS, CREATE_MODE);
            dup2(inputFile, 0);
            args[argc - 1] = NULL;
            args[argc - 2] = NULL;
            argc = argc - 2;
        }

        if (isOutputRedirection) {
            dup2(outputFile, 1);
            args[argc - 1] = NULL;
            args[argc - 2] = NULL;
            argc = argc - 2;
        }
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        fprintf(stderr, "Error: Fork failed.\n");
        return;
    }
    if (child_pid == 0) { // child code
        if (background) {
            args[--argc] = NULL;
        }

        if (strcmp(program, "ps_all") == 0) {
            ps_all();
        } else if (strcmp(program, "search") == 0) {
            search(args, argc);
        } else if (strcmp(program, "bookmark") == 0) {
            bookmark(args + 1, argc);
        } else {
            execute(args);
        }
    }
    if (child_pid > 0) { // parent code
        push_process(program, child_pid);
        pid_t waitedPid = waitpid(child_pid, NULL, background ? WNOHANG : 0);
        if (waitedPid > 0) {
            finish_process(waitedPid);
        }
    }

    close(outputFile);
    close(inputFile);
}

int main() {
    char inputBuffer[MAX_LINE];
    char *args[MAX_LINE];
    numOfProcesses = 0;
    while (1) {
        int background = 0;
        int argc = 0;
        char commandStr[MAX_LINE] = "";
        write(1, "myshell>> ", 10);
        setup(inputBuffer, args, &background, &argc);
        int i;
        for (i = 0; i < argc; i++) {
            strcat(commandStr, args[i]);
            if (i < argc - 1) {
                strcat(commandStr, " ");
            }
        }
        operate(args, argc, background);
    }
}