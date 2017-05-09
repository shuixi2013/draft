#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netdissect-stdinc.h>

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netdissect-stdinc.h>

#include "netdissect.h"
#include "extract.h"
#include "tcp.h"
#include "ip.h"

/************************************************
 * defines and types
 */
#define OK				(0)
#define ERR				(-1)
#define OUTPUT_FILE		"/tmp/capture_tool_output"

typedef struct _TUPLE
{
	struct in_addr saddr, daddr;
	uint16_t sport, dport;
} TUPLE;

typedef struct _STREAM
{
	TUPLE tuple;
	time_t time;
	FILE *fp;
	int total_len; /* TCP payload only */
	struct _STREAM *next;
} STREAM;


/************************************************
 * function declares
 */


/************************************************
 * global variables
 */
STREAM g_stream_head = {0};
static FILE *g_output = NULL;
static STREAM *g_stream_list = NULL;

void statistic_cap_tcp(register const u_char *tcphdr, register const u_char *iphdr)
{
	uint16_t sport, dport, win, urp;
	uint32_t seq, ack, thseq, thack;
	register u_char flags;
	register const struct tcphdr *tp;
	register const struct ip *ip;
	struct in_addr saddr, daddr;
	const u_char *p;

	tp = (const struct tcphdr *)tcphdr;
	ip = (const struct ip *)iphdr;

	if (IP_V(ip) == 6)
	{
		(void)fprintf(stderr, "Do not track IPv6\n");
		return;
	}

	p = (const u_char *)&ip->ip_src;
	memcpy(&saddr.s_addr, p, sizeof(uint32_t));

	p = (const u_char *)&ip->ip_dst;
	memcpy(&daddr.s_addr, p, sizeof(uint32_t));

	sport = EXTRACT_16BITS(&tp->th_sport);
	dport = EXTRACT_16BITS(&tp->th_dport);

	seq = EXTRACT_32BITS(&tp->th_seq);
	win = EXTRACT_16BITS(&tp->th_win);

	flags = tp->th_flags;

	if (flags & TH_SYN) {
		/* sip:sport>dip:dport win seq */
		fprintf(g_output, "%s:%d", inet_ntoa(saddr), sport);
		fprintf(g_output, ">%s:%d", inet_ntoa(daddr), dport);
		fprintf(g_output, " %d %u\n", win, seq);
	}
}

static STREAM *stream_create(void)
{
	STREAM *stream = (STREAM *)malloc(sizeof(STREAM));
	if (!stream) {
		fprintf(stderr, "create stream node failed\n");
		return NULL;
	}
	memset(stream, 0, sizeof(STREAM));

	return stream;
}

static void stream_destroy(STREAM *node)
{
	if (node->fp) {
		fflush(fp);
		fclose(fp);
	}

	free(node);
	node = NULL;
}

static STREAM *stream_list_add(STREAM *node)
{
	STREAM *p = g_stream_list;
	while (p->next) {
		p = p->next;
	}
	p->next = node;

	return g_stream_list;
}

static STREAM *stream_list_remove(STREAM *node)
{
	STREAM *pre;
	STREAM *next;

	pre = stream_list_lookup_by_pointer(node);
	next = node->next;
}

/* do not check member */
/* return the parent pointer */
static STREAM *stream_list_lookup_by_pointer(STREAM *node)
{
	STREAM *p = g_stream_list;
	STREAM *result = NULL;

	while (p->next) {
		if (node == p->next) {
			result = p;
			break;
		}
		p = p->next;
	}

	return result;
}

/* return the parent pointer */
static STREAM *stream_list_lookup_by_tuple(TUPLE *tuple)
{
}

void statistic_init(void)
{
	if (!g_output) {
		g_output = fopen(OUTPUT_FILE, "a");
		if (!g_output) {
			fprintf(stderr, "unable to open file %s\n", OUTPUT_FILE);
			return;
		}
	}

	g_stream_list = &g_stream_head;
}

void statistic_exit(void)
{
	if (g_output) {
		(void)fflush(g_output);
		fclose(g_output);
		g_output = NULL;
	}
}

