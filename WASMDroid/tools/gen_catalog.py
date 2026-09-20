#!/usr/bin/env python3
"""Emit src/catalog.cc — curated Android/Bionic/Binder/kernel/hypervisor rows."""

from pathlib import Path

ROWS = []


def add(ns, lib, *names):
    for n in names:
        ROWS.append((ns, lib, n))


# --- Bionic libc (AOSP bionic) ---
add("Android.Bionic.unistd", "libc",
    "getpid", "getppid", "getuid", "geteuid", "getgid", "getegid", "gettid",
    "getpgid", "setpgid", "getpgrp", "getsid", "setsid", "setuid", "setgid",
    "geteuid", "getegid", "getgroups", "setgroups", "chdir", "fchdir", "getcwd",
    "chroot", "fork", "vfork", "clone", "clone3", "execve", "execl", "execv",
    "execvp", "execvpe", "_exit", "exit", "_Exit", "sleep", "usleep", "nanosleep",
    "alarm", "pause", "pipe", "pipe2", "dup", "dup2", "dup3", "read", "write",
    "pread", "pwrite", "readv", "writev", "lseek", "close", "fsync", "fdatasync",
    "sync", "truncate", "ftruncate", "link", "unlink", "rmdir", "symlink",
    "readlink", "access", "faccessat", "chown", "fchown", "lchown", "chmod",
    "fchmod", "umask", "gethostname", "sethostname", "getpagesize", "sysconf",
    "getentropy", "getrandom", "copy_file_range", "pthread_gettid_np",
    "android_get_device_api_level", "android_get_application_target_sdk_version",
    "android_set_application_target_sdk_version", "android_set_abort_message",
    "getprogname", "setprogname", "arc4random", "arc4random_buf",
    "arc4random_uniform", "getauxval")

add("Android.Bionic.fcntl", "libc",
    "open", "openat", "openat2", "creat", "fcntl", "flock", "posix_fadvise",
    "posix_fallocate", "__open_2")

add("Android.Bionic.stat", "libc",
    "stat", "lstat", "fstat", "fstatat", "statx", "mkdir", "mkdirat", "mknod",
    "mkfifo", "utime", "utimes", "utimensat", "futimens")

add("Android.Bionic.mman", "libc",
    "mmap", "munmap", "mprotect", "msync", "madvise", "mlock", "munlock",
    "mlockall", "munlockall", "mincore", "mremap", "memfd_create",
    "userfaultfd", "ashmem_create_region", "ashmem_set_prot_region",
    "ashmem_pin_region", "ashmem_unpin_region")

add("Android.Bionic.stdlib", "libc",
    "malloc", "calloc", "realloc", "free", "aligned_alloc", "posix_memalign",
    "mallinfo", "malloc_info", "malloc_usable_size", "getenv", "setenv",
    "unsetenv", "putenv", "clearenv", "system", "realpath", "mkstemp",
    "mkdtemp", "abort", "atexit", "atoi", "strtol", "strtoul", "qsort",
    "bsearch", "arc4random")

add("Android.Bionic.stdio", "libc",
    "fopen", "fdopen", "freopen", "fclose", "fread", "fwrite", "fgets", "fputs",
    "fprintf", "snprintf", "vsnprintf", "fseek", "ftell", "fflush", "fileno",
    "perror", "remove", "rename")

add("Android.Bionic.string", "libc",
    "memcpy", "memmove", "memset", "memcmp", "strlen", "strcpy", "strncpy",
    "strlcpy", "strlcat", "strcat", "strcmp", "strncmp", "strcasecmp",
    "strncasecmp", "strchr", "strrchr", "strstr", "strtok_r", "strdup",
    "strerror", "strerror_r")

add("Android.Bionic.pthread", "libpthread",
    "pthread_create", "pthread_join", "pthread_detach", "pthread_exit",
    "pthread_self", "pthread_equal", "pthread_mutex_init", "pthread_mutex_lock",
    "pthread_mutex_trylock", "pthread_mutex_unlock", "pthread_mutex_destroy",
    "pthread_cond_init", "pthread_cond_wait", "pthread_cond_timedwait",
    "pthread_cond_signal", "pthread_cond_broadcast", "pthread_rwlock_rdlock",
    "pthread_rwlock_wrlock", "pthread_rwlock_unlock", "pthread_once",
    "pthread_key_create", "pthread_setspecific", "pthread_getspecific",
    "pthread_gettid_np", "pthread_setname_np", "pthread_getname_np",
    "pthread_attr_setstacksize", "pthread_condattr_setclock")

add("Android.Bionic.dlfcn", "libdl",
    "dlopen", "dlclose", "dlsym", "dlerror", "dladdr", "dlvsym",
    "android_dlopen_ext", "android_get_exported_namespace",
    "android_set_application_target_sdk_version")

add("Android.Bionic.fdsan", "libc",
    "android_fdsan_set_error_level", "android_fdsan_get_error_level",
    "android_fdsan_close_with_tag", "android_fdsan_exchange_owner_tag",
    "android_fdtrack_set_enabled")

add("Android.Bionic.net", "libc",
    "socket", "bind", "listen", "accept", "accept4", "connect", "shutdown",
    "send", "sendto", "sendmsg", "recv", "recvfrom", "recvmsg", "getsockopt",
    "setsockopt", "getaddrinfo", "freeaddrinfo", "android_getaddrinfofornet",
    "inet_pton", "inet_ntop", "poll", "ppoll", "select", "epoll_create1",
    "epoll_ctl", "epoll_wait", "epoll_pwait")

add("Android.Bionic.log", "liblog",
    "__android_log_write", "__android_log_print", "__android_log_vprint",
    "__android_log_assert", "__android_log_is_loggable",
    "__android_log_buf_write", "__android_log_buf_print",
    "__android_log_error_write", "ALOGV", "ALOGD", "ALOGI", "ALOGW", "ALOGE")

add("Android.Bionic.property", "libc",
    "__system_property_get", "__system_property_set", "__system_property_find",
    "__system_property_read", "__system_property_read_callback",
    "__system_property_foreach", "__system_property_wait",
    "__system_property_area_init", "property_get", "property_set",
    "property_list", "PROP_VALUE_MAX")

add("Android.Bionic.cutils", "libcutils",
    "property_get", "property_set", "property_list", "strlcpy", "strlcat",
    "android_atomic_inc", "android_atomic_dec", "android_atomic_or",
    "android_atomic_and", "android_atomic_cmpxchg", "socket_local_client",
    "socket_local_server", "socket_network_client", "klog_write")

add("Android.Bionic.signal", "libc",
    "signal", "sigaction", "sigprocmask", "sigpending", "sigsuspend",
    "kill", "killpg", "raise", "sigaltstack", "sigwait", "sigwaitinfo",
    "sigtimedwait", "pthread_kill", "pthread_sigmask", "sigemptyset",
    "sigfillset", "sigaddset", "sigdelset", "sigismember", "abort",
    "tgkill", "tkill", "rt_sigqueueinfo")

add("Android.Bionic.time", "libc",
    "time", "gettimeofday", "settimeofday", "clock_gettime", "clock_settime",
    "clock_getres", "clock_nanosleep", "nanosleep", "times", "localtime",
    "gmtime", "mktime", "strftime", "strptime", "tzset", "timer_create",
    "timer_settime", "timer_gettime", "timer_delete", "clock_getcpuclockid")

add("Android.Bionic.dirent", "libc",
    "opendir", "fdopendir", "readdir", "readdir_r", "rewinddir", "closedir",
    "dirfd", "scandir", "alphasort", "seekdir", "telldir")

add("Android.Bionic.wait", "libc",
    "wait", "waitpid", "waitid", "wait3", "wait4", "wait6")

add("Android.Bionic.ioctl", "libc",
    "ioctl", "syscall", "prctl", "ptrace", "capget", "capset", "personality",
    "unshare", "setns", "pivot_root", "mount", "umount", "umount2",
    "swapon", "swapoff", "reboot", "sysinfo", "uname")

add("Android.Bionic.resource", "libc",
    "getrlimit", "setrlimit", "prlimit", "getrusage", "getpriority",
    "setpriority", "nice", "sched_yield", "sched_setscheduler",
    "sched_getscheduler", "sched_setaffinity", "sched_getaffinity",
    "sched_get_priority_max", "sched_get_priority_min")

add("Android.Bionic.pwd", "libc",
    "getpwnam", "getpwuid", "getpwnam_r", "getpwuid_r", "getgrnam",
    "getgrgid", "getgrnam_r", "getgrgid_r", "getlogin", "getlogin_r")

add("Android.Bionic.termios", "libc",
    "tcgetattr", "tcsetattr", "tcsendbreak", "tcdrain", "tcflush", "tcflow",
    "cfgetispeed", "cfgetospeed", "cfsetispeed", "cfsetospeed", "cfmakeraw")

