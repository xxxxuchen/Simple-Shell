/**
 * Author: Xu Chen
 * McGill ID: 260952566
 * Assumptions: 
 * - jobs command will list the PID and the command name (with arguments) of the background process.
 * - For fg command, use the PID that listed in the jobs command to bring the process back to the foreground.
 * - If there is no PID provided, the last background process will be brought back to the foreground.
 * - echo hello world will print hello world (without quotes). echo "hello world" will print "hello world" (with quotes).
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// getcmd() reads in the next command line, separating it into distinct tokens
int getcmd(char *prompt, char *args[], int *background) {
  int length, i = 0;
  char *token, *loc;
  char *line = NULL;
  size_t linecap = 0;
  printf("%s", prompt);
  length = getline(&line, &linecap, stdin);
  if (length <= 0) {
    exit(-1);
  }
  // Check if background is specified..
  if ((loc = index(line, '&')) != NULL) {
    *background = 1;
    *loc = ' ';
  } else
    *background = 0;

  while ((token = strsep(&line, " \t\n")) != NULL) {
    for (int j = 0; j < strlen(token); j++)
      if (token[j] <= 32)
        token[j] = '\0';
    if (strlen(token) > 0)
      args[i++] = token;
  }
  free(token);
  free(line);
  return i; // number of tokens
}

struct bgProcess {
  int pid; // process id
  char *cmd; // command string, e.g. ls -l
};

// a helper function to remove a process with its pid from the background list
int removeProcess(struct bgProcess list[], int size, int pid) {
  for (int i = 0; i < size; i++) {
    if (list[i].pid == pid) {
      for (int j = i; j < size - 1; j++) {
        list[j] = list[j + 1];
      }
      return 1; // success remove
    }
  }
}

/* _________self built-in commands__________ */
char *echo(char *args[], int cnt) {
  char *str = malloc(1024);
  strcpy(str, "");
  for (int i = 1; i < cnt; i++) {
    strcat(str, args[i]);
    strcat(str, " ");
  }
  return str;
}

void cd(char *args[], int cnt) {
  if (cnt == 1) {
    printf("Please provide a directory!\n");
  } else {
    if (chdir(args[1]) != 0) {
      printf("Please enter a valid dirctory!\n");
    }
  }
}

void pwd() {
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  printf("Current working dirctory: %s\n", cwd);
}

void myExit(struct bgProcess list[], int size) {
  // kill all background processes first
  for (int i = 0; i < size; i++) {
    kill(list[i].pid, SIGKILL);
  }
  exit(0);
}

void fg(char *args[], int cnt, struct bgProcess bgList[], int *bgSize) {
  if (*bgSize == 0) {
    printf("You have not created the background process yet...\n");
    return;
  }
  int pid;
  int status;
  if (cnt == 1) { // if no pid, bring the last background process to the foreground
    pid = bgList[*bgSize - 1].pid;
    waitpid(pid, &status, 0);
  } else {
    int pid = atoi(args[1]);
    waitpid(pid, &status, 0);
  }
  // remove the process from the background list
  if (removeProcess(bgList, *bgSize, pid)) {
    printf("Process %d has been removed from the background list.\n", pid);
  } else {
    printf("Process %d not found in the background list.\n", pid);
  }
  *bgSize -= 1;
}

void jobs(struct bgProcess list[], int size) {
  if (size == 0) {
    printf("There is no background process.., use & to run process background\n");
    return;
  }
  printf("PID\tCMD\t(Please pass PID to fg command to bring it back to the foreground)\n");
  for (int i = 0; i < size; i++) {
    printf("%d %s\n", list[i].pid, list[i].cmd);
  }
}

