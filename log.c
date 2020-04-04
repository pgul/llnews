#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "conf.h"
#include "send.h"

#ifdef LOG
void open_log(char *name, int opt1, int opt2)
{}

void log(int level, char *format, ...)
{
  va_list args;
  FILE *flog;
  time_t curtime;
  struct tm *curtm;
  char stime[256];
  char *p;

  flog=fopen(LOG, "a");
  if (flog==NULL) return;
  curtime=time(NULL);
  curtm=localtime(&curtime);
  strcpy(stime, asctime(curtm));
  if ((p=strchr(stime, '\n'))!=NULL) *p='\0';
  fprintf(flog, "%s ", stime);
  va_start(args, format);
  vfprintf(flog, format, args);
  va_end(args);
  fprintf(flog, "\n");
  fclose(flog);
}
#endif

