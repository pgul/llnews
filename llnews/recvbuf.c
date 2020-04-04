#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "conf.h"
#include "recv.h"

static int  sockfd;
static long upaddr=0;

int open_socket(void)
{
  int  opt=1;
  struct sockaddr_in downlink_addr;
#ifdef UPLINK
  char *uplink=UPLINK;
  struct hostent *remote;

  if (!isdigit((unsigned char)uplink[0]) || \
      (upaddr=inet_addr(uplink))==-1)
  {
    if ((remote=gethostbyname(uplink))==NULL)
    { syslog(LOG_CRIT, "Unknown or illegal hostname %s", uplink);
      return 1;
    }
    if (remote->h_addr_list[0]==NULL)
    { syslog(LOG_ERR, "Can't gethostbyname for %s", uplink);
      return 1;
    }
    upaddr=*(long *)(remote->h_addr_list[0]);
  }
#endif
  if ((sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    return 1;
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                (char *) &opt, sizeof opt) == -1)
  {
    syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %s\n", strerror(errno));
    close(sockfd);
    return 1;
  }
  downlink_addr.sin_family = AF_INET;
  downlink_addr.sin_addr.s_addr = htonl (INADDR_ANY);
  downlink_addr.sin_port = htons (PORT);
  if (bind(sockfd, (struct sockaddr *)&downlink_addr, sizeof(downlink_addr)) != 0)
  {
    syslog(LOG_ERR, "bind: %s\n", strerror(errno));
    close(sockfd);
    return 1;
  }
  return 0;
}

void sendack(char *fname, long file_len, long file_time,
             long file_type, long ll_stream)
{
  struct sockaddr_in uplink_addr;
  char buf[MTU];
  int  ack_sockfd;
  int  opt=1;

  if ((ack_sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    return;
  }
  if (setsockopt(ack_sockfd, SOL_SOCKET, SO_REUSEADDR,
                (char *) &opt, sizeof opt) == -1)
  {
    syslog(LOG_WARNING, "setsockopt (SO_REUSEADDR): %s\n", strerror(errno));
    close(ack_sockfd);
    return;
  }
  uplink_addr.sin_family = AF_INET;
  uplink_addr.sin_addr.s_addr = upaddr;
  uplink_addr.sin_port = htons(ACKPORT);
  *(long *)buf=MAGIC_ACK;
  *((long *)buf+1)=htonl(file_len);
  *((long *)buf+2)=htonl(file_type);
  *((long *)buf+3)=htonl(file_time);
  *((long *)buf+4)=htonl(ll_stream);
  strncpy(buf+4*5, fname, sizeof(buf)-4*5-1);
  if (sendto(ack_sockfd, (void *)&buf, 4*5+strlen(fname)+1, 0,
             (struct sockaddr *)&uplink_addr, sizeof(uplink_addr))==-1)
    syslog(LOG_WARNING, "Can't sendto: %s", strerror(errno));
  close(ack_sockfd);
}

int recvbuf(char *buf, int *buf_len, int max_buf_len, int getaddr)
{
  int uplink_addr_len;
  struct sockaddr_in uplink_addr;
  struct timeval tm;
  fd_set fd_in;

  uplink_addr_len=sizeof(uplink_addr);
  FD_ZERO(&fd_in);
  FD_SET(sockfd, &fd_in);
  tm.tv_sec=TIMEOUT;
  tm.tv_usec=0;

again:
  *buf_len=0;
  switch(select(FD_SETSIZE,&fd_in,NULL,NULL,&tm))
  {
    case -1:
      if (errno==EINTR)
        goto again; /* SIGCHLD */
      syslog(LOG_ERR, "Can't select: %s", strerror(errno));
      return 1;
    case 0: /* timeout */
      return 0;
  }
  if (!FD_ISSET(sockfd,&fd_in))
  { /* impossible */
    syslog(LOG_ERR, "internal error");
    return 1;
  }
  *buf_len=recvfrom(sockfd, buf, max_buf_len, 0,
                    (struct sockaddr *)&uplink_addr, &uplink_addr_len);
  if (*buf_len==-1)
  {
    syslog(LOG_WARNING, "recvfrom error: %s", strerror(errno));
    *buf_len=0;
    return 1;
  }
#ifndef UPLINK
  if (getaddr)
  { if (upaddr && memcmp(&uplink_addr.sin_addr.s_addr, &upaddr, sizeof(upaddr)))
    { unsigned char *p=(char *)&uplink_addr.sin_addr.s_addr;
      syslog(LOG_WARNING, "Uplink address changed to %d.%d.%d.%d",
             p[0], p[1], p[2], p[3]);
    }
    memcpy(&upaddr, &uplink_addr.sin_addr.s_addr, sizeof(upaddr));
  }
  else
#endif
    if (memcmp(&uplink_addr.sin_addr.s_addr, &upaddr, sizeof(upaddr)))
    { unsigned char *p=(char *)&uplink_addr.sin_addr.s_addr;
      syslog(LOG_WARNING, "packet from unexpected addr %d.%d.%d.%d",
             p[0], p[1], p[2], p[3]);
#ifdef UPLINK
      goto again;
#else
      *buf_len=0;
      return 1;
#endif
    }
  return 0;
}

void close_socket(void)
{
  close(sockfd);
}
