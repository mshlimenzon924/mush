/* Mush2 Asgn 6
   Creating our own shell.
   By Mira Shlimenzon */

#include "mush2.h"

static int signal_caught;

void handler(int signum) {
  /* show that we received signal */
  signal_caught = 1;
}

int main(int argc, char *argv[]) {
  int v = 0; /* help debug */
  int child_count; /* to count for waits */
  int shell_type = INTERACTIVE; 
  FILE *in; /* where we get input from */

  /* set in to be the STDIN_FILENO */
  in = fdopen(STDIN_FILENO, "r");
  if(in == NULL) {
    perror("fdopen\n");
    exit(-1);
  }

  /* 1) Turning off SIGINT with signal handling */
  struct sigaction sa;
  sa.sa_handler = handler; /* set up handler */
  sa.sa_flags = 0; /* no special handling */
  if(sigemptyset(&sa.sa_mask) == -1) { /* masks nothing */
    perror("Sigempty\n");
    exit(-1);
  } 
  signal_caught = 0; /* sets signal_caught to 0 */
  if(sigaction(SIGINT, &sa, NULL) == -1) {
    /* sets that when SIGINT happens we handle it */
    perror("Sigaction\n");
    exit(-1);
  }

  /* checking if we are in BATCH mode */
  if(!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    shell_type = BATCH;
  }

  /* 2) Figure out where we are reading in and writing out to */
  if(argc > 1) { /* if given other instruction besides mush2 */
    /* let's check if argument given is a proper file */
    in = fopen(argv[1], "r");
    if(in == NULL) { /* failure to open file for read */
      printf("Non-existent input file.\n");
      exit(-1); /* not proper file so we need to exit */
    }
    else { /* succesfully opened */
      shell_type = BATCH;
    }
  }

  if(v) { /* debugging */
    printf("%d\n", isatty(STDIN_FILENO)); /* this is what is in stdin file no */
    printf("%d\n", isatty(STDOUT_FILENO)); /* this is what is in stdout file no */
    printf("%d\n", shell_type); /* this is shell_type */
  }

  /* 3) Looping through each line of command line - breaks out*/
  while(1) {
    int i, j, k = 0;
    child_count = 0;

    /* Print out to prompt if in interactive mode */
    if(shell_type == INTERACTIVE) {
      if(write(STDOUT_FILENO, "8-P ", 5) == -1) {
        perror("write\n");
        continue; /* failure of system call */
        
        /* but I do want to note that in a way
           this seems stupid bc what if the problem
           continues and we just forever loop */
      }
    }

    /* Read in from parser */
    pipeline p; /* pointer to struct pipeline */
    char *cl_buffer = readLongString(in); 

    /* Encountered EOF (due to EOF, error, or SIGINT) */
    if(cl_buffer == NULL) {
      if(signal_caught) { /* SIGINT */
        signal_caught = 0;
        printf("\n");
        continue; /* abandon + reprompt command line */
      }
      else if(feof(in)) { /* EOF */
        printf("\n");
        /* No more to read in */
        break; 
      }
      else { /* Other error */
        /* reprompt */
        fprintf(stderr, "other error\n");
        perror("");
        break;
      }
    }
    p = crack_pipeline(cl_buffer);
    if(p == NULL) {
      /* if we fail */
      free(cl_buffer);
      continue;
    }
    if(v) {
      print_pipeline(stdout, p); /* to debug */
    }

  /* 5) Launching commands: either cd or loop through commands */
    if(strcmp(p->stage->argv[0], "cd")) { /* isn't cd */
      int numOfPipes = p->length - 1;
      int issue = 0;
      int **tbforpipefds;

      if(v) {
        printf("%d\n", numOfPipes);
      }

      /* Setting up pipe fd array */
      if(numOfPipes > 0) {
        tbforpipefds = (int **)calloc(numOfPipes, sizeof(int *));
        if(tbforpipefds == NULL) {
          perror("calloc\n");
          free_pipeline(p);
          free(cl_buffer);
          continue;
        }

        if(v){
          for(i = 0; i < numOfPipes; i++) {
            printf("%d\n", tbforpipefds[i][0]);
          }
        }

        issue = 0;
        for(i = 0; i < numOfPipes; i++) {
          tbforpipefds[i] = (int *)calloc(2, sizeof(int));
          /* problem creating stuff here */
          if(tbforpipefds[i] == NULL) {
            perror("calloc\n");
            issue = 1;
            break;
          }
    
          /* Create pipes */
          if(pipe(tbforpipefds[i]) == -1) {
            perror("pipe\n");
            issue = 1;
            break;
          }
        }
      
        if(issue) {
          perror("issue with setting up pipes\n");
          /* clean up pipes */
          for(j = 0; j < i; j++) {
            /* clean up as far as allocation worked */
            free(tbforpipefds[j]);
          }
          free(tbforpipefds); 
          free_pipeline(p);
          free(cl_buffer);
          continue;
        }
      }

      if(v) {
        /* check that my pipes work */
        for(k = 0; k < numOfPipes; k++) {
          for(j = 0; j < 2; j++){
           printf("%d\n", tbforpipefds[k][j]);
          }
        }
      }

      issue = 0;
      int child; /* pid */
      int first;
      int last;
      
      /* Time to create the children */
      for(i = 0; i < p->length; i++) {
        /* let's figure out if first, middle, or last child in piping */
        first = 0;
        last = 0;
        if(i == 0) {
          first = 1;
        }
        if(i == p->length - 1) {
          last = 1;
        }

        /* created child */
        if((child = fork()) == -1) {
          perror("forking issue\n");
          issue = 1;
          break;
        }
        child_count++;

        if(child == 0) { /* child */
          int in_fd, out_fd;
      
          if(v){
            for(j = 0; j < p->stage[i].argc; j++) {
              printf("i:%d j:%d %s\n",i ,j, p->stage[i].argv[j]);
            }
          }

          /* Turn on SIGINT for child */
          sigset_t new_set;
          if(sigemptyset(&new_set) == -1) {
            perror("sigempty\n");
            /* if there is any problem in the child, 
               child should report error,
               close all fds, and exit */
            closeAllFds(tbforpipefds, numOfPipes, v);
            /* I'm also not checking closeAllFds for failure
            bc let's be real we already failing let's just 
            leave and abort as best as we can */
            exit(-1);
          }
          if(sigaddset(&new_set, SIGINT) == -1) {
            perror("sigadd\n");
            /* the rest of the problems with system calls
              will follow suit */
            closeAllFds(tbforpipefds, numOfPipes, v);
            exit(-1);
          }
          if(sigprocmask(SIG_UNBLOCK, &new_set, NULL) == -1) {
            perror("sigproc\n");
            closeAllFds(tbforpipefds, numOfPipes, v);
            exit(-1);
          }

          /* setting up input + output given */
          if(p->stage[i].inname != NULL) {
            /* given file to get input from */
            if((in_fd = open(p->stage[i].inname, O_RDONLY))== -1) {
              perror("open\n");
              closeAllFds(tbforpipefds, numOfPipes, v);
              exit(-1);
            }
            /* duping closes stdin and replaces with in_fd */
            if(dup2(in_fd, STDIN_FILENO) == -1) {
              perror("dup2\n");
              closeAllFds(tbforpipefds, numOfPipes, v);
              exit(-1);
            }
            /* but after duping, close in_fd - don't need fd anymore */
            if(close(in_fd) == -1){
              perror("close in_fd\n");
              closeAllFds(tbforpipefds, numOfPipes, v);
              exit(-1);
            }
          }
          if(p->stage[i].outname != NULL) {
            /* given file to get output from */
            /* if file created, rw for all */
            /* if already exists, need to be truncated */ 
            if((out_fd = open(p->stage[i].outname, O_WRONLY | O_CREAT | O_TRUNC, 
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1){
              perror("open\n");
              closeAllFds(tbforpipefds, numOfPipes, v);
              exit(-1);
            }

            if(dup2(out_fd, STDOUT_FILENO) == -1) {
              perror("dup2\n");
              closeAllFds(tbforpipefds, numOfPipes, v);
              exit(-1);
            }

            /* but after duping, close out_fd - don't need fd anymore */
            if(close(out_fd) == -1){
              perror("close out_fd\n");
              closeAllFds(tbforpipefds, numOfPipes, v);
              exit(-1);
            }
          }
          /* Messing with pipe here */
          if(numOfPipes >= 1) {
            /* duping fds */
            if(first) {
              /* changing stdout to be the write end of pipe */
              /* dup2 automatically closes stdout and replaces it */
              if(dup2(tbforpipefds[i][1], STDOUT_FILENO) == -1) {
                perror("dup2\n");
                closeAllFds(tbforpipefds, numOfPipes, v);
                exit(-1);
              }
            }
            else if(last) {
              /* changign stdin to be the read end of pipe before */
              if(dup2(tbforpipefds[i-1][0], STDIN_FILENO) == -1) {
                perror("dup2\n");
                closeAllFds(tbforpipefds, numOfPipes, v);
                exit(-1);
              }
            }
            else {
              if(dup2(tbforpipefds[i][1], STDOUT_FILENO) == -1) {
                perror("dup2\n");
                closeAllFds(tbforpipefds, numOfPipes, v);
                exit(-1);
              }

              if(dup2(tbforpipefds[i-1][0], STDIN_FILENO) == -1) {
                perror("dup2\n");
                closeAllFds(tbforpipefds, numOfPipes, v);
                exit(-1);
              }
            }
          }

          /* Closing all fds */
          if(numOfPipes > 0) {
            if(v){
              printf("closing child fd's\n");
            }
            if(closeAllFds(tbforpipefds, numOfPipes, v)){
              perror("closing issue\n");
              exit(-1);
            }
          }
           
          /* Execing */
          execvp(p->stage[i].argv[0], p->stage[i].argv);
          fprintf(stderr, "%s: ", p->stage[i].argv[0]);
          perror("");
          exit(-1);
        }  
      }

      /* if pipe exists let's clean up */
      if(numOfPipes > 0) {
        int result = closeAllFds(tbforpipefds, numOfPipes, v);
        freeAllFds(tbforpipefds, numOfPipes, v);
        if(result){
          perror("closing issue\n");
          free_pipeline(p);
          free(cl_buffer);
          continue;
        }
      }  
    }
    else { /* cd command */
      /* If name not present, goes to user's home directory */
      if(p->stage->argc == 1) {
        char *homedir; 
        if((homedir = getenv("HOME")) == NULL) {
          if((homedir = getpwuid(getuid())->pw_dir) == NULL) {
            fprintf(stderr, "unable to determine home directory\n");
            free_pipeline(p);
            free(cl_buffer);
            continue; 
          }
        }
        if(chdir(homedir) == -1) {
          perror("chdir\n");
          free_pipeline(p);
          free(cl_buffer);
          continue;
        }
      }
      /* If argument given, goes to that directory */
      else if(p->stage->argc == 2) {
        if(chdir(p->stage->argv[1]) == -1) {
          /* If chdir fails */
          printf("%s: No such file or directory\n", p->stage->argv[1]);
          free_pipeline(p); 
          free(cl_buffer);
          continue;
        }
      }
      else {
        printf("Gave cd too many arguments.\n");
        free_pipeline(p); 
        free(cl_buffer);
        continue;
      }
    }

  /* 8) Parent clean up! */
  /* wait for all of its children to terminate */
  int status;
  if(v) { 
    printf("%d\n", child_count);
  }
  for(i = 0; i < child_count; i++) {
    if(wait(&status) == -1) {
      if(errno == EINTR) {
        i--;
      }
      else {
        perror("other wait error\n");
        /* should just exit */
        break; 
      }
    }
  }
    /* 9) Cleans up shell and we go again */
    /* freeing everything we need */
    free_pipeline(p);
    free(cl_buffer);

    if(signal_caught && shell_type == BATCH) {
      break;
    }
    else if(signal_caught) {
     signal_caught = 0;
    }
  }

  yylex_destroy();
  return 0;
}

/* close all file descriptors */
int closeAllFds(int ** tbforpipefds, int numOfPipes, int v) {
  int issue = 0, i, j;

  if(v) {
    /* check that my pipes work */
    for(i = 0; i < numOfPipes; i++) {
      printf("close\n");
      for(j = 0; j < 2; j++){
        printf("%d\n", tbforpipefds[i][j]);
      }
    }
  } 

  for(i = 0; i < numOfPipes; i++) {
    for(j = 0; j < 2; j++) {
      if(close(tbforpipefds[i][j]) == -1) {
        perror("close\n");
        issue = 1;
        break;
      }
    }
  }
  /* 1 on failure and 0 on success */
  return issue; 
}

/* frees all file descriptors in pipe */
void freeAllFds(int ** tbforpipefds, int numOfPipes, int v) {
  int i, j;

  if(v) {
    printf("free\n");
    /* check that my pipes work */
    for(i = 0; i < numOfPipes; i++) {
      for(j = 0; j < 2; j++){
        printf("%d\n", tbforpipefds[i][j]);
      }
    }
  } 

  for(i = 0; i < numOfPipes; i++) {
    free(tbforpipefds[i]);
  }

  free(tbforpipefds);
}