# --- Binder / ashmem / ion ---
add("Android.Binder", "libbinder",
    "BINDER_WRITE_READ", "BINDER_SET_IDLE_TIMEOUT", "BINDER_SET_MAX_THREADS",
    "BINDER_SET_IDLE_PRIORITY", "BINDER_SET_CONTEXT_MGR", "BINDER_THREAD_EXIT",
    "BINDER_VERSION", "BINDER_GET_NODE_DEBUG_INFO", "BINDER_SET_CONTEXT_MGR_EXT",
    "BINDER_ENABLE_ONEWAY_SPAM_DETECTION", "BINDER_GET_EXTENDED_ERROR",
    "BINDER_GET_NODE_INFO_FOR_REF", "BINDER_FREEZE", "BINDER_GET_FROZEN_INFO",
    "BC_TRANSACTION", "BC_REPLY", "BC_ACQUIRE_RESULT", "BC_FREE_BUFFER",
    "BC_INCREFS", "BC_ACQUIRE", "BC_RELEASE", "BC_DECREFS", "BC_INCREFS_DONE",
    "BC_ACQUIRE_DONE", "BC_ATTEMPT_ACQUIRE", "BC_REGISTER_LOOPER",
    "BC_ENTER_LOOPER", "BC_EXIT_LOOPER", "BC_REQUEST_DEATH_NOTIFICATION",
    "BC_CLEAR_DEATH_NOTIFICATION", "BC_DEAD_BINDER_DONE", "BC_TRANSACTION_SG",
    "BC_REPLY_SG",
    "BR_ERROR", "BR_OK", "BR_TRANSACTION", "BR_REPLY", "BR_ACQUIRE_RESULT",
    "BR_DEAD_REPLY", "BR_TRANSACTION_COMPLETE", "BR_INCREFS", "BR_ACQUIRE",
    "BR_RELEASE", "BR_DECREFS", "BR_ATTEMPT_ACQUIRE", "BR_NOOP",
    "BR_SPAWN_LOOPER", "BR_FINISHED", "BR_DEAD_BINDER",
    "BR_CLEAR_DEATH_NOTIFICATION_DONE", "BR_FAILED_REPLY", "BR_FROZEN_REPLY",
    "BR_ONEWAY_SPAM_SUSPECT", "BR_TRANSACTION_PENDING_FROZEN",
    "IBinder", "IBinder_transact", "IBinder_incStrong", "IBinder_decStrong",
    "IBinder_linkToDeath", "IBinder_unlinkToDeath", "IBinder_queryLocalInterface",
    "Parcel", "Parcel_writeInt32", "Parcel_readInt32", "Parcel_writeString",
    "Parcel_readString", "Parcel_writeStrongBinder", "Parcel_readStrongBinder",
    "Parcel_writeFileDescriptor", "Parcel_readFileDescriptor",
    "ProcessState", "IPCThreadState", "defaultServiceManager",
    "IServiceManager", "IServiceManager_addService", "IServiceManager_getService",
    "IServiceManager_checkService", "IServiceManager_listServices",
    "AIBinder_transact", "AIBinder_incStrong", "AIBinder_decStrong",
    "AIBinder_linkToDeath", "AIBinder_unlinkToDeath", "AIBinder_associateClass",
    "AParcel_create", "AParcel_writeString", "AParcel_readString",
    "AParcel_writeInt32", "AParcel_readInt32",
    "AServiceManager_addService", "AServiceManager_getService",
    "AServiceManager_checkService", "AServiceManager_listServices",
    "AServiceManager_registerForServiceNotifications")

add("Android.Ashmem", "libcutils",
    "ASHMEM_SET_NAME", "ASHMEM_GET_NAME", "ASHMEM_SET_SIZE", "ASHMEM_GET_SIZE",
    "ASHMEM_SET_PROT_MASK", "ASHMEM_GET_PROT_MASK", "ASHMEM_PIN", "ASHMEM_UNPIN",
    "ASHMEM_GET_PIN_STATUS", "ASHMEM_PURGE_ALL_CACHES",
    "ashmem_create_region", "ashmem_set_prot_region", "ashmem_pin_region",
    "ashmem_unpin_region", "ashmem_get_size_region")

add("Android.Ion", "libion",
    "ION_IOC_ALLOC", "ION_IOC_FREE", "ION_IOC_MAP", "ION_IOC_SHARE",
    "ION_IOC_IMPORT", "ION_IOC_SYNC", "ION_IOC_CUSTOM",
    "ION_HEAP_TYPE_SYSTEM", "ION_HEAP_TYPE_SYSTEM_CONTIG",
    "ION_HEAP_TYPE_CARVEOUT", "ION_HEAP_TYPE_CHUNK", "ION_HEAP_TYPE_DMA",
    "ION_HEAP_TYPE_SYSTEM_SECURE")

add("Android.Dmabuf", "libc",
    "DMA_BUF_IOCTL_SYNC", "DMA_BUF_SET_NAME", "DMA_BUF_IOCTL_EXPORT",
    "dma_buf_fd", "dma_buf_get", "dma_buf_put", "dma_buf_begin_cpu_access",
    "dma_buf_end_cpu_access", "dma_buf_map_attachment", "dma_buf_unmap_attachment")

# --- Kernel / vmlinux (ntoskrnl analog). Public EXPORT_SYMBOL
# names from include/linux/*.h — not a dump of vmlinux.o. ---
add("Android.Kernel.slab", "vmlinux",
    "kmalloc", "kzalloc", "kcalloc", "kfree", "krealloc", "kfree_sensitive",
    "kvmalloc", "kvzalloc", "kvfree", "kmalloc_array", "kmalloc_node",
    "kmem_cache_create", "kmem_cache_create_usercopy", "kmem_cache_destroy",
    "kmem_cache_alloc", "kmem_cache_alloc_node", "kmem_cache_free",
    "kmem_cache_alloc_bulk", "kmem_cache_free_bulk", "kmem_cache_size",
    "ksize", "kstrdup", "kstrndup", "kmemdup", "kmemdup_nul",
    "mempool_create", "mempool_alloc", "mempool_free", "mempool_destroy",
    "devm_kmalloc", "devm_kzalloc", "devm_kfree", "__kmalloc",
    "kmem_cache_alloc_lru", "slab_is_available")

add("Android.Kernel.mm", "vmlinux",
    "vmalloc", "vzalloc", "vmalloc_user", "vmalloc_node", "vfree", "vmap",
    "vunmap", "vmap_pfn", "__get_free_pages", "__get_free_page", "get_zeroed_page",
    "free_pages", "free_page", "alloc_pages", "alloc_pages_node", "__free_pages",
    "get_user_pages", "get_user_pages_fast", "pin_user_pages", "unpin_user_pages",
    "put_page", "get_page", "set_page_dirty", "lock_page", "unlock_page",
    "remap_pfn_range", "vm_insert_page", "vm_mmap", "vm_munmap",
    "ioremap", "ioremap_wc", "ioremap_wt", "ioremap_np", "iounmap",
    "memremap", "memunmap", "phys_to_virt", "virt_to_phys", "page_to_phys",
    "pfn_to_page", "page_to_pfn", "virt_to_page", "pfn_to_kaddr",
    "copy_to_user", "copy_from_user", "copy_in_user", "get_user", "put_user",
    "access_ok", "clear_user", "strncpy_from_user", "strnlen_user",
    "probe_kernel_read", "probe_kernel_write", "copy_from_kernel_nofault",
    "copy_to_kernel_nofault", "set_memory_ro", "set_memory_rw",
    "set_memory_x", "set_memory_nx", "set_pages_ro",
    "high_memory", "PAGE_OFFSET", "TASK_SIZE", "VMALLOC_START",
    "flush_dcache_page", "flush_icache_range", "invalidate_kernel_vmap_range",
    "kmap", "kunmap", "kmap_atomic", "kunmap_atomic", "kmap_local_page",
    "kunmap_local", "vmalloc_to_page", "vmalloc_to_pfn",
    "follow_pfn", "follow_pte", "apply_to_page_range")

add("Android.Kernel.sched", "vmlinux",
    "schedule", "schedule_timeout", "schedule_timeout_interruptible",
    "schedule_timeout_uninterruptible", "cond_resched", "need_resched",
    "preempt_disable", "preempt_enable", "preempt_count",
    "wake_up_process", "wake_up_state", "set_current_state",
    "kthread_run", "kthread_create", "kthread_stop", "kthread_should_stop",
    "kthread_park", "kthread_unpark", "kthread_bind", "kthread_data",
    "complete", "complete_all", "wait_for_completion",
    "wait_for_completion_timeout", "wait_for_completion_interruptible",
    "init_completion", "reinit_completion", "try_wait_for_completion",
    "wake_up", "wake_up_all", "wake_up_interruptible", "init_waitqueue_head",
    "add_wait_queue", "remove_wait_queue", "prepare_to_wait", "finish_wait",
    "yield", "msleep", "msleep_interruptible", "ssleep", "usleep_range",
    "ndelay", "udelay", "mdelay", "current", "get_current", "task_pid_nr",
    "pid_task", "find_task_by_vpid", "get_task_struct", "put_task_struct",
    "set_cpus_allowed_ptr", "cpumask_of", "smp_processor_id",
    "on_each_cpu", "smp_call_function", "smp_call_function_single",
    "kick_process", "send_sig", "force_sig", "allow_signal", "flush_signals")

add("Android.Kernel.irq", "vmlinux",
    "request_irq", "request_threaded_irq", "free_irq", "enable_irq",
    "disable_irq", "disable_irq_nosync", "synchronize_irq",
    "local_irq_save", "local_irq_restore", "local_irq_disable",
    "local_irq_enable", "irqs_disabled", "in_interrupt", "in_irq",
    "in_softirq", "in_nmi", "irq_set_chip", "irq_set_handler",
    "irq_set_chip_and_handler", "irq_set_affinity", "irq_get_irq_data",
    "handle_simple_irq", "handle_level_irq", "handle_edge_irq",
    "handle_fasteoi_irq", "generic_handle_irq", "irq_create_mapping",
    "irq_find_mapping", "irq_dispose_mapping", "irq_domain_add_linear",
    "irq_domain_remove", "tasklet_init", "tasklet_schedule",
    "tasklet_hi_schedule", "tasklet_kill", "tasklet_disable", "tasklet_enable",
    "raise_softirq", "open_softirq", "in_serving_softirq",
    "local_bh_disable", "local_bh_enable", "napi_schedule", "napi_complete")

