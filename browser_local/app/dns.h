#ifndef BROWSER_DNS_H
#define BROWSER_DNS_H
int browser_dns_configure(const unsigned char primary[4],const unsigned char secondary[4]);
int browser_dns_restore(void);
#endif
