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
#define WINDOW_FILE				"window.out"
#define STREAM_FILE				"stream.out"
#define CONFIG_FILE				"sos_config"
#define TSEQ_HASHSIZE			(919)
#define CONN_POOL_SIZE			(10)
#define TRASH_FRAME_THRESHOLD	(200)
#define TRASH_TIME_THRESHOLD	(5) /* 5 s */

#define DEF_OUTPUT_PATH			"/tmp/sos/"
#define DEF_STREAM_THRESHOLD	(256 * 1024) /* 256 kB */

#define RECORD(fp, ...) do { \
	if (fp) { \
		fprintf(fp, __VA_ARGS__); \
	} \
} while (0)

//#define DEBUG 1

typedef struct _TUPLE
{
	struct in_addr src;
	struct in_addr dst;
	uint32_t port;
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
	uint32_t seq_fence[2];
	uint32_t isn[2];
	//uint32_t missed_len[2]; /* TCP payload only */
	//uint32_t total_len[2]; /* TCP payload only */
	uint32_t missed_frames[2];
	uint32_t total_frames[2];
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
static STREAM *stream_list_lookup_by_tuple(const TUPLE *tuple);
static STREAM *stream_node_create(void);
static int stream_list_remove_subnode(STREAM *node);
static void stream_node_destroy(STREAM *node);
static void dump_stream_info(FILE *fp, STREAM *node);

static void read_config_file(void);
static void config_func_path(char *val);
static void config_func_threshold(char *val);


/********************************************************************
 * global variables
 ********************************************************************/
static STREAM g_stream_list_hash[TSEQ_HASHSIZE];
static FILE *g_window_fp = NULL;
static FILE *g_stream_fp = NULL;
static char g_out_path[NAME_MAX] = {0};
static uint32_t g_stream_threshold = 0;
static uint32_t g_conn_cnt = 0;
static uint32_t g_conn_del_cnt = 0;
static uint32_t g_conn_cnt_max = 200;

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
		RECORD(g_window_fp, "%s:%d", inet_ntoa(saddr), sport);
		RECORD(g_window_fp, ">%s:%d", inet_ntoa(daddr), dport);
		RECORD(g_window_fp, " %d %u\n", win, seq);
	}

	//length -= TH_OFF(tp) * 4;

	if (!(flags & TH_ACK)) {
		return;
	}

	STREAM *parent;
	register STREAM *node;
	register int rev;
	TUPLE tuple;

	rev = 0;
	if (sport > dport) {
		rev = 1;
	} else if (sport == dport) {
		if (UNALIGNED_MEMCMP(&saddr, &daddr, sizeof(daddr)) > 0) {
			rev = 1;
		}
	}

	if (rev) {
		UNALIGNED_MEMCPY(&tuple.src, &daddr, sizeof(daddr));
		UNALIGNED_MEMCPY(&tuple.dst, &saddr, sizeof(saddr));
		tuple.port = dport << 16 | sport;
	} else {
		UNALIGNED_MEMCPY(&tuple.dst, &daddr, sizeof(daddr));
		UNALIGNED_MEMCPY(&tuple.src, &saddr, sizeof(saddr));
		tuple.port = sport << 16 | dport;
	}

	int cnt = 0;

	for (node = &g_stream_list_hash[tuple.port % TSEQ_HASHSIZE];
			node->next; node = node->next) {
		cnt++;
		if (memcmp((char *)&tuple, (char *)&node->next->tuple,
					sizeof(TUPLE)) == 0) {
			break;
		}
	}

	parent = node;
	if (node->next) {
		node = node->next;

	} else if (flags & TH_SYN) {
		if (cnt > CONN_POOL_SIZE) {
			//fprintf(stderr, "conn pool full: %u\n", node->tuple.port);
			return;
		}

		node->next = (STREAM *)calloc(1, sizeof(STREAM));
		if (!node->next) {
			fprintf(stderr, "create stream node failed\n");
			return;
		}

#ifdef DEBUG
		g_conn_cnt += 1;
#endif
		node = node->next;
		memcpy(&node->tuple, &tuple, sizeof(TUPLE));
		gettimeofday(&node->timestamp, NULL);
		node->isn[rev] = node->seq_fence[rev] = seq;
		node->isn[!rev] = node->seq_fence[!rev] = ack - 1;
		node->state = STREAM_RUNNING;
	} else {
		return;
	}

#ifdef DEBUG
	if (g_conn_cnt > g_conn_cnt_max) {
		g_conn_cnt_max = g_conn_cnt;
		if (g_conn_cnt_max % 100 == 0) {
			fprintf(stderr, "conn: %u\n", g_conn_cnt_max);
		}
	}
