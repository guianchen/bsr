#ifndef DRBDADM_DUMP_H
#define DRBDADM_DUMP_H

#include "bsradm.h"

extern void print_dump_xml_header(void);
extern void print_dump_header(void);
extern int adm_dump(const struct cfg_ctx *ctx);
extern int adm_dump_xml(const struct cfg_ctx *ctx);

#endif
