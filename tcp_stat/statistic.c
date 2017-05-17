/********************************************************************
 * Copyright (c) 2017, TENCENT TECHNOLOGIES CO., LTD.
 *
 * Brief        : Statistics of Stream, based on tcpdump.
 * Author       : zaynli
 * Created Date : 2017-05-15
 ********************************************************************/

/********************************************************************
 * headers
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
#include <sys/time.h>
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
#define CONFIG_LEN				(200)
#define SEC_IN_MICROSECONDS		(1000000) /* 1 s = 10^6 us */
#define SYN_FILE				"syn_collection"
#define CONFIG_FILE				"sos_config"
#define TSEQ_HASHSIZE			(919)

#define DEF_OUTPUT_PATH			"/tmp/statistic/"
#define DEF_STREAM_THRESHOLD	(256 * 1024) /* 256 kB */

#define RECORD(fp, ...) do { \
	if (fp) { \
		fprintf(fp, __VA_ARGS__); \
	} \
} while (0)


typedef struct _TUPLE
{
	struct in_addr src;
	struct in_addr dst;
	uint32_t port; /* sport at high 16 bit, dport at low 16 bit */
} TUPLE;
#define GET_SPORT(port)			(port >> 16)
#define GET_DPORT(port)			(port & 0xFFFF)

typedef enum _STREAM_STATE
{
	STREAM_START = 0,
	STREAM_RUNNING,
	STREAM_FULL,
	STREAM_FIN
} STREAM_STATE;

typedef struct _STREAM
{
	TUPLE tuple;
	struct timeval timestamp;
	FILE *fp;
	uint32_t seq_fence;
	uint32_t missed_len; /* TCP payload only */
	uint32_t total_len; /* TCP payload only */
	uint32_t missed_frames;
	uint32_t total_frames;
	STREAM_STATE state;
	struct _STREAM *next;
} STREAM;


typedef struct _CONFIG_PACK
{
	char *key;
	void (*func)(char *val);
} CONFIG_PACK;

/********************************************************************
 * function declares
 ********************************************************************/
static STREAM *stream_list_lookup_by_pointer(STREAM *node);
static STREAM *stream_list_lookup_by_tuple(const TUPLE *tuple);
static STREAM *stream_node_create(void);
static int stream_list_add(STREAM *node);
static int stream_list_remove(STREAM *node);
static void stream_node_destroy(STREAM *node);
static void stream_node_update(
		struct in_addr saddr, struct in_addr daddr,
		register const u_char *tcphdr, uint32_t length);
static void dump_stream_info(FILE *fp, STREAM *node);

static void read_config_file(void);
static void config_func_path(char *val);
static void config_func_threshold(char *val);


/********************************************************************
 * global variables
 ********************************************************************/
static STREAM g_stream_list_hash[TSEQ_HASHSIZE];
static FILE *g_fp_syn = NULL;
static char g_output_path[NAME_MAX] = {0};
static uint32_t g_stream_threshold = 0;

static CONFIG_PACK g_config_pack[] = {
	{ "path", config_func_path }, /* output path */
	{ "threshold", config_func_threshold }, /* stream length threshold(KB) */
};
#define CONFIG_PACK_SIZE (sizeof(g_config_pack)/sizeof(CONFIG_PACK))


/********************************************************************
 * function implements
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
	ack = EXTRACT_32BITS(&tp->th_ack);
	win = EXTRACT_16BITS(&tp->th_win);

	flags = tp->th_flags;

	if (flags & TH_SYN) {
		/* sip:sport>dip:dport win seq */
		RECORD(g_fp_syn, "%s:%d", inet_ntoa(saddr), sport);
		RECORD(g_fp_syn, ">%s:%d", inet_ntoa(daddr), dport);
		RECORD(g_fp_syn, " %d %u\n", win, seq);
	}

	length -= TH_OFF(tp) * 4;
	stream_node_update(saddr, daddr, tcphdr, length);
}

