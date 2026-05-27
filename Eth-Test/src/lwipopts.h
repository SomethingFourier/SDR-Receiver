#pragma once

#define NO_SYS                          1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define LWIP_TCP                        1
#define LWIP_DHCP                       1
#define LWIP_ICMP                       1
#define LWIP_UDP                        1
#define LWIP_RAW                        1
#define SYS_LIGHTWEIGHT_PROT            1

#define MEM_ALIGNMENT                   4
#define ETH_PAD_SIZE                    0

#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_STATUS_CALLBACK      1

#define TCP_MSS                         (1500 - 20 - 20)
#define TCP_SND_BUF                     (2 * TCP_MSS)