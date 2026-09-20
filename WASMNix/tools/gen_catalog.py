#!/usr/bin/env python3
"""Emit src/catalog.cc — curated Linux/WSL/Nix rows, not a dump."""

from pathlib import Path

ROWS = []


def add(ns, lib, *names):
    for n in names:
        ROWS.append((ns, lib, n))


# --- POSIX / libc wrappers (man 2 + man 3) ---
add("Linux.POSIX.unistd", "libc",
    "getpid", "getppid", "getuid", "geteuid", "getgid", "getegid",
    "gettid", "getpgid", "setpgid", "getpgrp", "setpgrp", "getsid", "setsid",
    "getgroups", "setgroups", "setuid", "seteuid", "setgid", "setegid",
    "setreuid", "setregid", "setresuid", "setresgid",
    "getresuid", "getresgid", "getlogin", "getlogin_r", "ttyname", "ttyname_r",
    "isatty", "tcgetpgrp", "tcsetpgrp", "gethostname", "sethostname",
    "getdomainname", "setdomainname", "nice", "daemon", "chdir", "fchdir",
    "getcwd", "getwd", "get_current_dir_name", "chroot", "fork", "vfork",
    "execve", "execveat", "execl", "execlp", "execle", "execv", "execvp",
    "execvpe", "fexecve", "_exit", "exit", "_Exit", "sleep", "usleep",
    "ualarm", "alarm", "pause", "pipe", "pipe2", "dup", "dup2", "dup3",
    "read", "write", "pread", "pwrite", "pread64", "pwrite64", "readv", "writev",
    "preadv", "pwritev", "preadv2", "pwritev2", "lseek", "lseek64", "close",
    "close_range", "fsync", "fdatasync", "sync", "syncfs", "truncate",
    "ftruncate", "ftruncate64", "truncate64", "link", "linkat", "unlink",
    "unlinkat", "rmdir", "symlink", "symlinkat", "readlink", "readlinkat",
    "access", "euidaccess", "faccessat", "faccessat2", "chown", "fchown",
    "lchown", "fchownat", "chmod", "fchmod", "fchmodat", "fchmodat2",
    "umask", "getdtablesize", "sysconf", "pathconf", "fpathconf", "confstr",
    "ctermid", "lockf", "crypt", "encrypt", "swab", "getpagesize",
    "getentropy", "getrandom", "copy_file_range", "sync_file_range",
    "posix_fadvise", "posix_fallocate", "fallocate", "readahead",
    "process_vm_readv", "process_vm_writev", "process_madvise",
    "process_mrelease")

add("Linux.POSIX.fcntl", "libc",
    "open", "openat", "openat2", "open_by_handle_at", "name_to_handle_at",
    "creat", "fcntl", "fcntl64", "flock", "posix_fadvise", "posix_fallocate")

add("Linux.POSIX.sys.stat", "libc",
    "stat", "lstat", "fstat", "fstatat", "newfstatat", "statx",
    "stat64", "lstat64", "fstat64", "fstatat64",
    "statfs", "fstatfs", "statfs64", "fstatfs64", "ustat",
    "mkdir", "mkdirat", "mknod", "mknodat", "mkfifo", "mkfifoat",
    "utime", "utimes", "lutimes", "futimes", "futimesat", "utimensat",
    "futimens", "umask")

add("Linux.POSIX.sys.mman", "libc",
    "mmap", "mmap64", "mmap2", "munmap", "mprotect", "msync", "madvise",
    "posix_madvise", "mlock", "mlock2", "munlock", "mlockall", "munlockall",
    "mincore", "mremap", "remap_file_pages", "memfd_create", "memfd_secret",
    "userfaultfd", "pkey_alloc", "pkey_free", "pkey_mprotect",
    "shm_open", "shm_unlink", "posix_typed_mem_open", "mseal",
    "map_shadow_stack", "process_madvise", "cachestat")

add("Linux.POSIX.stdlib", "libc",
    "malloc", "calloc", "realloc", "reallocarray", "free", "aligned_alloc",
    "posix_memalign", "valloc", "pvalloc", "memalign", "malloc_usable_size",
    "malloc_trim", "malloc_stats", "mallinfo", "mallinfo2", "mallopt",
    "abort", "atexit", "on_exit", "quick_exit", "at_quick_exit",
    "getenv", "secure_getenv", "setenv", "unsetenv", "putenv", "clearenv",
    "system", "realpath", "canonicalize_file_name", "mktemp", "mkstemp",
    "mkstemps", "mkdtemp", "mkostemp", "mkostemps", "tmpfile", "tmpnam",
    "tempnam", "abs", "labs", "llabs", "div", "ldiv", "lldiv",
    "atoi", "atol", "atoll", "atof", "strtol", "strtoul", "strtoll",
    "strtoull", "strtod", "strtof", "strtold", "strtoq", "strtouq",
    "rand", "srand", "rand_r", "random", "srandom", "initstate", "setstate",
    "drand48", "erand48", "lrand48", "nrand48", "mrand48", "jrand48",
    "srand48", "seed48", "lcong48", "qsort", "qsort_r", "bsearch",
    "ptsname", "ptsname_r", "grantpt", "unlockpt", "posix_openpt",
    "getloadavg", "getsubopt", "rpmatch")

add("Linux.POSIX.stdio", "libc",
    "fopen", "fdopen", "freopen", "fclose", "fcloseall", "fileno",
    "fread", "fwrite", "fgetc", "fputc", "getc", "putc", "getchar", "putchar",
    "ungetc", "fgets", "fputs", "puts", "getline", "getdelim",
    "fprintf", "printf", "sprintf", "snprintf", "dprintf",
    "vfprintf", "vprintf", "vsprintf", "vsnprintf", "vdprintf",
    "fscanf", "scanf", "sscanf", "vfscanf", "vscanf", "vsscanf",
    "fseek", "fseeko", "ftell", "ftello", "rewind", "fgetpos", "fsetpos",
    "feof", "ferror", "clearerr", "fflush", "setvbuf", "setbuf", "setbuffer",
    "setlinebuf", "perror", "remove", "rename", "renameat", "renameat2",
    "popen", "pclose", "flockfile", "funlockfile", "ftrylockfile")

add("Linux.POSIX.string", "libc",
    "memcpy", "memmove", "memset", "memcmp", "memchr", "memrchr", "memmem",
    "strlen", "strnlen", "strcpy", "strncpy", "stpcpy", "stpncpy",
    "strcat", "strncat", "strcmp", "strncmp", "strcasecmp", "strncasecmp",
    "strchr", "strrchr", "strchrnul", "strstr", "strcasestr", "strpbrk",
    "strspn", "strcspn", "strtok", "strtok_r", "strsep", "strdup", "strndup",
    "strerror", "strerror_r", "strsignal", "strcoll", "strxfrm",
    "ffs", "ffsl", "ffsll", "bzero", "bcopy", "bcmp", "index", "rindex",
    "explicit_bzero", "strlcpy", "strlcat", "memccpy", "rawmemchr")

add("Linux.POSIX.dirent", "libc",
    "opendir", "fdopendir", "readdir", "readdir_r", "readdir64",
    "closedir", "rewinddir", "seekdir", "telldir", "dirfd",
    "scandir", "scandirat", "alphasort", "versionsort",
    "getdents", "getdents64")

add("Linux.POSIX.time", "libc",
    "time", "stime", "gettimeofday", "settimeofday", "adjtime", "adjtimex",
    "clock_gettime", "clock_settime", "clock_getres", "clock_nanosleep",
    "clock_getcpuclockid", "clock_adjtime", "nanosleep",
    "localtime", "localtime_r", "gmtime", "gmtime_r", "mktime",
    "timelocal", "timegm", "strftime", "strptime", "asctime", "asctime_r",
    "ctime", "ctime_r", "tzset", "dysize", "timegm",
    "timer_create", "timer_delete", "timer_settime", "timer_gettime",
    "timer_getoverrun", "getitimer", "setitimer",
    "timerfd_create", "timerfd_settime", "timerfd_gettime",
    "timespec_get", "clock")

add("Linux.POSIX.signal", "libc",
    "signal", "sigaction", "sigprocmask", "sigpending", "sigsuspend",
    "sigwait", "sigwaitinfo", "sigtimedwait", "sigqueue", "kill", "killpg",
    "raise", "tgkill", "tkill", "sigemptyset", "sigfillset", "sigaddset",
    "sigdelset", "sigismember", "sigaltstack", "siginterrupt", "psignal",
    "psiginfo", "strsignal", "sysv_signal", "bsd_signal", "sigblock",
    "sigsetmask", "siggetmask", "sighold", "sigrelse", "sigignore",
    "sigpause", "sigreturn", "rt_sigaction", "rt_sigprocmask",
    "rt_sigpending", "rt_sigsuspend", "rt_sigtimedwait", "rt_sigqueueinfo",
    "rt_tgsigqueueinfo", "rt_sigreturn", "signalfd", "signalfd4",
    "pidfd_send_signal")

add("Linux.POSIX.sys.wait", "libc",
    "wait", "waitpid", "waitid", "wait3", "wait4")

add("Linux.POSIX.sys.resource", "libc",
    "getrlimit", "setrlimit", "prlimit", "prlimit64", "getrusage",
    "getpriority", "setpriority", "getloadavg")

add("Linux.POSIX.sys.socket", "libc",
    "socket", "socketpair", "bind", "listen", "accept", "accept4",
    "connect", "shutdown", "send", "sendto", "sendmsg", "sendmmsg",
    "recv", "recvfrom", "recvmsg", "recvmmsg", "getsockopt", "setsockopt",
    "getsockname", "getpeername", "sockatmark", "isfdtype", "socketcall")

add("Linux.POSIX.netdb", "libc",
    "getaddrinfo", "freeaddrinfo", "getnameinfo", "gai_strerror",
    "gethostbyname", "gethostbyname2", "gethostbyaddr", "gethostent",
    "sethostent", "endhostent", "getnetbyname", "getnetbyaddr",
    "getservbyname", "getservbyport", "getprotobyname", "getprotobynumber",
    "getservent", "getprotoent")

add("Linux.POSIX.arpa.inet", "libc",
    "inet_pton", "inet_ntop", "inet_aton", "inet_ntoa", "inet_addr",
    "inet_network", "inet_makeaddr", "inet_lnaof", "inet_netof",
    "htons", "htonl", "ntohs", "ntohl")

add("Linux.POSIX.poll", "libc",
    "poll", "ppoll", "select", "pselect", "pselect6")

add("Linux.POSIX.sys.epoll", "libc",
    "epoll_create", "epoll_create1", "epoll_ctl", "epoll_wait",
    "epoll_pwait", "epoll_pwait2")

add("Linux.POSIX.sys.eventfd", "libc",
    "eventfd", "eventfd2", "eventfd_read", "eventfd_write")