static void stream_node_update(
		struct in_addr saddr, struct in_addr daddr,
		register const u_char *tcphdr, uint32_t length)
{
	register const struct tcphdr *tp;
	register u_char flags;
	uint16_t sport, dport, win, urp;
	uint32_t seq, ack, thseq, thack;

	tp = (const struct tcphdr *)tcphdr;

	flags = tp->th_flags;
	sport = EXTRACT_16BITS(&tp->th_sport);
	dport = EXTRACT_16BITS(&tp->th_dport);

	seq = EXTRACT_32BITS(&tp->th_seq);
	ack = EXTRACT_32BITS(&tp->th_ack);
	win = EXTRACT_16BITS(&tp->th_win);

	TUPLE tuple;

	UNALIGNED_MEMCPY(&tuple.dst, &daddr, sizeof(daddr));
	UNALIGNED_MEMCPY(&tuple.src, &saddr, sizeof(saddr));
	tuple.port = sport << 16 | dport;

	/* handle the stream node in memory */
	STREAM *node = stream_list_lookup_by_tuple(&tuple);
	if (node) {
		/* get this node self */
		node = node->next;

	} else {
		char filename[NAME_MAX] = {0};
		node = stream_node_create();
		if (!node) {
			return;
		}

		memcpy(&node->tuple, &tuple, sizeof(TUPLE));
		gettimeofday(&node->timestamp, NULL);
		node->seq_fence = seq;
		node->missed_len = 0;
		node->missed_frames = 0;
		node->total_len = 0;
		node->total_frames = 0;
		node->state = STREAM_RUNNING;
		node->next = NULL;
		node->fp = NULL;
		/*
		 * store the tuple info to file name asume
		 * file name: sip-sport_dip-dport_timestamp
		 */
		//sprintf(filename, "%s", OUTPUT_PATH);
		//sprintf(filename, "%s%s:%d", filename, inet_ntoa(node->tuple.saddr), node->tuple.sport);
		//sprintf(filename, "%s_%s:%d", filename, inet_ntoa(node->tuple.daddr), node->tuple.dport);
		//sprintf(filename, "%s_%lx", filename, node->timestamp);
		//node->fp = fopen(filename, "a");
		//if (!node->fp) {
		//	fprintf(stderr, "unable to open file %s\n", filename);
		//	return;
		//}

		stream_list_add(node);
	}

	if (node->state >= STREAM_FULL) {
		goto handle_state;
	}

	/* store the stream node to file */
	//RECORD(node->fp, "%d %u %u", win, length, seq);
	//if (length > 0) {
	//	RECORD(node->fp, ":%u", seq + length);
	//}
	//if (flags & TH_ACK) {
	//	RECORD(node->fp, " %u", ack);
	//}
	//RECORD(node->fp, "\n");

	node->total_len += length;
	node->total_frames += 1;

	if (node->seq_fence <= seq) {
		node->seq_fence = seq;
	}
	else {
		node->missed_len += length;
		node->missed_frames += 1;
	}

handle_state:
	/* handle the stream node state */
	if (node->total_len >= g_stream_threshold && node->state <= STREAM_RUNNING) {
		time_t now = time(NULL);
		//RECORD(node->fp, "full %u\n", node->total_len);
		dump_stream_info(stdout, node);
		fprintf(stdout, ", stream full.\n");
		node->state = STREAM_FULL;
	}

	if (flags & TH_FIN && node->state <= STREAM_FULL) {
		//RECORD(node->fp, "finish %u\n", node->total_len);
		if (node->total_len > 0) {
			dump_stream_info(stdout, node);
			if (node->state == STREAM_FULL) {
				fprintf(stdout, ", stream full and finished.\n");
			}
			else {
				fprintf(stdout, ", stream finished.\n");
			}
		}

		node->state = STREAM_FIN;
	}

	if (node->state == STREAM_FULL) {
		if (node->fp) {
			fflush(node->fp);
			fclose(node->fp);
			node->fp = NULL;
		}
	} else if (node->state == STREAM_FIN) {
		stream_list_remove(node);
	}
}

