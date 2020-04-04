#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <netinet/in.h>
#include "conf.h"
#include "send.h"

#define INITSIZE	16384

int main(int argc, char *argv[])
{
  int  mtu=MTU;
  int  i, n;
  char buf[MTU];

  /* params */
  if ((argc<3) || (!isdigit(argv[2][0])))
  {
    puts("Usage: sendtest <host> <n>\n, <n> - number of packets");
    return 0;
  }
  openlog("file2space", LOG_PID, LOG_NEWS);
  open_socket(argv[1]);
  n=atol(argv[2]);
  for (i=0; i<mtu; i++)
    buf[i]=i;
  for (i=0; i<n; i++)
  {
    printf("%d\n", i);
    *(long *)buf=htonl(i);
    sendbuf(buf, mtu);
  }
  close_socket();
  return 0;
}
