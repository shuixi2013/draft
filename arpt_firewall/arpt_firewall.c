/*****************************************************************************
 * Copyright (C), 2014, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * File name:   netfilter/arpt_firewall.c
 *
 *
 * Author:      Li Zheng  <lizheng_w5625@tp-link.net>
 *
 * History:
 * -----------------------------
 * v1.0, 2014-01-02, Li Zheng, create file.
 *****************************************************************************/
/* module that allows firewall-filter of the arp payload */

#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_arp/arpt_firewall.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>

#include <net/arp.h>
#include <net/route.h>

#define MODULE_NAME "firewall"

/* MIN_SAFE costs more operations than MIN */
#define MIN_SAFE(x, y) ({\
		typeof (a) _a = (a);\
		typeof (b) _b = (b);\
		_a < _b ? _a : _b; })
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define DEBUG_MODE
#ifdef DEBUG_MODE
	#define DEBUG_PRINT printk
#else
	#define DEBUG_PRINT
#endif

static LIST_HEAD(g_request_list);
static LIST_HEAD(g_response_list);

static spinlock_t wa_lock = SPIN_LOCK_UNLOCKED;

typedef struct arp_ip_node {
	struct in_addr ip;
	struct list_head p_list;
} ARP_IP_NODE;

static bool add_ip_to_list(struct in_addr ip, struct list_head *list)
{
	ARP_IP_NODE *node = NULL;
	unsigned long flags;

	node = (ARP_IP_NODE *)kmalloc(sizeof(ARP_IP_NODE), GFP_ATOMIC);
	if (node == NULL) {
		printk(KERN_WARNING MODULE_NAME ": Failed to add a new ip node.\n");
		return false;
	}

	node->ip.s_addr = ip.s_addr;

	spin_lock_irqsave(&wa_lock, flags);
	list_add(&(node->p_list), list);
	spin_unlock_irqrestore(&wa_lock, flags);

	return true;
}

/*
 * If find, delete the node and return true.
 */
static bool search_and_del_from_list(struct in_addr ip, struct list_head *list)
{
	bool ret = false;
	unsigned long flags;
	ARP_IP_NODE *pos;

	spin_lock_irqsave(&wa_lock, flags);
	list_for_each_entry(pos, list, p_list)
	{
		if (pos->ip.s_addr == ip.s_addr) {
			list_del(&(pos->p_list));
			kfree(pos);
			ret = true;
			break;
		}
	}
	spin_unlock_irqrestore(&wa_lock, flags);

	return ret;
}

/*
 * If find, return true.
 */
static bool search_from_list(struct in_addr ip, struct list_head *list)
{
	bool ret = false;
	ARP_IP_NODE *pos;
	unsigned long flags;

	spin_lock_irqsave(&wa_lock, flags);
	list_for_each_entry(pos, list, p_list)
	{
		if (pos->ip.s_addr == ip.s_addr) {
			ret = true;
			break;
		}
	}
	spin_unlock_irqrestore(&wa_lock, flags);

	return ret;
}

static void free_list(struct list_head *list)
{
	struct list_head *p,*n;
	ARP_IP_NODE *node;
	unsigned long flags;

	spin_lock_irqsave(&wa_lock, flags);
	list_for_each_safe(p, n, list)
	{
		node = list_entry(p, ARP_IP_NODE, p_list);
		list_del(&(node->p_list));
		kfree(node);
	}
	spin_unlock_irqrestore(&wa_lock, flags);
}

/*
 * Convert in_addr to number-and-dots string.
 * This is implemented by myself, and Im not sure its OK for every case.
 * Thus I only use it in debug mode.
 */
static char *inet_ntoa(const struct in_addr addr)
{
	static char s_str_addr[16] = {0};
	const unsigned long int mask = 0xFF;

	memset(s_str_addr, 0, 16);
	sprintf(s_str_addr, "%lu:%lu:%lu:%lu",
			(addr.s_addr >> 24) & mask,
			(addr.s_addr >> 16) & mask,
			(addr.s_addr >> 8) & mask,
			addr.s_addr & mask);
	return s_str_addr;
}

/*
 * Convert hardware address to human-style string.
 * This is implemented by myself, and Im not sure its OK for every case.
 * Thus I only use it in debug mode.
 */
static char *ha_tostr(const unsigned char *ha)
{
	static char s_str_ha[18] = {0};

	memset(s_str_ha, 0, 18);
	sprintf(s_str_ha, "%02x:%02x:%02x:%02x:%02x:%02x",
			ha[0], ha[1], ha[2],
			ha[3], ha[4], ha[5]);
	return s_str_ha;
}

/*
 * Format the arp info.
 * This is implemented by myself, and Im not sure its OK for every case.
 * Thus I only use it in debug mode.
 */