add("Android.Kernel.lock", "vmlinux",
    "spin_lock", "spin_unlock", "spin_lock_irqsave", "spin_unlock_irqrestore",
    "spin_lock_irq", "spin_unlock_irq", "spin_lock_bh", "spin_unlock_bh",
    "spin_trylock", "spin_is_locked", "spin_lock_init",
    "raw_spin_lock", "raw_spin_unlock", "raw_spin_lock_irqsave",
    "mutex_init", "mutex_lock", "mutex_unlock", "mutex_trylock",
    "mutex_is_locked", "mutex_lock_interruptible", "mutex_destroy",
    "rwsem_down_read", "rwsem_up_read", "rwsem_down_write", "rwsem_up_write",
    "init_rwsem", "down_read", "up_read", "down_write", "up_write",
    "down", "up", "sema_init", "down_interruptible", "down_trylock",
    "read_lock", "read_unlock", "write_lock", "write_unlock", "rwlock_init",
    "seqlock_init", "write_seqlock", "write_sequnlock", "read_seqbegin",
    "read_seqretry", "atomic_read", "atomic_set", "atomic_inc", "atomic_dec",
    "atomic_add", "atomic_sub", "atomic_cmpxchg", "atomic_xchg",
    "atomic64_read", "atomic64_set", "smp_mb", "smp_rmb", "smp_wmb",
    "smp_load_acquire", "smp_store_release", "barrier",
    "rcu_read_lock", "rcu_read_unlock", "synchronize_rcu", "call_rcu",
    "rcu_barrier", "srcu_read_lock", "srcu_read_unlock", "synchronize_srcu",
    "rcu_assign_pointer", "rcu_dereference", "kfree_rcu",
    "wake_lock", "wake_unlock", "pm_stay_awake", "pm_relax",
    "preempt_notifier_register", "lockdep_assert_held")

add("Android.Kernel.workqueue", "vmlinux",
    "schedule_work", "schedule_delayed_work", "queue_work", "queue_delayed_work",
    "mod_delayed_work", "flush_work", "flush_delayed_work", "cancel_work_sync",
    "cancel_delayed_work_sync", "flush_workqueue", "destroy_workqueue",
    "alloc_workqueue", "alloc_ordered_workqueue", "create_workqueue",
    "create_singlethread_workqueue", "INIT_WORK", "INIT_DELAYED_WORK",
    "system_wq", "system_highpri_wq", "system_long_wq", "system_unbound_wq",
    "system_freezable_wq", "queue_work_on", "schedule_work_on",
    "execute_in_process_context", "task_work_add")

add("Android.Kernel.module", "vmlinux",
    "module_init", "module_exit", "request_module", "try_then_request_module",
    "try_module_get", "module_put", "__module_get", "symbol_get", "symbol_put",
    "find_module", "find_symbol", "kallsyms_lookup_name",
    "kallsyms_lookup", "kallsyms_on_each_symbol", "sprint_symbol",
    "lookup_symbol_name", "module_kallsyms_lookup_name",
    "register_module_notifier", "unregister_module_notifier",
    "init_module", "finit_module", "delete_module", "kernel_read_file_from_path",
    "load_module", "MODULE_LICENSE", "MODULE_AUTHOR", "MODULE_DESCRIPTION",
    "EXPORT_SYMBOL", "EXPORT_SYMBOL_GPL", "THIS_MODULE")

add("Android.Kernel.binder", "vmlinux",
    "binder_ioctl", "binder_transaction", "binder_thread_write",
    "binder_thread_read", "binder_alloc_new_buf", "binder_alloc_free_buf",
    "binder_get_node", "binder_inc_node", "binder_dec_node",
    "binder_update_page_range", "binder_deferred_release",
    "binder_open", "binder_mmap", "binder_poll", "binder_flush",
    "binder_alloc_mmap_handler", "binder_alloc_vma_close",
    "binder_translate_binder", "binder_translate_handle",
    "binder_translate_fd", "binder_proc_transaction",
    "binder_send_deferred_complete", "binder_free_buf",
    "binder_node_release", "binder_wakeup_thread_ilocked")

add("Android.Kernel.ashmem", "vmlinux",
    "ashmem_shrink", "ashmem_pin", "ashmem_unpin", "ashmem_ioctl",
    "ashmem_mmap", "ashmem_open", "ashmem_release", "ashmem_read_iter",
    "ashmem_llseek", "ashmem_shrink_scan", "ashmem_shrink_count")

add("Android.Kernel.lmk", "vmlinux",
    "lowmemorykiller", "lmkd", "oom_score_adj", "try_to_freeze",
    "register_oom_notifier", "unregister_oom_notifier",
    "lowmem_shrink", "lowmem_adj", "lowmem_minfree",
    "lmkd_wakeup", "psi_memstall_enter", "psi_memstall_leave")

add("Android.Kernel.kvm_host", "vmlinux",
    "kvm_arch_vcpu_create", "kvm_arch_vcpu_destroy", "kvm_arch_vcpu_load",
    "kvm_arch_vcpu_put", "kvm_arch_vcpu_ioctl_run", "kvm_vcpu_halt",
    "kvm_vcpu_block", "kvm_vcpu_kick", "kvm_vcpu_wake_up", "kvm_vcpu_yield_to",
    "kvm_vcpu_on_spin", "kvm_vcpu_mark_page_dirty", "mark_page_dirty",
    "kvm_read_guest", "kvm_write_guest", "kvm_read_guest_page",
    "kvm_write_guest_page", "kvm_vcpu_read_guest", "kvm_vcpu_write_guest",
    "kvm_vcpu_read_guest_page", "kvm_vcpu_write_guest_page",
    "kvm_clear_guest", "kvm_gfn_to_hva_cache_init",
    "gfn_to_hva", "gfn_to_hva_prot", "gfn_to_pfn", "gfn_to_pfn_prot",
    "gfn_to_page", "hva_to_gfn", "gpa_to_gfn", "gfn_to_gpa",
    "kvm_is_error_hva", "kvm_is_error_pfn", "kvm_is_error_gpa",
    "kvm_flush_remote_tlbs", "kvm_flush_remote_tlbs_range",
    "kvm_mmu_unload", "kvm_mmu_load", "kvm_mmu_reload", "kvm_mmu_sync_roots",
    "kvm_mmu_slot_remove_write_access", "kvm_zap_gfn_range",
    "kvm_emulate_instruction", "kvm_skip_emulated_instruction",
    "kvm_emulate_halt", "kvm_emulate_wbinvd", "kvm_emulate_hypercall",
    "kvm_emulate_cpuid", "kvm_fast_pio", "kvm_emulate_pio",
    "emulator_read_std", "emulator_write_std", "emulator_cmpxchg_emulated",
    "kvm_io_bus_write", "kvm_io_bus_read", "kvm_io_bus_register_dev",
    "kvm_io_bus_unregister_dev", "kvm_iodevice_init",
    "kvm_set_memory_region", "__kvm_set_memory_region",
    "kvm_get_dirty_log", "kvm_get_dirty_log_protect",
    "kvm_vm_ioctl", "kvm_vcpu_ioctl", "kvm_dev_ioctl",
    "kvm_vm_ioctl_check_extension", "kvm_dev_ioctl_create_vm",
    "kvm_vm_ioctl_create_vcpu", "kvm_set_cr0", "kvm_set_cr3", "kvm_set_cr4",
    "kvm_set_cr8", "kvm_get_cr8", "kvm_set_msr", "kvm_get_msr",
    "kvm_set_msr_common", "kvm_get_msr_common", "kvm_scale_tsc",
    "kvm_write_tsc", "kvm_read_l1_tsc", "kvm_get_linear_rip",
    "kvm_inject_gp", "kvm_inject_ud", "kvm_inject_page_fault",
    "kvm_queue_exception", "kvm_requeue_exception", "kvm_make_request",
    "kvm_check_request", "kvm_test_request", "kvm_clear_request",
    "kvm_apic_has_interrupt", "kvm_cpu_has_interrupt", "kvm_cpu_has_extint",
    "kvm_pic_read_irq", "kvm_set_irq", "kvm_set_msi", "kvm_irq_delivery_to_apic",
    "kvm_arch_init", "kvm_arch_exit", "kvm_init", "kvm_exit",
    "kvm_get_kvm", "kvm_put_kvm", "kvm_get_vcpu", "kvm_for_each_vcpu",
    "gfn_to_memslot", "search_memslots", "id_to_memslot",
    "kvm_vcpu_gfn_to_memslot", "kvm_vcpu_gfn_to_hva", "kvm_vcpu_gfn_to_pfn")

add("Android.Kernel.pci", "vmlinux",
    "pci_enable_device", "pci_disable_device", "pci_request_regions",
    "pci_release_regions", "pci_set_master", "pci_clear_master",
    "pci_set_drvdata", "pci_get_drvdata", "pci_find_capability",
    "pci_find_next_capability", "pci_read_config_byte", "pci_read_config_word",
    "pci_read_config_dword", "pci_write_config_byte", "pci_write_config_word",
    "pci_write_config_dword", "pci_iomap", "pci_iounmap", "pci_resource_start",
    "pci_resource_len", "pci_irq_vector", "pci_alloc_irq_vectors",
    "pci_free_irq_vectors", "pci_enable_msix_range", "pci_enable_msi",
    "pci_disable_msi", "pci_disable_msix", "pci_save_state", "pci_restore_state",
    "pci_register_driver", "pci_unregister_driver", "pci_get_device",
    "pci_dev_put", "pci_dev_get", "pci_walk_bus", "pci_scan_bus")

