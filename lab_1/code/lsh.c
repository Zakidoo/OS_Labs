/*
 * Main source file for the lsh shell program.
 *
 * You are free to add functions to this file.
 * If you want to add functions in separate files,
 * you will need to modify CMakeLists.txt to compile
 * your additional files.
 *
 * Add appropriate comments to make your code
 * easier for us to grade.
 *
 * Using assert statements is a good way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
void execute_program(Command *cmd);

int main(void)
{
  signal(SIGINT, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);

  setpgid(0, 0);
  tcsetpgrp(STDIN_FILENO, getpgrp());
  for (;;)
  {
    char *line;
    line = readline("> ");

    if (line == NULL)
    {
      printf("\n");
      break;
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If the stripped line is not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Print the parsed command
        execute_program(&cmd);
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Free the input buffer
    free(line);
  }
  return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}
//
void execute_program(Command *cmd)
{
  Pgm *pgm = cmd->pgm;
  int background = cmd->background;
  if (strcmp(pgm->pgmlist[0], "cd") == 0)
  {
    if (pgm->pgmlist[1] != NULL)
    {
      if (chdir(pgm->pgmlist[1]) != 0)
        perror("cd");
    }
    return;
  }

  if (strcmp(pgm->pgmlist[0], "exit") == 0)
  {
    exit(0);
  }

  pid_t pid = fork();

  if (pid < 0)
  {
    perror("fork");
    return;
  }

  if (pid == 0)
  {
    setpgid(0, 0);
    signal(SIGINT, SIG_DFL);

    if (cmd->rstdin != NULL)
    {
      int fd = open(cmd->rstdin, O_RDONLY);
      if (fd < 0)
      {
        perror(cmd->rstdin);
        exit(EXIT_FAILURE);
      }
      dup2(fd, STDIN_FILENO);
      close(fd);
    }
    if (cmd->rstdout != NULL)
    {
      int fd = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0)
      {
        perror(cmd->rstdout);
        exit(EXIT_FAILURE);
      }

      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    execvp(pgm->pgmlist[0], pgm->pgmlist);

    perror("execvp");
    exit(EXIT_FAILURE);
  }
  setpgid(pid, pid);

  if (!background)
  {
    if (isatty(STDIN_FILENO))
    {
      tcsetpgrp(STDIN_FILENO, pid);
    }
    waitpid(pid, NULL, 0);

    if (isatty(STDIN_FILENO))
    {
      tcsetpgrp(STDIN_FILENO, getpgrp());
    }
  }
}
/* Print a linked list of Pgm structures.
 *
 * Helper function, no need to change. It may be useful to study for inspiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is stored in reverse order, so print
     * it in reverse to restore the original order.
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}