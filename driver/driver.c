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

#include "include/ftrace_wrapper.h"
#include "include/resolve_kallsyms.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wintermute");
MODULE_DESCRIPTION("software watchpoints + physical page swapping on execute/read via page fault handler hooking");
MODULE_VERSION("0.2");

static ulong hooked_addr;
static int hooked_pid;

module_param_named(addr, hooked_addr, ulong, 0644);
module_param_named(pid, hooked_pid, int, 0644);

static pte_t *hooked_ptep;
struct vm_area_struct *hooked_vma;
static struct task_struct *hooked_task;
static int wait;

void invd(void){
    asm volatile ("invd");
}

void flush_all(void) {
    on_each_cpu((void (*)(void *)) __flush_tlb_all, NULL, 1);
    on_each_cpu((void (*)(void *)) invd, NULL, 1);
}

static pte_t *virt_to_pte(struct task_struct *task, unsigned long addr) {
    struct mm_struct *mm = task->mm;

    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;
    pte_t *pte;

    pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        return NULL;
    }

    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        return NULL;
    }

    pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        return NULL;
    }

    pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        return NULL;
    }

    ptep = pte_offset_kernel(pmd, addr);
    if (!ptep) {
        return NULL;
    }

    return ptep;
}

static void (*user_enable_single_step_)(struct task_struct *);
static void (*user_disable_single_step_)(struct task_struct *);
static pte_t (*ptep_clear_flush_)(struct vm_area_struct *, unsigned long, pte_t *);


static asmlinkage vm_fault_t (*orig_handle_pte_fault)(struct vm_fault *);
static asmlinkage void (*orig_arch_do_signal_or_restart)(struct pt_regs *);


asmlinkage vm_fault_t hooked_handle_pte_fault(struct vm_fault *vmf) {
    if (current == hooked_task && pte_offset_map(vmf->pmd, vmf->address) == hooked_ptep && !(vmf->flags & FAULT_FLAG_REMOTE)) {
        vmf->pte = pte_offset_map(vmf->pmd, vmf->address);

        // if (vmf->flags & FAULT_FLAG_INSTRUCTION) {
        if (vmf->real_address == task_pt_regs(current)->ip) {
            printk(KERN_INFO "[swap]: handle_pte_fault INS FETCH ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
        }
        else {
            printk(KERN_INFO "[swap]: handle_pte_fault READ ip @ %llx, vmf->real_address @ %llx", task_pt_regs(current)->ip, vmf->real_address);
        }

        local_irq_disable();
        set_pte(vmf->pte, pte_set_flags(*vmf->pte, _PAGE_PRESENT));
        flush_all();

        user_enable_single_step_(current);
        local_irq_enable();

        return 0;
    }

    return orig_handle_pte_fault(vmf);
}


void hooked_arch_do_signal_or_restart(struct pt_regs *regs) {
    if (current == hooked_task) {
        // printk(KERN_INFO "[swap]: arch_do_signal_or_restart called on task @ %llx\n", hooked_task);
        // printk(KERN_INFO "[swap]: current->ip @ %llx", task_pt_regs(current)->ip);

        local_irq_disable();
        set_pte(hooked_ptep, pte_clear_flags(*hooked_ptep, _PAGE_PRESENT));
        // flush_all();

        user_disable_single_step_(current);
        local_irq_enable();

        sigdelset(&current->pending.signal, SIGTRAP);
        recalc_sigpending();

        return;
    }

    return orig_arch_do_signal_or_restart(regs);
}

static struct ftrace_hook hooks[] = {
    HOOK("handle_pte_fault", hooked_handle_pte_fault, &orig_handle_pte_fault),
    HOOK("arch_do_signal_or_restart", hooked_arch_do_signal_or_restart, &orig_arch_do_signal_or_restart),
};

static int __init swap_driver_init(void) {
    printk(KERN_INFO "[swap]: module loaded\n");

    user_enable_single_step_ = kallsyms_lookup_name_("user_enable_single_step");
    user_disable_single_step_ = kallsyms_lookup_name_("user_disable_single_step");
    ptep_clear_flush_ = kallsyms_lookup_name_("ptep_clear_flush");

    hooked_task = pid_task(find_vpid(hooked_pid), PIDTYPE_PID);
    printk(KERN_INFO "[swap]: hooked task with pid %i found @ %llx", hooked_pid, hooked_task);

    hooked_ptep = virt_to_pte(hooked_task, hooked_addr);
    printk(KERN_INFO "[swap]: hooked ptep for addr %llx found @ %llx", hooked_addr, hooked_ptep);

    hooked_vma = vma_lookup(hooked_task->mm, hooked_addr);

    set_pte(hooked_ptep, pte_clear_flags(*hooked_ptep, _PAGE_PRESENT));
    flush_all();

    int err;
    err = fh_install_hooks(hooks, ARRAY_SIZE(hooks));
    if (err) {
        return err;
    }

    return 0;
}

static void __exit swap_driver_exit(void) {
    fh_remove_hooks(hooks, ARRAY_SIZE(hooks));

    set_pte(hooked_ptep, pte_set_flags(*hooked_ptep, _PAGE_PRESENT));
    flush_all();

    printk(KERN_INFO "[swap]: module unloaded\n");
}

module_init(swap_driver_init);
module_exit(swap_driver_exit);
