#define PORT		1971
#define ACKPORT		1971
#define MTU		1400      /* bytes, don't change! */
/* receive packets from any host if UPLINK undefined */
/* #define UPLINK	"news1.lucky.net" */
#define XOR_FACTOR 	5         /* protocol-specific param, don't change */
#define MAGIC_START	*(long *)"1791"
#define MAGIC_ACK	*(long *)"@lug"

/* user-defined parameters */
#define TIMEOUT		10 /* sec */
#define CMD             "/usr/local/ll/bin/llrecv"
#define CMDARG          

#define SYSLOG

#ifndef SYSLOG
#define LOG		"/var/log/news/llnews"
#endif
