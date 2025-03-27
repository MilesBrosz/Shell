#define _POSIX_C_SOURCE 200112L
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

char command_history[MAX_HISTORY][200];  
int history_count = 0; 

int main() {

    JobManager job_manager = {.job_count = 0 };

    while (1) {

        for (int i = 0; i < job_manager.job_count; i++) {
            if (job_manager.jobs[i].pid > 0) {
                int status;
                pid_t result = waitpid(job_manager.jobs[i].pid, &status, WNOHANG);
        
                if (result > 0) {  // Process finished
                    printf("\n[%d] + done %s\n", job_manager.jobs[i].job_num, job_manager.jobs[i].command);
                    free(job_manager.jobs[i].command);
                    job_manager.jobs[i].command = NULL;
                    job_manager.jobs[i].pid = 0;
                }
            }
        }



        display_PWD();

        char* input = get_input();
        if (strlen(input) == 0) {
            free(input);
            continue;
        }

        store_command(input); //keeps history of commands

        tokenlist* tokens = get_tokens(input);
        if (tokens->size == 0) {
            free(input);
            free_tokens(tokens);
            continue;
        }

        checkEnv(tokens);
        checkTilda(tokens);

        if (is_cd_command(tokens)) {
                if (tokens->size == 1) {
        // No argument provided, pass NULL to default to HOME
        change_directory(NULL);
    } else if (tokens->size == 2) {
        // Single argument provided, use it as the directory
        change_directory(tokens->items[1]);
    } else {
        // Too many arguments
        fprintf(stderr, "cd: too many arguments\n");
    }
                continue;
            }

        if (is_exit_command(tokens)) {
             exit_shell(&job_manager);
        }

        if (is_jobs_command(tokens)) {
            jobs_list(&job_manager);
            continue;
        }


        if (tokens->size > 0 && strcmp(tokens->items[tokens->size - 1], "&") == 0) {
            background_process(tokens, &job_manager); // Run in background
        } 

        // Check if the input contains pipes
        int num_commands = 1;
        for (int i = 0; i < tokens->size; i++) {
            if (strcmp(tokens->items[i], "|") == 0) {
                num_commands++;
            }
        }

        if (num_commands > 1) {
            // Split tokens into separate commands
            tokenlist** commands = malloc(num_commands * sizeof(tokenlist*));
            int command_index = 0;
            commands[command_index] = new_tokenlist();

            for (int i = 0; i < tokens->size; i++) {
                if (strcmp(tokens->items[i], "|") == 0) {
                    command_index++;
                    commands[command_index] = new_tokenlist();
                } else {
                    add_token(commands[command_index], tokens->items[i]);
                }
            }

            // Handle piping
            handle_piping(commands, num_commands);

            // Free commands
            for (int i = 0; i < num_commands; i++) {
                free_tokens(commands[i]);
            }
            free(commands);
        } else {
            //Execute any single command
                
            //We check if the input is cd if so we call the function.
            ioRedirection(tokens);
        }

        free(input);
        free_tokens(tokens);
    }

    return 0;
}
char lastdir[PATH_MAX] ="";
void change_directory(char *arg) {
    char curdir[PATH_MAX];
    char new_pwd[PATH_MAX];
    char path[PATH_MAX];

    //Save directory in case destination does not exist.
    if (getcwd(curdir, sizeof(curdir)) == NULL) {
        perror("getcwd failed");
        *curdir = '\0';
    }

    if (arg == NULL) {
        //assume user wants to go to home directory.
        arg = getenv("HOME");
        if (arg == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
    } else if (strcmp(arg, "-") == 0) {
        //go to previous directory.
        if (*lastdir == '\0') {
            fprintf(stderr, "cd: no previous directory\n");
            return;
        }
        arg = lastdir;
    } else if (*arg == '~') {
        // expand tilda to home variable.
        if (arg[1] == '/' || arg[1] == '\0') {
            snprintf(path, sizeof(path), "%s%s", getenv("HOME"), arg + 1);
            arg = path;
        } else {
            fprintf(stderr, "cd: syntax not supported: %s\n", arg);
            return;
        }
    }

    // If there is no error in changing directory we change to that directory.
    if (chdir(arg) != 0) {
        fprintf(stderr, "cd: %s: %s\n", strerror(errno), arg);
        return;
    }

    // Since we changed directory we should update lastdir.
    strcpy(lastdir, curdir);

    // Update the PWD variable to be the new_pwd.
    if (getcwd(new_pwd, sizeof(new_pwd)) != NULL) {
        if (setenv("PWD", new_pwd, 1) != 0) {
            perror("Failed to set PWD environment variable");
        }
    } else {
        perror("Failed to get new working directory");
    }
}

void store_command(const char *cmd){

    if (strlen(cmd) == 0) return; // Ignore empty commands

    char cmd_copy[200];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char *token = strtok(cmd_copy, " "); 

    if (token == NULL) {
        return; // If there's no command, do nothing
    }

    if (!is_builtin(token) && path_search(token) == NULL) {
        static char last_invalid[200] = "";  // Store last invalid command
        if (strcmp(last_invalid, token) != 0) {  
            // printf("Command not found in $PATH: %s\n", token);
            strncpy(last_invalid, token, sizeof(last_invalid) - 1);
            last_invalid[sizeof(last_invalid) - 1] = '\0'; 
        }
        return;
    }

    if (history_count < MAX_HISTORY) {
        strcpy(command_history[history_count], cmd);
        history_count++;
    } else {
        // Shift history up and add new command at the end
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(command_history[i], command_history[i + 1]);
        }
        strcpy(command_history[MAX_HISTORY - 1], cmd);
    }

}