add("Linux.POSIX.pthread", "libpthread",
    "pthread_create", "pthread_join", "pthread_detach", "pthread_exit",
    "pthread_self", "pthread_equal", "pthread_tryjoin_np", "pthread_timedjoin_np",
    "pthread_attr_init", "pthread_attr_destroy", "pthread_attr_setdetachstate",
    "pthread_attr_getdetachstate", "pthread_attr_setschedparam",
    "pthread_attr_getschedparam", "pthread_attr_setschedpolicy",
    "pthread_attr_getschedpolicy", "pthread_attr_setinheritsched",
    "pthread_attr_getinheritsched", "pthread_attr_setscope",
    "pthread_attr_getscope", "pthread_attr_setstacksize",
    "pthread_attr_getstacksize", "pthread_attr_setstack",
    "pthread_attr_getstack", "pthread_attr_setguardsize",
    "pthread_attr_getguardsize", "pthread_attr_setaffinity_np",
    "pthread_attr_getaffinity_np", "pthread_getattr_np",
    "pthread_mutex_init", "pthread_mutex_destroy", "pthread_mutex_lock",
    "pthread_mutex_trylock", "pthread_mutex_timedlock", "pthread_mutex_unlock",
    "pthread_mutex_consistent", "pthread_mutexattr_init",
    "pthread_mutexattr_destroy", "pthread_mutexattr_settype",
    "pthread_mutexattr_gettype", "pthread_mutexattr_setpshared",
    "pthread_mutexattr_getpshared", "pthread_mutexattr_setrobust",
    "pthread_mutexattr_getrobust", "pthread_mutexattr_setprotocol",
    "pthread_mutexattr_getprotocol", "pthread_mutexattr_setprioceiling",
    "pthread_mutexattr_getprioceiling",
    "pthread_cond_init", "pthread_cond_destroy", "pthread_cond_wait",
    "pthread_cond_timedwait", "pthread_cond_signal", "pthread_cond_broadcast",
    "pthread_condattr_init", "pthread_condattr_destroy",
    "pthread_condattr_setclock", "pthread_condattr_getclock",
    "pthread_condattr_setpshared", "pthread_condattr_getpshared",
    "pthread_rwlock_init", "pthread_rwlock_destroy", "pthread_rwlock_rdlock",
    "pthread_rwlock_wrlock", "pthread_rwlock_tryrdlock",
    "pthread_rwlock_trywrlock", "pthread_rwlock_timedrdlock",
    "pthread_rwlock_timedwrlock", "pthread_rwlock_unlock",
    "pthread_rwlockattr_init", "pthread_rwlockattr_destroy",
    "pthread_rwlockattr_setpshared", "pthread_rwlockattr_getpshared",
    "pthread_rwlockattr_setkind_np", "pthread_rwlockattr_getkind_np",
    "pthread_barrier_init", "pthread_barrier_destroy", "pthread_barrier_wait",
    "pthread_barrierattr_init", "pthread_barrierattr_destroy",
    "pthread_barrierattr_setpshared", "pthread_barrierattr_getpshared",
    "pthread_spin_init", "pthread_spin_destroy", "pthread_spin_lock",
    "pthread_spin_trylock", "pthread_spin_unlock",
    "pthread_once", "pthread_key_create", "pthread_key_delete",
    "pthread_setspecific", "pthread_getspecific",
    "pthread_cancel", "pthread_setcancelstate", "pthread_setcanceltype",
    "pthread_testcancel", "pthread_cleanup_push", "pthread_cleanup_pop",
    "pthread_kill", "pthread_sigmask", "pthread_sigqueue",
    "pthread_setschedparam", "pthread_getschedparam", "pthread_setschedprio",
    "pthread_setaffinity_np", "pthread_getaffinity_np",
    "pthread_setname_np", "pthread_getname_np", "pthread_getattr_default_np",
    "pthread_setattr_default_np", "pthread_getcpuclockid",
    "pthread_atfork", "pthread_yield", "pthread_concurrency",
    "pthread_setconcurrency", "pthread_getconcurrency")

add("Linux.POSIX.dlfcn", "libdl",
    "dlopen", "dlclose", "dlsym", "dlvsym", "dlerror", "dladdr", "dladdr1",
    "dlinfo", "dlmopen", "dl_iterate_phdr")

add("Linux.POSIX.sys.ioctl", "libc", "ioctl")

add("Linux.POSIX.termios", "libc",
    "tcgetattr", "tcsetattr", "tcsendbreak", "tcdrain", "tcflush", "tcflow",
    "cfgetispeed", "cfgetospeed", "cfsetispeed", "cfsetospeed",
    "cfsetspeed", "cfmakeraw", "tcgetsid")

add("Linux.POSIX.pwd", "libc",
    "getpwuid", "getpwuid_r", "getpwnam", "getpwnam_r", "getpwent",
    "setpwent", "endpwent", "getpw", "fgetpwent", "putpwent")

add("Linux.POSIX.grp", "libc",
    "getgrgid", "getgrgid_r", "getgrnam", "getgrnam_r", "getgrent",
    "setgrent", "endgrent", "fgetgrent", "putgrent", "getgrouplist",
    "initgroups")

add("Linux.POSIX.syslog", "libc",
    "openlog", "syslog", "vsyslog", "closelog", "setlogmask")

add("Linux.POSIX.glob", "libc", "glob", "globfree", "glob_pattern_p")

add("Linux.POSIX.wordexp", "libc", "wordexp", "wordfree")

add("Linux.POSIX.fnmatch", "libc", "fnmatch")

add("Linux.POSIX.regex", "libc",
    "regcomp", "regexec", "regerror", "regfree")

add("Linux.POSIX.iconv", "libc", "iconv_open", "iconv", "iconv_close")

add("Linux.POSIX.locale", "libc",
    "setlocale", "localeconv", "newlocale", "duplocale", "uselocale",
    "freelocale", "nl_langinfo", "nl_langinfo_l", "strcoll_l", "strxfrm_l",
    "strcasecmp_l", "strncasecmp_l", "gettext", "dgettext", "dcgettext",
    "ngettext", "textdomain", "bindtextdomain", "bind_textdomain_codeset")

add("Linux.POSIX.sys.uio", "libc",
    "readv", "writev", "preadv", "pwritev", "preadv2", "pwritev2",
    "process_vm_readv", "process_vm_writev")

add("Linux.POSIX.sys.un", "libc", "socket", "bind")  # AF_UNIX lives on socket

add("Linux.POSIX.sys.shm", "libc",
    "shmget", "shmat", "shmdt", "shmctl")

add("Linux.POSIX.sys.msg", "libc",
    "msgget", "msgsnd", "msgrcv", "msgctl")

add("Linux.POSIX.sys.sem", "libc",
    "semget", "semop", "semtimedop", "semctl")

add("Linux.POSIX.semaphore", "librt",
    "sem_init", "sem_destroy", "sem_open", "sem_close", "sem_unlink",
    "sem_wait", "sem_trywait", "sem_timedwait", "sem_clockwait",
    "sem_post", "sem_getvalue")

add("Linux.POSIX.mqueue", "librt",
    "mq_open", "mq_close", "mq_unlink", "mq_send", "mq_receive",
    "mq_timedsend", "mq_timedreceive", "mq_getattr", "mq_setattr",
    "mq_notify", "mq_getsetattr")

add("Linux.POSIX.aio", "librt",
    "aio_read", "aio_write", "aio_fsync", "aio_error", "aio_return",
    "aio_suspend", "aio_cancel", "lio_listio", "aio_init")

add("Linux.POSIX.sys.statvfs", "libc", "statvfs", "fstatvfs")

add("Linux.POSIX.sys.mount", "libc",
    "mount", "umount", "umount2", "pivot_root", "swapon", "swapoff")

add("Linux.POSIX.sys.xattr", "libc",
    "getxattr", "lgetxattr", "fgetxattr", "setxattr", "lsetxattr", "fsetxattr",
    "listxattr", "llistxattr", "flistxattr", "removexattr", "lremovexattr",
    "fremovexattr", "setxattrat", "getxattrat", "listxattrat", "removexattrat")

add("Linux.POSIX.ifaddrs", "libc", "getifaddrs", "freeifaddrs")

add("Linux.POSIX.net.if", "libc",
    "if_nametoindex", "if_indextoname", "if_nameindex", "if_freenameindex")

add("Linux.POSIX.poll", "libc", "poll")  # already added; harmless dups filtered later

add("Linux.POSIX.spawn", "libc",
    "posix_spawn", "posix_spawnp", "posix_spawn_file_actions_init",
    "posix_spawn_file_actions_destroy", "posix_spawn_file_actions_addopen",
    "posix_spawn_file_actions_addclose", "posix_spawn_file_actions_adddup2",
    "posix_spawnattr_init", "posix_spawnattr_destroy",
    "posix_spawnattr_setflags", "posix_spawnattr_getflags",
    "posix_spawnattr_setpgroup", "posix_spawnattr_getpgroup",
    "posix_spawnattr_setsigmask", "posix_spawnattr_getsigmask",
    "posix_spawnattr_setsigdefault", "posix_spawnattr_getsigdefault",
    "posix_spawnattr_setschedparam", "posix_spawnattr_getschedparam",
    "posix_spawnattr_setschedpolicy", "posix_spawnattr_getschedpolicy")

add("Linux.POSIX.sys.select", "libc", "select", "pselect", "FD_SET", "FD_CLR",
    "FD_ISSET", "FD_ZERO")

add("Linux.POSIX.sys.times", "libc", "times")

add("Linux.POSIX.sys.utsname", "libc", "uname")

add("Linux.POSIX.sys.sysinfo", "libc", "sysinfo")

add("Linux.POSIX.sched", "libc",
    "sched_yield", "sched_setscheduler", "sched_getscheduler",
    "sched_setparam", "sched_getparam", "sched_get_priority_max",
    "sched_get_priority_min", "sched_rr_get_interval",
    "sched_setaffinity", "sched_getaffinity", "sched_setattr",
    "sched_getattr", "clone", "clone3", "unshare", "setns", "getcpu")

add("Linux.POSIX.sys.prctl", "libc", "prctl", "arch_prctl", "personality")

add("Linux.POSIX.sys.ptrace", "libc", "ptrace")

add("Linux.POSIX.sys.reboot", "libc", "reboot", "kexec_load", "kexec_file_load")

add("Linux.POSIX.sys.module", "libc",
    "init_module", "finit_module", "delete_module")

add("Linux.POSIX.sys.acct", "libc", "acct")

add("Linux.POSIX.sys.quota", "libc", "quotactl", "quotactl_fd")

add("Linux.POSIX.sys.sysfs", "libc", "sysfs", "syslog")

add("Linux.POSIX.sys.timex", "libc", "adjtimex", "ntp_adjtime", "ntp_gettime")

add("Linux.POSIX.sys.sendfile", "libc", "sendfile", "sendfile64")

add("Linux.POSIX.fcntl", "libc", "posix_fadvise", "sync_file_range")

add("Linux.GNU", "libc",
    "getopt", "getopt_long", "getopt_long_only", "error", "error_at_line",
    "backtrace", "backtrace_symbols", "backtrace_symbols_fd",
    "argz_create", "argz_create_sep", "argz_count", "argz_extract",
    "argz_stringify", "argz_add", "argz_add_sep", "argz_append",
    "argz_delete", "argz_insert", "argz_next", "argz_replace",
    "envz_entry", "envz_get", "envz_add", "envz_merge", "envz_remove",
    "envz_strip", "obstack_init", "obstack_alloc", "obstack_free",
    "asprintf", "vasprintf", "getauxval", "getcontext", "setcontext",
    "makecontext", "swapcontext", "get_nprocs", "get_nprocs_conf",
    "get_phys_pages", "get_avphys_pages", "gnu_get_libc_version",
    "gnu_get_libc_release", "program_invocation_name",
    "program_invocation_short_name", "error_print_progname",
    "register_printf_function", "parse_printf_format",
    "memfd_create", "renameat2", "statx", "copy_file_range",
    "getdents64", "syncfs", "close_range", "pidfd_open", "pidfd_getfd",
    "pidfd_send_signal", "openat2", "faccessat2", "execveat",
    "setns", "unshare", "clone3", "io_uring_setup")

add("Linux.GNU.libm", "libm",
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
    "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
    "exp", "exp2", "expm1", "log", "log2", "log10", "log1p",
    "pow", "sqrt", "cbrt", "hypot", "ceil", "floor", "trunc", "round",
    "rint", "nearbyint", "remainder", "remquo", "fmod", "modf",
    "frexp", "ldexp", "scalbn", "ilogb", "logb", "fabs", "fmin", "fmax",
    "fdim", "fma", "nan", "copysign", "nextafter", "nexttoward",
    "erf", "erfc", "tgamma", "lgamma", "j0", "j1", "jn", "y0", "y1", "yn",
    "sinf", "cosf", "tanf", "expf", "logf", "powf", "sqrtf",
    "sinl", "cosl", "expl", "logl", "powl", "sqrtl")

add("Linux.GNU.libresolv", "libresolv",
    "res_init", "res_query", "res_search", "res_send", "res_mkquery",
    "dn_comp", "dn_expand", "ns_initparse", "ns_parserr")

add("Linux.GNU.libcrypt", "libcrypt", "crypt", "crypt_r", "crypt_gensalt",
    "crypt_gensalt_rn")

add("Linux.GNU.libutil", "libutil",
    "openpty", "forkpty", "login", "logout", "logwtmp", "logutmp",
    "login_tty")

# --- Linux-specific syscalls (syscalls(2), skip arch-only / removed ports) ---
add("Linux.Syscall.fs", "linux",
    "fsopen", "fsconfig", "fsmount", "move_mount", "open_tree", "fspick",
    "mount_setattr", "statmount", "listmount", "file_getattr", "file_setattr",
    "openat2", "close_range", "copy_file_range", "statx", "name_to_handle_at",
    "open_by_handle_at", "syncfs", "sync_file_range", "sync_file_range2",
    "fallocate", "fadvise64", "fadvise64_64", "readahead",
    "inotify_init", "inotify_init1", "inotify_add_watch", "inotify_rm_watch",
    "fanotify_init", "fanotify_mark", "lookup_dcookie",
    "splice", "tee", "vmsplice", "sendfile", "sendfile64",
    "flistxattr", "listxattr", "getxattr", "setxattr", "removexattr",
    "renameat2", "mkdirat", "mknodat", "unlinkat", "linkat", "symlinkat",
    "readlinkat", "fchmodat", "fchownat", "utimensat", "futimesat",
    "faccessat", "faccessat2", "openat", "newfstatat", "fstatat64",
    "dup3", "pipe2", "preadv2", "pwritev2", "lseek", "llseek",
    "_llseek", "readdir", "getdents", "getdents64",
    "quotactl", "quotactl_fd", "ioprio_get", "ioprio_set")

