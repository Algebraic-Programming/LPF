#ifndef SRC_PERFS_H
#define SRC_PERFS_H

#include <stdio.h>

char *size_str(char *str, uint64_t size);
char *cnt_str(char *str, size_t size, uint64_t cnt);
void show_perf(char *name, int tsize, int sent, int acked, uint64_t start, uint64_t end, int xfers_per_iter, FILE *fptr);

#endif