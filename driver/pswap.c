/****************************************************************************
 * Copyright (C) 2023 by wintermute                                         *
 *                                                                          *
 * This file is part of pswap.                                              *
 *                                                                          *
 *   pswap is free software: you can redistribute it and/or modify it       *
 *   under the terms of the GNU Lesser General Public License as published  *
 *   by the Free Software Foundation, either version 3 of the License, or   *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   pswap is distributed in the hope that it will be useful,               *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Lesser General Public License for more details.                    *
 *                                                                          *
 *   You should have received a copy of the GNU Lesser General Public       *
 *   License along with pswap.  If not, see <http://www.gnu.org/licenses/>. *
 ****************************************************************************/

/**
 * @file pswap.c
 * @author wintermute
 * @date 5/20/2023
 * @brief entry point for pswap driver
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/smp.h>
#include <linux/sched/signal.h>
#include <asm/traps.h>
#include <asm/tlbflush.h>
#include <asm/io.h>

#include "include/ftrace_helper.h"
#include "include/resolve_ksyms.h"
#include "include/util.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wintermute");
MODULE_DESCRIPTION("software watchpoints + physical page swapping on execute/read via page fault handler hooking");
MODULE_VERSION("0.2");

struct pswap_page_context {
    unsigned long page_addr;
    pte_t *pte;
    struct vm_area_struct *vma;

    unsigned long read_phys_page;
    unsigned long exec_phys_page;

    struct pswap_page_context *next;
};

inline struct pswap_page_context *pswap_find_page_context_by_addr(struct pswap_page_context *watched_pages_contexts, unsigned long page_addr) {
    struct pswap_page_context *current_watched_page_context = watched_pages_contexts;
    if (!current_watched_page_context) {
        goto not_found;
    }

    while (current_watched_page_context->next) {
        if (current_watched_page_context->page_addr == page_addr) {
            goto found;
        }
        current_watched_page_context = current_watched_page_context->next;
    }
    goto not_found;

found:
    return current_watched_page_context;
not_found:
    return NULL;
}

/**
 * replace this with with ioctls, currently only 1 page is hooked
 */
static int pswap_param_pid;
static ulong pswap_param_page_addr;

module_param_named(pid, pswap_param_pid, int, 0644);
module_param_named(page, pswap_param_page_addr, ulong, 0644);

static struct task_struct *pswap_task;

static struct pswap_page_context *pswap_watched_pages_contexts;
static struct pswap_page_context *pswap_current_watched_page_context;

static void (*pswap_user_enable_single_step)(struct task_struct *);
static void (*pswap_user_disable_single_step)(struct task_struct *);

static asmlinkage vm_fault_t (*pswap_orig_handle_pte_fault)(struct vm_fault *);
static asmlinkage void (*pswap_orig_arch_do_signal_or_restart)(struct pt_regs *);

asmlinkage vm_fault_t pswap_hooked_handle_pte_fault(struct vm_fault *vmf) {
    if (!(current == pswap_task)) {
        goto orig;
    }

    if (vmf->flags & FAULT_FLAG_REMOTE) {
        goto orig;
    }

    struct pswap_page_context *current_watched_page_context = pswap_watched_pages_contexts;
    if (!current_watched_page_context) {
        goto orig;
    }

    pte_t *faulting_pte = pte_offset_map(vmf->pmd, vmf->address);

    while (current_watched_page_context->next) {
        if (current_watched_page_context->pte == faulting_pte) {
            goto found;
        }
        current_watched_page_context = current_watched_page_context->next;
    }
    goto orig;

found:
    // if (vmf->flags & FAULT_FLAG_INSTRUCTION) {
    if (vmf->real_address == task_pt_regs(current)->ip) {
        printk(KERN_DEBUG "[pswap]: handle_pte_fault INS FETCH ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
    }
    else {
        printk(KERN_DEBUG "[pswap]: handle_pte_fault READ ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
    }

    set_pte(current_watched_page_context->pte, pte_set_flags(*current_watched_page_context->pte, _PAGE_PRESENT));
    pswap_flush_all();

    pswap_user_enable_single_step(current);

    pswap_current_watched_page_context = current_watched_page_context;

    /**
     * not sure whether to return 0 or orig
     */
    return 0;

orig:
    return pswap_orig_handle_pte_fault(vmf);
}


void pswap_hooked_arch_do_signal_or_restart(struct pt_regs *regs) {
    if (!(current == pswap_task)) {
        goto orig;
    }

    // printk(KERN_DEBUG "[pswap]: arch_do_signal_or_restart called on task @ %llx\n", hooked_task);

    set_pte(pswap_current_watched_page_context->pte, pte_clear_flags(*pswap_current_watched_page_context->pte, _PAGE_PRESENT));
    pswap_flush_all();

    pswap_user_disable_single_step(current);

    sigdelset(&current->pending.signal, SIGTRAP);
    recalc_sigpending();

    return;

orig:
    return pswap_orig_arch_do_signal_or_restart(regs);
}

static struct ftrace_hook pswap_hooks[] = {
    HOOK("handle_pte_fault", pswap_hooked_handle_pte_fault, &pswap_orig_handle_pte_fault),
    HOOK("arch_do_signal_or_restart", pswap_hooked_arch_do_signal_or_restart, &pswap_orig_arch_do_signal_or_restart),
};

static int __init pswap_driver_init(void) {
    printk(KERN_DEBUG "[pswap]: module loaded\n");

    pswap_user_enable_single_step = rk_kallsyms_lookup_name("user_enable_single_step");
    pswap_user_disable_single_step = rk_kallsyms_lookup_name("user_disable_single_step");

    pswap_task = pid_task(find_vpid(pswap_param_pid), PIDTYPE_PID);
    printk(KERN_DEBUG "[pswap]: hooked task with pid %i found @ %llx", pswap_param_pid, pswap_task);

    /**
     * currently only 1 page is hooked, this code is kind of unclean
     */
    pswap_watched_pages_contexts = vmalloc(sizeof(struct pswap_page_context));
    pswap_watched_pages_contexts->page_addr = pswap_param_page_addr;
    pswap_watched_pages_contexts->pte = pswap_virt_to_pte(pswap_task, pswap_watched_pages_contexts->page_addr);
    pswap_watched_pages_contexts->vma = vma_lookup(pswap_task->mm, pswap_watched_pages_contexts->page_addr);
    pswap_watched_pages_contexts->next = NULL;

    printk(KERN_DEBUG "[pswap]: hooked ptep for addr %llx found @ %llx", pswap_watched_pages_contexts->page_addr, pswap_watched_pages_contexts->pte);

    set_pte(pswap_watched_pages_contexts->pte, pte_clear_flags(*pswap_watched_pages_contexts->pte, _PAGE_PRESENT));
    pswap_flush_all();

    int err;
    err = fh_install_hooks(pswap_hooks, ARRAY_SIZE(pswap_hooks));
    if (err) {
        return err;
    }

    return 0;
}

static void __exit pswap_driver_exit(void) {
    fh_remove_hooks(pswap_hooks, ARRAY_SIZE(pswap_hooks));

    struct pswap_page_context *current_watched_page_context = pswap_watched_pages_contexts;
    if (!current_watched_page_context) {
        goto exit;
    }

    while (current_watched_page_context->next) {
        set_pte(current_watched_page_context->pte, pte_set_flags(*current_watched_page_context->pte, _PAGE_PRESENT));
        current_watched_page_context = current_watched_page_context->next;
    }
    pswap_flush_all();

exit:
    printk(KERN_DEBUG "[pswap]: module unloaded\n");
}

module_init(pswap_driver_init);
module_exit(pswap_driver_exit);