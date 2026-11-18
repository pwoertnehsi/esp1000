#ifndef BROWSER_STATE_H
#define BROWSER_STATE_H
int browser_request_close(void);
void browser_log_state(void (*report)(const char *,...));
#endif
