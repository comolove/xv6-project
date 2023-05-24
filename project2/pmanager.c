#include "types.h"
#include "user.h"
#include "fcntl.h"

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit();
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

int
getcmd(char *buf, int nbuf)
{
  printf(2, "- ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  buf[strlen(buf)-1] = '\0';
  return 0;
}

int stringToInt(char *str) {
  int i = 0;
  int result = 0;

  while (str[i] != '\0') {
    if(str[i] < '0' || str[i] > '9') return -1;
    result = result * 10 + (str[i] - '0');
    i++;
  }

  return result;
}

// Parse command line 
// Split by space ' '
// return string list
char **split(char *str, char *delim, int* cnt) {
  int count = 0, i;
  char *token = str, *betoken;
  while ((token = strchr(token, *delim))) {
    count++;
    token++;
  }

  char **tokens = malloc(sizeof(char *) * (count + 1));
  for(i = 0 ; i < count+1 ; i++) tokens[i] = 0;

  i = 0;
  token = str;
  betoken = str;
  while ((token = strchr(token, *delim))) {
    *token = '\0';
    tokens[i++] = betoken;
    token++;
    betoken = token;
  }
  tokens[i] = betoken;

  *cnt = count + 1;

  return tokens;
}

int main() {
  static char buf[300];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    int pid, count, stacksize, memlimit;
    char* path;
    char** commands = split(buf, " ", &count);

    // just call system call
    if(!strcmp(commands[0],"list")) {
      printprocesses();
    } 

    else if(!strcmp(commands[0],"kill")) {
      // parse
      if(count < 2 || (pid = stringToInt(commands[1])) == -1) {
        printf(2,"undefined command\n");
        continue;
      }

      // execute kill
      if(kill(pid) == -1) {
        printf(2,"kill failed\n");
        continue;
      }
      
      printf(2,"kill succeed\n");
    } 
    else if(!strcmp(commands[0],"execute")) {
      // parse
      if(count < 3 || (path = commands[1]) == 0 || (stacksize = stringToInt(commands[2])) == -1) {
        printf(2,"undefined command\n");
        continue;
      }

      // to execute synchronous, fork and exec
      if(fork1() == 0) {
        if(exec2(path, &path, stacksize) == -1)
          printf(2,"exec failed\n");
        exit();
      }
    } 
    else if(!strcmp(commands[0],"memlim")) {
      // parse
      if(count < 3 || (pid = stringToInt(commands[1])) == -1 || (memlimit = stringToInt(commands[2])) == -1) {
        printf(2,"undefined command\n");
        continue;
      }

      // set memory limit by setmemorylimit system call
      if(memlimit < 0 || setmemorylimit(pid,memlimit) == -1) {
        printf(2,"memlim failed\n");
        continue;
      }

      printf(2,"memlim succeed\n");
    } 
    else if(!strcmp(commands[0],"exit")) {
      exit();
    } 
    else {
      printf(2,"undefined command\n");
    }
    
  }
  exit();
}