#endif

	if (flags & TH_FIN) {
		node->state = STREAM_FIN;
	}

	if (node->state >= STREAM_FULL) {
		goto handle_state;
	}

	//node->total_len[rev] += length;
	node->total_frames[rev] += 1;
	if (node->total_frames[rev] > TRASH_FRAME_THRESHOLD) {
		node->state = STREAM_FIN;
	}

	struct timeval now;
	gettimeofday(&now, NULL);
	if (now.tv_sec > node->timestamp.tv_sec + TRASH_TIME_THRESHOLD) {
		node->state = STREAM_FIN;
	}

	if (node->seq_fence[rev] <= seq) {
		node->seq_fence[rev] = seq;
	}
	else {
		//node->missed_len[rev] += length;
		node->missed_frames[rev] += 1;
	}

	/* stream full */
	if (ack > node->isn[!rev] + g_stream_threshold) {
		dump_stream_info(g_stream_fp, node);
		node->state = STREAM_FULL;
	}

handle_state:
	if (node->state >= STREAM_FULL) {
		stream_list_remove_subnode(parent);
#ifdef DEBUG
		g_conn_cnt -= 1;
		//g_conn_del_cnt += 1;
		//if (g_conn_del_cnt % 500 == 0) {
		//	fprintf(stderr, "del conn: %u\n", g_conn_del_cnt);
		//}
#endif
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

	char buf[150] = {0};
	char *p = buf;
	p += sprintf(p, "%s:%u", inet_ntoa(node->tuple.src), GET_SPORT(node->tuple.port));
	p += sprintf(p, " %s:%u", inet_ntoa(node->tuple.dst), GET_DPORT(node->tuple.port));
	sprintf(p, " %u/%u %u/%u %ld.%ld\n",
			node->missed_frames[0], node->total_frames[0],
			node->missed_frames[1], node->total_frames[1],
			delta.tv_sec, delta.tv_usec);

	fprintf(fp, "%s", buf);
	//fprintf(fp, "%s:%u", inet_ntoa(node->tuple.src), GET_SPORT(node->tuple.port));
	//fprintf(fp, " %s:%u", inet_ntoa(node->tuple.dst), GET_DPORT(node->tuple.port));
	//fprintf(fp, " %u/%u %u/%u %ld.%ld\n",
	//		node->missed_frames[0], node->total_frames[0],
	//		node->missed_frames[1], node->total_frames[1],
	//		delta.tv_sec, delta.tv_usec);
}

static STREAM *stream_node_create(void)
{
}

static void stream_node_destroy(STREAM *node)
{
	free(node);
}

static int stream_list_remove_subnode(STREAM *parent)
{
	if (!parent || !parent->next) {
		return ERR;
	}

	STREAM *node = parent->next;
	parent->next = node->next;
	stream_node_destroy(node);

	return OK;
}

/* return the parent's pointer */
static STREAM *stream_list_lookup_by_tuple(const TUPLE *tuple)
{
	register STREAM *node;


	return node;
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
		strncpy(g_out_path, val, NAME_MAX);
		fprintf(stderr, "Set output path: \"%s\"\n", g_out_path);
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
	char file_path[NAME_MAX] = {0};

	read_config_file();

	if (g_out_path[0] == 0) {
		strncpy(g_out_path, DEF_OUTPUT_PATH, NAME_MAX);
		fprintf(stderr, "Set output path to default: \"%s\"\n", g_out_path);
	}
	mkdir(g_out_path, 0775);

	if (!g_window_fp) {
		memset(file_path, 0, NAME_MAX);
		snprintf(file_path, NAME_MAX, "%s/%s", g_out_path, WINDOW_FILE);
		g_window_fp = fopen(file_path, "a");
		if (!g_window_fp) {
			fprintf(stderr, "unable to open file %s\n", file_path);
			return;
		}
	}

	if (!g_stream_fp) {
		memset(file_path, 0, NAME_MAX);
		snprintf(file_path, NAME_MAX, "%s/%s", g_out_path, STREAM_FILE);
		g_stream_fp = fopen(file_path, "a");
		if (!g_stream_fp) {
			fprintf(stderr, "unable to open file %s\n", file_path);
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
	if (g_window_fp) {
		fflush(g_window_fp);
		fclose(g_window_fp);
		g_window_fp = NULL;
	}

	if (g_stream_fp) {
		fflush(g_stream_fp);
		fclose(g_stream_fp);
		g_stream_fp = NULL;
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
