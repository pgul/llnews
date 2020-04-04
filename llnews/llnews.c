/* todo: recover lost of xor-packet and first packet in text xor-block */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <utime.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>
#include <netinet/in.h>
#include "conf.h"
#include "recv.h"

static void chld(int signo)
{
  int status;
  wait(&status); /* no zombie */
  signal(SIGCHLD, chld);
}

static void complete(char *file_name, long file_len, long file_time,
                     long file_type, long ll_stream, char *temp_name)
{ int r;
  struct utimbuf ut;

  sendack(file_name, file_len, file_time, file_type, ll_stream);
  ut.actime=ut.modtime=file_time;
  utime(temp_name, &ut);
  syslog(LOG_NOTICE, "File %s (type %ld) received successfully", file_name, file_type);
repfork:
  if ((r=fork())==-1)
  { 
    if (errno == EINTR) goto repfork;
    syslog(LOG_ERR, "Can't fork(): %s", strerror(errno));
    unlink(temp_name);
    return;
  }
  else if (r==0)
  { char str_type[30];
    signal(SIGCHLD, SIG_DFL);
    sprintf(str_type, "%ld", file_type);
    execl(CMD, CMD, CMDARG str_type, temp_name, NULL);
    syslog(LOG_ERR, "Can't exec %s", CMD);
    exit(1);
  }
}

