int  open_socket(void);
void close_socket(void);
int  recvbuf(char *buf, int *buf_len, int max_buf_len, int getaddr);
void sendack(char *fname, long file_len, long file_time,
             long file_type, long ll_stream);

#ifndef SYSLOG
#define syslog log
#define openlog open_log
void log(int level, char *format, ...);
void open_log(char *name, int opt1, int opt2);
#endif
