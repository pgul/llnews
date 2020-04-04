#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "conf.h"
#include "send.h"

#define PACKINTERVAL	(MTU*8*1000/(SPEED/1024)) /* mksec/packet */
//#define BINDADDR	0x7f000001ul /* 127.0.0.1 */
//#define BINDADDR	0xc1c1c166ul /* 193.193.193.102 */
//#define BINDADDR        INADDR_ANY

#ifndef INADDR_NONE
#define INADDR_NONE	(in_addr_t)-1
#endif

static int sockfd, ack_sockfd, can_send;
static struct sockaddr_in down_addr;

static void alrm(int signo)
{
  can_send=1;
  //signal(SIGALRM, alrm);
}

int open_socket(char *downlink)
{
  int  opt=1;
  struct hostent *remote;
  struct itimerval tim;
#ifdef BINDADDR
  struct sockaddr_in my_addr;
#endif

  can_send=0;
  sigset(SIGALRM, alrm);
  tim.it_interval.tv_sec =tim.it_value.tv_sec =PACKINTERVAL/1000000;
  tim.it_interval.tv_usec=tim.it_value.tv_usec=PACKINTERVAL%1000000;
  if (PACKINTERVAL==0)
    tim.it_interval.tv_usec=tim.it_value.tv_usec=1;
  setitimer(ITIMER_REAL, &tim, NULL);

  if (!isdigit((unsigned char)downlink[0]) || \
      (down_addr.sin_addr.s_addr=inet_addr(downlink))==INADDR_NONE)
  {
    if ((remote=gethostbyname(downlink))==NULL)
    { syslog(LOG_CRIT, "Unknown or illegal hostname %s", downlink);
      return 1;
    }
    if (remote->h_addr_list[0]==NULL)
    { syslog(LOG_CRIT, "Can't gethostbyname for %s", downlink);
      return 1;
    }
#if 0
    syslog(LOG_DEBUG, "Downlink address %d.%d.%d.%d",
     (unsigned char)remote->h_addr_list[0][0],
     (unsigned char)remote->h_addr_list[0][1],
     (unsigned char)remote->h_addr_list[0][2],
     (unsigned char)remote->h_addr_list[0][3]);
#endif
    down_addr.sin_addr.s_addr=*(long *)(remote->h_addr_list[0]);
  }
  if ((sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    return 1;
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                (char *) &opt, sizeof(opt)) == -1)
  {
    syslog(LOG_WARNING, "setsockopt (SO_REUSEADDR): %s", strerror(errno));
    close(sockfd);
    return 1;
  }
#ifdef TTL
  opt=TTL;
  if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, (void *)&opt, sizeof(opt)))
    syslog(LOG_WARNING, "setsockopt (IP_TTL): %s", strerror(errno));
#endif

#ifdef BINDADDR
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = htonl(BINDADDR);
  my_addr.sin_port = htons(PORT2);
  if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) != 0)
  {
    syslog(LOG_ERR, "bind: %s", strerror(errno));
    close(sockfd);
    return 1;
  }
#endif

  down_addr.sin_family=AF_INET;
  down_addr.sin_port=htons(PORT);
  if (connect(sockfd, (struct sockaddr *)&down_addr, sizeof(down_addr)))
  {
    close(sockfd);
    syslog(LOG_ERR, "can't connect: %s", strerror(errno));
    return 1;
  }
  return 0;
}

int sendbuf(char *buf, int buflen)
{
  struct timeval tm;
  fd_set fd_out;
  time_t tstart;
  struct itimerval tim;

  tstart=time(NULL);
repselect:
  FD_ZERO(&fd_out);
  FD_SET(sockfd, &fd_out);
  tm.tv_sec=tstart+TIMEOUT-time(NULL);
  if (tstart+TIMEOUT<time(NULL))
    tm.tv_sec=0;
  tm.tv_usec=0;
  switch(select(FD_SETSIZE,NULL,&fd_out,NULL,&tm))
  {
    case -1:
      if (errno==ECONNREFUSED || errno==EHOSTUNREACH ||
          errno==EINTR || errno==ERESTART)
        goto repselect;
      syslog(LOG_ERR, "Can't select: %s", strerror(errno));
      close(sockfd);
      return 1;
    case 0:
//      syslog(LOG_NOTICE, "Timeout");
      return 1;
  }
  if (!FD_ISSET(sockfd,&fd_out))
  { /* impossible */
    syslog(LOG_ERR, "internal error");
    return 1;
  }
  // if (!can_send) syslog(LOG_DEBUG, "waiting for can_send");
  if (!can_send)
  {
    getitimer(ITIMER_REAL, &tim);
    if (tim.it_value.tv_sec) sleep(tim.it_value.tv_sec);
    usleep(tim.it_value.tv_usec);
    tim.it_interval.tv_sec =tim.it_value.tv_sec =PACKINTERVAL/1000000;
    tim.it_interval.tv_usec=tim.it_value.tv_usec=PACKINTERVAL%1000000;
    if (PACKINTERVAL==0)
      tim.it_interval.tv_usec=tim.it_value.tv_usec=1;
    setitimer(ITIMER_REAL, &tim, NULL);
    can_send=0;
  }
//  while (!can_send)
//    usleep((PACKINTERVAL>3) ? PACKINTERVAL/4 : 1);
//  syslog(LOG_DEBUG, "sending packet");
repsend:
  if (send(sockfd, buf, buflen, 0)==-1)
  {
    if (errno==ECONNREFUSED || errno==EHOSTUNREACH ||
        errno==EINTR || errno==ERESTART)
      goto repsend;
    syslog(LOG_ERR, "send error: %s", strerror(errno));
    return 1;
  }
  can_send=0;
  return 0;
}

