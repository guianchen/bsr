﻿
#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#ifdef _WIN32
#include "linux-compat/spinlock.h"
#include "linux-compat/seq_file.h"
#else
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/kref.h>
#endif
#include "drbd_wrappers.h"
#include "kref_debug.h"

#ifdef _WIN32_V9
// _WIN32_V9_KREF_DEBUG: CONFIG_KREF_DEBUG 디버깅을 하지 않겠다는 의도로 일단 회피처리
#ifdef CONFIG_KREF_DEBUG // && _WIN32

	// 사용할 경우만 대비...

#endif
#else
struct list_head kref_debug_objects;
spinlock_t kref_debug_lock;

void initialize_kref_debugging(void)
{
	INIT_LIST_HEAD(&kref_debug_objects);
	spin_lock_init(&kref_debug_lock);
}

void kref_debug_init(struct kref_debug_info *debug_info,
		     struct kref *kref,
		     const struct kref_debug_class *class)
{
	unsigned long irq_flags;
	int i;

	debug_info->class = class;
	debug_info->kref = kref;
	for (i = 0; i < KREF_DEBUG_HOLDER_MAX ; i++)
		debug_info->holders[i] = 0;
	spin_lock_irqsave(&kref_debug_lock, irq_flags);
	list_add(&debug_info->objects, &kref_debug_objects);
	spin_unlock_irqrestore(&kref_debug_lock, irq_flags);
}

static int number_of_debug_refs(struct kref_debug_info *debug_info)
{
	int i, refs = 0;

	for (i = 0; i < KREF_DEBUG_HOLDER_MAX; i++)
		refs += debug_info->holders[i];

	return refs;
}

static bool has_refs(struct kref_debug_info *debug_info)
{
	return number_of_debug_refs(debug_info) != -1;
}

void kref_debug_destroy(struct kref_debug_info *debug_info)
{
	unsigned long irq_flags;
	int i;

	spin_lock_irqsave(&kref_debug_lock, irq_flags);
	if (has_refs(debug_info)) {
		pr_err("ASSERT FAILED\n");
		pr_err("object of class: %s\n", debug_info->class->name);
		for (i = 0; i < KREF_DEBUG_HOLDER_MAX; i++) {
			if (debug_info->holders[i] == 0)
				continue;
			pr_err("  [%d] = %d (%s)\n", i, debug_info->holders[i],
			       debug_info->class->holder_name[i] ?: "");
		}
		pr_err("\n");
	}

	list_del(&debug_info->objects);
	spin_unlock_irqrestore(&kref_debug_lock, irq_flags);
}

void kref_debug_get(struct kref_debug_info *debug_info, int holder_nr)
{
	unsigned long irq_flags;

	if (holder_nr >= KREF_DEBUG_HOLDER_MAX) {
		pr_err("Increase KREF_DEBUG_HOLDER_MAX\n");
		return;
	}

	spin_lock_irqsave(&kref_debug_lock, irq_flags);
	debug_info->holders[holder_nr]++;
	spin_unlock_irqrestore(&kref_debug_lock, irq_flags);
}

void kref_debug_sub(struct kref_debug_info *debug_info, int refs, int holder_nr)
{
	unsigned long irq_flags;

	if (holder_nr >= KREF_DEBUG_HOLDER_MAX) {
		pr_err("Increase KREF_DEBUG_HOLDER_MAX\n");
		return;
	}

	spin_lock_irqsave(&kref_debug_lock, irq_flags);
	debug_info->holders[holder_nr] -= refs;
	spin_unlock_irqrestore(&kref_debug_lock, irq_flags);
}

void print_kref_debug_info(struct seq_file *seq)
{
	struct kref_debug_info *debug_info;
	int i;

	spin_lock_irq(&kref_debug_lock);
	list_for_each_entry(debug_info, &kref_debug_objects, objects) {
		int debug_refs, refs;

		debug_refs = number_of_debug_refs(debug_info);
		refs = atomic_read(&debug_info->kref->refcount);

		seq_printf(seq, "object of class: %s (r = %d, dr = %d)\n",
			   debug_info->class->name, refs, debug_refs);
		for (i = 0; i < KREF_DEBUG_HOLDER_MAX; i++) {
			if (debug_info->holders[i] == 0)
				continue;
			seq_printf(seq, "  [%d] = %d", i, debug_info->holders[i]);
			if (debug_info->class->holder_name[i])
				seq_printf(seq, " (%s)", debug_info->class->holder_name[i]);
			seq_putc(seq, '\n');
		}
		seq_putc(seq, '\n');
	}
	spin_unlock_irq(&kref_debug_lock);
}

#endif