add("Android.Kernel.dma", "vmlinux",
    "dma_alloc_coherent", "dma_free_coherent", "dma_alloc_attrs",
    "dma_free_attrs", "dma_map_single", "dma_unmap_single",
    "dma_map_page", "dma_unmap_page", "dma_map_sg", "dma_unmap_sg",
    "dma_sync_single_for_cpu", "dma_sync_single_for_device",
    "dma_set_mask", "dma_set_coherent_mask", "dma_set_mask_and_coherent",
    "dma_get_required_mask", "dma_mmap_coherent", "dma_declare_coherent_memory")

add("Android.Kernel.iommu", "vmlinux",
    "iommu_domain_alloc", "iommu_domain_free", "iommu_attach_device",
    "iommu_detach_device", "iommu_map", "iommu_unmap", "iommu_iova_to_phys",
    "iommu_set_fault_handler", "iommu_present", "iommu_capable",
    "iommu_group_get", "iommu_group_put", "iommu_group_add_device",
    "iommu_register_device", "iommu_fwspec_init", "iommu_dev_enable_feature",
    "iommu_map_sg", "iommu_unmap_fast", "iommu_flush_iotlb_all",
    "iommu_aux_attach_device", "iommu_sva_bind_device", "iommu_sva_unbind_device")

add("Android.Kernel.net", "vmlinux",
    "alloc_netdev", "alloc_etherdev", "free_netdev", "register_netdev",
    "unregister_netdev", "netif_start_queue", "netif_stop_queue",
    "netif_wake_queue", "netif_carrier_on", "netif_carrier_off",
    "netif_rx", "netif_receive_skb", "napi_gro_receive", "napi_enable",
    "napi_disable", "dev_queue_xmit", "dev_kfree_skb", "dev_alloc_skb",
    "skb_put", "skb_push", "skb_pull", "skb_reserve", "skb_copy",
    "netdev_priv", "ether_setup", "eth_type_trans", "eth_mac_addr",
    "rtnl_lock", "rtnl_unlock", "register_netdevice_notifier",
    "sock_create", "sock_create_kern", "sock_release", "kernel_sendmsg",
    "kernel_recvmsg", "kernel_bind", "kernel_listen", "kernel_accept",
    "kernel_connect", "sock_register", "proto_register")

add("Android.Kernel.fs", "vmlinux",
    "vfs_read", "vfs_write", "vfs_fsync", "vfs_getattr", "vfs_setattr",
    "vfs_create", "vfs_mkdir", "vfs_rmdir", "vfs_unlink", "vfs_link",
    "vfs_symlink", "vfs_rename", "filp_open", "filp_close", "kernel_read",
    "kernel_write", "kernel_read_file", "invalidate_mapping_pages",
    "iget_locked", "unlock_new_inode", "iput", "igrab", "d_instantiate",
    "d_alloc", "dput", "dget", "mntget", "mntput", "simple_fill_super",
    "register_filesystem", "unregister_filesystem", "kern_mount",
    "kern_unmount", "sysfs_create_file", "sysfs_remove_file",
    "sysfs_create_group", "proc_create", "proc_remove", "seq_printf",
    "debugfs_create_dir", "debugfs_create_file", "debugfs_remove")

add("Android.Kernel.time", "vmlinux",
    "ktime_get", "ktime_get_ns", "ktime_get_real", "ktime_get_boottime",
    "ktime_get_clocktai", "jiffies", "jiffies_to_msecs", "msecs_to_jiffies",
    "nsecs_to_jiffies", "get_jiffies_64", "ktime_add", "ktime_sub",
    "hrtimer_init", "hrtimer_start", "hrtimer_cancel", "hrtimer_forward",
    "mod_timer", "add_timer", "del_timer", "del_timer_sync", "timer_setup",
    "from_timer", "time64_to_tm", "ktime_to_timespec64")

add("Android.Kernel.trace", "vmlinux",
    "trace_event", "tracepoint_probe_register", "tracepoint_probe_unregister",
    "register_kprobe", "unregister_kprobe", "register_kretprobe",
    "unregister_kretprobe", "register_ftrace_function",
    "unregister_ftrace_function", "ftrace_set_filter", "register_die_notifier",
    "register_nmi_handler", "perf_event_create_kernel_counter",
    "perf_event_release_kernel", "perf_sw_event", "trace_clock",
    "ring_buffer_alloc", "ring_buffer_free", "ring_buffer_lock_reserve")

add("Android.Kernel.ns", "vmlinux",
    "copy_namespaces", "unshare_nsproxy_namespaces", "switch_task_namespaces",
    "free_nsproxy", "create_pid_namespace", "copy_pid_ns", "put_pid_ns",
    "copy_net_ns", "get_net", "put_net", "copy_mnt_ns", "copy_uts_ns",
    "copy_ipc_ns", "copy_user_ns", "copy_cgroup_ns", "copy_time_ns",
    "ns_get_path", "proc_ns_file", "setns", "unshare",
    "CLONE_NEWNS", "CLONE_NEWCGROUP", "CLONE_NEWUTS", "CLONE_NEWIPC",
    "CLONE_NEWUSER", "CLONE_NEWPID", "CLONE_NEWNET", "CLONE_NEWTIME",
    "CLONE_VM", "CLONE_FS", "CLONE_FILES", "CLONE_SIGHAND", "CLONE_PTRACE",
    "CLONE_VFORK", "CLONE_THREAD")

add("Android.Kernel.cgroup", "vmlinux",
    "cgroup_procs_write", "cgroup_attach_task", "cgroup_mkdir", "cgroup_rmdir",
    "cgroup_add_cftypes", "cgroup_path", "task_cgroup", "css_set_move_task",
    "cgroup_get", "cgroup_put", "cgroup_lock", "cgroup_unlock",
    "cgroup_ssid_enabled", "cgroup_on_dfl", "cgroup_is_populated",
    "mem_cgroup_from_task", "memcg_kmem_enabled", "cgroup_bpf_run_filter_skb")

add("Android.SELinux", "libselinux",
    "is_selinux_enabled", "security_getenforce", "security_setenforce",
    "getcon", "setcon", "getpidcon", "setexeccon", "selinux_android_restorecon",
    "selinux_android_setcontext", "selinux_check_access", "avc_has_perm",
    "security_compute_av", "selinux_status_open", "selinux_android_use_data_policy")

add("Android.BPF", "libbpf",
    "bpf", "bpf_prog_load", "bpf_map_create", "bpf_map_update_elem",
    "bpf_map_lookup_elem", "bpf_map_delete_elem", "bpf_map_get_next_key",
    "bpf_obj_pin", "bpf_obj_get", "bpf_prog_attach", "bpf_prog_detach",
    "bpf_prog_test_run", "bpf_prog_get_fd_by_id", "bpf_map_get_fd_by_id",
    "bpf_prog_get_next_id", "bpf_map_get_next_id", "bpf_btf_load",
    "bpf_link_create", "bpf_link_update", "bpf_link_detach",
    "BPF_MAP_CREATE", "BPF_MAP_LOOKUP_ELEM", "BPF_MAP_UPDATE_ELEM",
    "BPF_MAP_DELETE_ELEM", "BPF_MAP_GET_NEXT_KEY", "BPF_PROG_LOAD",
    "BPF_OBJ_PIN", "BPF_OBJ_GET", "BPF_PROG_ATTACH", "BPF_PROG_DETACH",
    "BPF_PROG_TEST_RUN", "BPF_PROG_GET_NEXT_ID", "BPF_MAP_GET_NEXT_ID",
    "BPF_PROG_GET_FD_BY_ID", "BPF_MAP_GET_FD_BY_ID", "BPF_OBJ_GET_INFO_BY_FD",
    "BPF_PROG_QUERY", "BPF_RAW_TRACEPOINT_OPEN", "BPF_BTF_LOAD",
    "BPF_BTF_GET_FD_BY_ID", "BPF_TASK_FD_QUERY", "BPF_MAP_LOOKUP_AND_DELETE_ELEM",
    "BPF_MAP_FREEZE", "BPF_BTF_GET_NEXT_ID", "BPF_MAP_LOOKUP_BATCH",
    "BPF_MAP_UPDATE_BATCH", "BPF_LINK_CREATE", "BPF_LINK_UPDATE",
    "BPF_LINK_GET_FD_BY_ID", "BPF_ENABLE_STATS", "BPF_ITER_CREATE",
    "BPF_LINK_DETACH", "BPF_PROG_BIND_MAP",
    "BPF_MAP_TYPE_HASH", "BPF_MAP_TYPE_ARRAY", "BPF_MAP_TYPE_PROG_ARRAY",
    "BPF_MAP_TYPE_PERF_EVENT_ARRAY", "BPF_MAP_TYPE_PERCPU_HASH",
    "BPF_MAP_TYPE_PERCPU_ARRAY", "BPF_MAP_TYPE_STACK_TRACE",
    "BPF_MAP_TYPE_CGROUP_ARRAY", "BPF_MAP_TYPE_LRU_HASH",
    "BPF_MAP_TYPE_LRU_PERCPU_HASH", "BPF_MAP_TYPE_LPM_TRIE",
    "BPF_MAP_TYPE_ARRAY_OF_MAPS", "BPF_MAP_TYPE_HASH_OF_MAPS",
    "BPF_MAP_TYPE_DEVMAP", "BPF_MAP_TYPE_SOCKMAP", "BPF_MAP_TYPE_CPUMAP",
    "BPF_MAP_TYPE_XSKMAP", "BPF_MAP_TYPE_SOCKHASH", "BPF_MAP_TYPE_CGROUP_STORAGE",
    "BPF_MAP_TYPE_REUSEPORT_SOCKARRAY", "BPF_MAP_TYPE_PERCPU_CGROUP_STORAGE",
    "BPF_MAP_TYPE_QUEUE", "BPF_MAP_TYPE_STACK", "BPF_MAP_TYPE_SK_STORAGE",
    "BPF_MAP_TYPE_DEVMAP_HASH", "BPF_MAP_TYPE_STRUCT_OPS", "BPF_MAP_TYPE_RINGBUF",
    "BPF_MAP_TYPE_INODE_STORAGE", "BPF_MAP_TYPE_TASK_STORAGE",
    "BPF_PROG_TYPE_SOCKET_FILTER", "BPF_PROG_TYPE_KPROBE",
    "BPF_PROG_TYPE_SCHED_CLS", "BPF_PROG_TYPE_SCHED_ACT",
    "BPF_PROG_TYPE_TRACEPOINT", "BPF_PROG_TYPE_XDP", "BPF_PROG_TYPE_PERF_EVENT",
    "BPF_PROG_TYPE_CGROUP_SKB", "BPF_PROG_TYPE_CGROUP_SOCK",
    "BPF_PROG_TYPE_LWT_IN", "BPF_PROG_TYPE_LWT_OUT", "BPF_PROG_TYPE_LWT_XMIT",
    "BPF_PROG_TYPE_SOCK_OPS", "BPF_PROG_TYPE_SK_SKB", "BPF_PROG_TYPE_CGROUP_DEVICE",
    "BPF_PROG_TYPE_SK_MSG", "BPF_PROG_TYPE_RAW_TRACEPOINT",
    "BPF_PROG_TYPE_CGROUP_SOCK_ADDR", "BPF_PROG_TYPE_LWT_SEG6LOCAL",
    "BPF_PROG_TYPE_LIRC_MODE2", "BPF_PROG_TYPE_SK_REUSEPORT",
    "BPF_PROG_TYPE_FLOW_DISSECTOR", "BPF_PROG_TYPE_CGROUP_SYSCTL",
    "BPF_PROG_TYPE_RAW_TRACEPOINT_WRITABLE", "BPF_PROG_TYPE_CGROUP_SOCKOPT",
    "BPF_PROG_TYPE_TRACING", "BPF_PROG_TYPE_STRUCT_OPS", "BPF_PROG_TYPE_EXT",
    "BPF_PROG_TYPE_LSM", "BPF_PROG_TYPE_SK_LOOKUP", "BPF_PROG_TYPE_SYSCALL")

