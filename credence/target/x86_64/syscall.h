/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include "assembly.h"  // for Instructions, Storage
#include "memory.h"    // for Memory_Accessor
#include <array>       // for array
#include <cstddef>     // for size_t
#include <deque>       // for deque
#include <map>         // for map
#include <stdint.h>    // for uint32_t
#include <string>      // for basic_string, string
#include <string_view> // for basic_string_view, string_view
#include <utility>     // for pair
#include <vector>      // for vector

namespace credence::target::x86_64::syscall_ns {

using Instructions = x86_64::assembly::Instructions;
using Register = x86_64::assembly::Register;

using syscall_t = std::array<std::size_t, 2>;
using syscall_list_t = std::map<std::string_view, syscall_t>;
using syscall_arguments_t = std::deque<assembly::Storage>;

namespace common {

// cppcheck-suppress constParameterReference
void exit_syscall(Instructions& instructions, int exit_status = 0);
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame = nullptr,
    memory::Memory_Access* accessor = nullptr);

} // namespace common

namespace linux_ns {

/**
 * @brief
 * On x64 Linux, you load the system call number into rax and arguments
 * into rdi, rsi, rdx, r10, r8, r9. Then you execute the syscall
 * instruction
 *
 */
const syscall_list_t syscall_list = {
    { "read",                    { 0, 3 }   },
    { "write",                   { 1, 3 }   },
    { "open",                    { 2, 3 }   },
    { "close",                   { 3, 1 }   },
    { "stat",                    { 4, 2 }   },
    { "fstat",                   { 5, 2 }   },
    { "lstat",                   { 6, 2 }   },
    { "poll",                    { 7, 3 }   },
    { "lseek",                   { 8, 3 }   },
    { "mmap",                    { 9, 6 }   },
    { "mprotect",                { 10, 3 }  },
    { "munmap",                  { 11, 2 }  },
    { "brk",                     { 12, 1 }  },
    { "rt_sigaction",            { 13, 4 }  },
    { "rt_sigprocmask",          { 14, 4 }  },
    { "rt_sigreturn",            { 15, 0 }  },
    { "ioctl",                   { 16, 3 }  },
    { "pread64",                 { 17, 4 }  },
    { "pwrite64",                { 18, 4 }  },
    { "readv",                   { 19, 3 }  },
    { "writev",                  { 20, 3 }  },
    { "access",                  { 21, 2 }  },
    { "pipe",                    { 22, 1 }  },
    { "select",                  { 23, 5 }  },
    { "sched_yield",             { 24, 0 }  },
    { "mremap",                  { 25, 5 }  },
    { "msync",                   { 26, 3 }  },
    { "mincore",                 { 27, 3 }  },
    { "madvise",                 { 28, 3 }  },
    { "shmget",                  { 29, 3 }  },
    { "shmat",                   { 30, 3 }  },
    { "shmctl",                  { 31, 3 }  },
    { "dup",                     { 32, 1 }  },
    { "dup2",                    { 33, 2 }  },
    { "pause",                   { 34, 0 }  },
    { "nanosleep",               { 35, 2 }  },
    { "getitimer",               { 36, 2 }  },
    { "alarm",                   { 37, 1 }  },
    { "setitimer",               { 38, 3 }  },
    { "getpid",                  { 39, 0 }  },
    { "sendfile",                { 40, 4 }  },
    { "socket",                  { 41, 3 }  },
    { "connect",                 { 42, 3 }  },
    { "accept",                  { 43, 3 }  },
    { "sendto",                  { 44, 6 }  },
    { "recvfrom",                { 45, 6 }  },
    { "sendmsg",                 { 46, 3 }  },
    { "recvmsg",                 { 47, 3 }  },
    { "shutdown",                { 48, 2 }  },
    { "bind",                    { 49, 3 }  },
    { "listen",                  { 50, 2 }  },
    { "getsockname",             { 51, 3 }  },
    { "getpeername",             { 52, 3 }  },
    { "socketpair",              { 53, 4 }  },
    { "setsockopt",              { 54, 5 }  },
    { "getsockopt",              { 55, 5 }  },
    { "clone",                   { 56, 5 }  },
    { "fork",                    { 57, 0 }  },
    { "vfork",                   { 58, 0 }  },
    { "execve",                  { 59, 3 }  },
    { "exit",                    { 60, 1 }  },
    { "wait4",                   { 61, 4 }  },
    { "kill",                    { 62, 2 }  },
    { "uname",                   { 63, 1 }  },
    { "semget",                  { 64, 3 }  },
    { "semop",                   { 65, 3 }  },
    { "semctl",                  { 66, 4 }  },
    { "shmdt",                   { 67, 1 }  },
    { "msgget",                  { 68, 2 }  },
    { "msgsnd",                  { 69, 4 }  },
    { "msgrcv",                  { 70, 5 }  },
    { "msgctl",                  { 71, 3 }  },
    { "fcntl",                   { 72, 3 }  },
    { "flock",                   { 73, 2 }  },
    { "fsync",                   { 74, 1 }  },
    { "fdatasync",               { 75, 1 }  },
    { "truncate",                { 76, 2 }  },
    { "ftruncate",               { 77, 2 }  },
    { "getdents",                { 78, 3 }  },
    { "getcwd",                  { 79, 2 }  },
    { "chdir",                   { 80, 1 }  },
    { "fchdir",                  { 81, 1 }  },
    { "rename",                  { 82, 2 }  },
    { "mkdir",                   { 83, 2 }  },
    { "rmdir",                   { 84, 1 }  },
    { "creat",                   { 85, 2 }  },
    { "link",                    { 86, 2 }  },
    { "unlink",                  { 87, 1 }  },
    { "symlink",                 { 88, 2 }  },
    { "readlink",                { 89, 3 }  },
    { "chmod",                   { 90, 2 }  },
    { "fchmod",                  { 91, 2 }  },
    { "chown",                   { 92, 3 }  },
    { "fchown",                  { 93, 3 }  },
    { "lchown",                  { 94, 3 }  },
    { "umask",                   { 95, 1 }  },
    { "gettimeofday",            { 96, 2 }  },
    { "getrlimit",               { 97, 2 }  },
    { "getrusage",               { 98, 2 }  },
    { "sysinfo",                 { 99, 1 }  },
    { "times",                   { 100, 1 } },
    { "ptrace",                  { 101, 4 } },
    { "getuid",                  { 102, 0 } },
    { "syslog",                  { 103, 3 } },
    { "getgid",                  { 104, 0 } },
    { "setuid",                  { 105, 1 } },
    { "setgid",                  { 106, 1 } },
    { "geteuid",                 { 107, 0 } },
    { "getegid",                 { 108, 0 } },
    { "setpgid",                 { 109, 2 } },
    { "getppid",                 { 110, 0 } },
    { "getpgrp",                 { 111, 0 } },
    { "setsid",                  { 112, 0 } },
    { "setreuid",                { 113, 2 } },
    { "setregid",                { 114, 2 } },
    { "getgroups",               { 115, 2 } },
    { "setgroups",               { 116, 2 } },
    { "setresuid",               { 117, 3 } },
    { "getresuid",               { 118, 3 } },
    { "setresgid",               { 119, 3 } },
    { "getresgid",               { 120, 3 } },
    { "getpgid",                 { 121, 1 } },
    { "setfsuid",                { 122, 1 } },
    { "setfsgid",                { 123, 1 } },
    { "getsid",                  { 124, 1 } },
    { "capget",                  { 125, 2 } },
    { "capset",                  { 126, 2 } },
    { "rt_sigpending",           { 127, 2 } },
    { "rt_sigtimedwait",         { 128, 4 } },
    { "rt_sigqueueinfo",         { 129, 3 } },
    { "rt_sigsuspend",           { 130, 2 } },
    { "sigaltstack",             { 131, 2 } },
    { "utime",                   { 132, 2 } },
    { "mknod",                   { 133, 3 } },
    { "uselib",                  { 134, 1 } },
    { "personality",             { 135, 1 } },
    { "ustat",                   { 136, 2 } },
    { "statfs",                  { 137, 2 } },
    { "fstatfs",                 { 138, 2 } },
    { "sysfs",                   { 139, 3 } },
    { "getpriority",             { 140, 2 } },
    { "setpriority",             { 141, 3 } },
    { "sched_setparam",          { 142, 2 } },
    { "sched_getparam",          { 143, 2 } },
    { "sched_setscheduler",      { 144, 3 } },
    { "sched_getscheduler",      { 145, 1 } },
    { "sched_get_priority_max",  { 146, 1 } },
    { "sched_get_priority_min",  { 147, 1 } },
    { "sched_rr_get_interval",   { 148, 2 } },
    { "mlock",                   { 149, 2 } },
    { "munlock",                 { 150, 2 } },
    { "mlockall",                { 151, 1 } },
    { "munlockall",              { 152, 0 } },
    { "vhangup",                 { 153, 0 } },
    { "modify_ldt",              { 154, 3 } },
    { "pivot_root",              { 155, 2 } },
    { "_sysctl",                 { 156, 1 } },
    { "prctl",                   { 157, 5 } },
    { "arch_prctl",              { 158, 2 } },
    { "adjtimex",                { 159, 1 } },
    { "setrlimit",               { 160, 2 } },
    { "chroot",                  { 161, 1 } },
    { "sync",                    { 162, 0 } },
    { "acct",                    { 163, 1 } },
    { "settimeofday",            { 164, 2 } },
    { "mount",                   { 165, 5 } },
    { "umount2",                 { 166, 2 } },
    { "swapon",                  { 167, 2 } },
    { "swapoff",                 { 168, 1 } },
    { "reboot",                  { 169, 4 } },
    { "sethostname",             { 170, 2 } },
    { "setdomainname",           { 171, 2 } },
    { "iopl",                    { 172, 1 } },
    { "ioperm",                  { 173, 3 } },
    { "create_module",           { 174, 2 } },
    { "init_module",             { 175, 3 } },
    { "delete_module",           { 176, 2 } },
    { "get_kernel_syms",         { 177, 1 } },
    { "query_module",            { 178, 5 } },
    { "quotactl",                { 179, 4 } },
    { "nfsservctl",              { 180, 3 } },
    { "getpmsg",                 { 181, 5 } },
    { "putpmsg",                 { 182, 5 } },
    { "afs_syscall",             { 183, 5 } },
    { "tuxcall",                 { 184, 3 } },
    { "security",                { 185, 3 } },
    { "gettid",                  { 186, 0 } },
    { "readahead",               { 187, 3 } },
    { "setxattr",                { 188, 5 } },
    { "lsetxattr",               { 189, 5 } },
    { "fsetxattr",               { 190, 5 } },
    { "getxattr",                { 191, 4 } },
    { "lgetxattr",               { 192, 4 } },
    { "fgetxattr",               { 193, 4 } },
    { "listxattr",               { 194, 3 } },
    { "llistxattr",              { 195, 3 } },
    { "flistxattr",              { 196, 3 } },
    { "removexattr",             { 197, 2 } },
    { "lremovexattr",            { 198, 2 } },
    { "fremovexattr",            { 199, 2 } },
    { "tkill",                   { 200, 2 } },
    { "time",                    { 201, 1 } },
    { "futex",                   { 202, 6 } },
    { "sched_setaffinity",       { 203, 3 } },
    { "sched_getaffinity",       { 204, 3 } },
    { "set_thread_area",         { 205, 1 } },
    { "io_setup",                { 206, 2 } },
    { "io_destroy",              { 207, 1 } },
    { "io_getevents",            { 208, 5 } },
    { "io_submit",               { 209, 3 } },
    { "io_cancel",               { 210, 3 } },
    { "get_thread_area",         { 211, 1 } },
    { "lookup_dcookie",          { 212, 5 } },
    { "epoll_create",            { 213, 1 } },
    { "epoll_ctl_old",           { 214, 4 } },
    { "epoll_wait_old",          { 215, 4 } },
    { "remap_file_pages",        { 216, 5 } },
    { "getdents64",              { 217, 3 } },
    { "set_tid_address",         { 218, 1 } },
    { "restart_syscall",         { 219, 0 } },
    { "semtimedop",              { 220, 4 } },
    { "fadvise64",               { 221, 4 } },
    { "timer_create",            { 222, 3 } },
    { "timer_settime",           { 223, 4 } },
    { "timer_gettime",           { 224, 2 } },
    { "timer_getoverrun",        { 225, 1 } },
    { "timer_delete",            { 226, 1 } },
    { "clock_settime",           { 227, 2 } },
    { "clock_gettime",           { 228, 2 } },
    { "clock_getres",            { 229, 2 } },
    { "clock_nanosleep",         { 230, 4 } },
    { "exit_group",              { 231, 1 } },
    { "epoll_wait",              { 232, 4 } },
    { "epoll_ctl",               { 233, 4 } },
    { "tgkill",                  { 234, 3 } },
    { "utimes",                  { 235, 2 } },
    { "vserver",                 { 236, 5 } },
    { "mbind",                   { 237, 6 } },
    { "set_mempolicy",           { 238, 3 } },
    { "get_mempolicy",           { 239, 5 } },
    { "mq_open",                 { 240, 4 } },
    { "mq_unlink",               { 241, 1 } },
    { "mq_timedsend",            { 242, 5 } },
    { "mq_timedreceive",         { 243, 5 } },
    { "mq_notify",               { 244, 2 } },
    { "mq_getsetattr",           { 245, 3 } },
    { "kexec_load",              { 246, 4 } },
    { "waitid",                  { 247, 4 } },
    { "add_key",                 { 248, 4 } },
    { "request_key",             { 249, 4 } },
    { "keyctl",                  { 250, 5 } },
    { "ioprio_set",              { 251, 3 } },
    { "ioprio_get",              { 252, 2 } },
    { "inotify_init",            { 253, 0 } },
    { "inotify_add_watch",       { 254, 3 } },
    { "inotify_rm_watch",        { 255, 2 } },
    { "migrate_pages",           { 256, 4 } },
    { "openat",                  { 257, 4 } },
    { "mkdirat",                 { 258, 3 } },
    { "mknodat",                 { 259, 4 } },
    { "fchownat",                { 260, 5 } },
    { "futimesat",               { 261, 3 } },
    { "newfstatat",              { 262, 4 } },
    { "unlinkat",                { 263, 3 } },
    { "renameat",                { 264, 4 } },
    { "linkat",                  { 265, 5 } },
    { "symlinkat",               { 266, 3 } },
    { "readlinkat",              { 267, 4 } },
    { "fchmodat",                { 268, 3 } },
    { "faccessat",               { 269, 3 } },
    { "pselect6",                { 270, 6 } },
    { "ppoll",                   { 271, 5 } },
    { "unshare",                 { 272, 1 } },
    { "set_robust_list",         { 273, 2 } },
    { "get_robust_list",         { 274, 3 } },
    { "splice",                  { 275, 6 } },
    { "tee",                     { 276, 4 } },
    { "sync_file_range",         { 277, 4 } },
    { "vmsplice",                { 278, 4 } },
    { "move_pages",              { 279, 6 } },
    { "utimensat",               { 280, 4 } },
    { "epoll_pwait",             { 281, 6 } },
    { "signalfd",                { 282, 3 } },
    { "timerfd_create",          { 283, 2 } },
    { "eventfd",                 { 284, 1 } },
    { "fallocate",               { 285, 4 } },
    { "timerfd_settime",         { 286, 4 } },
    { "timerfd_gettime",         { 287, 2 } },
    { "accept4",                 { 288, 4 } },
    { "signalfd4",               { 289, 4 } },
    { "eventfd2",                { 290, 2 } },
    { "epoll_create1",           { 291, 1 } },
    { "dup3",                    { 292, 3 } },
    { "pipe2",                   { 293, 2 } },
    { "inotify_init1",           { 294, 1 } },
    { "preadv",                  { 295, 4 } },
    { "pwritev",                 { 296, 4 } },
    { "rt_tgsigqueueinfo",       { 297, 4 } },
    { "perf_event_open",         { 298, 5 } },
    { "recvmmsg",                { 299, 5 } },
    { "fanotify_init",           { 300, 2 } },
    { "fanotify_mark",           { 301, 5 } },
    { "prlimit64",               { 302, 4 } },
    { "name_to_handle_at",       { 303, 5 } },
    { "open_by_handle_at",       { 304, 3 } },
    { "clock_adjtime",           { 305, 2 } },
    { "syncfs",                  { 306, 1 } },
    { "sendmmsg",                { 307, 4 } },
    { "setns",                   { 308, 2 } },
    { "getcpu",                  { 309, 3 } },
    { "process_vm_readv",        { 310, 6 } },
    { "process_vm_writev",       { 311, 6 } },
    { "kcmp",                    { 312, 5 } },
    { "finit_module",            { 313, 3 } },
    { "sched_setattr",           { 314, 3 } },
    { "sched_getattr",           { 315, 4 } },
    { "renameat2",               { 316, 5 } },
    { "seccomp",                 { 317, 3 } },
    { "getrandom",               { 318, 3 } },
    { "memfd_create",            { 319, 2 } },
    { "kexec_file_load",         { 320, 5 } },
    { "bpf",                     { 321, 3 } },
    { "execveat",                { 322, 4 } },
    { "userfaultfd",             { 323, 2 } },
    { "membarrier",              { 324, 1 } },
    { "mlock2",                  { 325, 3 } },
    { "copy_file_range",         { 326, 6 } },
    { "preadv2",                 { 327, 6 } },
    { "pwritev2",                { 328, 6 } },
    { "pkey_mprotect",           { 329, 4 } },
    { "pkey_alloc",              { 330, 2 } },
    { "pkey_free",               { 331, 1 } },
    { "statx",                   { 332, 5 } },
    { "io_pgetevents",           { 333, 4 } },
    { "rseq",                    { 334, 4 } },
    { "uretprobe",               { 335, 5 } },
    { "pidfd_send_signal",       { 424, 4 } },
    { "io_uring_setup",          { 425, 2 } },
    { "io_uring_enter",          { 426, 6 } },
    { "io_uring_register",       { 427, 4 } },
    { "open_tree",               { 428, 3 } },
    { "move_mount",              { 429, 5 } },
    { "fsopen",                  { 430, 2 } },
    { "fsconfig",                { 431, 5 } },
    { "fsmount",                 { 432, 3 } },
    { "fspick",                  { 433, 4 } },
    { "pidfd_open",              { 434, 2 } },
    { "clone3",                  { 435, 2 } },
    { "close_range",             { 436, 3 } },
    { "openat2",                 { 437, 4 } },
    { "pidfd_getfd",             { 438, 3 } },
    { "faccessat2",              { 439, 4 } },
    { "process_madvise",         { 440, 5 } },
    { "epoll_pwait2",            { 441, 5 } },
    { "mount_setattr",           { 442, 5 } },
    { "quotactl_fd",             { 443, 4 } },
    { "landlock_create_ruleset", { 444, 2 } },
    { "landlock_add_rule",       { 445, 4 } },
    { "landlock_restrict_self",  { 446, 1 } },
    { "memfd_secret",            { 447, 1 } },
    { "process_mrelease",        { 448, 3 } },
    { "futex_waitv",             { 449, 5 } },
    { "set_mempolicy_home_node", { 450, 5 } },
    { "cachestat",               { 451, 5 } },
    { "fchmodat2",               { 452, 4 } },
    { "map_shadow_stack",        { 453, 3 } },
    { "futex_wake",              { 454, 5 } },
    { "futex_wait",              { 455, 5 } },
    { "futex_requeue",           { 456, 5 } },
    { "statmount",               { 457, 4 } },
    { "listmount",               { 458, 4 } },
    { "lsm_get_self_attr",       { 459, 4 } },
    { "lsm_set_self_attr",       { 460, 4 } },
    { "lsm_list_modules",        { 461, 3 } },
    { "mseal",                   { 462, 4 } },
    { "setxattrat",              { 463, 6 } },
    { "getxattrat",              { 464, 5 } },
    { "listxattrat",             { 465, 4 } },
    { "removexattrat",           { 466, 4 } },
    { "open_tree_attr",          { 467, 5 } }
};
// clang-format on

void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame = nullptr,
    memory::Memory_Access* accessor = nullptr);

} // namespace linux

