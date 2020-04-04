#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "conf.h"
#include "recv.h"

#ifdef LOG
void open_log(char *name, int opt1, int opt2)
{}

void log(int level, char *format, ...)
{
  va_list args;
  FILE *flog;

  flog=fopen(LOG, "a");
  if (flog==NULL) return;
  va_start(args, format);
  switch (level)
  {
    case LOG_CRIT:   fprintf(flog, "CRIT: "); break;
    case LOG_ERR:    fprintf(flog, "ERR:  "); break;
    case LOG_NOTICE: fprintf(flog, "NOT:  "); break;
    case LOG_WARNING:fprintf(flog, "WARN: "); break;
    case LOG_INFO:   fprintf(flog, "INFO: "); break;
    case LOG_DEBUG:  fprintf(flog, "DEBG: "); break;
    default:         fprintf(flog, "UNKN: "); break;
  }
  vfprintf(flog, format, args);
  va_end(args);
  fprintf(flog, "\n");
  fclose(flog);
}
#endif

