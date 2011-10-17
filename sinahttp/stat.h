#ifndef _SINA_STAT_H
#define _SINA_STAT_H

#define STAT_WINDOW 1000

void sinahttp_stats_store(int index, uint64_t val);
void sinahttp_stats_init();
void sinahttp_stats_destruct();

#endif
