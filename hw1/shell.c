#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

#include "tokenizer.h"

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_builtin(struct tokens *tokens);
int file_exist(const char *full_filename);
char* path_name_combine(const char * path, const char * name);

char* path_name_combine(const char* path, const char* name){
    char res[strlen(path)+strlen(name)+1];
    strcpy(res, path);
    strcat(res, "/");
    strcat(res, name);
    //strcat(res, "\0");
    return res;
}

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_cd, "cd", "change current working directory"},
  {cmd_pwd, "pwd", "show current working directory"},
  //{cmd_builtin, "builtin", "usr built-in command under /usr/bin"},
};

/* Prints a helpful description for the given command */
int cmd_help(struct tokens *tokens) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(struct tokens *tokens) {
  exit(0);
}
/* Change current working directory */
int cmd_cd(struct tokens *tokens){
  char *dir_name = tokens_get_token(tokens, 1);

  if(dir_name == NULL){
    fputs("cd: Missing working directory\n", stderr);
    return -1;
  }
  if(chdir(dir_name) == -1){
    perror("cd");
    puts("");
  }
  return 0;
}

/* Show current working directory */
int cmd_pwd(struct tokens *tokens){
  char *current_dir_name = getcwd(NULL, 0);
  if(current_dir_name == NULL){
    perror("pwd");
    puts("");
    return errno;
  }
  printf("%s\n", current_dir_name);
  free(current_dir_name);
  return 0;
}

/* Call build-in command under /usr/bin */
int cmd_builtin(struct tokens *tokens){
  char *cmd_filename = tokens_get_token(tokens, 0);
  char *file_name = tokens_get_token(tokens, 1);

  if(cmd_filename == NULL){
    fputs("built-in command is not given correctly\n", stderr);
    return -1;
  }
  if(file_name == NULL){
    fputs("file name is not given correctly\n", stderr);
    return -1;
  }

  //printf("cmd_filename: %s", cmd_filename);
  pid_t pid = fork();
  if(pid == -1){
    fputs("Failed to fork a child process to address user input\n", stderr);
    return -1;
  }else if(pid > 0){
    //int status;
    wait(NULL);
//    while (pid = waitpid(-1, NULL, 0)) {
//      if(errno == ECHILD) {
//        break;
//      }
//    }
  }else{
    // we will handle user input command by exec functions here
    //execl(cmd_fullpath, file_name);
    int len = tokens_get_length(tokens);
//    printf("command length: %d\n", len);
    char* builtin_argv[len+1];
    for(int i = 0; i < len; i++){
      builtin_argv[i] = tokens_get_token(tokens, i);
    }
    builtin_argv[0] = cmd_filename;
    builtin_argv[len] = (char *)NULL;
    
   //printf("argv0: %s, cmd_filename: %s\n", builtin_argv[0], cmd_filename);
//    char *builtin_argv[] = {"wc", file_name, (char *) NULL};
    //int res = execv(cmd_fullpath, builtin_argv);
    int res = 0;
//    printf("cmd_filename: %s\n", builtin_argv[0]);
//    bool iscmdexist = file_exist(cmd_filename);
//    if(iscmdexist){
//      printf("exist\n");
//    }else{
//      printf("not exist\n");
//    }
    if(file_exist(cmd_filename)){
      printf("%s,%s,%s,%s,\n", cmd_filename, builtin_argv[0], builtin_argv[1],builtin_argv[2]);
      res = execv(cmd_filename, builtin_argv);
    }else{
//      char* env_path = getenv("PATH");
//      printf("environment path: %s\n", env_path);
      char* env_path = getenv("PATH");
      char* pch;
      pch = strtok(env_path, ":");
      //printf("%s\n", env_path);
      //printf("%d\n", res);
      while(pch != NULL){
        //pch = strtok(NULL, ":");
        printf("%s,\n", path_name_combine(pch, cmd_filename));
        if(file_exist( path_name_combine(pch, cmd_filename) )){
          printf("Find\n");
          break;
        }else{
            pch = strtok(NULL, ":");
        }
      }
      if(pch == NULL){
        perror("I cannot find command");
      }else{
        char* full_filename = path_name_combine(pch, cmd_filename);
        builtin_argv[0] = full_filename;
        printf("%s,%s,%s,%s,\n", full_filename, builtin_argv[0], builtin_argv[1], builtin_argv[2]);
        res = execv(full_filename, builtin_argv);
        printf("execv success\n");
      }
    }
    if(res == -1){
      perror("execv error");
    }

    //printf("will fill exec family functions later on\n");
    exit(0);
  }
  return 0;
}

int file_exist(const char* full_filename){
  struct stat st;
  int res = stat(full_filename, &st);
  return res==0;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(int argc, char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      //fprintf(stdout, "This shell doesn't know how to run programs.\n");
      cmd_builtin(tokens);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