# --- Hypervisor: KVM + pKVM + Gunyah + AVF (WinHv analog) ---
add("Android.KVM", "libkvm",
    "KVM_GET_API_VERSION", "KVM_CREATE_VM", "KVM_GET_MSR_INDEX_LIST",
    "KVM_GET_MSR_FEATURE_INDEX_LIST", "KVM_CHECK_EXTENSION",
    "KVM_GET_VCPU_MMAP_SIZE", "KVM_GET_SUPPORTED_CPUID", "KVM_GET_EMULATED_CPUID",
    "KVM_CREATE_VCPU", "KVM_GET_DIRTY_LOG", "KVM_SET_NR_MMU_PAGES",
    "KVM_GET_NR_MMU_PAGES", "KVM_SET_USER_MEMORY_REGION",
    "KVM_SET_USER_MEMORY_REGION2", "KVM_SET_TSS_ADDR", "KVM_SET_IDENTITY_MAP_ADDR",
    "KVM_CREATE_IRQCHIP", "KVM_IRQ_LINE", "KVM_GET_IRQCHIP", "KVM_SET_IRQCHIP",
    "KVM_CREATE_PIT", "KVM_GET_PIT", "KVM_SET_PIT", "KVM_IRQ_LINE_STATUS",
    "KVM_REGISTER_COALESCED_MMIO", "KVM_UNREGISTER_COALESCED_MMIO",
    "KVM_SET_GSI_ROUTING", "KVM_REINJECT_CONTROL", "KVM_IRQFD", "KVM_CREATE_PIT2",
    "KVM_SET_BOOT_CPU_ID", "KVM_IOEVENTFD", "KVM_XEN_HVM_CONFIG",
    "KVM_SET_CLOCK", "KVM_GET_CLOCK", "KVM_GET_PIT2", "KVM_SET_PIT2",
    "KVM_SET_TSC_KHZ", "KVM_GET_TSC_KHZ", "KVM_SIGNAL_MSI",
    "KVM_ARM_SET_DEVICE_ADDR", "KVM_ARM_MTE_COPY_TAGS",
    "KVM_ARM_SET_COUNTER_OFFSET", "KVM_ARM_GET_REG_WRITABLE_MASKS",
    "KVM_SET_PMU_EVENT_FILTER",
    "KVM_CREATE_DEVICE", "KVM_SET_DEVICE_ATTR", "KVM_GET_DEVICE_ATTR",
    "KVM_HAS_DEVICE_ATTR",
    "KVM_RUN", "KVM_GET_REGS", "KVM_SET_REGS", "KVM_GET_SREGS", "KVM_SET_SREGS",
    "KVM_TRANSLATE", "KVM_INTERRUPT", "KVM_GET_MSRS", "KVM_SET_MSRS",
    "KVM_SET_CPUID", "KVM_SET_SIGNAL_MASK", "KVM_GET_FPU", "KVM_SET_FPU",
    "KVM_GET_LAPIC", "KVM_SET_LAPIC", "KVM_SET_CPUID2", "KVM_GET_CPUID2",
    "KVM_TPR_ACCESS_REPORTING", "KVM_SET_VAPIC_ADDR",
    "KVM_GET_MP_STATE", "KVM_SET_MP_STATE", "KVM_NMI", "KVM_SET_GUEST_DEBUG",
    "KVM_X86_SETUP_MCE", "KVM_X86_GET_MCE_CAP_SUPPORTED", "KVM_X86_SET_MCE",
    "KVM_GET_VCPU_EVENTS", "KVM_SET_VCPU_EVENTS", "KVM_GET_DEBUGREGS",
    "KVM_SET_DEBUGREGS", "KVM_ENABLE_CAP", "KVM_GET_XSAVE", "KVM_SET_XSAVE",
    "KVM_GET_XCRS", "KVM_SET_XCRS", "KVM_DIRTY_TLB", "KVM_GET_ONE_REG",
    "KVM_SET_ONE_REG", "KVM_KVMCLOCK_CTRL", "KVM_ARM_VCPU_INIT",
    "KVM_ARM_PREFERRED_TARGET", "KVM_GET_REG_LIST",
    "KVM_SMI", "KVM_MEMORY_ENCRYPT_OP", "KVM_MEMORY_ENCRYPT_REG_REGION",
    "KVM_MEMORY_ENCRYPT_UNREG_REGION", "KVM_HYPERV_EVENTFD",
    "KVM_GET_NESTED_STATE", "KVM_SET_NESTED_STATE", "KVM_CLEAR_DIRTY_LOG",
    "KVM_GET_SUPPORTED_HV_CPUID", "KVM_ARM_VCPU_FINALIZE",
    "KVM_X86_SET_MSR_FILTER", "KVM_RESET_DIRTY_RINGS",
    "KVM_GET_SREGS2", "KVM_SET_SREGS2", "KVM_GET_STATS_FD", "KVM_GET_XSAVE2",
    "KVM_SET_MEMORY_ATTRIBUTES", "KVM_CREATE_GUEST_MEMFD", "KVM_PRE_FAULT_MEMORY",
    "KvmEmulateIo", "KvmEmulateMmio",
    "KVM_CAP_ARM_PROTECTED_VM", "KVM_VM_TYPE_ARM_PROTECTED", "KVM_CAP_ARM_EL2")

add("Android.KVM.Exit", "libkvm",
    "KVM_EXIT_UNKNOWN", "KVM_EXIT_EXCEPTION", "KVM_EXIT_IO", "KVM_EXIT_HYPERCALL",
    "KVM_EXIT_DEBUG", "KVM_EXIT_HLT", "KVM_EXIT_MMIO", "KVM_EXIT_IRQ_WINDOW_OPEN",
    "KVM_EXIT_SHUTDOWN", "KVM_EXIT_FAIL_ENTRY", "KVM_EXIT_INTR",
    "KVM_EXIT_SET_TPR", "KVM_EXIT_TPR_ACCESS",
    "KVM_EXIT_NMI", "KVM_EXIT_INTERNAL_ERROR",
    "KVM_EXIT_WATCHDOG", "KVM_EXIT_SYSTEM_EVENT", "KVM_EXIT_IOAPIC_EOI",
    "KVM_EXIT_HYPERV", "KVM_EXIT_ARM_NISV", "KVM_EXIT_X86_RDMSR",
    "KVM_EXIT_X86_WRMSR", "KVM_EXIT_DIRTY_RING_FULL", "KVM_EXIT_AP_RESET_HOLD",
    "KVM_EXIT_X86_BUS_LOCK", "KVM_EXIT_XEN",
    "KVM_EXIT_NOTIFY", "KVM_EXIT_MEMORY_FAULT", "KVM_EXIT_TDX", "KVM_EXIT_ARM_SEA",
    "KVM_EXIT_ARM_LDST64B",
    "KVM_EXIT_IO_IN", "KVM_EXIT_IO_OUT",
    "KVM_SYSTEM_EVENT_SHUTDOWN", "KVM_SYSTEM_EVENT_RESET",
    "KVM_SYSTEM_EVENT_CRASH", "KVM_SYSTEM_EVENT_WAKEUP",
    "KVM_SYSTEM_EVENT_SUSPEND")