void exit_shell(JobManager *job_manager) {

    if (job_manager == NULL) {
        fprintf(stderr, "Error: job manager is invalid. Exiting shell.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < job_manager->job_count; i++) {
        if (job_manager->jobs[i].pid > 0) {
            waitpid(job_manager->jobs[i].pid, NULL, 0);
        }
    }

    // Print the last three commands (history tracking)
    printf("Recent Commands:\n");

    char* valid_commands[3] = {NULL, NULL, NULL};
    int valid_count = 0;

   
    for (int i = history_count - 1; i >= 0 && valid_count < 3; i--) {
        if (is_builtin(command_history[i]) || path_search(command_history[i]) != NULL) {
            valid_commands[valid_count] = command_history[i];
            valid_count++;
        }
    }

    if (valid_count == 0) {
        printf("No valid commands executed.\n");
    } else if (valid_count <= 2) {
        // Print only the last command if 1 or 2 commands exist
        printf("%s\n", valid_commands[0]);
    } else {
        // Print the last 3 commands if history_count is 3 or more
        for (int i = valid_count - 3; i < valid_count; i++) {
          printf("%s\n", valid_commands[i]);
        }
    }

    printf("Exiting shell.\n");
    exit(0);  // Exit the program

}

void jobs_list(JobManager* job_manager) {
    int active_jobs = 0;

    for (int i = 0; i < job_manager->job_count; i++) {
        printf("Stored PID = %d\n", job_manager->jobs[i].pid);
        if (job_manager->jobs[i].pid > 0) { // If job is still active
            int status;
            pid_t result = waitpid(job_manager->jobs[i].pid, &status, WNOHANG);
            printf("Result = %d\n", result);


            if (result == 0) {  // Process is still running
                if (active_jobs == 0) {
                    printf("Active background jobs:\n");
                }
                printf("[%d] %d %s\n", job_manager->jobs[i].job_num, 
                       job_manager->jobs[i].pid, job_manager->jobs[i].command);
                active_jobs++;
            } else{  // Process has finished, clean it up
                free(job_manager->jobs[i].command);
                job_manager->jobs[i].command = NULL;
                job_manager->jobs[i].pid = 0;
            }
        }
    }

    if (active_jobs == 0) {
        printf("No active background jobs.\n");
    }
}


void checkEnv(tokenlist* tokens) {
    for (int i = 0; i < tokens->size; i++) {
        if (strchr(tokens->items[i], '$') != NULL) {
            const char* envVariable = tokens->items[i] + 1;
            char* value = getenv(envVariable);

            if (value != NULL) {
                size_t valueLength = strlen(value) + 1;
                free(tokens->items[i]);
                tokens->items[i] = malloc(valueLength);
                strcpy(tokens->items[i], value);
            } else {
                fprintf(stderr, "Environment Variable Not Found\n");
            }
        }
    }
}

void externalCommandexec(tokenlist* tokens) {
    //args contains the whole tokenlist, pathname only includes the path, but also the first index of the tokenlist (presumably)
    //all tokens need to be parsed before this
    //last token needs to be set to NULL

    /*for(int i=0;i<strlen(tokens);i++) {
            char* token[i] = (char*) malloc(strlen(tokens)+1);
            strcpy(tokenList[i], token);
    }*/
    
    //ioRedirection(tokens); //change descriptors if needed
    
    tokens->items[tokens->size] = NULL;

    char* max_path = path_search(tokens->items[0]);
    if (max_path == NULL) {
        fprintf(stderr, "Cannot execute: %s\n", tokens->items[0]);
        return;
    }

    pid_t pId = fork();
    if (pId < 0) {
        perror("fork failed");
        free(max_path);
        return;
    } else if (pId == 0) {
        execv(max_path, tokens->items);
        perror("execv failed");
        free(max_path);
        exit(EXIT_FAILURE);
    } else {
        waitpid(pId, NULL, 0);
    }

    free(max_path);
}
/*
This is relatively simple and  essentially takes our num_commands and call's execute_pipeline based
on the number of commands we have. If there are only two, we have our third item as null.
However if there is any more we will return an error.
*/
void handle_piping(tokenlist** commands, int num_commands) {
    if (num_commands == 2) {
        
        execute_pipeline(commands[0]->items, commands[1]->items, NULL);
    } else if (num_commands == 3) {
        execute_pipeline(commands[0]->items, commands[1]->items, commands[2]->items);
    } else {
        fprintf(stderr, "Error: Invalid number of pipes.\n");
    }
}
/*
Create your first pipe, as at this point we know there is pipe involved.
If the pipe returns to -1 then we have an error however, after this we check if we
passed NULL for the command3. As long as it's not null we will then perform error handling
to ensure our pipe was not created with errors.
*/
void execute_pipeline(char** command1, char** command2, char** command3) {
    int pipe1[2], pipe2[2];
    pid_t pid1, pid2, pid3;

    
    if (pipe(pipe1) == -1) {
        perror("pipe1");
        exit(EXIT_FAILURE);
    }

    // Create second pipe if we have a third command
    if (command3 != NULL) {
        if (pipe(pipe2) == -1) {
            perror("pipe2");
            close(pipe1[0]);
            close(pipe1[1]);
            exit(EXIT_FAILURE);
        }
    }

    /*Fork the first child process and then close the read end of pipe1.
    If you have a third command you need to close bothe ends of pipe 2(pipe2[0] and pipe2[1]).
    Redirect pipe1 write end to STDOUT.
    */
    pid1 = fork();
    if (pid1 == -1) {
        perror("fork1");
        exit(EXIT_FAILURE);
    }
    
    if (pid1 == 0) {  
        
        close(pipe1[0]);//read end
        
        if (command3 != NULL) {
            close(pipe2[0]);
            close(pipe2[1]);//write end
        }
        dup2(pipe1[1], STDOUT_FILENO); 
        close(pipe1[1]);//write end

        // Execute the first command
        execvp(command1[0], command1);
        perror("execvp cmd1");
        exit(EXIT_FAILURE);
    }

    // Similar process as above however, we check to see if there is a third command.
    //if so then we will repeat this process one last time. 
    pid2 = fork();
    if (pid2 == -1) {
        perror("fork2");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {  
        // Setup pipe1 with STDIN
        dup2(pipe1[0], STDIN_FILENO);
        close(pipe1[0]);
        close(pipe1[1]);

        if (command3 != NULL) {
            // If there's a third command, redirect output to pipe2
            close(pipe2[0]);
            dup2(pipe2[1], STDOUT_FILENO);
            close(pipe2[1]);
        }

        // Execute second command
        execvp(command2[0], command2);
        perror("execvp cmd2");
        exit(EXIT_FAILURE);
    }

    // Now that we no longer needs these pipes for command2, we can close them
    //and free up memory.
    close(pipe1[0]);
    close(pipe1[1]);


    // This is where we check for third command. If we only have a singular pipe
    // we will skip this if block. If we have a third command, we will simply fork the process.
    // Then we can set up the read of the second pipe to be the STDIN. Then we fully close the second pipe.
    //then we can execute the command. Lastly we wait to ensure all processes close properly.
    if (command3 != NULL) {
        pid3 = fork();
        if (pid3 == -1) {
            perror("fork3");
            exit(EXIT_FAILURE);
        }

        if (pid3 == 0) {  
            
            dup2(pipe2[0], STDIN_FILENO);
            close(pipe2[0]);
            close(pipe2[1]);

            
            execvp(command3[0], command3);
            perror("execvp cmd3");
            exit(EXIT_FAILURE);
        }

        // Close pipe2 in parent
        close(pipe2[0]);
        close(pipe2[1]);

        
        waitpid(pid3, NULL, 0);
    }

    // Close all child processes and then close the parent processes.
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void ioRedirection(tokenlist* tokens) {
    int infd = dup(STDIN_FILENO);
    int outfd = dup(STDOUT_FILENO);

    int inputfd = -1;
    int outputfd = -1;

    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "<") == 0) {
            char* input = tokens->items[i + 1];
            inputfd = open(input, O_RDONLY);
            if (inputfd < 0) {
                fprintf(stderr, "Failed to open input");
                return;
            }
            //THIS IS THE OUTPUT:V
        } else if (strcmp(tokens->items[i], ">") == 0) {
            char* output = tokens->items[i + 1];
            outputfd = open(output, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        }
    }

    if (inputfd != -1) {
        dup2(inputfd, STDIN_FILENO);
        close(inputfd);
    }
    if (outputfd != -1) {
        dup2(outputfd, STDOUT_FILENO);
        close(outputfd);
    }

    int size = 0;
    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "<") == 0 || strcmp(tokens->items[i], ">") == 0) {
            i++;
        } else {
            tokens->items[size++] = tokens->items[i];
        }
    }

    tokens->size = size;
    tokens->items[tokens->size] = NULL;

    externalCommandexec(tokens);

    dup2(infd, STDIN_FILENO);
    close(infd);

    dup2(outfd, STDOUT_FILENO);
    close(outfd);
}

