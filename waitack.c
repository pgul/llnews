#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "conf.h"
#include "send.h"

static int sockfd, ack_sockfd;

static void chld(int signo)
{
  int status, pid;
  do
    pid=waitpid(-1, &status, WNOHANG);
  while (pid!=-1 && pid!=0);
  signal(SIGCHLD, chld);
}

int open_ack_socket(int ll_stream)
{
  int  opt=1;
  struct sockaddr_in my_addr;
#ifdef UPLINK
  char *uplink=UPLINK;
  struct hostent *local;

  if (!isdigit(uplink[0]) || \
      (my_addr.sin_addr.s_addr=inet_addr(uplink))==INADDR_NONE)
  {
    if ((local=gethostbyname(uplink))==NULL)
    { syslog(LOG_CRIT, "Unknown or illegal hostname %s", uplink);
      return 1;
    }
    if (local->h_addr_list[0]==NULL)
    { syslog(LOG_CRIT, "Can't gethostbyname for %s", uplink);
      return 1;
    }
    syslog(LOG_DEBUG, "Local address %d.%d.%d.%d",
     (unsigned char)local->h_addr_list[0][0],
     (unsigned char)local->h_addr_list[0][1],
     (unsigned char)local->h_addr_list[0][2],
     (unsigned char)local->h_addr_list[0][3]);
    my_addr.sin_addr.s_addr=*(long *)(local->h_addr_list[0]);
  }
#endif
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
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  my_addr.sin_port = htons(ACKPORT);
  if (bind(ack_sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) != 0)
  {
    syslog(LOG_ERR, "bind: %s\n", strerror(errno));
    close(ack_sockfd);
    return 1;
  }
  if ((sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    close(ack_sockfd);
    return 1;
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                (char *) &opt, sizeof opt) == -1)
  {
    syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %s\n", strerror(errno));
    close(ack_sockfd);
    close(sockfd);
    return 1;
  }
  return 0;
}

int wait_ack(void)
{
  char buf[MTU];
  struct sockaddr_in down_addr;
  int down_addr_len, r, i;
  struct hostent *remote;
  char ipaddr[16];
  char *p;
  int ll_stream;

  down_addr_len=sizeof(down_addr);
  if ((r=recvfrom(ack_sockfd, buf, sizeof(buf), 0,
                  (struct sockaddr *)&down_addr, &down_addr_len))==-1)
  {
    if (errno!=ECONNREFUSED && errno!=EINTR && errno!=ERESTART)
      syslog(LOG_ERR, "waitack recvfrom error: %s", strerror(errno));
    return 1;
  }
//  syslog(LOG_DEBUG, "receive packet (ack?)");
  if (r<5*4+1 || buf[r-1])
    return 2;
  if (*(long *)buf!=MAGIC_ACK || r!=strlen(buf+4*5)+4*5+1)
    return 2;
  ll_stream=(unsigned short)ntohl(*((long *)buf+4));
  if (ll_stream<1 || ll_stream>8)
    return 2;
  if ((i=fork())>0)
    return 0;
  if (i<0)
    return 2;
  /* child */

  if ((p=strdup(buf+4*5))!=NULL)
  {
    strncpy(buf+4*6, p, sizeof(buf)-4*6);
    free(p);
  }
  else
  {
    syslog(LOG_ERR, "Can't strdup()");
    return 2;
  }
  memcpy(buf+4*5, &down_addr.sin_addr, sizeof(down_addr.sin_addr));
sendagain:
  down_addr.sin_addr.s_addr=htonl(0x7f000001ul);
  down_addr.sin_family=AF_INET;
  down_addr.sin_port=htons(ACK2PORT+ll_stream);
  if (sendto(sockfd, (void *)&buf, r+4, 0,
             (struct sockaddr *)&down_addr, sizeof(down_addr))==-1)
  {
    if (errno==ECONNREFUSED || errno==EINTR || errno==ERESTART)
      goto sendagain;
    syslog(LOG_ERR, "sendto error: %s", strerror(errno));
  }
  memcpy(&down_addr.sin_addr, buf+4*5, sizeof(down_addr.sin_addr));
  sprintf(ipaddr, "%d.%d.%d.%d",
         *((unsigned char *)&down_addr.sin_addr.s_addr),
         *((unsigned char *)&down_addr.sin_addr.s_addr+1),
         *((unsigned char *)&down_addr.sin_addr.s_addr+2),
         *((unsigned char *)&down_addr.sin_addr.s_addr+3));
  remote=gethostbyaddr((char *)&down_addr.sin_addr, sizeof(down_addr.sin_addr),
                       AF_INET);
  if (remote)
    syslog(LOG_DEBUG, "Received ACK for %s"
#ifdef MULTISTREAM
           " (stream %u)"
#endif
           " from %s (%s)", buf+4*6,
#ifdef MULTISTREAM
           ll_stream, 
#endif
           remote->h_name, ipaddr);
  else
    syslog(LOG_DEBUG, "Received ACK for %s "
#ifdef MULTISTREAM
           "(stream %u) "
#endif
           "from %s", buf+4*6,
#ifdef MULTISTREAM
           ll_stream, 
#endif
           ipaddr);
  exit(0);
}

int main(void)
{
  signal(SIGCHLD, chld);
  if (open_ack_socket(0))
    return 2;
  while (1) wait_ack();
}