add("Android.KVM.Cap", "libkvm",
    "KVM_CAP_IRQCHIP", "KVM_CAP_HLT", "KVM_CAP_MMU_SHADOW_CACHE_CONTROL",
    "KVM_CAP_USER_MEMORY", "KVM_CAP_SET_TSS_ADDR", "KVM_CAP_VAPIC",
    "KVM_CAP_EXT_CPUID", "KVM_CAP_CLOCKSOURCE", "KVM_CAP_NR_VCPUS",
    "KVM_CAP_NR_MEMSLOTS", "KVM_CAP_PIT", "KVM_CAP_NOP_IO_DELAY",
    "KVM_CAP_MP_STATE", "KVM_CAP_COALESCED_MMIO", "KVM_CAP_SYNC_MMU",
    "KVM_CAP_IOMMU", "KVM_CAP_USER_NMI", "KVM_CAP_SET_GUEST_DEBUG",
    "KVM_CAP_IRQ_ROUTING", "KVM_CAP_IRQ_INJECT_STATUS", "KVM_CAP_MCE",
    "KVM_CAP_IRQFD", "KVM_CAP_PIT2", "KVM_CAP_SET_BOOT_CPU_ID",
    "KVM_CAP_IOEVENTFD", "KVM_CAP_SET_IDENTITY_MAP_ADDR",
    "KVM_CAP_VCPU_EVENTS", "KVM_CAP_HYPERV", "KVM_CAP_XSAVE", "KVM_CAP_XCRS",
    "KVM_CAP_ASYNC_PF", "KVM_CAP_TSC_CONTROL", "KVM_CAP_GET_TSC_KHZ",
    "KVM_CAP_MAX_VCPUS", "KVM_CAP_ONE_REG", "KVM_CAP_TSC_DEADLINE_TIMER",
    "KVM_CAP_SYNC_REGS", "KVM_CAP_SIGNAL_MSI", "KVM_CAP_READONLY_MEM",
    "KVM_CAP_IRQFD_RESAMPLE", "KVM_CAP_DEVICE_CTRL", "KVM_CAP_ARM_PSCI",
    "KVM_CAP_ARM_SET_DEVICE_ADDR", "KVM_CAP_ARM_PSCI_0_2",
    "KVM_CAP_CHECK_EXTENSION_VM", "KVM_CAP_ARM_PMU_V3", "KVM_CAP_VCPU_ATTRIBUTES",
    "KVM_CAP_MAX_VCPU_ID", "KVM_CAP_IMMEDIATE_EXIT", "KVM_CAP_NESTED_STATE",
    "KVM_CAP_ARM_VM_IPA_SIZE", "KVM_CAP_ARM_SVE",
    "KVM_CAP_ARM_PTRAUTH_ADDRESS", "KVM_CAP_ARM_PTRAUTH_GENERIC",
    "KVM_CAP_ARM_NISV_TO_USER", "KVM_CAP_HALT_POLL",
    "KVM_CAP_DIRTY_LOG_RING", "KVM_CAP_SREGS2", "KVM_CAP_BINARY_STATS_FD",
    "KVM_CAP_ARM_MTE", "KVM_CAP_VM_GPA_BITS", "KVM_CAP_XSAVE2",
    "KVM_CAP_ARM_SYSTEM_SUSPEND", "KVM_CAP_USER_MEMORY2",
    "KVM_CAP_MEMORY_FAULT_INFO", "KVM_CAP_MEMORY_ATTRIBUTES",
    "KVM_CAP_GUEST_MEMFD", "KVM_CAP_VM_TYPES", "KVM_CAP_PRE_FAULT_MEMORY",
    "KVM_CAP_ARM_EL2", "KVM_CAP_GUEST_MEMFD_FLAGS", "KVM_CAP_ARM_SEA_TO_USER",
    "KVM_CAP_ARM_PROTECTED_VM")

add("Android.KVM.Device", "libkvm",
    "KVM_DEV_TYPE_VFIO", "KVM_DEV_TYPE_ARM_VGIC_V2", "KVM_DEV_TYPE_ARM_VGIC_V3",
    "KVM_DEV_TYPE_ARM_VGIC_ITS", "KVM_DEV_TYPE_ARM_VGIC_V5",
    "KVM_DEV_TYPE_ARM_PV_TIME", "KVM_DEV_VFIO_FILE_ADD", "KVM_DEV_VFIO_FILE_DEL")

add("Android.pKVM", "libpkvm",
    "PKVM_CREATE_VM", "PKVM_CREATE_VCPU", "PKVM_RUN", "PKVM_MAP_MEM",
    "PKVM_UNMAP_MEM", "PKVM_SHARE_MEM", "PKVM_UNSHARE_MEM", "PKVM_RELINQUISH_MEM",
    "PKVM_INSTALL_IHANDLER", "PKVM_TEARDOWN_VM", "PKVM_INIT",
    "PKVM_HOST_SHARE_GUEST", "PKVM_HOST_UNSHARE_GUEST",
    "PKVM_HOST_RELINQUISH_TO_EL1", "KVM_CAP_ARM_PROTECTED_VM",
    "kvm_call_hyp", "kvm_call_hyp_nvhe", "pkvm_init_host_vm",
    "pkvm_create_hyp_vm", "pkvm_destroy_hyp_vm", "pkvm_create_hyp_vcpu")

add("Android.Gunyah", "libgunyah",
    "GUNYAH_CREATE_VM", "GUNYAH_CREATE_VCPU", "GUNYAH_RUN", "GH_CREATE_VM",
    "GH_VM_SET_USER_MEM_REGION", "GH_VM_REMOVE_USER_MEM_REGION",
    "GH_VM_SET_DTB_CONFIG", "GH_VM_START", "GH_VCPU_RUN", "GH_VCPU_MMAP_SIZE",
    "GH_VM_ADD_FUNCTION", "GH_VM_REMOVE_FUNCTION", "GH_IOEVENTFD", "GH_IRQFD",
    "gunyah_vm_create", "gunyah_vcpu_create", "gunyah_vm_start",
    "gunyah_rm_vm_alloc_vmid", "gunyah_rm_vm_start", "gunyah_rm_mem_qcom_lookup_s2")

add("Android.AVF", "libvirtualizationservice",
    "VirtualizationService", "IVirtualMachine_create", "IVirtualMachine_start",
    "IVirtualMachine_stop", "IVirtualMachine_getCid", "IVirtualMachine_connectVsock",
    "IVirtualMachineCallback_onPayloadStarted", "IVirtualMachineCallback_onError",
    "IVirtualMachineCallback_onDied", "AVF_CREATE_VM", "AVF_CREATE_VCPU", "AVF_RUN",
    "AVF_MAP_MEMORY", "AVF_CONNECT_VSOCK", "crosvm", "protected_vm",
    "android.system.virtualizationservice", "android.system.virtualmachine",
    "VirtualMachineConfig", "VirtualMachineConfig_builder",
    "VirtualMachine_create", "VirtualMachine_run", "VirtualMachine_stop",
    "VmPayload_main", "AVmPayload_notifyPayloadReady",
    "AVmPayload_getVmInstanceSecret", "AVmPayload_getDiceAttestationChain")

add("Android.VFIO", "libvfio",
    "VFIO_GET_API_VERSION", "VFIO_CHECK_EXTENSION", "VFIO_SET_IOMMU",
    "VFIO_GROUP_GET_STATUS", "VFIO_GROUP_SET_CONTAINER",
    "VFIO_GROUP_UNSET_CONTAINER", "VFIO_GROUP_GET_DEVICE_FD",
    "VFIO_DEVICE_GET_INFO", "VFIO_DEVICE_GET_REGION_INFO",
    "VFIO_DEVICE_GET_IRQ_INFO", "VFIO_DEVICE_SET_IRQS", "VFIO_DEVICE_RESET",
    "VFIO_DEVICE_GET_PCI_HOT_RESET_INFO", "VFIO_DEVICE_PCI_HOT_RESET",
    "VFIO_DEVICE_QUERY_GFX_PLANE", "VFIO_DEVICE_GET_GFX_DMABUF",
    "VFIO_DEVICE_IOEVENTFD", "VFIO_DEVICE_FEATURE",
    "VFIO_IOMMU_GET_INFO", "VFIO_IOMMU_MAP_DMA", "VFIO_IOMMU_UNMAP_DMA",
    "VFIO_IOMMU_ENABLE", "VFIO_IOMMU_DISABLE", "VFIO_IOMMU_DIRTY_PAGES",
    "VFIO_DEVICE_BIND_IOMMUFD", "VFIO_DEVICE_UNBIND_IOMMUFD",
    "VFIO_DEVICE_ATTACH_IOMMUFD_PT", "VFIO_DEVICE_DETACH_IOMMUFD_PT")