void display_PWD() {
    char* user = getenv("USER");
    char* machine = getenv("HOSTNAME");
    char* pwd = getenv("PWD");
    if (user && machine && pwd) {
        printf("%s@%s:%s>", user, machine, pwd);
    } else {
        printf("USER MACHINE OR PWD could not be shown");
    }
}

void checkTilda(tokenlist* tokens) {
    for (int i = 0; i < tokens->size; i++) {
        if (strchr(tokens->items[i], '~') != NULL) {
            const char* home = getenv("HOME");
            if (home != NULL) {
                if (strcmp(tokens->items[i], "~") == 0) {
                    char* new_home = malloc(strlen(home) + 1);
                    if (new_home) {
                        strcpy(new_home, home);
                        free(tokens->items[i]);
                        tokens->items[i] = new_home;
                    }
                } else if (strncmp(tokens->items[i], "~/", 2) == 0) {
                    char* new_path = malloc(strlen(home) + strlen(tokens->items[i]));
                    if (new_path) {
                        sprintf(new_path, "%s%s", home, tokens->items[i] + 1);
                        free(tokens->items[i]);
                        tokens->items[i] = new_path;
                    }
                }
            }
        }
    }
}

char* get_input(void) {
    char* buffer = NULL;
    int bufsize = 0;
    char line[5];
    while (fgets(line, 5, stdin) != NULL) {
        int addby = 0;
        char* newln = strchr(line, '\n');
        if (newln != NULL)
            addby = newln - line;
        else
            addby = 5 - 1;
        buffer = (char*)realloc(buffer, bufsize + addby);
        memcpy(&buffer[bufsize], line, addby);
        bufsize += addby;
        if (newln != NULL)
            break;
    }
    buffer = (char*)realloc(buffer, bufsize + 1);
    buffer[bufsize] = 0;
    return buffer;
}