static void dump_stream_info(FILE *fp, STREAM *node)
{
	if (!fp || !node) {
		return;
	}

	struct timeval now;
	struct timeval delta;
	gettimeofday(&now, NULL);

	delta.tv_sec = now.tv_sec - node->timestamp.tv_sec;
	delta.tv_usec = now.tv_usec - node->timestamp.tv_usec;
	if (delta.tv_usec < 0) {
		delta.tv_usec = SEC_IN_MICROSECONDS + delta.tv_usec;
		delta.tv_sec -= 1;
	}

	fprintf(fp, "%s:%u", inet_ntoa(node->tuple.src), GET_SPORT(node->tuple.port));
	fprintf(fp, " -> %s:%u", inet_ntoa(node->tuple.dst), GET_DPORT(node->tuple.port));
	fprintf(fp, ", missed data %u/%u, missed frames %u/%u",
			node->missed_len, node->total_len,
			node->missed_frames, node->total_frames);
	fprintf(fp, ", time %ld.%ld ~ %ld.%ld (%ld.%ld)",
			node->timestamp.tv_sec, node->timestamp.tv_usec,
			now.tv_sec, now.tv_usec,
			delta.tv_sec, delta.tv_usec);
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

static int stream_list_add(STREAM *node)
{
	register STREAM *p;

	for (p = &g_stream_list_hash[node->tuple.port % TSEQ_HASHSIZE];
			p->next; p = p->next) {
		if (memcmp((char *)&(node->tuple), &(p->next->tuple),
					sizeof(TUPLE)) == 0) {
			return ERR;
		}
	}

	if (!p->next) {
		p->next = node;
	}

	return OK;
}

static int stream_list_remove(STREAM *node)
{
	STREAM *pre;
	STREAM *next;

	pre = stream_list_lookup_by_tuple(&node->tuple);
	if (!pre) {
		return ERR;
	}

	next = node->next;
	pre->next = next;

	stream_node_destroy(node);

	return OK;
}

/* return the parent's pointer */
static STREAM *stream_list_lookup_by_tuple(const TUPLE *tuple)
{
	register STREAM *node;
	STREAM *result = NULL;

	for (node = &g_stream_list_hash[tuple->port % TSEQ_HASHSIZE];
			node->next; node = node->next) {
		if (memcmp((char *)tuple, (char *)&node->next->tuple,
					sizeof(TUPLE)) == 0) {
			result = node;
			break;
		}
	}

	return result;
}

static void read_config_file(void)
{
	FILE *fp = NULL;
	char line[CONFIG_LEN] = {0};
	char key[CONFIG_LEN];
	char val[CONFIG_LEN];
	char *p;

	fp = fopen(CONFIG_FILE, "r");
	if (!fp) {
		fprintf(stderr, "unable to open config file %s\n", CONFIG_FILE);
		return;
	}

	while (!feof(fp)) {
		memset(key, 0, CONFIG_LEN);
		memset(val, 0, CONFIG_LEN);

		if (fgets(line, CONFIG_LEN, fp) == NULL) {
			continue;
		}

		p = strstr(line, "=");
		strncpy(key, line, p - line);
		strncpy(val, p + 1, strlen(p + 1) - 1);

		for (int i = 0; i < CONFIG_PACK_SIZE; i++) {
			if (strcmp(key, g_config_pack[i].key) == 0) {
				g_config_pack[i].func(val);
			}
		}
	}
}

static void config_func_path(char *val)
{
	if (val) {
		strncpy(g_output_path, val, NAME_MAX);
		fprintf(stderr, "Set output path: \"%s\"\n", g_output_path);
	}
}

static void config_func_threshold(char *val)
{
	if (val) {
		g_stream_threshold = atoi(val);
		fprintf(stderr, "Set stream threshold: %d KB\n", g_stream_threshold);
		g_stream_threshold *= 1024;
	}
}

void statistic_init(void)
{
	read_config_file();

	if (g_output_path[0] == 0) {
		strncpy(g_output_path, DEF_OUTPUT_PATH, NAME_MAX);
		fprintf(stderr, "Set output path to default: \"%s\"\n", g_output_path);
	}
	if (!g_fp_syn) {
		char syn_path[NAME_MAX] = {0};
		mkdir(g_output_path, 0775);
		snprintf(syn_path, NAME_MAX, "%s%s", g_output_path, SYN_FILE);
		g_fp_syn = fopen(syn_path, "a");
		if (!g_fp_syn) {
			fprintf(stderr, "unable to open file %s\n", syn_path);
			return;
		}
	}

	if (g_stream_threshold == 0) {
		g_stream_threshold = DEF_STREAM_THRESHOLD;
		fprintf(stderr, "Set stream threshold to default: %d KB\n", DEF_STREAM_THRESHOLD / 1024);
	}
}

void statistic_exit(void)
{
	if (g_fp_syn) {
		fflush(g_fp_syn);
		fclose(g_fp_syn);
		g_fp_syn = NULL;
	}

	STREAM *p;
	STREAM *next;
	for (int i = 0; i < TSEQ_HASHSIZE; i++) {
		p = &g_stream_list_hash[i];
		next = p->next;

		while (next) {
			p = next;
			next = p->next;

			stream_node_destroy(p);
		}
	}
}
