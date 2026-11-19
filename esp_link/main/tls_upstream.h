#ifndef ESP_TLS_UPSTREAM_H
#define ESP_TLS_UPSTREAM_H
#include "esp_tls.h"
#include "esp_crt_bundle.h"
static inline esp_tls_cfg_t tls_upstream_config(void)
{
    static const char *alpn[]={"http/1.1",NULL};
    esp_tls_cfg_t config={.crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=12000,.alpn_protos=alpn};
    return config;
}
void tls_upstream_probe(void);
#endif