static char *format_arp_info(const unsigned char *src, const unsigned char *dst,
		const unsigned char *sha, const struct in_addr sip,
		const unsigned char *tha, const struct in_addr tip)
{
	static char s_str_arp_info[140] = {0};

	memset(s_str_arp_info, 0, 140);
	sprintf(s_str_arp_info, "src=%s", ha_tostr(src));
	sprintf(s_str_arp_info, "%s dst=%s", s_str_arp_info, ha_tostr(dst));
	sprintf(s_str_arp_info, "%s sha=%s", s_str_arp_info, ha_tostr(sha));
	sprintf(s_str_arp_info, "%s sip=%s", s_str_arp_info, inet_ntoa(sip));
	sprintf(s_str_arp_info, "%s tha=%s", s_str_arp_info, ha_tostr(tha));
	sprintf(s_str_arp_info, "%s tip=%s", s_str_arp_info, inet_ntoa(tip));
	return s_str_arp_info;
}

/*
 * Invalid header will return false, otherwise return true.
 */
static bool is_valid_header(int chain, struct sk_buff *skb)
{
	struct net_device *dev;
	struct net *net;
	const struct ethhdr *eth;
	const struct arphdr *arp;
	int pln, hln;
	unsigned char *arpptr;

	unsigned char	ar_sha[ETH_ALEN];	/* sender hardware address */
	struct in_addr	ar_sip;				/* sender IP address */
	unsigned char	ar_tha[ETH_ALEN];	/* target hardware address */
	struct in_addr	ar_tip;				/* target IP address */

	if (chain == CHAIN_OUTPUT) {
		return true;
	}

	dev = skb->dev;
	net = dev_net(dev);

	eth = eth_hdr(skb);
	arp = arp_hdr(skb);

	/* We assume that pln and hln were checked in the match */
	pln = MIN(arp->ar_pln, ARPT_FIREWALL_ADDR_LEN_MAX);
	hln = MIN(arp->ar_hln, ETH_ALEN);

	arpptr = skb_network_header(skb) + sizeof(*arp);

	memcpy(ar_sha, arpptr, hln);
	arpptr += hln;
	memcpy(&ar_sip, arpptr, pln);
	arpptr += pln;
	memcpy(ar_tha, arpptr, hln);
	arpptr += hln;
	memcpy(&ar_tip, arpptr, pln);

	/* The coordinate judge */
	if (memcmp(eth->h_source, ar_sha, hln)) {
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, coordinate-src problem, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
		return false;
	}
	if (arp->ar_op == __constant_htons(ARPOP_REPLY) &&
			memcmp(eth->h_dest, ar_tha, hln)) {
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, coordinate-dst problem, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
		return false;
	}

	/* The bogus gateway, maybe we should make some GARPs here */
	if (inet_addr_type(net, ar_sip.s_addr) == RTN_LOCAL) {
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, bogus gateway, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
		return false;
	}

	/* The standard rules judge */
	if (arp->ar_op == __constant_htons(ARPOP_REQUEST)) {
		if (!is_broadcast_ether_addr(eth->h_dest))
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, std-rules: req, not broadcast, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
			return false;
		if (!is_zero_ether_addr(ar_tha))
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, std-rules: req, not zero tha, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
			return false;
	}
	if (arp->ar_op == __constant_htons(ARPOP_REPLY)) {
		if (is_broadcast_ether_addr(eth->h_dest))
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, std-rules: rep, is broadcast, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
			return false;
		if (chain == CHAIN_FORWARD &&
			inet_addr_type(net, ar_tip.s_addr) == RTN_LOCAL) {
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": is_valid_header, std-rules: rep, forward, arp: %s", format_arp_info(eth->h_source, eth->h_dest, ar_sha, ar_sip, ar_tha, ar_tip));
			return false;
		}
	}

	return true;
}

/*
 * If we do not trust this arp packet, return false.
 * Otherwise(this packet is OK, or do nothing), need to CONTINUE, return true.
 */
static bool check_request_arp(int chain, struct sk_buff *skb)
{
	const struct arphdr *arp;
	int pln, hln;
	unsigned char *arpptr;
	struct in_addr	ar_tip;				/* target IP address */

	arp = arp_hdr(skb);

	/* We assume that pln and hln were checked in the match */
	pln = MIN(arp->ar_pln, ARPT_FIREWALL_ADDR_LEN_MAX);
	hln = MIN(arp->ar_hln, ETH_ALEN);

	arpptr = skb_network_header(skb) + sizeof(*arp);

	arpptr += hln;
	arpptr += pln;
	arpptr += hln;
	memcpy(&ar_tip, arpptr, pln);

	if (chain == CHAIN_INPUT) {
		DEBUG_PRINT(KERN_WARNING MODULE_NAME ": check_request_arp, input, tip=%s", inet_ntoa(ar_tip));
		return false;
	}

	else if (chain == CHAIN_OUTPUT ||
			chain == CHAIN_FORWARD) {
		if (!search_from_list(ar_tip, &g_request_list)) {
			add_ip_to_list(ar_tip, &g_request_list);
			DEBUG_PRINT(KERN_WARNING MODULE_NAME ": check_request_arp, add to req, tip=%s (CONTINUE)", inet_ntoa(ar_tip));
			return true;
		}
	}

	return true;
}

