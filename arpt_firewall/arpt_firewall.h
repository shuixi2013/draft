#ifndef ARPT_FIREWALL_H
#define ARPT_FIREWALL_H

#include <linux/netfilter_arp/arp_tables.h>

#define ARPT_FIREWALL_ADDR_LEN_MAX  (sizeof(struct in_addr))

struct arpt_firewall
{
    int target;
    int chain;
};

#define CHAIN_INPUT     (0)
#define CHAIN_OUTPUT    (1)
#define CHAIN_FORWARD   (2)

#endif /* end of include guard: ARPT_FIREWALL_H */