namespace bsd_ns {

constexpr uint32_t SYSCALL_CLASS_UNIX = 0x2000000;

/*
  clang-format off
 * (DARWIN) SYSCALL REGISTER CONVENTIONS (x86_64)
 * ------------------------------------------------------------------------------------------------------
 * REGISTER | PURPOSE / ROLE                | CONTENT DURING SYSCALL
 * ------------------------------------------------------------------------------------------------------
 * rax      | Syscall Class/Return Value    | 0x2000000 + Syscall Number (Set by libSystem wrapper)
 * rdi      | Argument 1                    | The first parameter
 * rsi      | Argument 2                    | The second parameter
 * rdx      | Argument 3                    | The third parameter
 * r10      | Argument 4                    | The fourth parameter
 * r8       | Argument 5                    | The five parameter
 * r9       | Argument 6                    | The sixth parameter
 * ------------------------------------------------------------------------------------------------------
 * NOTES:
 * - The Syscall instruction on x86_64 is used.
 * - Unlike Linux, the raw syscall number (rdi) is separate from the final kernel entry value (rax).
 * - Errors are indicated by the processor's Carry Flag (CF) being set after the syscall instruction returns.
  clang-format on
 */

const syscall_list_t syscall_list = {
    { "exit",        { 1, 1 }  },
    { "fork",        { 2, 0 }  },
    { "read",        { 3, 3 }  },
    { "write",       { 4, 3 }  },
    { "open",        { 5, 3 }  },
    { "close",       { 6, 1 }  },
    { "wait4",       { 7, 4 }  },
    { "mmap",        { 9, 6 }  },
    { "unlink",      { 10, 1 } },
    { "chdir",       { 12, 1 } },
    { "fchdir",      { 13, 1 } },
    { "chmod",       { 15, 2 } },
    { "chown",       { 16, 3 } },
    { "getpid",      { 20, 0 } },
    { "setuid",      { 23, 1 } },
    { "getuid",      { 24, 0 } },
    { "geteuid",     { 25, 0 } },
    { "ptrace",      { 26, 4 } },
    { "recvmsg",     { 27, 3 } },
    { "sendmsg",     { 28, 3 } },
    { "recvfrom",    { 29, 6 } },
    { "accept",      { 30, 3 } },
    { "getpeername", { 31, 3 } },
    { "getsockname", { 32, 3 } },
    { "access",      { 33, 2 } },
    { "kill",        { 37, 2 } },
    { "getppid",     { 39, 0 } },
    { "dup",         { 41, 1 } },
    { "pipe",        { 42, 1 } },
    { "getegid",     { 43, 0 } },
    { "sigaction",   { 46, 3 } },
    { "getgid",      { 47, 0 } },
    { "sigprocmask", { 48, 3 } },
    { "getlogin",    { 49, 1 } },
    { "setlogin",    { 50, 1 } },
    { "acct",        { 51, 1 } },
    { "sigpending",  { 52, 1 } },
    { "sigaltstack", { 53, 2 } },
    { "ioctl",       { 54, 3 } },
    { "reboot",      { 55, 3 } },
    { "revoke",      { 56, 1 } },
    { "symlink",     { 57, 2 } },
    { "readlink",    { 58, 3 } },
    { "execve",      { 59, 3 } },
    { "umask",       { 60, 1 } },
    { "chroot",      { 61, 1 } },
    { "msync",       { 65, 3 } },
    { "vfork",       { 66, 0 } },
    { "munmap",      { 73, 2 } },
    { "mprotect",    { 74, 3 } },
    { "madvise",     { 75, 3 } },
    { "mincore",     { 78, 3 } },
    { "getgroups",   { 79, 2 } },
    { "setgroups",   { 80, 2 } },
    { "setitimer",   { 83, 3 } },
    { "getitimer",   { 86, 2 } },
    { "dup2",        { 90, 2 } },
    { "fcntl",       { 92, 3 } },
    { "select",      { 93, 5 } },
    { "fsync",       { 95, 1 } },
    { "setpriority", { 96, 3 } },
    { "socket",      { 97, 3 } }
};

void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame = nullptr,
    memory::Memory_Access* accessor = nullptr);

} // namespace bsd

namespace common {

std::vector<std::string> get_platform_syscall_symbols();

inline syscall_ns::syscall_list_t get_syscall_list()
{
#if defined(CREDENCE_TEST) || defined(__linux__)
    return syscall_ns::linux_ns::syscall_list;
#elif defined(__APPLE__) || defined(__bsdi__)
    return syscall_ns::bsd_ns::syscall_list;
#else
    credence_error("Operating system not supported");
    return {};
#endif
}
} // namespace common

}