tokenlist* new_tokenlist(void) {
    tokenlist* tokens = (tokenlist*)malloc(sizeof(tokenlist));
    tokens->size = 0;
    tokens->items = (char**)malloc(sizeof(char*));
    tokens->items[0] = NULL;
    return tokens;
}

void add_token(tokenlist* tokens, char* item) {
    int i = tokens->size;

    tokens->items = (char**)realloc(tokens->items, (i + 2) * sizeof(char*));
    tokens->items[i] = (char*)malloc(strlen(item) + 1);
    tokens->items[i + 1] = NULL;
    strcpy(tokens->items[i], item);

    tokens->size += 1;
}

tokenlist* get_tokens(char* input) {
    char* buf = malloc(strlen(input) + 1);
    strcpy(buf, input);
    tokenlist* tokens = new_tokenlist();

    char* tok = strtok(buf, " ");
    while (tok != NULL) {
        add_token(tokens, tok);
        tok = strtok(NULL, " ");
    }

    free(buf);
    return tokens;
}

void free_tokens(tokenlist* tokens) {
    for (int i = 0; i < tokens->size; i++)
        free(tokens->items[i]);
    free(tokens->items);
    free(tokens);
}
 
//  void cd(char **args){
//     char *target_directory = NULL;
//     if(args[1] == NULL)
//     {
//         if(target_directory = getenv("HOME")){
//             fprintf(stderr, "cd: Please set the Home Variable.");
//         }
//     }else if(args[2]!=NULL)
//     {
//         fprintf(stderr, "cd: You have entered too many variables.\n");
//     }else{
//         target_directory = args[1];
//     }
//     if(access(target_directory, free_tokens))
//  }
int is_cd_command(tokenlist* tokens) {
    return (tokens->size > 0 && strcmp(tokens->items[0], "cd") == 0);
}

