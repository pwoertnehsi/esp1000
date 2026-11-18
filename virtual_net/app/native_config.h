#ifndef PSP_VNET_NATIVE_CONFIG_H
#define PSP_VNET_NATIVE_CONFIG_H
typedef int (*vnet_ioctl_fn)(int socket, unsigned command, void *request);
vnet_ioctl_fn vnet_resolve_ioctl(void);
int vnet_configure_lab(int (*report)(const char *, int));
#endif