add("Linux.Syscall.process", "linux",
    "clone", "clone2", "clone3", "fork", "vfork", "execve", "execveat",
    "exit", "exit_group", "wait4", "waitid", "waitpid",
    "ptrace", "prctl", "arch_prctl", "personality", "unshare", "setns",
    "pidfd_open", "pidfd_getfd", "pidfd_send_signal",
    "gettid", "set_tid_address", "set_thread_area", "get_thread_area",
    "getcpu", "sched_setattr", "sched_getattr", "sched_setaffinity",
    "sched_getaffinity", "kcmp", "rseq", "membarrier",
    "capget", "capset", "seccomp", "bpf",
    "landlock_create_ruleset", "landlock_add_rule", "landlock_restrict_self",
    "lsm_get_self_attr", "lsm_set_self_attr", "lsm_list_modules",
    "prlimit64", "get_robust_list", "set_robust_list",
    "futex", "futex_waitv", "futex_wait", "futex_wake", "futex_requeue",
    "set_mempolicy", "get_mempolicy", "mbind", "migrate_pages", "move_pages",
    "set_mempolicy_home_node", "restart_syscall")

add("Linux.Syscall.memory", "linux",
    "brk", "mmap", "mmap2", "munmap", "mprotect", "mremap", "madvise",
    "process_madvise", "process_mrelease", "mlock", "mlock2", "munlock",
    "mlockall", "munlockall", "mincore", "memfd_create", "memfd_secret",
    "userfaultfd", "pkey_alloc", "pkey_free", "pkey_mprotect",
    "remap_file_pages", "mseal", "map_shadow_stack", "cachestat",
    "process_vm_readv", "process_vm_writev")

add("Linux.Syscall.io_uring", "linux",
    "io_uring_setup", "io_uring_enter", "io_uring_register",
    "io_setup", "io_destroy", "io_submit", "io_cancel", "io_getevents",
    "io_pgetevents")

add("Linux.Syscall.key", "linux",
    "add_key", "request_key", "keyctl")

add("Linux.Syscall.perf", "linux", "perf_event_open")

add("Linux.Syscall.time", "linux",
    "clock_gettime", "clock_settime", "clock_getres", "clock_nanosleep",
    "clock_adjtime", "gettimeofday", "settimeofday", "adjtimex",
    "time", "stime", "nanosleep", "timer_create", "timer_delete",
    "timer_settime", "timer_gettime", "timer_getoverrun",
    "timerfd_create", "timerfd_settime", "timerfd_gettime",
    "getitimer", "setitimer", "times", "gettimeofday")

add("Linux.Syscall.net", "linux",
    "socket", "socketpair", "bind", "listen", "accept", "accept4",
    "connect", "shutdown", "sendto", "sendmsg", "sendmmsg",
    "recvfrom", "recvmsg", "recvmmsg", "getsockname", "getpeername",
    "getsockopt", "setsockopt", "socketcall")

add("Linux.Syscall.ipc", "linux",
    "ipc", "shmget", "shmat", "shmdt", "shmctl",
    "msgget", "msgsnd", "msgrcv", "msgctl",
    "semget", "semop", "semtimedop", "semctl",
    "mq_open", "mq_unlink", "mq_timedsend", "mq_timedreceive",
    "mq_notify", "mq_getsetattr")

add("Linux.Syscall.misc", "linux",
    "syscall", "ioctl", "fcntl", "fcntl64", "flock",
    "getrandom", "sysinfo", "uname", "olduname", "oldolduname",
    "syslog", "sysfs", "acct", "reboot", "kexec_load", "kexec_file_load",
    "init_module", "finit_module", "delete_module",
    "ioperm", "iopl", "modify_ldt", "create_module", "query_module",
    "get_kernel_syms", "nfsservctl", "_sysctl", "bdflush", "uselib",
    "vhangup", "idle", "sync", "umask", "getpid", "getppid",
    "getuid", "geteuid", "getgid", "getegid", "setuid", "setgid",
    "chdir", "fchdir", "chroot", "getcwd", "dup", "dup2", "pipe",
    "alarm", "pause", "access", "nice", "kill", "signal",
    "sgetmask", "ssetmask", "sigaction", "sigprocmask", "sigpending",
    "sigsuspend", "sigreturn", "rt_sigaction", "rt_sigprocmask",
    "select", "_newselect", "poll", "ppoll", "epoll_create",
    "epoll_create1", "epoll_ctl", "epoll_wait", "epoll_pwait", "epoll_pwait2",
    "eventfd", "eventfd2", "signalfd", "signalfd4",
    "inotify_init", "fanotify_init", "io_setup",
    "mincore", "madvise", "brk", "munmap", "mprotect",
    "swapon", "swapoff", "mount", "umount", "umount2", "pivot_root",
    "sethostname", "setdomainname", "getrlimit", "setrlimit", "ugetrlimit",
    "getrusage", "getpriority", "setpriority", "ioprio_set", "ioprio_get",
    "setfsgid", "setfsuid", "capget", "capset", "prctl",
    "getcwd", "lookup_dcookie", "epoll_create",
    "restart_syscall", "tgkill", "utimes", "fadvise64",
    "vserver", "unshare", "splice", "sync_file_range", "tee", "vmsplice",
    "move_pages", "utimensat", "epoll_pwait", "signalfd", "timerfd_create",
    "eventfd", "fallocate", "timerfd_settime", "timerfd_gettime",
    "signalfd4", "eventfd2", "epoll_create1", "dup3", "pipe2", "inotify_init1",
    "preadv", "pwritev", "rt_tgsigqueueinfo", "perf_event_open",
    "recvmmsg", "fanotify_init", "fanotify_mark", "prlimit64",
    "name_to_handle_at", "open_by_handle_at", "clock_adjtime", "syncfs",
    "sendmmsg", "setns", "getcpu", "process_vm_readv", "process_vm_writev",
    "kcmp", "finit_module", "sched_setattr", "sched_getattr", "renameat2",
    "seccomp", "getrandom", "memfd_create", "kexec_file_load", "bpf",
    "execveat", "userfaultfd", "membarrier", "mlock2", "copy_file_range",
    "preadv2", "pwritev2", "pkey_mprotect", "pkey_alloc", "pkey_free",
    "statx", "io_pgetevents", "rseq", "pidfd_send_signal", "io_uring_setup",
    "io_uring_enter", "io_uring_register", "open_tree", "move_mount",
    "fsopen", "fsconfig", "fsmount", "fspick", "pidfd_open", "clone3",
    "close_range", "openat2", "pidfd_getfd", "faccessat2", "process_madvise",
    "epoll_pwait2", "mount_setattr", "quotactl_fd", "landlock_create_ruleset",
    "landlock_add_rule", "landlock_restrict_self", "memfd_secret",
    "process_mrelease", "futex_waitv", "set_mempolicy_home_node",
    "cachestat", "fchmodat2", "map_shadow_stack", "futex_wake", "futex_wait",
    "futex_requeue", "statmount", "listmount", "lsm_get_self_attr",
    "lsm_set_self_attr", "lsm_list_modules", "mseal",
    "setxattrat", "getxattrat", "listxattrat", "removexattrat",
    "file_getattr", "file_setattr")

add("Linux.Syscall.capability", "libcap",
    "cap_get_proc", "cap_set_proc", "cap_get_pid", "cap_from_text",
    "cap_to_text", "cap_from_name", "cap_to_name", "cap_get_flag",
    "cap_set_flag", "cap_clear", "cap_clear_flag", "cap_compare",
    "cap_dup", "cap_free", "cap_init", "cap_copy_ext", "cap_copy_int",
    "cap_size", "cap_get_bound", "cap_drop_bound", "cap_get_ambient",
    "cap_set_ambient", "cap_reset_ambient", "cap_get_secbits",
    "cap_set_secbits", "cap_get_nsowner", "cap_set_nsowner",
    "cap_set_file", "cap_get_file", "cap_set_fd", "cap_get_fd")

add("Linux.Syscall.seccomp", "libseccomp",
    "seccomp_init", "seccomp_reset", "seccomp_load", "seccomp_release",
    "seccomp_rule_add", "seccomp_rule_add_array", "seccomp_rule_add_exact",
    "seccomp_export_pfc", "seccomp_export_bpf", "seccomp_attr_set",
    "seccomp_attr_get", "seccomp_arch_add", "seccomp_arch_remove",
    "seccomp_arch_exist", "seccomp_arch_native", "seccomp_syscall_resolve_name",
    "seccomp_syscall_resolve_num_arch", "seccomp_merge", "seccomp_notify_alloc",
    "seccomp_notify_receive", "seccomp_notify_respond", "seccomp_notify_free")

# --- Hypervisor hop: kvm.h / vfio.h / vhost.h / mshv.h ---
# Analog of WinHvPlatform + WinHvEmulation. Not a dump of kvm_host.c.
add("Linux.KVM", "libkvm",
    "KVM_GET_API_VERSION", "KVM_CREATE_VM", "KVM_GET_MSR_INDEX_LIST",
    "KVM_GET_MSR_FEATURE_INDEX_LIST", "KVM_CHECK_EXTENSION",
    "KVM_GET_VCPU_MMAP_SIZE", "KVM_GET_SUPPORTED_CPUID", "KVM_GET_EMULATED_CPUID",
    "KVM_S390_ENABLE_SIE",
    "KVM_CREATE_VCPU", "KVM_GET_DIRTY_LOG", "KVM_SET_NR_MMU_PAGES",
    "KVM_GET_NR_MMU_PAGES", "KVM_SET_USER_MEMORY_REGION",
    "KVM_SET_USER_MEMORY_REGION2", "KVM_SET_TSS_ADDR", "KVM_SET_IDENTITY_MAP_ADDR",
    "KVM_S390_UCAS_MAP", "KVM_S390_UCAS_UNMAP", "KVM_S390_VCPU_FAULT", "KVM_S390_KEYOP",
    "KVM_CREATE_IRQCHIP", "KVM_IRQ_LINE", "KVM_GET_IRQCHIP", "KVM_SET_IRQCHIP",
    "KVM_CREATE_PIT", "KVM_GET_PIT", "KVM_SET_PIT", "KVM_IRQ_LINE_STATUS",
    "KVM_REGISTER_COALESCED_MMIO", "KVM_UNREGISTER_COALESCED_MMIO",
    "KVM_SET_GSI_ROUTING", "KVM_REINJECT_CONTROL", "KVM_IRQFD", "KVM_CREATE_PIT2",
    "KVM_SET_BOOT_CPU_ID", "KVM_IOEVENTFD", "KVM_XEN_HVM_CONFIG",
    "KVM_SET_CLOCK", "KVM_GET_CLOCK", "KVM_GET_PIT2", "KVM_SET_PIT2",
    "KVM_PPC_GET_PVINFO", "KVM_SET_TSC_KHZ", "KVM_GET_TSC_KHZ", "KVM_SIGNAL_MSI",
    "KVM_PPC_GET_SMMU_INFO", "KVM_PPC_ALLOCATE_HTAB", "KVM_CREATE_SPAPR_TCE",
    "KVM_CREATE_SPAPR_TCE_64", "KVM_ALLOCATE_RMA", "KVM_PPC_GET_HTAB_FD",
    "KVM_ARM_SET_DEVICE_ADDR", "KVM_PPC_RTAS_DEFINE_TOKEN",
    "KVM_PPC_RESIZE_HPT_PREPARE", "KVM_PPC_RESIZE_HPT_COMMIT",
    "KVM_PPC_CONFIGURE_V3_MMU", "KVM_PPC_GET_RMMU_INFO", "KVM_PPC_GET_CPU_CHAR",
    "KVM_SET_PMU_EVENT_FILTER", "KVM_PPC_SVM_OFF", "KVM_ARM_MTE_COPY_TAGS",
    "KVM_ARM_SET_COUNTER_OFFSET", "KVM_ARM_GET_REG_WRITABLE_MASKS",
    "KVM_PPC_GET_COMPAT_CAPS",
    "KVM_CREATE_DEVICE", "KVM_SET_DEVICE_ATTR", "KVM_GET_DEVICE_ATTR",
    "KVM_HAS_DEVICE_ATTR",
    "KVM_RUN", "KVM_GET_REGS", "KVM_SET_REGS", "KVM_GET_SREGS", "KVM_SET_SREGS",
    "KVM_TRANSLATE", "KVM_INTERRUPT", "KVM_GET_MSRS", "KVM_SET_MSRS",
    "KVM_SET_CPUID", "KVM_SET_SIGNAL_MASK", "KVM_GET_FPU", "KVM_SET_FPU",
    "KVM_GET_LAPIC", "KVM_SET_LAPIC", "KVM_SET_CPUID2", "KVM_GET_CPUID2",
    "KVM_TPR_ACCESS_REPORTING", "KVM_SET_VAPIC_ADDR", "KVM_S390_INTERRUPT",
    "KVM_S390_STORE_STATUS", "KVM_S390_SET_INITIAL_PSW", "KVM_S390_INITIAL_RESET",
    "KVM_GET_MP_STATE", "KVM_SET_MP_STATE", "KVM_NMI", "KVM_SET_GUEST_DEBUG",
    "KVM_X86_SETUP_MCE", "KVM_X86_GET_MCE_CAP_SUPPORTED", "KVM_X86_SET_MCE",
    "KVM_GET_VCPU_EVENTS", "KVM_SET_VCPU_EVENTS", "KVM_GET_DEBUGREGS",
    "KVM_SET_DEBUGREGS", "KVM_ENABLE_CAP", "KVM_GET_XSAVE", "KVM_SET_XSAVE",
    "KVM_GET_XCRS", "KVM_SET_XCRS", "KVM_DIRTY_TLB", "KVM_GET_ONE_REG",
    "KVM_SET_ONE_REG", "KVM_KVMCLOCK_CTRL", "KVM_ARM_VCPU_INIT",
    "KVM_ARM_PREFERRED_TARGET", "KVM_GET_REG_LIST", "KVM_S390_MEM_OP",
    "KVM_S390_GET_SKEYS", "KVM_S390_SET_SKEYS", "KVM_S390_IRQ",
    "KVM_S390_SET_IRQ_STATE", "KVM_S390_GET_IRQ_STATE", "KVM_SMI",
    "KVM_S390_GET_CMMA_BITS", "KVM_S390_SET_CMMA_BITS",
    "KVM_MEMORY_ENCRYPT_OP", "KVM_MEMORY_ENCRYPT_REG_REGION",
    "KVM_MEMORY_ENCRYPT_UNREG_REGION", "KVM_HYPERV_EVENTFD",
    "KVM_GET_NESTED_STATE", "KVM_SET_NESTED_STATE", "KVM_CLEAR_DIRTY_LOG",
    "KVM_GET_SUPPORTED_HV_CPUID", "KVM_ARM_VCPU_FINALIZE",
    "KVM_S390_NORMAL_RESET", "KVM_S390_CLEAR_RESET", "KVM_S390_PV_COMMAND",
    "KVM_X86_SET_MSR_FILTER", "KVM_RESET_DIRTY_RINGS",
    "KVM_XEN_HVM_GET_ATTR", "KVM_XEN_HVM_SET_ATTR",
    "KVM_XEN_VCPU_GET_ATTR", "KVM_XEN_VCPU_SET_ATTR", "KVM_XEN_HVM_EVTCHN_SEND",
    "KVM_GET_SREGS2", "KVM_SET_SREGS2", "KVM_GET_STATS_FD", "KVM_GET_XSAVE2",
    "KVM_S390_PV_CPU_COMMAND", "KVM_S390_ZPCI_OP", "KVM_SET_MEMORY_ATTRIBUTES",
    "KVM_CREATE_GUEST_MEMFD", "KVM_PRE_FAULT_MEMORY",
    "KvmEmulateIo", "KvmEmulateMmio")

