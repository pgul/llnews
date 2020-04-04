/* broadcast send file to downlinks via LuckyLink */
/* usage:  file2space <type> [name] < file        */
/*   or    file2space <type> name file            */

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
#include <signal.h>
#include "conf.h"
#include "send.h"

#define INITSIZE	16384

void sigpipe(int signo)
{
  syslog(LOG_ERR, "SIGPIPE received");
}

int main(int argc, char *argv[])
{
  long membufsize, cursend;
  char *membuf=NULL, *buf, *xorbuf;
  int  mtu=MTU;
  int  i, xorsize, packetnum, fromfile;
  long *p;
  FILE *fin;
  long file_type, file_ctime, file_len;
  char *file_name;
  struct stat st;
  int  tries=0;
  int  ll_stream;

  /* params */
  if ((argc<4) || (!isdigit((unsigned char)argv[1][0])) ||
      (!isdigit((unsigned char)argv[3][0])))
  {
    puts("File2Space, ver. 0.2   " __DATE__);
    puts("Copyright (C) Pavel Gulchouck <gul@lucky.net>");
    puts("");
    puts("  Usage:");
    puts("file2space <stream> <remote> <type> [name] < file");
    puts("  or");
    puts("file2space <stream> <remote> <type> name file");
    puts("  Example:");
    puts("file2space 2 news-2.ll.net.ua 100 active.zip /etc/news/active.zip");
    return 0;
  }
  openlog("file2space", LOG_PID, LOG_NEWS);
  ll_stream=atoi(argv[1]);
  file_type=atol(argv[3]);
  if (argc>3)
    file_name=argv[4];
  else
    file_name="";
  if (argc>5)
  {
    fin=fopen(argv[5], "r");
    if (fin==NULL)
    {
      syslog(LOG_ERR, "Can't open %s: %s", argv[5], strerror(errno));
      return 3;
    }
  }
  else
    fin=stdin;

  if (fstat(fileno(fin), &st))
  {
    syslog(LOG_ERR, "Can't fstat: %s", strerror(errno));
    fclose(fin);
    return 3;
  }
  if (!S_ISREG(st.st_mode))
  { int c;

    file_ctime=time(NULL);
    /* read to memory buffer */
    membufsize=INITSIZE;
    membuf=malloc(membufsize);
    if (membuf==NULL)
    { syslog(LOG_ERR, "Can't malloc()");
      return 1;
    }
    for (file_len=0; (c=fgetc(fin))!=EOF; file_len++)
    {
      if (membufsize==file_len)
      { membufsize+=INITSIZE;
        membuf=realloc(membuf, membufsize);
        if (membuf==NULL)
        { syslog(LOG_ERR, "Can't realloc()");
          return 1;
        }
      }
      membuf[file_len]=(char)c;
    }
    fromfile=0;
  }
  else
  {
    file_ctime=st.st_ctime;
    file_len=st.st_size;
    fromfile=1;
  }

  buf=malloc(mtu);
  xorbuf=malloc(mtu);
  if (buf==NULL || xorbuf==NULL)
  { syslog(LOG_ERR, "Can't malloc()");
    return 1;
  }

  sigset(SIGPIPE, sigpipe);

nexttry:
  syslog(LOG_INFO, "Sending %s (%lu bytes)"
#ifdef MULTISTREAM
         " stream %u"
#endif
         " attempt %u"
         // " to %s"
         , file_name, file_len,
#ifdef MULTISTREAM
         ll_stream,
#endif
         tries+1
         // , argv[2]
         );
  if (open_socket(argv[2]))
  { syslog(LOG_ERR, "Can't open_socket!");
    return 2;
  }
  p=(long *)buf;
  *p++=MAGIC_START;
  *p++=htonl(file_len);
  *p++=htonl(file_type);
  *p++=htonl(file_ctime);
  *p++=htonl(ll_stream);
  cursend=0;
  strncpy((char *)p, file_name, mtu-((char *)p-buf)-1);
  i=(char *)p-buf+strlen(file_name)+1;
  if (i>mtu) i=mtu;
  sendbuf(buf, i);
  sendbuf(buf, i); /* first magic packet send twice */
  for (i=0; i<mtu; i++)
    xorbuf[i]=0;
  packetnum=1;
  cursend=0;
  xorsize=0;
  while (cursend!=file_len)
  {
    if (packetnum%XOR_FACTOR==0)
    { xorbuf[0]=(char)(packetnum++);
      sendbuf(xorbuf, xorsize);
      for (i=0; i<mtu; i++)
        xorbuf[i]=0;
      xorsize=0;
    }
    buf[0]=(char)packetnum++;
    for (i=1; i<mtu && cursend!=file_len; i++)
    { int c;

      if (fromfile)
      { if ((c=fgetc(fin))==EOF)
        { syslog(LOG_ERR, "Can't read file: %s", strerror(errno));
          return 5;
        }
        cursend++;
      }
      else
        c=membuf[cursend++];
     
      buf[i]=c;
      xorbuf[i]^=buf[i];
    }
    if (i>xorsize) xorsize=i;
    sendbuf(buf, i);
  }
  if (packetnum>1)
  { xorbuf[0]=(char)packetnum++;
    sendbuf(xorbuf, xorsize);
  }
  close_socket();
//  printf("Sent, waiting for ack...\n");
  if (open_ack_socket(ll_stream))
  { syslog(LOG_ERR, "Can't open_ack_socket!");
    return 2;
  }
  if (waitack(file_name, file_len, file_ctime, file_type, ll_stream))
  {
    if (++tries==TRIES)
    {
      syslog(LOG_ERR, "No ACK for %s"
#ifdef MULTISTREAM
      " (stream %u)"
#endif
      , file_name
#ifdef MULTISTREAM
      , ll_stream
#endif
      );
      return 1;
    }
    if (fromfile)
      fseek(fin, 0, SEEK_SET);
    syslog(LOG_NOTICE, "No ACK for %s"
#ifdef MULTISTREAM
           " (stream %u)"
#endif
           ", retrying", file_name
#ifdef MULTISTREAM
           , ll_stream
#endif
           );
    goto nexttry;
  }
  else
    return 0;
}
