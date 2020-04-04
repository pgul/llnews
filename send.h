int  open_socket(char *downlink);
void close_socket(void);
int  sendbuf(char *buf, int bufsize);
int  open_ack_socket(int ll_stream);
int  waitack(char *fname, long file_len, long file_time, long file_type, int ll_stream);
extern long ll_stream;

#ifdef LOG
#define syslog log
#define openlog open_log

void log(int level, char *fmt, ...);
void open_log(char *name, int, int);
#endif
