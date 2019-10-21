#include "seq_file.h"

int seq_putc(struct seq_file *m, char c)
{
	UNREFERENCED_PARAMETER(m);
	UNREFERENCED_PARAMETER(c);
    return 0;
}

int seq_puts(struct seq_file *m, const char *s)
{
	UNREFERENCED_PARAMETER(m);
	UNREFERENCED_PARAMETER(s);
    return 0;
}


int seq_printf(struct seq_file *m, const char *f, ...)
{
    int ret;
    va_list args;

    va_start(args, f);
	ret = _vsnprintf(m->buf + seq_file_idx, sizeof(m->buf) - seq_file_idx - 1, f, args);
    va_end(args);
    seq_file_idx += ret;
    ASSERT(seq_file_idx < MAX_PROC_BUF);
    return ret;
}