int main(void) {
  int bg = 0;
  struct bgProcess bgList[16];
  int bgSize = 0;

  while (1) {
    char *args[20];
    bg = 0;
    int cnt = getcmd("\nsh>>  ", args, &bg);
    if (cnt == 0){
      printf("Please enter a valid command!\n");
      continue;
    }
    args[cnt] = NULL;

    // check if bgList is full
    if (bgSize == 16 && bg) {
      printf("Background is full, you may want to use fg command to bring some process foreground first\n");
      continue;
    }

    // check if it is a built-in command
    if (strcmp(args[0], "echo") == 0) {
      char *str = echo(args, cnt);
      printf("%s\n", str);
      free(str);
      continue;
    }
    if (strcmp(args[0], "cd") == 0) {
      cd(args, cnt);
      continue;
    }
    if (strcmp(args[0], "pwd") == 0) {
      pwd();
      continue;
    }
    if (strcmp(args[0], "exit") == 0) {
      myExit(bgList, bgSize);
      continue;
    }
    if (strcmp(args[0], "fg") == 0) {
      fg(args, cnt, bgList, &bgSize);
      continue;
    }
    if (strcmp(args[0], "jobs") == 0) {
      printf("bgSize: %d\n", bgSize);
      jobs(bgList, bgSize);
      continue;
    }

    // check if need pipe
    int isPipe = 0;
    int pipeIndex = 0;
    for (int i = 0; i < cnt; i++) {
      if (strcmp(args[i], "|") == 0) {
        isPipe = 1;
        args[i] = NULL;
        pipeIndex = i;
      }
    }
    // if pipe, then let parent process wait for the child process to finish executing the input command
    if (isPipe == 1) {
      printf("Pipe detected..\n");
      char *args1[20]; // fisrt command and its arguments
      char *args2[20]; // second command and its arguments
      int i = 0;
      for (; i < pipeIndex; i++) {
        args1[i] = args[i];
      }
      args1[i] = NULL;
      int j = 0;
      for (i = pipeIndex + 1; i < cnt; i++) {
        args2[j] = args[i];
        j++;
      }
      args2[j] = NULL;

      // create a pipe
      int fd[2];
      pipe(fd);
      int cid = fork();
      if (cid == 0) {
        close(1);
        dup(fd[1]);
        close(fd[0]);
        execvp(args1[0], args1);
        printf("Command not found..\n");
        exit(0);
      } else if (cid > 0) {
        int cid2 = fork();
        if (cid2 == 0) {
          close(0);
          dup(fd[0]);
          close(fd[1]);
          execvp(args2[0], args2);
          printf("Command not found..\n");
          exit(0);
        } else if (cid2 > 0) {
          close(fd[0]);
          close(fd[1]);
          wait(NULL);
          wait(NULL);
        } else {
          printf("Error..\n");
          exit(0);
        }
      } else {
        printf("Error..\n");
        exit(0);
      }
      continue;
    }

    int cid = fork();
    if (cid == 0) {
      // check if need to redirect..
      int isRedirect = 0;
      char *file;
      for (int i = 0; i < cnt; i++) {
        if (strcmp(args[i], ">") == 0) {
          isRedirect = 1;
          file = args[i + 1];
          args[i] = NULL;
        }
      }
      if (isRedirect) {
        printf("Redirect detected..\n");
        close(1);
        open(file, O_CREAT | O_WRONLY | O_TRUNC, 0777);
      }
      execvp(args[0], args);
      printf("Command not found..\n");
      exit(0);
    } else if (cid > 0) {
      if (bg == 0) { // foreground
        waitpid(cid, NULL, 0);
      } else { // background, construct a bgProcess and store it in bgList
        struct bgProcess bgp;
        bgp.pid = cid;
        char *arguments = echo(args, cnt);
        char *wholeCmd = args[0];
        strcat(wholeCmd, " ");
        strcat(wholeCmd, arguments);
        free(arguments);
        bgp.cmd = wholeCmd;
        bgList[bgSize++] = bgp;
      }
    } else {
      printf("Error..\n");
      exit(0);
    }
  }
  return 0;
}