add("Android.Vhost", "libvhost",
    "VHOST_GET_FEATURES", "VHOST_SET_FEATURES", "VHOST_SET_OWNER",
    "VHOST_RESET_OWNER", "VHOST_SET_MEM_TABLE", "VHOST_SET_LOG_BASE",
    "VHOST_SET_LOG_FD", "VHOST_SET_VRING_NUM", "VHOST_SET_VRING_ADDR",
    "VHOST_SET_VRING_BASE", "VHOST_GET_VRING_BASE", "VHOST_SET_VRING_KICK",
    "VHOST_SET_VRING_CALL", "VHOST_SET_VRING_ERR", "VHOST_SET_VRING_ENDIAN",
    "VHOST_GET_VRING_ENDIAN", "VHOST_SET_VRING_BUSYLOOP_TIMEOUT",
    "VHOST_GET_VRING_BUSYLOOP_TIMEOUT", "VHOST_NET_SET_BACKEND",
    "VHOST_SCSI_SET_ENDPOINT", "VHOST_SCSI_CLEAR_ENDPOINT",
    "VHOST_SCSI_GET_ABI_VERSION", "VHOST_VSOCK_SET_GUEST_CID",
    "VHOST_VSOCK_SET_RUNNING", "VHOST_VDPA_GET_DEVICE_ID",
    "VHOST_VDPA_GET_STATUS", "VHOST_VDPA_SET_STATUS", "VHOST_VDPA_GET_CONFIG",
    "VHOST_VDPA_SET_CONFIG", "VHOST_VDPA_SET_VRING_ENABLE",
    "VHOST_SET_BACKEND_FEATURES", "VHOST_GET_BACKEND_FEATURES",
    "VHOST_SET_INFLIGHT_FD", "VHOST_GET_INFLIGHT_FD")

add("Android.MSHV", "libmshv",
    "MSHV_CREATE_PARTITION", "MSHV_CREATE_VP", "MSHV_RUN_VP",
    "MSHV_GET_VP_REGISTERS", "MSHV_SET_VP_REGISTERS",
    "MSHV_MAP_GUEST_MEMORY", "MSHV_UNMAP_GUEST_MEMORY",
    "MSHV_GET_PARTITION_PROPERTY", "MSHV_SET_PARTITION_PROPERTY",
    "MSHV_IRQFD", "MSHV_IOEVENTFD", "MSHV_INSTALL_INTERCEPT",
    "MSHV_ASSERT_INTERRUPT", "MSHV_GET_GPA_ACCESS_STATE",
    "MSHV_VP_TRANSLATE_GVA", "MSHV_CREATE_PARTITION_V2",
    "MSHV_VP_REGISTER_INTERCEPT_RESULT", "MSHV_GET_VP_STATE",
    "MSHV_SET_VP_STATE", "MSHV_CREATE_DEVICE", "MSHV_SET_DEVICE_MMIO",
    "MSHV_MAP_GPA_PAGES", "MSHV_UNMAP_GPA_PAGES",
    "MSHV_GET_VP_CPUID_VALUES", "MSHV_ROOT_HVCALL",
    "MSHV_GET_GPAP_ACCESS_BITMAP", "MSHV_MODIFY_GPA_HOST_ACCESS")

add("Android.Virtio", "libvirtio",
    "VIRTIO_MMIO_MAGIC", "VIRTIO_MMIO_VERSION", "VIRTIO_MMIO_DEVICE_ID",
    "VIRTIO_MMIO_VENDOR_ID", "VIRTIO_MMIO_DEVICE_FEATURES",
    "VIRTIO_MMIO_DRIVER_FEATURES", "VIRTIO_MMIO_QUEUE_SEL",
    "VIRTIO_MMIO_QUEUE_NUM_MAX", "VIRTIO_MMIO_QUEUE_NUM",
    "VIRTIO_MMIO_QUEUE_READY", "VIRTIO_MMIO_QUEUE_NOTIFY",
    "VIRTIO_MMIO_INTERRUPT_STATUS", "VIRTIO_MMIO_INTERRUPT_ACK",
    "VIRTIO_MMIO_STATUS", "VIRTIO_CONFIG_S_ACKNOWLEDGE",
    "VIRTIO_CONFIG_S_DRIVER", "VIRTIO_CONFIG_S_DRIVER_OK",
    "VIRTIO_CONFIG_S_FEATURES_OK", "VIRTIO_F_VERSION_1",
    "VIRTIO_NET_F_CSUM", "VIRTIO_NET_F_MAC", "VIRTIO_BLK_F_SIZE_MAX",
    "VIRTIO_VSOCK_F_STREAM")

# --- HAL / JNI / services ---
add("Android.HAL", "libhardware",
    "hw_get_module", "hw_get_module_by_class", "hw_device_close",
    "gralloc_open", "gralloc_close", "gralloc_alloc", "gralloc_free",
    "gralloc_lock", "gralloc_unlock", "hwc_open_1", "hwc_prepare", "hwc_set",
    "hwc_eventControl", "hwc_blank", "hwc_getDisplayConfigs",
    "camera_open_legacy", "audio_hw_device_open", "audio_hw_device_close",
    "sensors_open", "sensors_close", "lights_open", "vibrator_open",
    "fingerprint_open", "gatekeeper_open", "keymaster_open",
    "hw_module_t", "hw_device_t")

add("Android.JNI", "libnativehelper",
    "JNI_OnLoad", "JNI_OnUnload", "JNI_GetDefaultJavaVMInitArgs",
    "JNI_CreateJavaVM", "JNI_GetCreatedJavaVMs", "GetEnv", "AttachCurrentThread",
    "DetachCurrentThread", "FindClass", "GetMethodID", "GetStaticMethodID",
    "CallObjectMethod", "CallVoidMethod", "NewStringUTF", "GetStringUTFChars",
    "ReleaseStringUTFChars", "RegisterNatives", "GetJavaVM",
    "ANativeWindow_fromSurface", "ANativeWindow_release",
    "ANativeActivity_onCreate")

add("Android.ART", "libart",
    "art::Runtime::Create", "art::Runtime::Start", "art::Thread::Attach",
    "art::JavaVMExt::AddGlobalRef", "ArtMethod", "ArtField", "JniIdType",
    "Dbg::SuspendVM", "Dbg::ResumeVM", "heap::CollectGarbage")

add("Android.Service", "libandroid_runtime",
    "ActivityManager", "ActivityTaskManager", "PackageManager", "WindowManager",
    "PowerManager", "NotificationManager", "LocationManager", "WifiManager",
    "ConnectivityManager", "AudioManager", "CameraManager", "InputManager",
    "DisplayManager", "UserManager", "DevicePolicyManager", "AppOpsManager",
    "BatteryManager", "UsbManager", "BluetoothManager", "NfcManager",
    "TelephonyManager", "ClipboardManager", "AlarmManager", "JobScheduler",
    "UsageStatsManager", "StorageManager", "MountService", "Vold",
    "Netd", "Installd", "Zygote", "SystemServer", "SurfaceFlinger",
    "AudioFlinger", "CameraService", "MediaServer", "Keystore", "Gatekeeper",
    "servicemanager", "hwservicemanager", "vndservicemanager")

add("Android.Adb", "adbd",
    "adb", "AdbDevices", "AdbShell", "AdbExec", "AdbPush", "AdbPull",
    "AdbInstall", "AdbUninstall", "AdbForward", "AdbReverse", "AdbRoot",
    "AdbUnroot", "AdbRemount", "AdbReboot", "AdbWaitForDevice")

add("Android.Sysctl", "linux",
    "sysctl", "_sysctl", "proc_dointvec", "proc_douintvec", "proc_dointvec_minmax",
    "proc_dostring", "register_sysctl", "register_sysctl_table",
    "unregister_sysctl_table",
    "kernel.pid_max", "kernel.threads-max", "kernel.randomize_va_space",
    "kernel.kptr_restrict", "kernel.dmesg_restrict", "kernel.modules_disabled",
    "kernel.unprivileged_bpf_disabled", "kernel.yama.ptrace_scope",
    "kernel.perf_event_paranoid",
    "vm.overcommit_memory", "vm.max_map_count", "vm.swappiness",
    "net.core.somaxconn", "net.ipv4.ip_forward",
    "fs.file-max", "fs.suid_dumpable", "fs.protected_hardlinks",
    "fs.protected_symlinks", "user.max_user_namespaces", "user.max_pid_namespaces")

add("Android.Ptrace", "linux",
    "PTRACE_TRACEME", "PTRACE_PEEKTEXT", "PTRACE_PEEKDATA", "PTRACE_PEEKUSER",
    "PTRACE_POKETEXT", "PTRACE_POKEDATA", "PTRACE_POKEUSER", "PTRACE_CONT",
    "PTRACE_KILL", "PTRACE_SINGLESTEP", "PTRACE_GETREGS", "PTRACE_SETREGS",
    "PTRACE_GETFPREGS", "PTRACE_SETFPREGS", "PTRACE_ATTACH", "PTRACE_DETACH",
    "PTRACE_SYSCALL", "PTRACE_SETOPTIONS", "PTRACE_GETEVENTMSG",
    "PTRACE_GETSIGINFO", "PTRACE_SETSIGINFO", "PTRACE_GETREGSET",
    "PTRACE_SETREGSET", "PTRACE_SEIZE", "PTRACE_INTERRUPT", "PTRACE_LISTEN",
    "PTRACE_O_TRACESYSGOOD", "PTRACE_O_TRACEFORK", "PTRACE_O_TRACECLONE",
    "PTRACE_O_TRACEEXEC", "PTRACE_O_TRACEEXIT", "PTRACE_O_TRACESECCOMP",
    "PTRACE_EVENT_FORK", "PTRACE_EVENT_CLONE", "PTRACE_EVENT_EXEC",
    "PTRACE_EVENT_EXIT", "PTRACE_EVENT_SECCOMP", "PTRACE_EVENT_STOP")

add("Android.Prctl", "linux",
    "PR_SET_PDEATHSIG", "PR_GET_PDEATHSIG", "PR_GET_DUMPABLE", "PR_SET_DUMPABLE",
    "PR_SET_NAME", "PR_GET_NAME", "PR_GET_SECCOMP", "PR_SET_SECCOMP",
    "PR_CAPBSET_READ", "PR_CAPBSET_DROP", "PR_GET_SECUREBITS", "PR_SET_SECUREBITS",
    "PR_SET_PTRACER", "PR_SET_NO_NEW_PRIVS", "PR_GET_NO_NEW_PRIVS",
    "PR_SET_THP_DISABLE", "PR_GET_THP_DISABLE", "PR_SET_VMA",
    "PR_SET_TAGGED_ADDR_CTRL", "PR_GET_TAGGED_ADDR_CTRL",
    "PR_SET_SYSCALL_USER_DISPATCH", "PR_SET_MDWE", "PR_GET_MDWE",
    "PR_GET_AUXV", "PR_SET_MEMORY_MERGE", "PR_GET_MEMORY_MERGE")

