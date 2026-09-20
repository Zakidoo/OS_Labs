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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>
#include <sys/wait.h>

#include "parse.h"

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
static void execute_command(Command *cmd);
static void reap_zombies(void);
void stripwhite(char *);
void handler(int sig);
static pid_t foreground_pgid = 0;
static volatile sig_atomic_t starting_foreground = 0;


int main(void)
{
  for (;;)
  {
    reap_zombies(); 
    char *line;
    signal(SIGINT, handler);
    signal(SIGCHLD, handler);
    line = readline("> ");
    if(line == NULL)
    {
      printf("exit\n");
      //send sighup to all background processes
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
        print_cmd(&cmd);
        execute_command(&cmd);
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

static void execute_command(Command *cmd)
{

  assert(cmd != NULL);
  assert(cmd->pgm != NULL);
  Pgm *programs[20];
  Pgm *program = cmd->pgm;
  int count = 0;

  while (program != NULL) {
    programs[count++] = program;
    program = program->next;
  }

  for (int i = 0; i < count / 2; i++) {
    Pgm *tmp = programs[i];
    programs[i] = programs[count - i - 1];
    programs[count - i - 1] = tmp;
  }

  pid_t pgid = 0;
  int previous_read = -1;

  starting_foreground = !cmd->background;

  for (int i = 0; i < count; i++) {
   if (strcmp(programs[i]->pgmlist[0], "cd") == 0)
   {
     chdir(programs[i]->pgmlist[1]);
     return;
   }
   else if (strcmp(programs[i]->pgmlist[0], "exit") == 0)
   {
     exit(0);
   }
    int current_pipe[2];

    if (i < count - 1 && pipe(current_pipe) == -1) {
      perror("pipe");
      return;
    }

    pid_t pid = fork();

    if (pid == -1) {
      perror("fork");
      return;
    }

    if (pid == 0) {
      if (pgid == 0)
        setpgid(0, 0);
      else
        setpgid(0, pgid);

      signal(SIGINT, SIG_DFL);

      if (previous_read != -1) {
        dup2(previous_read, STDIN_FILENO);
        close(previous_read);
      }

      if (i < count - 1) {
        close(current_pipe[0]);
        dup2(current_pipe[1], STDOUT_FILENO);
        close(current_pipe[1]);
      }

      execvp(programs[i]->pgmlist[0], programs[i]->pgmlist);
      perror(programs[i]->pgmlist[0]);
      _exit(127);
    }

    if (pgid == 0)
      pgid = pid;

    setpgid(pid, pgid);

    if (previous_read != -1)
      close(previous_read);

    if (i < count - 1) {
      close(current_pipe[1]);
      previous_read = current_pipe[0];
    }
  }
   if (previous_read != -1)
    close(previous_read);

  if (!cmd->background)
    foreground_pgid = pgid;
  starting_foreground = 0;

  if (!cmd->background) {
    int status;
    for (;;) {
      pid_t waited_pid = waitpid(-pgid, &status, 0);
      if (waited_pid > 0)
        continue;
      if (waited_pid == -1 && errno == EINTR)
        continue;
      if (waited_pid == -1 && errno != ECHILD)
        perror("waitpid");
      break;
    }
  }
  starting_foreground = 0;
  foreground_pgid = 0;
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
static void reap_zombies(void)
{
  while (waitpid(-1, NULL, WNOHANG) > 0);

}

void handler(int sig)
{
  if (sig == SIGINT)
  {
    if (foreground_pgid != 0)
    {
      kill(-foreground_pgid, SIGINT);
    }
  }
  else if (sig == SIGCHLD &&
           foreground_pgid == 0 &&
           !starting_foreground)
  {
    reap_zombies();
  }
}
