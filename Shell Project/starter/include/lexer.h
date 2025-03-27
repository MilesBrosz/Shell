#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#ifndef PATH_MAX
#define PATH_MAX 4096  // Common value for Linux/macOS
#endif


#define MAX_JOBS 10  // Max background jobs
#define MAX_HISTORY 3 // command history

typedef struct {
    int job_num;
    pid_t pid;
    char* command;
} background_job;

typedef struct {
    background_job jobs[MAX_JOBS];
    int job_count;
} JobManager;

typedef struct {
    char ** items;
    size_t size;
} tokenlist;

char * get_input(void);
char *path_search(const char *com);
tokenlist * get_tokens(char *input);
tokenlist * new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);
void display_PWD();
void checkTilda(tokenlist *tokens);
void checkEnv(tokenlist* tokens);
void externalCommandexec(tokenlist* tokens);
void ioRedirection(tokenlist* tokens);
void handle_piping(tokenlist** commands, int num_commands);
void execute_pipeline(char** command1, char** command2, char** command3);
int is_cd_command(tokenlist* tokens);
int is_exit_command(tokenlist* tokens);
int is_jobs_command(tokenlist* tokens);
int is_builtin(const char* command);
int is_executable(const char *command);
int cmd_cd(tokenlist* tokens);
void change_directory(char *path);
void store_command(const char *cmd);
void exit_shell(JobManager *job_manager);
void jobs_list(JobManager *job_manager);
void background_process(tokenlist* tokens, JobManager* job_manager);
