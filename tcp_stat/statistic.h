#ifndef STATISTIC_H
#define STATISTIC_H

void statistic_cap_tcp(register const u_char *tcphdr, uint32_t length,
		register const u_char *iphdr, int fragmented);
void statistic_init(void);
void statistic_exit(void);

#endif /* end of include guard: STATISTIC_H */
