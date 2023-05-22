#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x8798f688, "pv_ops" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x92997ed8, "_printk" },
	{ 0xfcca5424, "register_kprobe" },
	{ 0x63026490, "unregister_kprobe" },
	{ 0x9a994cf7, "current_task" },
	{ 0x5a5a2271, "__cpu_online_mask" },
	{ 0x40a0aafc, "__flush_tlb_all" },
	{ 0x63f835ba, "on_each_cpu_cond_mask" },
	{ 0xfb6af58d, "recalc_sigpending" },
	{ 0x6f935e35, "ftrace_set_filter_ip" },
	{ 0x90de72a9, "register_ftrace_function" },
	{ 0xa5a1793e, "unregister_ftrace_function" },
	{ 0x1d19f77b, "physical_mask" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x6a53721c, "find_vpid" },
	{ 0x235bb942, "pid_task" },
	{ 0x72d79d83, "pgdir_shift" },
	{ 0xa648e561, "__ubsan_handle_shift_out_of_bounds" },
	{ 0xd7b002d, "boot_cpu_data" },
	{ 0x8a35b432, "sme_me_mask" },
	{ 0xdad13544, "ptrs_per_p4d" },
	{ 0x39d2d147, "find_vma" },
	{ 0x10f57d97, "get_user_pages_remote" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0xf301d0c, "kmalloc_caches" },
	{ 0x35789eee, "kmem_cache_alloc_trace" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0xc512626a, "__supported_pte_mask" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x64de735d, "param_ops_ulong" },
	{ 0x4fa8f1f1, "param_ops_int" },
	{ 0x541a6db8, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "8A8E1DECFD1349EF8D46793");