/*
 * If we do not trust this arp packet, return false.
 * Otherwise(this packet is OK, or do nothing), need to CONTINUE, return true.
 */
static bool check_reply_arp(int chain, struct sk_buff *skb)
{
	const struct arphdr *arp;
	int pln, hln;
	unsigned char *arpptr;

	unsigned char	ar_sha[ETH_ALEN];	/* sender hardware address */
	struct in_addr	ar_sip;				/* sender IP address */
	unsigned char	ar_tha[ETH_ALEN];	/* target hardware address */
	struct in_addr	ar_tip;				/* target IP address */

	arp = arp_hdr(skb);

	/* We assume that pln and hln were checked in the match */
	pln = MIN(arp->ar_pln, ARPT_FIREWALL_ADDR_LEN_MAX);
	hln = MIN(arp->ar_hln, ETH_ALEN);

	arpptr = skb_network_header(skb) + sizeof(*arp);

	memcpy(ar_sha, arpptr, hln);
	arpptr += hln;
	memcpy(&ar_sip, arpptr, pln);
	arpptr += pln;
	memcpy(ar_tha, arpptr, hln);
	arpptr += hln;
	memcpy(&ar_tip, arpptr, pln);

	//struct neighbour *n = 0;

	if (chain == CHAIN_INPUT ||
			chain == CHAIN_FORWARD) {
		if (search_and_del_from_list(ar_sip, &g_request_list)) {
			add_ip_to_list(ar_sip, &g_response_list);
			DEBUG_PRINT(KERN_WARNING MODULE_NAME ": check_reply_arp, del form req and add to rep, sip=%s (CONTINUE)", inet_ntoa(ar_sip));
			return true;
		}
		else {
			if (search_from_list(ar_sip, &g_response_list)) {
				//n = neigh_lookup(&arp_tbl, &ar_sip, skb->dev);
				//if (n && !memcmp(ar_sha, n->ha, ARPT_DEV_ADDR_LEN_MAX)) {
				//	return false;
				//}
				//else {
				//	return true;
				//}
				//
				//if (n) {
				//	return true;
				//}
				return true;
			}
			else {
				DEBUG_PRINT(KERN_WARNING MODULE_NAME ": check_reply_arp, drop, sip=%s", inet_ntoa(ar_sip));
				return false;
			}
		}
	}

	else if (chain == CHAIN_OUTPUT) {
		/* do nothing */
		return true;
	}

	return true;
}


static unsigned int
target(struct sk_buff *skb, const struct xt_target_param *par)
{
	const struct arpt_firewall *firewall;
	const struct arphdr *arp;

	firewall = par->targinfo;
	arp = arp_hdr(skb);

	if (arp->ar_hrd != __constant_htons(ARPHRD_ETHER) &&
			arp->ar_hrd != __constant_htons(ARPHRD_IEEE802)) {
		goto out_continue;
	}

	if (arp->ar_pro != __constant_htons(ETH_P_IP)) {
		goto out_continue;
	}

	if (!is_valid_header(firewall->chain, skb)) {
		goto firewall_taget;
	}

	if (arp->ar_op == __constant_htons(ARPOP_REQUEST) &&
			!check_request_arp(firewall->chain, skb)) {
		goto firewall_taget;
	}

	if (arp->ar_op == __constant_htons(ARPOP_REPLY) &&
			!check_reply_arp(firewall->chain, skb)) {
		goto firewall_taget;
	}

out_continue:
	return ARPT_CONTINUE;

firewall_taget:
	return firewall->target;
}

static bool checkentry(const struct xt_tgchk_param *par)
{
	const struct arpt_firewall *firewall = par->targinfo;

	if (firewall->target != NF_DROP && firewall->target != NF_ACCEPT &&
		firewall->target != ARPT_CONTINUE)
		return false;

	if (firewall->chain != CHAIN_INPUT &&
			firewall->chain != CHAIN_OUTPUT &&
			firewall->chain != CHAIN_FORWARD)
		return false;

	return true;
}

static struct xt_target arpt_firewall_reg __read_mostly = {
	.name		= MODULE_NAME,
	.family		= NFPROTO_ARP,
	.target		= target,
	.targetsize	= sizeof(struct arpt_firewall),
	.checkentry	= checkentry,
	.me		= THIS_MODULE,
};

static int __init arpt_firewall_init(void)
{
	return xt_register_target(&arpt_firewall_reg);
}

static void __exit arpt_firewall_fini(void)
{
	free_list(&g_request_list);
	free_list(&g_response_list);
	xt_unregister_target(&arpt_firewall_reg);
}

module_init(arpt_firewall_init);
module_exit(arpt_firewall_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zheng <lizheng_w5625@tp-link.net>");
MODULE_DESCRIPTION("arptables arp payload firewall target");
