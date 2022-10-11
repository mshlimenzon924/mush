/* Mush2 Asgn 6 Header 
   By Mira Shlimenzon */

#ifndef MUSH
#define MUSH
#include "mush.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

#define INTERACTIVE 0 /* print prompt */
#define BATCH 1
#define READ_END 0
#define WRITE_END 1

void freeAllFds(int ** tbforpipefds, int numOfPipes, int v);
int closeAllFds(int ** tbforpipefds, int numOfPipes, int v);

#endif
