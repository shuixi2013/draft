#ifndef STATISTIC_H
#define STATISTIC_H

void statistic_cap_tcp(register const u_char *tcphdr, register const u_char *iphdr);
void statistic_init(void);
void statistic_exit(void);

#endif /* end of include guard: STATISTIC_H */
