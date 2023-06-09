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
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/smp.h>
#include <linux/sched/signal.h>
#include <asm/traps.h>
#include <asm/tlbflush.h>
#include <asm/io.h>
#include <asm/pgtable_types.h>

#include "include/ftrace_helper.h"
#include "include/resolve_ksyms.h"
#include "include/util.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wintermute");
MODULE_DESCRIPTION("software watchpoints + physical page swapping on execute/read via page fault handler hooking");
MODULE_VERSION("0.2");

#define MAX_MARKED_RIPS 16

struct pswap_context {
    unsigned long marked_virt_addr;
    pte_t *ptep;
    struct vm_area_struct *vma;

    unsigned long read_virt_addr;
    unsigned long exec_virt_addr;
    pte_t read_pte;
    pte_t exec_pte;

    /**
     * swap to read page but copy 15 bytes to read page, assumes ins is not reading within a 15 byte distance
     */
    unsigned long marked_rips[MAX_MARKED_RIPS];
};

int pswap_is_marked_rip(struct pswap_context *context, unsigned long addr) {
    for (int i = 0; i < MAX_MARKED_RIPS; i++) {
        if (addr == context->marked_rips[i]) {
            return 1;
        }
    }
    return 0;
}

/**
 * replace this with with ioctls, currently only 1 page is hooked
 */
static int pswap_param_pid;
static ulong pswap_param_marked_virt_addr;
static ulong pswap_param_marked_rip;

module_param_named(pid, pswap_param_pid, int, 0644);
module_param_named(addr, pswap_param_marked_virt_addr, ulong, 0644);
module_param_named(rip, pswap_param_marked_rip, ulong, 0644);


static struct task_struct *pswap_task;
static struct pswap_context pswap_global_context;

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

    pte_t *faulting_pte = pte_offset_map(vmf->pmd, vmf->address);
    if (!(pswap_global_context.ptep == faulting_pte)) {
        goto orig;
    }

    if (pswap_is_marked_rip(&pswap_global_context, task_pt_regs(current)->ip)) {
        printk(KERN_DEBUG "handle_pte_fault READ hooked ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
        set_pte(pswap_global_context.ptep, pswap_global_context.read_pte);
    }
    else if (vmf->real_address == task_pt_regs(current)->ip) {
        printk(KERN_DEBUG "[pswap]: handle_pte_fault INS FETCH ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
        set_pte(pswap_global_context.ptep, pswap_global_context.exec_pte);
    }
    else {
        printk(KERN_DEBUG "[pswap]: handle_pte_fault READ ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
        set_pte(pswap_global_context.ptep, pswap_global_context.read_pte);
    }

    set_pte(pswap_global_context.ptep, pte_set_flags(*pswap_global_context.ptep, _PAGE_PRESENT));
    pswap_flush_all();

    pswap_user_enable_single_step(current);

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

    set_pte(pswap_global_context.ptep, pte_clear_flags(*pswap_global_context.ptep, _PAGE_PRESENT));
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

    pswap_global_context.marked_rips[0] = pswap_param_marked_rip;

    /**
     * currently only 1 page is hooked, this code is kind of unclean
     */
    pswap_global_context.marked_virt_addr = pswap_param_marked_virt_addr;
    pswap_global_context.ptep = pswap_virt_to_pte(pswap_task, pswap_global_context.marked_virt_addr);
    pswap_global_context.vma = vma_lookup(pswap_task->mm, pswap_global_context.marked_virt_addr);

    printk(KERN_DEBUG "[pswap]: hooked ptep for addr %llx found @ %llx", pswap_global_context.marked_virt_addr, pswap_global_context.ptep);

    struct page *p;
    int locked;
    get_user_pages_remote(pswap_task->mm, pswap_global_context.marked_virt_addr, 1, 0, &p, NULL, &locked);
    pswap_global_context.exec_virt_addr = kmap(p);
    pswap_global_context.exec_pte = *pswap_global_context.ptep;

    pswap_global_context.read_virt_addr = kmalloc(PAGE_SIZE, GFP_USER);
    pswap_global_context.read_pte = pfn_pte(virt_to_phys(pswap_global_context.read_virt_addr) >> PAGE_SHIFT, PAGE_SHARED_EXEC);

    /*
     * fun1: 555555555155
     * base: 555555555000
     */
    memcpy(pswap_global_context.read_virt_addr, pswap_global_context.exec_virt_addr, PAGE_SIZE);
    ((char *) pswap_global_context.read_virt_addr)[341] = 0xde;
    ((char *) pswap_global_context.read_virt_addr)[342] = 0xad;
    ((char *) pswap_global_context.read_virt_addr)[343] = 0xbe;
    ((char *) pswap_global_context.read_virt_addr)[344] = 0xef;


    set_pte(pswap_global_context.ptep, pte_clear_flags(*pswap_global_context.ptep, _PAGE_PRESENT));
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

    set_pte(pswap_global_context.ptep, pte_set_flags(*pswap_global_context.ptep, _PAGE_PRESENT));
    pswap_flush_all();

    printk(KERN_DEBUG "[pswap]: module unloaded\n");
}

module_init(pswap_driver_init);
module_exit(pswap_driver_exit);