add("Linux.KVM.Exit", "libkvm",
    "KVM_EXIT_UNKNOWN", "KVM_EXIT_EXCEPTION", "KVM_EXIT_IO", "KVM_EXIT_HYPERCALL",
    "KVM_EXIT_DEBUG", "KVM_EXIT_HLT", "KVM_EXIT_MMIO", "KVM_EXIT_IRQ_WINDOW_OPEN",
    "KVM_EXIT_SHUTDOWN", "KVM_EXIT_FAIL_ENTRY", "KVM_EXIT_INTR",
    "KVM_EXIT_SET_TPR", "KVM_EXIT_TPR_ACCESS", "KVM_EXIT_S390_SIEIC",
    "KVM_EXIT_S390_RESET", "KVM_EXIT_NMI", "KVM_EXIT_INTERNAL_ERROR",
    "KVM_EXIT_OSI", "KVM_EXIT_PAPR_HCALL", "KVM_EXIT_S390_UCONTROL",
    "KVM_EXIT_WATCHDOG", "KVM_EXIT_S390_TSCH", "KVM_EXIT_EPR",
    "KVM_EXIT_SYSTEM_EVENT", "KVM_EXIT_S390_STSI", "KVM_EXIT_IOAPIC_EOI",
    "KVM_EXIT_HYPERV", "KVM_EXIT_ARM_NISV", "KVM_EXIT_X86_RDMSR",
    "KVM_EXIT_X86_WRMSR", "KVM_EXIT_DIRTY_RING_FULL", "KVM_EXIT_AP_RESET_HOLD",
    "KVM_EXIT_X86_BUS_LOCK", "KVM_EXIT_XEN", "KVM_EXIT_RISCV_SBI",
    "KVM_EXIT_RISCV_CSR", "KVM_EXIT_NOTIFY", "KVM_EXIT_LOONGARCH_IOCSR",
    "KVM_EXIT_MEMORY_FAULT", "KVM_EXIT_TDX", "KVM_EXIT_ARM_SEA",
    "KVM_EXIT_ARM_LDST64B", "KVM_EXIT_SNP_REQ_CERTS",
    "KVM_EXIT_IO_IN", "KVM_EXIT_IO_OUT",
    "KVM_SYSTEM_EVENT_SHUTDOWN", "KVM_SYSTEM_EVENT_RESET",
    "KVM_SYSTEM_EVENT_CRASH", "KVM_SYSTEM_EVENT_WAKEUP",
    "KVM_SYSTEM_EVENT_SUSPEND", "KVM_SYSTEM_EVENT_SEV_TERM",
    "KVM_SYSTEM_EVENT_TDX_FATAL")

add("Linux.KVM.Cap", "libkvm",
    "KVM_CAP_IRQCHIP", "KVM_CAP_HLT", "KVM_CAP_MMU_SHADOW_CACHE_CONTROL",
    "KVM_CAP_USER_MEMORY", "KVM_CAP_SET_TSS_ADDR", "KVM_CAP_VAPIC",
    "KVM_CAP_EXT_CPUID", "KVM_CAP_CLOCKSOURCE", "KVM_CAP_NR_VCPUS",
    "KVM_CAP_NR_MEMSLOTS", "KVM_CAP_PIT", "KVM_CAP_NOP_IO_DELAY",
    "KVM_CAP_MP_STATE", "KVM_CAP_COALESCED_MMIO", "KVM_CAP_SYNC_MMU",
    "KVM_CAP_IOMMU", "KVM_CAP_DESTROY_MEMORY_REGION_WORKS", "KVM_CAP_USER_NMI",
    "KVM_CAP_SET_GUEST_DEBUG", "KVM_CAP_IRQ_ROUTING", "KVM_CAP_IRQ_INJECT_STATUS",
    "KVM_CAP_ASSIGN_DEV_IRQ", "KVM_CAP_JOIN_MEMORY_REGIONS_WORKS", "KVM_CAP_MCE",
    "KVM_CAP_IRQFD", "KVM_CAP_PIT2", "KVM_CAP_SET_BOOT_CPU_ID",
    "KVM_CAP_PIT_STATE2", "KVM_CAP_IOEVENTFD", "KVM_CAP_SET_IDENTITY_MAP_ADDR",
    "KVM_CAP_XEN_HVM", "KVM_CAP_ADJUST_CLOCK", "KVM_CAP_INTERNAL_ERROR_DATA",
    "KVM_CAP_VCPU_EVENTS", "KVM_CAP_HYPERV", "KVM_CAP_HYPERV_VAPIC",
    "KVM_CAP_HYPERV_SPIN", "KVM_CAP_PCI_SEGMENT", "KVM_CAP_INTR_SHADOW",
    "KVM_CAP_DEBUGREGS", "KVM_CAP_X86_ROBUST_SINGLESTEP", "KVM_CAP_ENABLE_CAP",
    "KVM_CAP_XSAVE", "KVM_CAP_XCRS", "KVM_CAP_ASYNC_PF", "KVM_CAP_TSC_CONTROL",
    "KVM_CAP_GET_TSC_KHZ", "KVM_CAP_MAX_VCPUS", "KVM_CAP_ONE_REG",
    "KVM_CAP_TSC_DEADLINE_TIMER", "KVM_CAP_SYNC_REGS", "KVM_CAP_PCI_2_3",
    "KVM_CAP_KVMCLOCK_CTRL", "KVM_CAP_SIGNAL_MSI", "KVM_CAP_READONLY_MEM",
    "KVM_CAP_IRQFD_RESAMPLE", "KVM_CAP_DEVICE_CTRL", "KVM_CAP_ARM_PSCI",
    "KVM_CAP_ARM_SET_DEVICE_ADDR", "KVM_CAP_EXT_EMUL_CPUID", "KVM_CAP_HYPERV_TIME",
    "KVM_CAP_ENABLE_CAP_VM", "KVM_CAP_IOEVENTFD_NO_LENGTH", "KVM_CAP_VM_ATTRIBUTES",
    "KVM_CAP_ARM_PSCI_0_2", "KVM_CAP_CHECK_EXTENSION_VM", "KVM_CAP_DISABLE_QUIRKS",
    "KVM_CAP_X86_SMM", "KVM_CAP_MULTI_ADDRESS_SPACE", "KVM_CAP_SPLIT_IRQCHIP",
    "KVM_CAP_IOEVENTFD_ANY_LENGTH", "KVM_CAP_HYPERV_SYNIC", "KVM_CAP_ARM_PMU_V3",
    "KVM_CAP_VCPU_ATTRIBUTES", "KVM_CAP_MAX_VCPU_ID", "KVM_CAP_X2APIC_API",
    "KVM_CAP_MSI_DEVID", "KVM_CAP_IMMEDIATE_EXIT", "KVM_CAP_X86_DISABLE_EXITS",
    "KVM_CAP_HYPERV_SYNIC2", "KVM_CAP_HYPERV_VP_INDEX", "KVM_CAP_GET_MSR_FEATURES",
    "KVM_CAP_HYPERV_EVENTFD", "KVM_CAP_HYPERV_TLBFLUSH", "KVM_CAP_NESTED_STATE",
    "KVM_CAP_MSR_PLATFORM_INFO", "KVM_CAP_HYPERV_SEND_IPI", "KVM_CAP_COALESCED_PIO",
    "KVM_CAP_HYPERV_ENLIGHTENED_VMCS", "KVM_CAP_EXCEPTION_PAYLOAD",
    "KVM_CAP_ARM_VM_IPA_SIZE", "KVM_CAP_HYPERV_CPUID",
    "KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2", "KVM_CAP_ARM_SVE",
    "KVM_CAP_ARM_PTRAUTH_ADDRESS", "KVM_CAP_ARM_PTRAUTH_GENERIC",
    "KVM_CAP_PMU_EVENT_FILTER", "KVM_CAP_HYPERV_DIRECT_TLBFLUSH",
    "KVM_CAP_ARM_NISV_TO_USER", "KVM_CAP_HALT_POLL", "KVM_CAP_ASYNC_PF_INT",
    "KVM_CAP_STEAL_TIME", "KVM_CAP_X86_USER_SPACE_MSR", "KVM_CAP_X86_MSR_FILTER",
    "KVM_CAP_ENFORCE_PV_FEATURE_CPUID", "KVM_CAP_SYS_HYPERV_CPUID",
    "KVM_CAP_DIRTY_LOG_RING", "KVM_CAP_X86_BUS_LOCK_EXIT",
    "KVM_CAP_SET_GUEST_DEBUG2", "KVM_CAP_SGX_ATTRIBUTE",
    "KVM_CAP_VM_COPY_ENC_CONTEXT_FROM", "KVM_CAP_HYPERV_ENFORCE_CPUID",
    "KVM_CAP_SREGS2", "KVM_CAP_EXIT_HYPERCALL", "KVM_CAP_BINARY_STATS_FD",
    "KVM_CAP_EXIT_ON_EMULATION_FAILURE", "KVM_CAP_ARM_MTE",
    "KVM_CAP_VM_MOVE_ENC_CONTEXT_FROM", "KVM_CAP_VM_GPA_BITS", "KVM_CAP_XSAVE2",
    "KVM_CAP_SYS_ATTRIBUTES", "KVM_CAP_VM_TSC_CONTROL", "KVM_CAP_SYSTEM_EVENT_DATA",
    "KVM_CAP_ARM_SYSTEM_SUSPEND", "KVM_CAP_X86_TRIPLE_FAULT_EVENT",
    "KVM_CAP_X86_NOTIFY_VMEXIT", "KVM_CAP_DIRTY_LOG_RING_ACQ_REL",
    "KVM_CAP_DIRTY_LOG_RING_WITH_BITMAP", "KVM_CAP_USER_MEMORY2",
    "KVM_CAP_MEMORY_FAULT_INFO", "KVM_CAP_MEMORY_ATTRIBUTES",
    "KVM_CAP_GUEST_MEMFD", "KVM_CAP_VM_TYPES", "KVM_CAP_PRE_FAULT_MEMORY",
    "KVM_CAP_X86_APIC_BUS_CYCLES_NS", "KVM_CAP_X86_GUEST_MODE",
    "KVM_CAP_ARM_EL2", "KVM_CAP_GUEST_MEMFD_FLAGS", "KVM_CAP_ARM_SEA_TO_USER")