int main(void)
{
  long *p;
  char *bufrecv[XOR_FACTOR-1];
  char *buf, *xorbuf;
  char file_name[256], tempname[256];
  FILE *fout;
  char *cp;
  int  lostnum, packet_num, xorsize, file_time, file_type, ll_stream;
  int  buf_len;
  long file_len, received, saved;
  int  i, mtu=MTU;

  openlog("llnews", LOG_PID, LOG_NEWS);
  for (i=0; i<XOR_FACTOR-1; i++)
    if ((bufrecv[i]=malloc(mtu))==NULL)
    { syslog(LOG_ERR, "Can't malloc())");
      return 1;
    }
  buf=malloc(mtu);
  xorbuf=malloc(mtu);
  if (buf==NULL || xorbuf==NULL)
  { syslog(LOG_ERR, "Can't malloc())");
    return 1;
  }

  if (open_socket())
    return 2;

  signal(SIGCHLD, chld);

  for (;;)
  {
    recvbuf(buf, &buf_len, mtu, 1);
searchfile:
    if (buf_len<5*sizeof(long)+1)
      continue;
    if (buf[buf_len-1]!='\0')
      continue;
    p=(long *)buf;
    if (*p++ != MAGIC_START)
      continue;
    file_len=ntohl(*p++);
    file_type=ntohl(*p++);
    file_time=ntohl(*p++);
    ll_stream=ntohl(*p++);
    if (strlen((char *)p)+1+4*5!=buf_len)
      continue;
    strncpy(file_name, (char *)p, sizeof(file_name));
    file_name[sizeof(file_name)-1]='\0';
    cp=strrchr(file_name, '/');
    if (cp) cp++;
    else cp=file_name;
    if (cp[0]=='\0')
      snprintf(tempname, sizeof(tempname), "/tmp/llfile.%04X_%08lX",
               getpid(), time(NULL));
    else
      snprintf(tempname, sizeof(tempname), "/tmp/%s", cp);
    tempname[sizeof(tempname)-1]='\0';
    fout=fopen(tempname, "w");
    if (fout==NULL)
    {
      syslog(LOG_ERR, "Can't create %s: %s", tempname, strerror(errno));
      return 3;
    }
    syslog(LOG_INFO, "Receiving \"%s\" (type %d, size %d)",
           file_name, file_type, file_len);

    packet_num=1;
    received=0;
    lostnum=-1;
    xorsize=0;
    saved=0;
    for (i=0; i<mtu; xorbuf[i++]=0);
    for (;;)
    {
      recvbuf(buf, &buf_len, mtu, 0);
      if (buf_len==0)
      { if (received==file_len)
          goto lostlast;
        else
          goto lost;
      }
      if ((buf[0]==(char)(packet_num+1)) || (received==file_len))
      { /* packet lost */
        if (lostnum!=-1)
        {
lost:
          syslog(LOG_NOTICE, "Error receiving %s (type %ld, size %ld)",
              file_name, file_type, file_len);
          fclose(fout);
          unlink(tempname);
          goto searchfile;
        }
        lostnum=packet_num%XOR_FACTOR;
        packet_num++;
        if ((lostnum==0) || (received==file_len))
        { /* xor packet lost */
lostlast:
          for (i=0; i<XOR_FACTOR-1 && saved<file_len; i++)
          { int l=(saved+mtu-1>file_len) ? (int)(file_len-saved) : mtu-1;
            if (fwrite(bufrecv[i]+1, l, 1, fout)!=1)
            { syslog(LOG_ERR, "Can't write to %s: %s", tempname, strerror(errno));
              fclose(fout);
              unlink(tempname);
              return 4;
            }
            saved+=l;
          }
          if (received==file_len)
          { fclose(fout);
            complete(file_name, file_len, file_time, file_type, ll_stream,
                     tempname);
            goto searchfile;
          }
          for (i=0; i<mtu; xorbuf[i++]=0);
          xorsize=0;
          lostnum=-1;
        }
        else
        {
          /* size of lost packet? */
          if (file_len-received-1>=mtu)
            i=mtu;
          else
            i=file_len-received;
          received+=i-1;
          if (i>xorsize) i=xorsize;
        }
      }
      if (buf[0]!=(char)packet_num)
      { 
        p=(long *)buf;
        if (buf_len<5*4+1 ||
            *p++ != MAGIC_START ||
            buf[buf_len-1]!=0)
#if 0
          continue; /* left packet, ignore */
#else
          goto lost;
#endif
        if (*p++!=htonl(file_len)) goto lost;
        if (*p++!=htonl(file_type)) goto lost;
        if (*p++!=htonl(file_time)) goto lost;
        if (*p++!=htonl(ll_stream)) goto lost;
        if (strcmp((char *)p, file_name)) goto lost;
        continue;
      }
      /* correct packet */
      if ((packet_num%XOR_FACTOR==0) || (received==file_len))
      {
        if (buf_len!=xorsize)
          goto lost;
        if (lostnum==-1)
        { /* check */
          for (i=1; i<xorsize; i++)
            if (xorbuf[i]!=buf[i])
              goto lost;
        }
        else
        { /* recover */
          bufrecv[lostnum-1][0]=lostnum;
          for (i=1; i<xorsize; i++)
            bufrecv[lostnum-1][i]=xorbuf[i]^buf[i];
          syslog(LOG_DEBUG, "lost packet recovered");
        }
        /* save bufrecv */
        for (i=0; i<XOR_FACTOR-1 && saved<file_len; i++)
        { int l=(saved+mtu-1>file_len) ? (int)(file_len-saved) : mtu-1;
          if (fwrite(bufrecv[i]+1, l, 1, fout)!=1)
          { syslog(LOG_ERR, "Can't write to %s: %s", tempname, strerror(errno));
            fclose(fout);
            unlink(tempname);
            return 4;
          }
          saved+=l;
        }

        if (received==file_len)
        { fclose(fout);
          complete(file_name, file_len, file_time, file_type, ll_stream,
                   tempname);
          break;
        }
        for (i=0; i<mtu; xorbuf[i++]=0);
        xorsize=0;
        lostnum=-1;
        packet_num++;
        continue;
      }
      memcpy(bufrecv[(packet_num-1)%XOR_FACTOR], buf, buf_len);
      packet_num++;
      received+=buf_len-1;
      if (received>file_len)
        goto lost;
      if (received<file_len && buf_len<mtu)
        goto lost;
      if (buf_len>xorsize) xorsize=buf_len;
      for (i=1; i<buf_len; i++)
        xorbuf[i]^=buf[i];
      continue;
    }
  }
}
