/********************************************************************
 * Copyright (c) 2017, TENCENT TECHNOLOGIES CO., LTD.
 *
 * Brief        : Statistics of TCP stream, based on tcpdump.
 * Author       : zaynli
 * Created Date : 2017-05-10
 ********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netdissect-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdissect-stdinc.h>

#include "netdissect.h"
#include "extract.h"
#include "tcp.h"
#include "ip.h"

/********************************************************************
 * defines and types
 ********************************************************************/
#define OK						(0)
#define ERR						(-1)
#define FILENAME_LEN			(50)
#define STREAM_LEN_THRESHOLD	(256 * (2<<10)) /* 256 kB */
#define OUTPUT_PATH				"/tmp/statistic/"
#define OUTPUT_SYN_FILE			OUTPUT_PATH"syn_collection"

typedef struct _TUPLE
{
	struct in_addr saddr, daddr;
	uint16_t sport, dport;
} TUPLE;

typedef enum _STREAM_STATE
{
	STREAM_START = 0,
	STREAM_RUNNING,
	STREAM_END,
	STREAM_CLEANUP
} STREAM_STATE;

typedef struct _STREAM
{
	TUPLE tuple;
	time_t timestamp;
	FILE *fp;
	uint32_t total_len; /* TCP payload only */
	STREAM_STATE state;
	struct _STREAM *next;
} STREAM;


/********************************************************************
 * function declares
 ********************************************************************/
static STREAM *stream_list_add(STREAM *node);
static STREAM *stream_list_lookup_by_pointer(STREAM *node);
static STREAM *stream_list_lookup_by_tuple(TUPLE *tuple);
static STREAM *stream_list_remove(STREAM *node);
static STREAM *stream_node_create(void);
static void stream_node_destroy(STREAM *node);
static void stream_node_update(
		struct in_addr saddr, struct in_addr daddr,
		register const u_char *tcphdr, uint32_t length);

/********************************************************************
 * global variables
 ********************************************************************/
STREAM g_stream_head = {0};
static FILE *g_output = NULL;
static STREAM *g_stream_list = NULL;


/********************************************************************
 * function defines
 ********************************************************************/
void statistic_cap_tcp(register const u_char *tcphdr, uint32_t length,
		register const u_char *iphdr, int fragmented)
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

	length -= TH_OFF(tp) * 4;
	stream_node_update(saddr, daddr, tcphdr, length);
		fprintf(stderr, "------------%d\n", __LINE__);
}

static void stream_node_update(
		struct in_addr saddr, struct in_addr daddr,
		register const u_char *tcphdr, uint32_t length)
{
	register const struct tcphdr *tp;
	register u_char flags;
	uint16_t sport, dport, win, urp;

	tp = (const struct tcphdr *)tcphdr;

	flags = tp->th_flags;
	sport = EXTRACT_16BITS(&tp->th_sport);
	dport = EXTRACT_16BITS(&tp->th_dport);

	TUPLE tuple = {
		.saddr = saddr,
		.daddr = daddr,
		.sport = sport,
		.dport = dport
	};

	/* handle the stream node in memory */
	STREAM *node = stream_list_lookup_by_tuple(&tuple);
	if (node) {
		/* get this node self */
		node = node->next;

		if (node->state >= STREAM_END) {
			goto handle_state;
		}

		node->total_len += length;
	} else {
		if (flags & TH_FIN) {
			/* did not track it */
			return;
		}

		char filename[NAME_MAX] = {0};
		node = stream_node_create();
		if (!node) {
			return;
		}

		memcpy(&node->tuple, &tuple, sizeof(TUPLE));
		node->timestamp = time(NULL);
		node->total_len = length;
		node->state = STREAM_RUNNING;
		node->next = NULL;
		/*
		 * store the tuple info to file name asume
		 * file name: sip-sport_dip-dport_timestamp
		 */
		sprintf(filename, "%s", OUTPUT_PATH);
		sprintf(filename, "%s%s:%d", filename, inet_ntoa(node->tuple.saddr), node->tuple.sport);
		sprintf(filename, "%s_%s:%d", filename, inet_ntoa(node->tuple.daddr), node->tuple.dport);
		sprintf(filename, "%s_%lx", filename, node->timestamp);
		fprintf(stderr, "%d------------file name :%s \n", __LINE__, filename);
		node->fp = fopen(filename, "a");
		if (!node->fp) {
			fprintf(stderr, "unable to open file %s\n", filename);
			return;
		}

		stream_list_add(node);
	}

	/* store the stream node to file */
	fprintf(node->fp, "%d %u\n", win, length);

handle_state:
	/* handle the stream node state */
	if (node->total_len >= STREAM_LEN_THRESHOLD) {
		node->state = STREAM_END;
	}

	if (flags & TH_FIN) {
		node->state = STREAM_CLEANUP;
	}

	if (node->state == STREAM_END) {
		if (node->fp) {
			fflush(node->fp);
			fclose(node->fp);
			node->fp = NULL;
		}
	} else if (node->state == STREAM_CLEANUP) {
		stream_list_remove(node);
	}
}

static STREAM *stream_node_create(void)
{
	STREAM *stream = (STREAM *)malloc(sizeof(STREAM));
	if (!stream) {
		fprintf(stderr, "create stream node failed\n");
		return NULL;
	}
	memset(stream, 0, sizeof(STREAM));

	return stream;
}

static void stream_node_destroy(STREAM *node)
{
	if (node->fp) {
		fflush(node->fp);
		fclose(node->fp);
		node->fp = NULL;
	}

	free(node);
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

	pre->next = next;

	stream_node_destroy(node);

	return g_stream_list;
}

/* do not check member */
/* return the parent's pointer */
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

/* return the parent's pointer */
static STREAM *stream_list_lookup_by_tuple(TUPLE *tuple)
{
	STREAM *p = g_stream_list;
	STREAM *result = NULL;

	while (p->next) {
		if (memcmp(tuple, &(p->next->tuple), sizeof(TUPLE)) == 0) {
			result = p;
			break;
		}
		p = p->next;
	}

	return result;
}

void statistic_init(void)
{
	mkdir(OUTPUT_PATH, 0775);
	if (!g_output) {
		g_output = fopen(OUTPUT_SYN_FILE, "a");
		if (!g_output) {
			fprintf(stderr, "unable to open file %s\n", OUTPUT_SYN_FILE);
			return;
		}
	}

	g_stream_list = &g_stream_head;
}

void statistic_exit(void)
{
	if (g_output) {
		fflush(g_output);
		fclose(g_output);
		g_output = NULL;
	}

	STREAM *p = g_stream_list;
	STREAM *next = p->next;
	while (next) {
		p = next;
		next = p->next;

		stream_node_destroy(p);
	}
}