add("Linux.KVM.Device", "libkvm",
    "KVM_DEV_TYPE_VFIO", "KVM_DEV_TYPE_ARM_VGIC_V2", "KVM_DEV_TYPE_ARM_VGIC_V3",
    "KVM_DEV_TYPE_ARM_VGIC_ITS", "KVM_DEV_TYPE_ARM_VGIC_V5", "KVM_DEV_TYPE_XICS",
    "KVM_DEV_TYPE_XIVE", "KVM_DEV_TYPE_FLIC", "KVM_DEV_TYPE_ARM_PV_TIME",
    "KVM_DEV_TYPE_RISCV_AIA", "KVM_DEV_VFIO_FILE_ADD", "KVM_DEV_VFIO_FILE_DEL")

add("Linux.VFIO", "libvfio",
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
    "VFIO_IOMMU_SPAPR_TCE_GET_INFO", "VFIO_IOMMU_SPAPR_REGISTER_MEMORY",
    "VFIO_IOMMU_SPAPR_UNREGISTER_MEMORY", "VFIO_IOMMU_SPAPR_TCE_CREATE",
    "VFIO_IOMMU_SPAPR_TCE_REMOVE", "VFIO_DEVICE_BIND_IOMMUFD",
    "VFIO_DEVICE_UNBIND_IOMMUFD", "VFIO_DEVICE_ATTACH_IOMMUFD_PT",
    "VFIO_DEVICE_DETACH_IOMMUFD_PT", "VFIO_DEVICE_GET_PCI_HOT_RESET_INFO",
    "VFIO_EEH_PE_OP", "VFIO_DEVICE_PCI_HOT_RESET")

add("Linux.Vhost", "libvhost",
    "VHOST_GET_FEATURES", "VHOST_SET_FEATURES", "VHOST_SET_OWNER",
    "VHOST_RESET_OWNER", "VHOST_SET_MEM_TABLE", "VHOST_SET_LOG_BASE",
    "VHOST_SET_LOG_FD", "VHOST_SET_VRING_NUM", "VHOST_SET_VRING_ADDR",
    "VHOST_SET_VRING_BASE", "VHOST_GET_VRING_BASE", "VHOST_SET_VRING_KICK",
    "VHOST_SET_VRING_CALL", "VHOST_SET_VRING_ERR", "VHOST_SET_VRING_ENDIAN",
    "VHOST_GET_VRING_ENDIAN", "VHOST_SET_VRING_BUSYLOOP_TIMEOUT",
    "VHOST_GET_VRING_BUSYLOOP_TIMEOUT", "VHOST_NET_SET_BACKEND",
    "VHOST_SCSI_SET_ENDPOINT", "VHOST_SCSI_CLEAR_ENDPOINT",
    "VHOST_SCSI_GET_ABI_VERSION", "VHOST_SCSI_GET_EVENTS_MISSED",
    "VHOST_SCSI_SET_EVENTS_MISSED", "VHOST_VSOCK_SET_GUEST_CID",
    "VHOST_VSOCK_SET_RUNNING", "VHOST_VDPA_GET_DEVICE_ID",
    "VHOST_VDPA_GET_STATUS", "VHOST_VDPA_SET_STATUS", "VHOST_VDPA_GET_CONFIG",
    "VHOST_VDPA_SET_CONFIG", "VHOST_VDPA_SET_VRING_ENABLE",
    "VHOST_SET_BACKEND_FEATURES", "VHOST_GET_BACKEND_FEATURES",
    "VHOST_VDPA_GET_IOVA_RANGE", "VHOST_VDPA_GET_CONFIG_SIZE",
    "VHOST_VDPA_GET_ASID", "VHOST_VDPA_SET_GROUP_ASID",
    "VHOST_VDPA_SUSPEND", "VHOST_VDPA_RESUME", "VHOST_VDPA_GET_VQS_COUNT",
    "VHOST_VDPA_GET_GROUP_NUM", "VHOST_VDPA_GET_VRING_GROUP",
    "VHOST_VDPA_SET_VRING_SEPARATE_BACKEND", "VHOST_SET_INFLIGHT_FD",
    "VHOST_GET_INFLIGHT_FD", "VHOST_IOTLB_CONFIG_V2", "VHOST_SET_IOTLB_FD")

