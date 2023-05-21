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
 * @file util.h
 * @author wintermute
 * @date 5/21/2023
 * @brief provides utilities for pswap
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/**
 * @brief invalidates cache on calling cpu
 */
void pswap_invd(void){
    asm volatile ("invd");
}

/**
 * @brief flushes all tlbs and invalidates caches on all cpus
 */
void pswap_flush_all(void) {
    on_each_cpu((void (*)(void *)) __flush_tlb_all, NULL, 1);
    on_each_cpu((void (*)(void *)) invd, NULL, 1);
}

/**
 * @brief walks the page table to find the pte for an addr
 *
 * @param task pointer to task_struct to use
 * @param addr address to translate
 * @return pointer to pte
 */
static pte_t *pswap_virt_to_pte(struct task_struct *task, unsigned long addr) {
    if (!task) {
        return NULL;
    }

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

#endif