add("Android.Netlink", "libnl",
    "nl_socket_alloc", "nl_socket_free", "nl_connect", "nl_send_auto",
    "nl_recvmsgs_default", "nl_socket_add_membership",
    "NETLINK_ROUTE", "NETLINK_USERSOCK", "NETLINK_FIREWALL",
    "NETLINK_SOCK_DIAG", "NETLINK_NFLOG", "NETLINK_XFRM", "NETLINK_SELINUX",
    "NETLINK_AUDIT", "NETLINK_NETFILTER", "NETLINK_KOBJECT_UEVENT",
    "NETLINK_GENERIC", "NETLINK_CRYPTO", "NETLINK_SMC",
    "RTM_NEWLINK", "RTM_DELLINK", "RTM_GETLINK", "RTM_SETLINK",
    "RTM_NEWADDR", "RTM_DELADDR", "RTM_GETADDR", "RTM_NEWROUTE",
    "RTM_DELROUTE", "RTM_GETROUTE", "NLMSG_NOOP", "NLMSG_ERROR",
    "NLMSG_DONE", "NLM_F_REQUEST", "NLM_F_MULTI", "NLM_F_ACK", "NLM_F_DUMP")

add("Android.Tun", "linux",
    "TUNSETIFF", "TUNGETIFF", "TUNSETPERSIST", "TUNSETOWNER", "TUNSETGROUP",
    "TUNSETLINK", "TUNSETDEBUG", "TUNSETOFFLOAD", "TUNSETTXFILTER",
    "TUNGETSNDBUF", "TUNSETSNDBUF", "TUNSETQUEUE", "TUNSETIFINDEX",
    "TUNSETSTEERINGEBPF", "TUNSETFILTEREBPF", "TUNSETCARRIER",
    "IFF_TUN", "IFF_TAP", "IFF_NO_PI", "IFF_VNET_HDR", "IFF_MULTI_QUEUE")

add("Android.Vsock", "linux",
    "AF_VSOCK", "VMADDR_CID_ANY", "VMADDR_CID_HYPERVISOR", "VMADDR_CID_LOCAL",
    "VMADDR_CID_HOST", "VMADDR_PORT_ANY", "IOCTL_VM_SOCKETS_GET_LOCAL_CID",
    "SO_VM_SOCKETS_BUFFER_SIZE", "SO_VM_SOCKETS_BUFFER_MIN_SIZE",
    "SO_VM_SOCKETS_BUFFER_MAX_SIZE", "SO_VM_SOCKETS_CONNECT_TIMEOUT")

add("Android.Userfaultfd", "linux",
    "UFFDIO_API", "UFFDIO_REGISTER", "UFFDIO_UNREGISTER", "UFFDIO_WAKE",
    "UFFDIO_COPY", "UFFDIO_ZEROPAGE", "UFFDIO_WRITEPROTECT", "UFFDIO_CONTINUE",
    "UFFDIO_POISON", "UFFDIO_MOVE", "UFFD_EVENT_PAGEFAULT", "UFFD_EVENT_FORK",
    "UFFD_EVENT_REMAP", "UFFD_EVENT_REMOVE", "UFFD_EVENT_UNMAP")

add("Android.IoUring", "linux",
    "IORING_OP_NOP", "IORING_OP_READV", "IORING_OP_WRITEV", "IORING_OP_FSYNC",
    "IORING_OP_POLL_ADD", "IORING_OP_TIMEOUT", "IORING_OP_ACCEPT",
    "IORING_OP_CONNECT", "IORING_OP_OPENAT", "IORING_OP_CLOSE",
    "IORING_OP_STATX", "IORING_OP_READ", "IORING_OP_WRITE", "IORING_OP_SEND",
    "IORING_OP_RECV", "IORING_OP_OPENAT2", "IORING_OP_EPOLL_CTL",
    "IORING_OP_SPLICE", "IORING_OP_SHUTDOWN", "IORING_OP_RENAMEAT",
    "IORING_OP_UNLINKAT", "IORING_OP_MKDIRAT", "IORING_OP_SOCKET",
    "IORING_SETUP_IOPOLL", "IORING_SETUP_SQPOLL", "IORING_SETUP_SQ_AFF",
    "IORING_SETUP_CQSIZE", "IORING_REGISTER_BUFFERS", "IORING_UNREGISTER_BUFFERS")

add("Android.NDK", "libandroid",
    "ALooper_prepare", "ALooper_pollOnce", "ALooper_pollAll", "ALooper_wake",
    "ALooper_addFd", "ALooper_removeFd",
    "AAssetManager_fromJava", "AAssetManager_open", "AAssetManager_openDir",
    "AAsset_read", "AAsset_seek", "AAsset_close", "AAsset_getLength",
    "ANativeWindow_fromSurface", "ANativeWindow_acquire", "ANativeWindow_release",
    "ANativeWindow_setBuffersGeometry", "ANativeWindow_lock",
    "ANativeWindow_unlockAndPost",
    "AHardwareBuffer_allocate", "AHardwareBuffer_acquire",
    "AHardwareBuffer_release", "AHardwareBuffer_describe",
    "AHardwareBuffer_lock", "AHardwareBuffer_unlock",
    "AChoreographer_getInstance", "AChoreographer_postFrameCallback",
    "AInputQueue_getEvent", "AInputQueue_preDispatchEvent",
    "AInputQueue_finishEvent", "AConfiguration_new",
    "AConfiguration_fromAssetManager")

add("Android.HIDL", "libhidlbase",
    "hwbinder", "IHwBinder", "IHwBinder_transact", "hwservicemanager",
    "defaultPassthroughServiceImplementation", "registerAsService",
    "getService", "tryGetService", "HIDL_FETCH", "Return", "Void",
    "android.hidl.base.V1_0.IBase", "android.hidl.manager.V1_0.IServiceManager")

add("Android.AIDL", "libbinder_ndk",
    "AIDL_CALL", "AIBinder_Class_define", "AIBinder_new", "AIBinder_incStrong",
    "AIBinder_decStrong", "AIBinder_isAlive", "AIBinder_ping",
    "AIBinder_linkToDeath", "AIBinder_unlinkToDeath", "AIBinder_associateClass",
    "AIBinder_prepareTransaction", "AIBinder_transact",
    "AParcel_writeStrongBinder", "AParcel_readStrongBinder",
    "AParcel_writeParcelFileDescriptor", "AParcel_readParcelFileDescriptor",
    "AStatus_isOk", "AStatus_getStatus", "AStatus_getExceptionCode")

add("Android.Hwbinder", "libhwbinder",
    "IPCThreadState_self", "ProcessState_self", "ProcessState_startThreadPool",
    "ProcessState_becomeContextManager", "BnHwBinder", "BpHwBinder",
    "hwbinder_open", "hwbinder_mmap", "hwbinder_ioctl")

add("Android.Keystore", "libkeystore",
    "KeyStore", "android.security.keystore", "KeyStore_get", "KeyStore_put",
    "KeyStore_del", "KeyStore_exist", "KeyStore_generate", "KeyStore_import",
    "KeyStore_sign", "KeyStore_verify", "AKeystore_open", "AKeystore_close",
    "AKeyMintDevice_create", "AKeyMintDevice_generateKey",
    "AKeyMintDevice_importKey", "AKeyMintOperation_update",
    "AKeyMintOperation_finish")

add("Android.Trusty", "libtrusty",
    "tipc_connect", "tipc_send", "tipc_recv", "tipc_close",
    "trusty_std_write", "trusty_std_read", "trusty_ipc_connect",
    "trusty_ipc_send", "trusty_ipc_recv", "TrustyGatekeeper",
    "TrustyKeymaster", "TrustyWidevine", "secure_vm")


def dedupe(rows):
    seen = set()
    out = []
    for ns, lib, name in rows:
        key = (ns, lib, name)
        if key in seen:
            continue
        seen.add(key)
        out.append(key)
    return out


def main():
    rows = dedupe(ROWS)
    root = Path(__file__).resolve().parents[1]
    dest = root / "src" / "catalog.cc"
    dest.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        '#include "droid/catalog.h"',
        "",
        "// Curated AOSP Bionic / Binder / kernel / hypervisor slice.",
        "// Names follow bionic, libbinder, uapi/linux/android/binder.h,",
        "// kvm.h, pKVM, Gunyah, and AVF — not a dump of android.jar.",
        "static const WasmDroidApi kApis[] = {",
    ]
    for ns, lib, name in rows:
        ns_s = ns.replace("\\", "\\\\").replace('"', '\\"')
        lib_s = lib.replace("\\", "\\\\").replace('"', '\\"')
        name_s = name.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'    {{"{ns_s}", "{lib_s}", "{name_s}"}},')
    lines.append("};")
    lines.append("")
    lines.append("const WasmDroidApi* wasmdroid_catalog(int* count) {")
    lines.append("  if (count) *count = (int)(sizeof(kApis) / sizeof(kApis[0]));")
    lines.append("  return kApis;")
    lines.append("}")
    lines.append("")
    dest.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {dest} ({len(rows)} apis)")


if __name__ == "__main__":
    main()