add("Linux.MSHV", "libmshv",
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

add("Linux.Virtio", "libvirtio",
    "VIRTIO_MMIO_MAGIC", "VIRTIO_MMIO_VERSION", "VIRTIO_MMIO_DEVICE_ID",
    "VIRTIO_MMIO_VENDOR_ID", "VIRTIO_MMIO_DEVICE_FEATURES",
    "VIRTIO_MMIO_DRIVER_FEATURES", "VIRTIO_MMIO_QUEUE_SEL",
    "VIRTIO_MMIO_QUEUE_NUM_MAX", "VIRTIO_MMIO_QUEUE_NUM",
    "VIRTIO_MMIO_QUEUE_READY", "VIRTIO_MMIO_QUEUE_NOTIFY",
    "VIRTIO_MMIO_INTERRUPT_STATUS", "VIRTIO_MMIO_INTERRUPT_ACK",
    "VIRTIO_MMIO_STATUS", "VIRTIO_MMIO_QUEUE_DESC_LOW",
    "VIRTIO_MMIO_QUEUE_AVAIL_LOW", "VIRTIO_MMIO_QUEUE_USED_LOW",
    "VIRTIO_PCI_CAP_COMMON_CFG", "VIRTIO_PCI_CAP_NOTIFY_CFG",
    "VIRTIO_PCI_CAP_ISR_CFG", "VIRTIO_PCI_CAP_DEVICE_CFG",
    "VIRTIO_PCI_CAP_PCI_CFG", "VIRTIO_CONFIG_S_ACKNOWLEDGE",
    "VIRTIO_CONFIG_S_DRIVER", "VIRTIO_CONFIG_S_DRIVER_OK",
    "VIRTIO_CONFIG_S_FEATURES_OK", "VIRTIO_CONFIG_S_NEEDS_RESET",
    "VIRTIO_CONFIG_S_FAILED", "VIRTIO_F_RING_INDIRECT_DESC",
    "VIRTIO_F_RING_EVENT_IDX", "VIRTIO_F_VERSION_1",
    "VIRTIO_F_ACCESS_PLATFORM", "VIRTIO_F_RING_PACKED",
    "VIRTIO_F_IN_ORDER", "VIRTIO_F_ORDER_PLATFORM",
    "VIRTIO_NET_F_CSUM", "VIRTIO_NET_F_MAC", "VIRTIO_NET_F_MQ",
    "VIRTIO_BLK_F_SIZE_MAX", "VIRTIO_BLK_F_SEG_MAX", "VIRTIO_CONSOLE_F_SIZE",
    "VIRTIO_GPU_F_VIRGL", "VIRTIO_BALLOON_F_MUST_TELL_HOST",
    "VIRTIO_SCSI_F_INOUT", "VIRTIO_VSOCK_F_STREAM")

# --- X11 / Wayland (user32/gdi analog) ---
add("Linux.X11", "libX11",
    "XOpenDisplay", "XCloseDisplay", "XCreateWindow", "XCreateSimpleWindow",
    "XDestroyWindow", "XMapWindow", "XMapRaised", "XUnmapWindow",
    "XStoreName", "XFetchName", "XSelectInput", "XNextEvent", "XPeekEvent",
    "XPending", "XFlush", "XSync", "XEventsQueued", "XSendEvent",
    "XCreateGC", "XFreeGC", "XChangeGC", "XGetGCValues",
    "XDrawLine", "XDrawLines", "XDrawRectangle", "XFillRectangle",
    "XDrawArc", "XFillArc", "XDrawString", "XDrawImageString",
    "XSetForeground", "XSetBackground", "XSetLineAttributes", "XSetFillStyle",
    "XInternAtom", "XGetAtomName", "XSetWMProtocols", "XGetWMProtocols",
    "XGetWindowAttributes", "XChangeWindowAttributes", "XMoveWindow",
    "XResizeWindow", "XMoveResizeWindow", "XRaiseWindow", "XLowerWindow",
    "XGetGeometry", "XTranslateCoordinates", "XRootWindow", "XDefaultScreen",
    "XDefaultRootWindow", "XDefaultGC", "XBlackPixel", "XWhitePixel",
    "XDisplayWidth", "XDisplayHeight", "XDisplayWidthMM", "XDisplayHeightMM",
    "XConnectionNumber", "XLookupKeysym", "XLookupString", "XKeysymToString",
    "XStringToKeysym", "XGrabKeyboard", "XUngrabKeyboard", "XGrabPointer",
    "XUngrabPointer", "XWarpPointer", "XQueryPointer", "XCreatePixmap",
    "XFreePixmap", "XCopyArea", "XCopyPlane", "XSetInputFocus",
    "XGetInputFocus", "XDefineCursor", "XUndefineCursor", "XCreateFontCursor",
    "XCreatePixmapCursor", "XFreeCursor", "XLoadQueryFont", "XFreeFont",
    "XSetFont", "XQueryFont", "XClearWindow", "XClearArea", "XBell",
    "XKillClient", "XSetErrorHandler", "XSetIOErrorHandler",
    "XInitThreads", "XLockDisplay", "XUnlockDisplay", "XFree",
    "XCreateColormap", "XFreeColormap", "XAllocColor", "XParseColor",
    "XMatchVisualInfo", "XGetVisualInfo", "XCreateImage", "XDestroyImage",
    "XPutImage", "XGetImage", "XAddToSaveSet", "XRemoveFromSaveSet",
    "XReparentWindow", "XConfigureWindow", "XSetWindowBackground",
    "XSetWindowBorder", "XChangeProperty", "XGetWindowProperty",
    "XDeleteProperty", "XListProperties", "XIconifyWindow", "XWithdrawWindow")

add("Linux.Wayland", "libwayland-client",
    "wl_display_connect", "wl_display_connect_to_fd", "wl_display_disconnect",
    "wl_display_get_fd", "wl_display_roundtrip", "wl_display_dispatch",
    "wl_display_dispatch_pending", "wl_display_flush", "wl_display_prepare_read",
    "wl_display_read_events", "wl_display_cancel_read",
    "wl_display_get_error", "wl_display_get_protocol_error",
    "wl_proxy_marshal", "wl_proxy_marshal_constructor",
    "wl_proxy_marshal_constructor_versioned", "wl_proxy_destroy",
    "wl_proxy_add_listener", "wl_proxy_get_listener", "wl_proxy_get_user_data",
    "wl_proxy_set_user_data", "wl_proxy_get_id", "wl_proxy_get_class",
    "wl_proxy_get_version", "wl_proxy_set_queue",
    "wl_event_queue_destroy", "wl_display_create_queue",
    "wl_compositor_create_surface", "wl_compositor_create_region",
    "wl_surface_attach", "wl_surface_damage", "wl_surface_damage_buffer",
    "wl_surface_commit", "wl_surface_set_buffer_scale",
    "wl_surface_set_buffer_transform", "wl_surface_destroy",
    "wl_surface_frame", "wl_surface_set_opaque_region",
    "wl_shm_create_pool", "wl_shm_pool_create_buffer", "wl_shm_pool_resize",
    "wl_shm_pool_destroy", "wl_buffer_destroy",
    "wl_seat_get_pointer", "wl_seat_get_keyboard", "wl_seat_get_touch",
    "wl_registry_bind", "wl_registry_add_listener",
    "xdg_wm_base_get_xdg_surface", "xdg_surface_get_toplevel",
    "xdg_toplevel_set_title", "xdg_toplevel_set_app_id",
    "xdg_toplevel_set_maximized", "xdg_toplevel_unset_maximized",
    "xdg_toplevel_set_fullscreen", "xdg_toplevel_unset_fullscreen",
    "xdg_toplevel_set_minimized")

# --- OpenSSL (bcrypt/ncrypt analog) ---
add("Linux.OpenSSL", "libcrypto",
    "OPENSSL_init_crypto", "OPENSSL_cleanup", "OPENSSL_malloc", "OPENSSL_free",
    "RAND_bytes", "RAND_priv_bytes", "RAND_status", "RAND_poll",
    "EVP_MD_CTX_new", "EVP_MD_CTX_free", "EVP_DigestInit_ex", "EVP_DigestUpdate",
    "EVP_DigestFinal_ex", "EVP_sha1", "EVP_sha256", "EVP_sha384", "EVP_sha512",
    "EVP_md5", "EVP_sha3_256", "EVP_blake2b512",
    "SHA1", "SHA256", "SHA384", "SHA512", "MD5",
    "EVP_CIPHER_CTX_new", "EVP_CIPHER_CTX_free", "EVP_EncryptInit_ex",
    "EVP_EncryptUpdate", "EVP_EncryptFinal_ex", "EVP_DecryptInit_ex",
    "EVP_DecryptUpdate", "EVP_DecryptFinal_ex",
    "EVP_aes_128_cbc", "EVP_aes_256_cbc", "EVP_aes_128_gcm", "EVP_aes_256_gcm",
    "EVP_chacha20_poly1305", "HMAC", "HMAC_CTX_new", "HMAC_CTX_free",
    "PKCS5_PBKDF2_HMAC", "EVP_PKEY_new", "EVP_PKEY_free", "EVP_PKEY_keygen",
    "EVP_PKEY_sign", "EVP_PKEY_verify", "EVP_PKEY_encrypt", "EVP_PKEY_decrypt",
    "BIO_new", "BIO_free", "BIO_read", "BIO_write", "BIO_puts", "BIO_gets",
    "BIO_ctrl", "BIO_new_file", "BIO_new_mem_buf", "PEM_read_bio_X509",
    "PEM_write_bio_X509", "PEM_read_bio_PrivateKey", "PEM_write_bio_PrivateKey",
    "X509_new", "X509_free", "X509_STORE_new", "X509_STORE_free",
    "X509_STORE_CTX_new", "X509_verify_cert", "X509_get_subject_name",
    "X509_NAME_oneline", "ERR_get_error", "ERR_error_string", "ERR_print_errors",
    "ERR_clear_error", "BN_new", "BN_free", "BN_bin2bn", "BN_bn2bin")

add("Linux.OpenSSL", "libssl",
    "OPENSSL_init_ssl", "SSL_library_init", "SSL_load_error_strings",
    "SSL_CTX_new", "SSL_CTX_free", "SSL_CTX_set_options", "SSL_CTX_use_certificate_file",
    "SSL_CTX_use_PrivateKey_file", "SSL_CTX_check_private_key",
    "SSL_CTX_set_verify", "SSL_CTX_load_verify_locations",
    "SSL_new", "SSL_free", "SSL_set_fd", "SSL_set_bio",
    "SSL_connect", "SSL_accept", "SSL_read", "SSL_write", "SSL_shutdown",
    "SSL_get_error", "SSL_get_peer_certificate", "SSL_get_verify_result",
    "SSL_set_connect_state", "SSL_set_accept_state", "SSL_do_handshake",
    "SSL_pending", "SSL_has_pending", "SSL_get_version", "SSL_get_cipher",
    "TLS_method", "TLS_client_method", "TLS_server_method",
    "TLS_1_2_client_method", "TLS_1_3_client_method")

# --- systemd (services analog) ---
add("Linux.Systemd", "libsystemd",
    "sd_notify", "sd_notifyf", "sd_pid_notify", "sd_booted",
    "sd_watchdog_enabled", "sd_watchdog_ping", "sd_listen_fds",
    "sd_listen_fds_with_names", "sd_is_fifo", "sd_is_socket",
    "sd_is_socket_inet", "sd_is_socket_unix", "sd_journal_print",
    "sd_journal_printv", "sd_journal_send", "sd_journal_sendv",
    "sd_journal_perror", "sd_journal_open", "sd_journal_open_directory",
    "sd_journal_close", "sd_journal_next", "sd_journal_previous",
    "sd_journal_next_skip", "sd_journal_previous_skip", "sd_journal_get_data",
    "sd_journal_enumerate_data", "sd_journal_restart_data",
    "sd_journal_seek_head", "sd_journal_seek_tail", "sd_journal_seek_cursor",
    "sd_journal_get_cursor", "sd_journal_add_match", "sd_journal_flush_matches",
    "sd_journal_get_realtime_usec", "sd_journal_get_monotonic_usec",
    "sd_journal_get_cutoff_realtime_usec", "sd_journal_get_fd",
    "sd_journal_process", "sd_journal_wait", "sd_journal_get_events",
    "sd_bus_open_system", "sd_bus_open_user", "sd_bus_open", "sd_bus_close",
    "sd_bus_unref", "sd_bus_ref", "sd_bus_call", "sd_bus_call_method",
    "sd_bus_call_method_async", "sd_bus_add_match", "sd_bus_add_object",
    "sd_bus_add_object_vtable", "sd_bus_process", "sd_bus_wait",
    "sd_bus_get_fd", "sd_bus_message_new_method_call", "sd_bus_message_append",
    "sd_bus_message_read", "sd_bus_message_unref", "sd_bus_error_free",
    "sd_bus_request_name", "sd_bus_release_name", "sd_bus_get_unique_name",
    "sd_id128_get_machine", "sd_id128_get_boot", "sd_id128_get_invocation",
    "sd_id128_randomize", "sd_id128_to_string", "sd_id128_from_string",
    "sd_get_machine_names", "sd_pid_get_session", "sd_pid_get_owner_uid",
    "sd_pid_get_unit", "sd_uid_get_state", "sd_session_get_state",
    "sd_session_get_type", "sd_session_get_class", "sd_session_get_desktop",
    "sd_login_monitor_new", "sd_login_monitor_unref", "sd_login_monitor_flush",
    "sd_login_monitor_get_fd")

add("Linux.Systemd", "systemctl",
    "systemctl", "journalctl", "loginctl", "hostnamectl", "timedatectl",
    "localectl", "busctl", "systemd-analyze", "systemd-run",
    "systemd-cgls", "systemd-cgtop", "systemd-detect-virt",
    "systemd-escape", "systemd-path", "systemd-id128",
    "systemd-tmpfiles", "systemd-sysusers", "resolvectl", "networkctl")

# --- Kernel / vmlinux (ntoskrnl + ntdll analog). Public EXPORT_SYMBOL
# names from include/linux/*.h — not a dump of vmlinux.o. ---
add("Linux.Kernel.slab", "vmlinux",
    "kmalloc", "kzalloc", "kcalloc", "kfree", "krealloc", "kfree_sensitive",
    "kvmalloc", "kvzalloc", "kvfree", "kmalloc_array", "kmalloc_node",
    "kmem_cache_create", "kmem_cache_create_usercopy", "kmem_cache_destroy",
    "kmem_cache_alloc", "kmem_cache_alloc_node", "kmem_cache_free",
    "kmem_cache_alloc_bulk", "kmem_cache_free_bulk", "kmem_cache_size",
    "ksize", "kstrdup", "kstrndup", "kmemdup", "kmemdup_nul",
    "mempool_create", "mempool_alloc", "mempool_free", "mempool_destroy",
    "devm_kmalloc", "devm_kzalloc", "devm_kfree", "__kmalloc",
    "kmem_cache_alloc_lru", "slab_is_available")

add("Linux.Kernel.mm", "vmlinux",
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
    "set_memory_x", "set_memory_nx", "set_memory_p", "set_pages_ro",
    "high_memory", "PAGE_OFFSET", "TASK_SIZE", "VMALLOC_START",
    "flush_dcache_page", "flush_icache_range", "invalidate_kernel_vmap_range",
    "kmap", "kunmap", "kmap_atomic", "kunmap_atomic", "kmap_local_page",
    "kunmap_local", "vmalloc_to_page", "vmalloc_to_pfn",
    "follow_pfn", "follow_pte", "apply_to_page_range")

add("Linux.Kernel.sched", "vmlinux",
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

add("Linux.Kernel.irq", "vmlinux",
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

add("Linux.Kernel.lock", "vmlinux",
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
    "preempt_notifier_register", "lockdep_assert_held")

add("Linux.Kernel.workqueue", "vmlinux",
    "schedule_work", "schedule_delayed_work", "queue_work", "queue_delayed_work",
    "mod_delayed_work", "flush_work", "flush_delayed_work", "cancel_work_sync",
    "cancel_delayed_work_sync", "flush_workqueue", "destroy_workqueue",
    "alloc_workqueue", "alloc_ordered_workqueue", "create_workqueue",
    "create_singlethread_workqueue", "INIT_WORK", "INIT_DELAYED_WORK",
    "system_wq", "system_highpri_wq", "system_long_wq", "system_unbound_wq",
    "system_freezable_wq", "queue_work_on", "schedule_work_on",
    "execute_in_process_context", "task_work_add")

add("Linux.Kernel.module", "vmlinux",
    "module_init", "module_exit", "request_module", "try_then_request_module",
    "try_module_get", "module_put", "__module_get", "symbol_get", "symbol_put",
    "find_module", "find_symbol", "kallsyms_lookup_name",
    "kallsyms_lookup", "kallsyms_on_each_symbol", "sprint_symbol",
    "lookup_symbol_name", "module_kallsyms_lookup_name",
    "register_module_notifier", "unregister_module_notifier",
    "init_module", "finit_module", "delete_module", "kernel_read_file_from_path",
    "load_module", "MODULE_LICENSE", "MODULE_AUTHOR", "MODULE_DESCRIPTION",
    "EXPORT_SYMBOL", "EXPORT_SYMBOL_GPL", "THIS_MODULE", "THIS_MODULE->name")

add("Linux.Kernel.kvm_host", "vmlinux",
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
    "vmx_vcpu_run", "svm_vcpu_run", "kvm_x86_ops", "kvm_x86_init_ops",
    "kvm_arch_init", "kvm_arch_exit", "kvm_init", "kvm_exit",
    "kvm_get_kvm", "kvm_put_kvm", "kvm_get_vcpu", "kvm_for_each_vcpu",
    "srcu_read_lock_kvm", "srcu_read_unlock_kvm",
    "gfn_to_memslot", "search_memslots", "id_to_memslot",
    "kvm_vcpu_gfn_to_memslot", "kvm_vcpu_gfn_to_hva", "kvm_vcpu_gfn_to_pfn")

add("Linux.Kernel.pci", "vmlinux",
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

add("Linux.Kernel.dma", "vmlinux",
    "dma_alloc_coherent", "dma_free_coherent", "dma_alloc_attrs",
    "dma_free_attrs", "dma_map_single", "dma_unmap_single",
    "dma_map_page", "dma_unmap_page", "dma_map_sg", "dma_unmap_sg",
    "dma_sync_single_for_cpu", "dma_sync_single_for_device",
    "dma_set_mask", "dma_set_coherent_mask", "dma_set_mask_and_coherent",
    "dma_get_required_mask", "dma_mmap_coherent", "dma_declare_coherent_memory")

add("Linux.Kernel.iommu", "vmlinux",
    "iommu_domain_alloc", "iommu_domain_free", "iommu_attach_device",
    "iommu_detach_device", "iommu_map", "iommu_unmap", "iommu_iova_to_phys",
    "iommu_set_fault_handler", "iommu_present", "iommu_capable",
    "iommu_group_get", "iommu_group_put", "iommu_group_add_device",
    "iommu_register_device", "iommu_fwspec_init", "iommu_dev_enable_feature",
    "iommu_map_sg", "iommu_unmap_fast", "iommu_flush_iotlb_all",
    "iommu_aux_attach_device", "iommu_sva_bind_device", "iommu_sva_unbind_device")

add("Linux.Kernel.net", "vmlinux",
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

add("Linux.Kernel.fs", "vmlinux",
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

add("Linux.Kernel.time", "vmlinux",
    "ktime_get", "ktime_get_ns", "ktime_get_real", "ktime_get_boottime",
    "ktime_get_clocktai", "jiffies", "jiffies_to_msecs", "msecs_to_jiffies",
    "nsecs_to_jiffies", "get_jiffies_64", "ktime_add", "ktime_sub",
    "hrtimer_init", "hrtimer_start", "hrtimer_cancel", "hrtimer_forward",
    "mod_timer", "add_timer", "del_timer", "del_timer_sync", "timer_setup",
    "from_timer", "time64_to_tm", "ktime_to_timespec64")

add("Linux.Kernel.trace", "vmlinux",
    "trace_event", "tracepoint_probe_register", "tracepoint_probe_unregister",
    "register_kprobe", "unregister_kprobe", "register_kretprobe",
    "unregister_kretprobe", "register_ftrace_function",
    "unregister_ftrace_function", "ftrace_set_filter", "register_die_notifier",
    "register_nmi_handler", "perf_event_create_kernel_counter",
    "perf_event_release_kernel", "perf_sw_event", "trace_clock",
    "ring_buffer_alloc", "ring_buffer_free", "ring_buffer_lock_reserve")

add("Linux.Kernel.ns", "vmlinux",
    "copy_namespaces", "unshare_nsproxy_namespaces", "switch_task_namespaces",
    "free_nsproxy", "create_pid_namespace", "copy_pid_ns", "put_pid_ns",
    "copy_net_ns", "get_net", "put_net", "copy_mnt_ns", "copy_uts_ns",
    "copy_ipc_ns", "copy_user_ns", "copy_cgroup_ns", "copy_time_ns",
    "ns_get_path", "proc_ns_file", "setns", "unshare",
    "CLONE_NEWNS", "CLONE_NEWCGROUP", "CLONE_NEWUTS", "CLONE_NEWIPC",
    "CLONE_NEWUSER", "CLONE_NEWPID", "CLONE_NEWNET", "CLONE_NEWTIME",
    "CLONE_VM", "CLONE_FS", "CLONE_FILES", "CLONE_SIGHAND", "CLONE_PTRACE",
    "CLONE_VFORK", "CLONE_THREAD", "CLONE_NEWCGROUP")

add("Linux.Kernel.cgroup", "vmlinux",
    "cgroup_procs_write", "cgroup_attach_task", "cgroup_mkdir", "cgroup_rmdir",
    "cgroup_add_cftypes", "cgroup_path", "task_cgroup", "css_set_move_task",
    "cgroup_get", "cgroup_put", "cgroup_lock", "cgroup_unlock",
    "cgroup_ssid_enabled", "cgroup_on_dfl", "cgroup_is_populated",
    "mem_cgroup_from_task", "memcg_kmem_enabled", "cgroup_bpf_run_filter_skb")

add("Linux.BPF", "libbpf",
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

add("Linux.Netlink", "libnl",
    "socket", "bind", "sendmsg", "recvmsg", "setsockopt",
    "nl_socket_alloc", "nl_socket_free", "nl_connect", "nl_send_auto",
    "nl_recvmsgs_default", "nl_socket_add_membership",
    "NETLINK_ROUTE", "NETLINK_UNUSED", "NETLINK_USERSOCK", "NETLINK_FIREWALL",
    "NETLINK_SOCK_DIAG", "NETLINK_NFLOG", "NETLINK_XFRM", "NETLINK_SELINUX",
    "NETLINK_ISCSI", "NETLINK_AUDIT", "NETLINK_FIB_LOOKUP", "NETLINK_CONNECTOR",
    "NETLINK_NETFILTER", "NETLINK_IP6_FW", "NETLINK_DNRTMSG", "NETLINK_KOBJECT_UEVENT",
    "NETLINK_GENERIC", "NETLINK_SCSITRANSPORT", "NETLINK_ECRYPTFS",
    "NETLINK_RDMA", "NETLINK_CRYPTO", "NETLINK_SMC",
    "RTM_NEWLINK", "RTM_DELLINK", "RTM_GETLINK", "RTM_SETLINK",
    "RTM_NEWADDR", "RTM_DELADDR", "RTM_GETADDR", "RTM_NEWROUTE",
    "RTM_DELROUTE", "RTM_GETROUTE", "RTM_NEWNEIGH", "RTM_DELNEIGH",
    "RTM_GETNEIGH", "RTM_NEWRULE", "RTM_DELRULE", "RTM_GETRULE",
    "RTM_NEWQDISC", "RTM_DELQDISC", "RTM_GETQDISC", "RTM_NEWTCLASS",
    "RTM_NEWTFILTER", "RTM_NEWNETCONF", "RTM_GETNETCONF", "RTM_NEWNSID",
    "RTM_GETNSID", "RTM_NEWSTATS", "RTM_GETSTATS", "RTM_NEWCHAIN",
    "RTM_NEWNEXTHOP", "RTM_GETNEXTHOP", "RTM_NEWLINKPROP",
    "NLMSG_NOOP", "NLMSG_ERROR", "NLMSG_DONE", "NLMSG_OVERRUN",
    "NLM_F_REQUEST", "NLM_F_MULTI", "NLM_F_ACK", "NLM_F_ECHO",
    "NLM_F_DUMP", "NLM_F_REPLACE", "NLM_F_EXCL", "NLM_F_CREATE",
    "NLM_F_APPEND", "NLA_U8", "NLA_U16", "NLA_U32", "NLA_U64", "NLA_STRING",
    "NLA_FLAG", "NLA_NESTED", "nla_put", "nla_get_u32", "nlmsg_parse")

add("Linux.Tun", "linux",
    "TUNSETIFF", "TUNGETIFF", "TUNSETPERSIST", "TUNSETOWNER", "TUNSETGROUP",
    "TUNSETLINK", "TUNSETDEBUG", "TUNSETOFFLOAD", "TUNSETTXFILTER",
    "TUNGETSNDBUF", "TUNSETSNDBUF", "TUNATTACHFILTER", "TUNDETACHFILTER",
    "TUNGETVNETHDRSZ", "TUNSETVNETHDRSZ", "TUNSETQUEUE", "TUNSETIFINDEX",
    "TUNGETFILTER", "TUNSETVNETLE", "TUNGETVNETLE", "TUNSETVNETBE",
    "TUNSETSTEERINGEBPF", "TUNSETFILTEREBPF", "TUNSETCARRIER",
    "TUNGETDEVNETNS", "TUNSETCARRIER", "IFF_TUN", "IFF_TAP", "IFF_NO_PI",
    "IFF_ONE_QUEUE", "IFF_VNET_HDR", "IFF_TUN_EXCL", "IFF_MULTI_QUEUE",
    "IFF_ATTACH_QUEUE", "IFF_DETACH_QUEUE", "IFF_PERSIST", "IFF_NOFILTER")

add("Linux.Vsock", "linux",
    "AF_VSOCK", "VMADDR_CID_ANY", "VMADDR_CID_HYPERVISOR", "VMADDR_CID_LOCAL",
    "VMADDR_CID_HOST", "VMADDR_PORT_ANY", "IOCTL_VM_SOCKETS_GET_LOCAL_CID",
    "SO_VM_SOCKETS_BUFFER_SIZE", "SO_VM_SOCKETS_BUFFER_MIN_SIZE",
    "SO_VM_SOCKETS_BUFFER_MAX_SIZE", "SO_VM_SOCKETS_PEER_HOST_VM_ID",
    "SO_VM_SOCKETS_CONNECT_TIMEOUT")

add("Linux.Sysctl", "linux",
    "sysctl", "_sysctl", "proc_dointvec", "proc_douintvec", "proc_dointvec_minmax",
    "proc_doulongvec_minmax", "proc_dostring", "register_sysctl",
    "register_sysctl_table", "unregister_sysctl_table", "sysctl_vfs_cache_pressure",
    "sysctl_overcommit_memory", "sysctl_max_map_count",
    "kernel.pid_max", "kernel.threads-max", "kernel.randomize_va_space",
    "kernel.kptr_restrict", "kernel.dmesg_restrict", "kernel.modules_disabled",
    "kernel.unprivileged_bpf_disabled", "kernel.yama.ptrace_scope",
    "vm.overcommit_memory", "vm.max_map_count", "vm.swappiness",
    "vm.dirty_ratio", "vm.nr_hugepages", "vm.hugetlb_shm_group",
    "net.core.somaxconn", "net.ipv4.ip_forward", "net.ipv4.tcp_syncookies",
    "fs.file-max", "fs.inotify.max_user_watches", "fs.suid_dumpable",
    "fs.protected_hardlinks", "fs.protected_symlinks",
    "user.max_user_namespaces", "user.max_pid_namespaces",
    "user.max_net_namespaces", "user.max_mnt_namespaces")

add("Linux.Ptrace", "linux",
    "PTRACE_TRACEME", "PTRACE_PEEKTEXT", "PTRACE_PEEKDATA", "PTRACE_PEEKUSER",
    "PTRACE_POKETEXT", "PTRACE_POKEDATA", "PTRACE_POKEUSER", "PTRACE_CONT",
    "PTRACE_KILL", "PTRACE_SINGLESTEP", "PTRACE_GETREGS", "PTRACE_SETREGS",
    "PTRACE_GETFPREGS", "PTRACE_SETFPREGS", "PTRACE_ATTACH", "PTRACE_DETACH",
    "PTRACE_GETFPXREGS", "PTRACE_SETFPXREGS", "PTRACE_SYSCALL",
    "PTRACE_SETOPTIONS", "PTRACE_GETEVENTMSG", "PTRACE_GETSIGINFO",
    "PTRACE_SETSIGINFO", "PTRACE_GETREGSET", "PTRACE_SETREGSET",
    "PTRACE_SEIZE", "PTRACE_INTERRUPT", "PTRACE_LISTEN", "PTRACE_PEEKSIGINFO",
    "PTRACE_GETSIGMASK", "PTRACE_SETSIGMASK", "PTRACE_SECCOMP_GET_FILTER",
    "PTRACE_SECCOMP_GET_METADATA", "PTRACE_GET_SYSCALL_INFO",
    "PTRACE_GET_RSEQ_CONFIGURATION", "PTRACE_SET_SYSCALL_USER_DISPATCH_CONFIG",
    "PTRACE_O_TRACESYSGOOD", "PTRACE_O_TRACEFORK", "PTRACE_O_TRACEVFORK",
    "PTRACE_O_TRACECLONE", "PTRACE_O_TRACEEXEC", "PTRACE_O_TRACEVFORKDONE",
    "PTRACE_O_TRACEEXIT", "PTRACE_O_TRACESECCOMP", "PTRACE_O_EXITKILL",
    "PTRACE_EVENT_FORK", "PTRACE_EVENT_VFORK", "PTRACE_EVENT_CLONE",
    "PTRACE_EVENT_EXEC", "PTRACE_EVENT_VFORK_DONE", "PTRACE_EVENT_EXIT",
    "PTRACE_EVENT_SECCOMP", "PTRACE_EVENT_STOP")

add("Linux.Prctl", "linux",
    "PR_SET_PDEATHSIG", "PR_GET_PDEATHSIG", "PR_GET_DUMPABLE", "PR_SET_DUMPABLE",
    "PR_GET_UNALIGN", "PR_SET_UNALIGN", "PR_GET_KEEPCAPS", "PR_SET_KEEPCAPS",
    "PR_GET_FPEMU", "PR_SET_FPEMU", "PR_GET_FPEXC", "PR_SET_FPEXC",
    "PR_GET_TIMING", "PR_SET_TIMING", "PR_SET_NAME", "PR_GET_NAME",
    "PR_GET_ENDIAN", "PR_SET_ENDIAN", "PR_GET_SECCOMP", "PR_SET_SECCOMP",
    "PR_CAPBSET_READ", "PR_CAPBSET_DROP", "PR_GET_TSC", "PR_SET_TSC",
    "PR_GET_SECUREBITS", "PR_SET_SECUREBITS", "PR_SET_TIMERSLACK",
    "PR_GET_TIMERSLACK", "PR_TASK_PERF_EVENTS_DISABLE",
    "PR_TASK_PERF_EVENTS_ENABLE", "PR_MCE_KILL", "PR_MCE_KILL_GET",
    "PR_SET_MM", "PR_SET_PTRACER", "PR_SET_CHILD_SUBREAPER",
    "PR_GET_CHILD_SUBREAPER", "PR_SET_NO_NEW_PRIVS", "PR_GET_NO_NEW_PRIVS",
    "PR_GET_TID_ADDRESS", "PR_SET_THP_DISABLE", "PR_GET_THP_DISABLE",
    "PR_MPX_ENABLE_MANAGEMENT", "PR_MPX_DISABLE_MANAGEMENT",
    "PR_SET_FP_MODE", "PR_GET_FP_MODE", "PR_CAP_AMBIENT",
    "PR_SVE_SET_VL", "PR_SVE_GET_VL", "PR_GET_SPECULATION_CTRL",
    "PR_SET_SPECULATION_CTRL", "PR_PAC_RESET_KEYS", "PR_SET_TAGGED_ADDR_CTRL",
    "PR_GET_TAGGED_ADDR_CTRL", "PR_SET_IO_FLUSHER", "PR_GET_IO_FLUSHER",
    "PR_SET_SYSCALL_USER_DISPATCH", "PR_PAC_SET_ENABLED_KEYS",
    "PR_PAC_GET_ENABLED_KEYS", "PR_SCHED_CORE", "PR_SME_SET_VL",
    "PR_SME_GET_VL", "PR_SET_MDWE", "PR_GET_MDWE", "PR_SET_VMA",
    "PR_GET_AUXV", "PR_SET_MEMORY_MERGE", "PR_GET_MEMORY_MERGE",
    "PR_RISCV_V_SET_CONTROL", "PR_RISCV_V_GET_CONTROL",
    "PR_RISCV_SET_ICACHE_FLUSH_CTX", "PR_PPC_GET_DEXCR", "PR_PPC_SET_DEXCR")

add("Linux.Userfaultfd", "linux",
    "UFFDIO_API", "UFFDIO_REGISTER", "UFFDIO_UNREGISTER", "UFFDIO_WAKE",
    "UFFDIO_COPY", "UFFDIO_ZEROPAGE", "UFFDIO_WRITEPROTECT", "UFFDIO_CONTINUE",
    "UFFDIO_POISON", "UFFDIO_MOVE", "UFFD_EVENT_PAGEFAULT", "UFFD_EVENT_FORK",
    "UFFD_EVENT_REMAP", "UFFD_EVENT_REMOVE", "UFFD_EVENT_UNMAP",
    "UFFDIO_REGISTER_MODE_MISSING", "UFFDIO_REGISTER_MODE_WP",
    "UFFDIO_REGISTER_MODE_MINOR")

add("Linux.IoUring", "linux",
    "IORING_OP_NOP", "IORING_OP_READV", "IORING_OP_WRITEV", "IORING_OP_FSYNC",
    "IORING_OP_READ_FIXED", "IORING_OP_WRITE_FIXED", "IORING_OP_POLL_ADD",
    "IORING_OP_POLL_REMOVE", "IORING_OP_SYNC_FILE_RANGE", "IORING_OP_SENDMSG",
    "IORING_OP_RECVMSG", "IORING_OP_TIMEOUT", "IORING_OP_TIMEOUT_REMOVE",
    "IORING_OP_ACCEPT", "IORING_OP_ASYNC_CANCEL", "IORING_OP_LINK_TIMEOUT",
    "IORING_OP_CONNECT", "IORING_OP_FALLOCATE", "IORING_OP_OPENAT",
    "IORING_OP_CLOSE", "IORING_OP_FILES_UPDATE", "IORING_OP_STATX",
    "IORING_OP_READ", "IORING_OP_WRITE", "IORING_OP_FADVISE",
    "IORING_OP_MADVISE", "IORING_OP_SEND", "IORING_OP_RECV",
    "IORING_OP_OPENAT2", "IORING_OP_EPOLL_CTL", "IORING_OP_SPLICE",
    "IORING_OP_PROVIDE_BUFFERS", "IORING_OP_REMOVE_BUFFERS",
    "IORING_OP_TEE", "IORING_OP_SHUTDOWN", "IORING_OP_RENAMEAT",
    "IORING_OP_UNLINKAT", "IORING_OP_MKDIRAT", "IORING_OP_SYMLINKAT",
    "IORING_OP_LINKAT", "IORING_OP_MSG_RING", "IORING_OP_FSETXATTR",
    "IORING_OP_SETXATTR", "IORING_OP_FGETXATTR", "IORING_OP_GETXATTR",
    "IORING_OP_SOCKET", "IORING_OP_URING_CMD", "IORING_OP_SEND_ZC",
    "IORING_OP_SENDMSG_ZC", "IORING_OP_READ_MULTISHOT", "IORING_OP_WAITID",
    "IORING_OP_FUTEX_WAIT", "IORING_OP_FUTEX_WAKE", "IORING_OP_FUTEX_WAITV",
    "IORING_OP_FIXED_FD_INSTALL", "IORING_OP_FTRUNCATE", "IORING_OP_BIND",
    "IORING_OP_LISTEN", "IORING_SETUP_IOPOLL", "IORING_SETUP_SQPOLL",
    "IORING_SETUP_SQ_AFF", "IORING_SETUP_CQSIZE", "IORING_SETUP_CLAMP",
    "IORING_SETUP_ATTACH_WQ", "IORING_SETUP_R_DISABLED",
    "IORING_SETUP_SUBMIT_ALL", "IORING_SETUP_COOP_TASKRUN",
    "IORING_SETUP_TASKRUN_FLAG", "IORING_SETUP_SQE128", "IORING_SETUP_CQE32",
    "IORING_SETUP_SINGLE_ISSUER", "IORING_SETUP_DEFER_TASKRUN",
    "IORING_SETUP_NO_MMAP", "IORING_SETUP_REGISTERED_FD_ONLY",
    "IORING_REGISTER_BUFFERS", "IORING_UNREGISTER_BUFFERS",
    "IORING_REGISTER_FILES", "IORING_UNREGISTER_FILES",
    "IORING_REGISTER_EVENTFD", "IORING_UNREGISTER_EVENTFD",
    "IORING_REGISTER_FILES_UPDATE", "IORING_REGISTER_EVENTFD_ASYNC",
    "IORING_REGISTER_PROBE", "IORING_REGISTER_PERSONALITY",
    "IORING_UNREGISTER_PERSONALITY", "IORING_REGISTER_RESTRICTIONS",
    "IORING_REGISTER_ENABLE_RINGS", "IORING_REGISTER_FILES2",
    "IORING_REGISTER_FILES_UPDATE2", "IORING_REGISTER_BUFFERS2",
    "IORING_REGISTER_BUFFERS_UPDATE", "IORING_REGISTER_IOWQ_AFF",
    "IORING_UNREGISTER_IOWQ_AFF", "IORING_REGISTER_IOWQ_MAX_WORKERS",
    "IORING_REGISTER_RING_FDS", "IORING_UNREGISTER_RING_FDS",
    "IORING_REGISTER_PBUF_RING", "IORING_UNREGISTER_PBUF_RING",
    "IORING_REGISTER_SYNC_CANCEL", "IORING_REGISTER_FILE_ALLOC_RANGE",
    "IORING_REGISTER_PBUF_STATUS", "IORING_REGISTER_NAPI",
    "IORING_UNREGISTER_NAPI", "IORING_REGISTER_CLOCK",
    "IORING_REGISTER_CLONE_BUFFERS", "IORING_REGISTER_ZCRX_IFQ",
    "IORING_REGISTER_RESIZE_RINGS")

# --- WSL Linux-side (public Microsoft WSL surface) ---
add("WSL", "wsl",
    "WslPath", "WslPathUnix", "WslPathWindows", "WslPathMixed",
    "WslInfo", "WslInfoNetworkingMode", "WslInfoVersion", "WslInfoVmId",
    "WslInfoNthreads", "WslInfoMsWsl",
    "WslVar", "WslEnv", "WslIsWsl", "WslDistro", "WslInterop",
    "WslInteropEnabled", "WslMountRoot", "WslDrvFs", "WslConf",
    "WslgDisplay", "WslgWayland", "WslgPulse",
    "WslInit", "WslInteropSocket", "WslLibPath",
    "WslLaunchWin32", "WslClip", "WslExplorer",
    "WslList", "WslExec", "WslShutdown", "WslTerminate",
    "WslStatus", "WslVersion", "WslSetVersion",
    "WslExport", "WslImport", "WslUnregister",
    "WslIsDistributionRegistered")

add("WSL", "wslpath",
    "wslpath")

add("WSL", "wslinfo",
    "wslinfo")

add("WSL", "wslvar",
    "wslvar")

add("WSL", "init",
    "/init", "WSLInterop", "WSLENV", "WSL_DISTRO_NAME", "WSL_INTEROP",
    "WSL2", "NAME", "DISPLAY", "WAYLAND_DISPLAY", "PULSE_SERVER",
    "WSL_INTEROP_IP")

# --- Nix CLI (nixos.org command-ref, legacy + nix 2.x) ---
add("Nix", "nix",
    "NixVersion", "NixRun", "NixBuild", "NixDevelop", "NixFlake",
    "NixProfile", "NixSearch", "NixRepl", "NixEval", "NixShell",
    "NixStore", "NixCopy", "NixEdit", "NixFmt", "NixFormatter",
    "NixLog", "NixPathInfo", "NixRegistry", "NixWhyDepends",
    "NixBundle", "NixConfig", "NixDaemon", "NixDerivation",
    "NixEnv", "NixHash", "NixKey", "NixNar", "NixPrintDevEnv",
    "NixUpgradeNix", "NixHelp", "NixHelpStores", "NixDoctor",
    "nix", "nix-build", "nix-shell", "nix-env", "nix-store",
    "nix-channel", "nix-collect-garbage", "nix-copy-closure",
    "nix-daemon", "nix-hash", "nix-instantiate", "nix-prefetch-url",
    "nix-prefetch-git", "nix --version", "nix build", "nix develop",
    "nix flake", "nix flake archive", "nix flake check", "nix flake clone",
    "nix flake init", "nix flake lock", "nix flake metadata",
    "nix flake new", "nix flake prefetch", "nix flake show",
    "nix flake update", "nix flake update-input",
    "nix profile", "nix profile install", "nix profile remove",
    "nix profile list", "nix profile upgrade", "nix profile rollback",
    "nix profile history", "nix profile diff-closures",
    "nix run", "nix search", "nix repl", "nix bundle", "nix copy",
    "nix edit", "nix eval", "nix fmt", "nix formatter", "nix log",
    "nix path-info", "nix registry", "nix registry list",
    "nix registry add", "nix registry remove", "nix registry pin",
    "nix why-depends", "nix config", "nix config show", "nix config check",
    "nix daemon", "nix derivation", "nix derivation show", "nix derivation add",
    "nix env", "nix env shell", "nix hash", "nix hash file", "nix hash path",
    "nix hash convert", "nix hash to-base16", "nix hash to-base32",
    "nix hash to-base64", "nix hash to-sri",
    "nix key", "nix key generate-secret", "nix key convert-secret-to-public",
    "nix nar", "nix nar cat", "nix nar dump-path", "nix nar ls", "nix nar pack",
    "nix print-dev-env", "nix store", "nix store gc", "nix store delete",
    "nix store dump-path", "nix store ping", "nix store repair",
    "nix store copy", "nix store verify", "nix store add", "nix store add-path",
    "nix store cat", "nix store diff-closures", "nix store make-content-addressed",
    "nix store optimise", "nix store prefetch-file", "nix store prefetch-tarball",
    "nix store path-from-hash-part", "nix store sign", "nix store copy-sigs",
    "nix upgrade-nix", "nix help", "nix help-stores",
    "nix-env -i", "nix-env -e", "nix-env -u", "nix-env -q",
    "nix-env --rollback", "nix-env --list-generations",
    "nix-store --gc", "nix-store --query", "nix-store --realise",
    "nix-store --delete", "nix-store --verify", "nix-store --optimise",
    "nix-store --dump", "nix-store --restore", "nix-store --export",
    "nix-store --import", "nix-store --add", "nix-store --read-log",
    "nix-channel --add", "nix-channel --remove", "nix-channel --list",
    "nix-channel --update", "nix-channel --rollback",
    "nix-build", "nix-instantiate", "nix-shell", "nix-prefetch-url")


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
        '#include "nix/catalog.h"',
        "",
        "// Curated Linux man-pages / POSIX slice plus WSL Linux-side and Nix.",
        "// Names follow syscalls(2), POSIX.1 libc, https://github.com/microsoft/WSL",
        "// and https://nixos.org/manual/nix/stable/command-ref/ — not a dump of",
        "// unistd.h or of every glibc export.",
        "// Query, filesystem, heap, and process (posix_spawn / std::thread child",
        "// on wasm; fork/exec on native Linux) run through wasmnix_call.",
        "// dlopen is a catalog module on wasm and libdl on native.",
        "// KVM / VFIO / vhost / MSHV (KVM_CREATE_VM, KVM_RUN, KvmEmulateIo)",
        "// is the same hop as WinHvPlatform / WinHvEmulation on win32.",
        "// Linux.Kernel.* is the ntoskrnl analog (kmalloc, kvm_read_guest).",
        "static const WasmNixApi kApis[] = {",
    ]
    for ns, lib, name in rows:
        ns_s = ns.replace("\\", "\\\\").replace('"', '\\"')
        lib_s = lib.replace("\\", "\\\\").replace('"', '\\"')
        name_s = name.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'    {{"{ns_s}", "{lib_s}", "{name_s}"}},')
    lines.append("};")
    lines.append("")
    lines.append("const WasmNixApi* wasmnix_catalog(int* count) {")
    lines.append("  if (count) *count = (int)(sizeof(kApis) / sizeof(kApis[0]));")
    lines.append("  return kApis;")
    lines.append("}")
    lines.append("")
    dest.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {dest} ({len(rows)} apis)")


if __name__ == "__main__":
    main()
