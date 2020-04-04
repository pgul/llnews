#define PORT		1971
#define PORT2		1970
#define ACKPORT		1971
#define ACK2PORT	1971
#define MTU		1400      /* bytes   */
#define SPEED		(64*1024) /* bit/sec */
//#define SPEED		(128*1024) /* bit/sec */
#define XOR_FACTOR 	5
#define MAGIC_START	*(long *)"1791"
#define MAGIC_ACK	*(long *)"@lug"
#define TIMEOUT		10 /* sec */
#define ACK_TIMEOUT	5 /* sec */
#define TRIES		5 /* if no ACK */
//#define MULTISTREAM
//#define UPLINK	"news.lucky.net"
#define TTL		7

#define LOG		"/var/log/news/llnews"