int open_ack_socket(int ll_stream)
{
  int  opt=1;
  struct sockaddr_in my_addr;

  if ((ack_sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    return 1;
  }
  if (setsockopt(ack_sockfd, SOL_SOCKET, SO_REUSEADDR,
                (char *) &opt, sizeof opt) == -1)
  {
    syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %s\n", strerror(errno));
    close(ack_sockfd);
    return 1;
  }
  my_addr.sin_family = AF_INET;
//  my_addr.sin_addr.s_addr = htonl(0x7f000001ul);
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  my_addr.sin_port = htons(ACK2PORT+ll_stream);
  if (bind(ack_sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) != 0)
  {
    syslog(LOG_ERR, "bind: %s\n", strerror(errno));
    close(ack_sockfd);
    return 1;
  }
  return 0;
}

int waitack(char *fname, long file_len, long file_time, long file_type, int ll_stream)
{
  struct timeval tm;
  fd_set fd_in;
  time_t timestart;
  char buf[MTU];
  struct sockaddr_in down_addr;
  int down_addr_len, r;
  struct hostent *remote;
  char ipaddr[16];
  char *reason;

  timestart=time(NULL);
waitack:
  FD_ZERO(&fd_in);
  FD_SET(ack_sockfd, &fd_in);
  tm.tv_sec=ACK_TIMEOUT-(time(NULL)-timestart);
  tm.tv_usec=0;
  if ((long)tm.tv_sec<=0) return 1;
  switch(select(FD_SETSIZE,&fd_in,NULL,NULL,&tm))
  {
    case -1:
      if (errno==ECONNREFUSED || errno==EINTR || errno==ERESTART)
        goto waitack;
      syslog(LOG_ERR, "Can't select: %s", strerror(errno));
      close(ack_sockfd);
      return 1;
    case 0:
      syslog(LOG_NOTICE, "Timeout");
      close(ack_sockfd);
      return 1;
  }
  if (!FD_ISSET(ack_sockfd,&fd_in))
  { /* impossible */
    syslog(LOG_ERR, "internal error");
    close(ack_sockfd);
    return 1;
  }
  down_addr_len=sizeof(down_addr);
  if ((r=recvfrom(ack_sockfd, buf, sizeof(buf), 0,
                  (struct sockaddr *)&down_addr, &down_addr_len))==-1)
  {
    if (errno==ECONNREFUSED || errno==EINTR || errno==ERESTART)
    {
      // syslog(LOG_DEBUG, "recvfrom: connection refused, ignored");
      goto waitack;
    }
    syslog(LOG_ERR, "recvfrom error: %s", strerror(errno));
    close(ack_sockfd);
    return 1;
  }
  //syslog(LOG_DEBUG, "receive packet (ack?)");
  if (r<6*4+1)
  {
    syslog(LOG_DEBUG, "packet too short (%d bytes)", r);
    goto waitack;
  }
  if (buf[r-1])
  {
    syslog(LOG_DEBUG, "bad packet end");
    goto waitack;
  }
  if (*(long *)buf!=MAGIC_ACK)
  {
    syslog(LOG_DEBUG, "no MAGIC_ACK");
    goto waitack;
  }
  if (r!=strlen(buf+4*6)+4*6+1)
  {
    syslog(LOG_DEBUG, "extra info after filename");
    goto waitack;
  }
  memcpy(&down_addr.sin_addr, buf+4*5, sizeof(down_addr.sin_addr));
  sprintf(ipaddr, "%d.%d.%d.%d",
         *((unsigned char *)&down_addr.sin_addr.s_addr),
         *((unsigned char *)&down_addr.sin_addr.s_addr+1),
         *((unsigned char *)&down_addr.sin_addr.s_addr+2),
         *((unsigned char *)&down_addr.sin_addr.s_addr+3));
  remote=gethostbyaddr((char *)&down_addr.sin_addr, sizeof(down_addr.sin_addr),
                       AF_INET);
  reason=NULL;
  if (strcmp(buf+4*6, fname))
    reason=" file name";
  else if (*((long *)buf+4)!=htonl(ll_stream))
    reason=" LL stream";
  else if (*((long *)buf+1)!=htonl(file_len) ||
           *((long *)buf+2)!=htonl(file_type) ||
           *((long *)buf+3)!=htonl(file_time))
    reason="";
  if (reason)
  {
#if 1
    if (remote)
      syslog(LOG_DEBUG, "Received unexpected%s ACK for %s from %s (%s)",
             reason, buf+4*6, remote->h_name, ipaddr);
    else
      syslog(LOG_INFO, "Received unexpected%s ACK for %s from %s",
             reason, buf+4*6, ipaddr);
#endif
    goto waitack;
  }
  close(ack_sockfd);
  if (remote)
    syslog(LOG_INFO, "ACK for %s "
#ifdef MULTISTREAM
           "(stream %u) "
#endif
           "from %s (%s)", fname,
#ifdef MULTISTREAM
           ll_stream, 
#endif
           remote->h_name, ipaddr);
  else
#ifdef MULTISTREAM
    syslog(LOG_INFO, "ACK for %s (stream %u) from %s", fname, ll_stream, ipaddr);
#else
    syslog(LOG_INFO, "ACK for %s from %s", fname, ipaddr);
#endif
  return 0;
}

void close_socket(void)
{
  struct itimerval tim;

  tim.it_interval.tv_sec =tim.it_value.tv_sec =
  tim.it_interval.tv_usec=tim.it_value.tv_usec=0;
  setitimer(ITIMER_REAL, &tim, NULL);
  sigset(SIGALRM, SIG_DFL);
  close(sockfd);
}