int is_exit_command(tokenlist* tokens) {
    return (tokens->size > 0 && strcmp(tokens->items[0], "exit") == 0);
}

int is_jobs_command(tokenlist* tokens) {
    return (tokens->size > 0 && strcmp(tokens->items[0], "jobs") == 0);
}

int is_builtin(const char* command) {
    const char* builtin[] = {"cd", "exit", "jobs", NULL};
    
    for (int i = 0; builtin[i] != NULL; i++) {
        if (strcmp(command, builtin[i]) == 0) {
            return 1;
        }
    }
    return 0;
}



char* path_search(const char* com) {
    if (strchr(com, '/')) {
        if (is_executable(com)) {
            char* search = malloc(strlen(com) + 1);
            if (search) {
                strcpy(search, com);
            }
            return search;
        }
        return NULL;
    
    }

    char* path_env = getenv("PATH");
    if (!path_env) {
        fprintf(stderr, "Error: PATH environment variable not set.\n");
        return NULL;
    }

    char* path_copy = malloc(strlen(path_env) + 1);
    if (!path_copy) {
        perror("strlen did not work");
        exit(EXIT_FAILURE);
    }
    strcpy(path_copy, path_env);

    char* dir = strtok(path_copy, ":");
    while (dir != NULL) {
        char max_path[1024];
        snprintf(max_path, sizeof(max_path), "%s/%s", dir, com);

        if (access(max_path, X_OK) == 0) {
            char* search = malloc(strlen(max_path) + 1);
            if (search) {
                strcpy(search, max_path);
            }
            free(path_copy);
            return search;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    fprintf(stderr, "Command not found in $PATH: %s\n", com);
    return NULL;
}

int is_executable(const char *command) {
    return access(command, X_OK) == 0;
}

void background_process(tokenlist* tokens, JobManager* job_manager){
    if (job_manager->job_count >= MAX_JOBS) {
        fprintf(stderr, "Job limit reached.\n");
        return;
    }

    if (strcmp(tokens->items[0], "cd") == 0 || strcmp(tokens->items[0], "exit") == 0 ||
        strcmp(tokens->items[0], "jobs") == 0) {
        fprintf(stderr, "Error: You can not run this in the background.\n");
        return;
    }

    if (tokens != NULL && tokens->size > 0) {
        free(tokens->items[tokens->size - 1]); 
        tokens->items[tokens->size - 1] = NULL;
        tokens->size--;
    } // Remove '&'

    int command_num = 1;
    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "|") == 0) {
            command_num++;
        }
    }

    int inputfd = -1, outputfd = -1;
    char* in_file = NULL;
    char* out_file = NULL;

    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "<") == 0) {
            in_file = tokens->items[i + 1];
            tokens->items[i] = NULL;
        } 
        else if (strcmp(tokens->items[i], ">") == 0) {
            out_file = tokens->items[i + 1];
            tokens->items[i] = NULL;
        }
    }

    pid_t tracked_pid;
    if (command_num > 1) {
        int pfd[2];
        if (pipe(pfd) < 0) {
            perror("pfd");
            exit(EXIT_FAILURE);
        }

        tokenlist* cmd1 = new_tokenlist();
        tokenlist* cmd2 = new_tokenlist();
        int i = 0;

        while (i < tokens->size && strcmp(tokens->items[i], "|") != 0) {
            add_token(cmd1, tokens->items[i]);
            i++;
        }

        i++;

        while (i < tokens->size) {
            add_token(cmd2, tokens->items[i]);
            i++;
        }

        pid_t pid1 = fork();
        if (pid1 < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid1 == 0) { 
            setpgid(0, 0);

            dup2(pfd[1], STDOUT_FILENO);
            close(pfd[0]);
            close(pfd[1]);

            char* max_path = path_search(cmd1->items[0]);
            execv(max_path, cmd1->items);
            perror("execv failed");
            exit(EXIT_FAILURE);
        }

        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) {
            setpgid(pid1, pid1);

            dup2(pfd[0], STDIN_FILENO);
            close(pfd[0]);
            close(pfd[1]);

            char* max_path = path_search(cmd2->items[2]);
            execv(max_path, cmd2->items);
            perror("execv failed");
            exit(EXIT_FAILURE);
        }

        close(pfd[0]);
        close(pfd[1]);

        tracked_pid = pid2;

        free_tokens(cmd1);
        free_tokens(cmd2);
    }

    else{
        tracked_pid = fork();
        if (tracked_pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (tracked_pid == 0) { 
            if (in_file) {
                inputfd = open(in_file, O_RDONLY);
                if (inputfd < 0) {
                    perror("Cannot open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(inputfd, STDIN_FILENO);
                close(inputfd);
            }
            
            if (out_file) {
                outputfd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (outputfd < 0) {
                    perror("Cannot open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(outputfd, STDOUT_FILENO);
                close(outputfd);
            }
    
            char* max_path = path_search(tokens->items[0]);
                if (max_path == NULL) {
                fprintf(stderr, "Cannot execute: %s\n", tokens->items[0]);
                exit(EXIT_FAILURE);
                }
    
            execv(max_path, tokens->items);
            perror("execv failed");
            exit(EXIT_FAILURE);
            }
    }
    
    

    if (tracked_pid > 0) { // Parent process
        int job_index = job_manager->job_count;
        job_manager->jobs[job_index].job_num = job_index + 1;
        job_manager->jobs[job_index].pid = tracked_pid;
           
        size_t cmd_length = 0;
        for (int i = 0; i < tokens->size; i++) {
            if (tokens->items[i] != NULL) {
                cmd_length += strlen(tokens->items[i]) + 1;
            }
        }

        job_manager->jobs[job_index].command = malloc(cmd_length + 1);
        job_manager->jobs[job_index].command[0] = '\0';

        for (int i = 0; i < tokens->size; i++) {
            if (tokens->items[i] != NULL) {
                strcat(job_manager->jobs[job_index].command, tokens->items[i]);
                strcat(job_manager->jobs[job_index].command, " ");
            }
        }
    
        job_manager->job_count++;
    
        printf("[%d] %d\n", job_manager->jobs[job_index].job_num, tracked_pid);
    }
    
}







