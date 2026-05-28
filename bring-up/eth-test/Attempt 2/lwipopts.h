#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* NO_SYS: use raw API without RTOS */
#define NO_SYS 1

/* Memory and buffer pool settings */
#define MEM_SIZE 16000
#define PBUF_POOL_SIZE 24
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)

/* TCP/IP settings */
#define LWIP_TCP 1
#define LWIP_UDP 0
#define LWIP_ICMP 1
#define LWIP_IGMP 0
#define LWIP_STATS 0

/* IPv4 settings */
#define LWIP_IPV4 1
#define LWIP_ARP 1
#define ARP_TABLE_SIZE 10

/* DHCP disabled (using static IP) */
#define LWIP_DHCP 0

/* DNS disabled for now */
#define LWIP_DNS 0

/* HTTP server settings */
#define LWIP_HTTPD 1
#define LWIP_HTTPD_MAX_CGI_HANDLERS 4
#define LWIP_HTTPD_CGI_SSI 1
#define LWIP_HTTPD_SSI 1
#define LWIP_HTTPD_SSI_MULTIPART 1
#define HTTPD_DEBUG LWIP_DBG_OFF
#define LWIP_HTTPD_SUPPORT_POST 0

/* Netif settings */
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 0

/* Debug settings */
#define LWIP_DEBUG LWIP_DBG_OFF
#define MEM_DEBUG LWIP_DBG_OFF
#define MEMP_DEBUG LWIP_DBG_OFF
#define PBUF_DEBUG LWIP_DBG_OFF
#define IP_DEBUG LWIP_DBG_OFF
#define TCP_DEBUG LWIP_DBG_OFF
#define ETHARP_DEBUG LWIP_DBG_OFF

/* Platform-specific clock tick */
#include <stdint.h>
uint32_t sys_now(void);

#endif /* __LWIPOPTS_H__ */
