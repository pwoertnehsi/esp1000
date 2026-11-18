#ifndef ESP_NETWORK_MENU_H
#define ESP_NETWORK_MENU_H
/* Return 1 for browser, 2 for connection test, 0 to exit. */
int network_menu(unsigned command,volatile int *exiting);
#endif
