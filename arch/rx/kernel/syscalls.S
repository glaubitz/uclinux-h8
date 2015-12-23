#include <linux/sys.h>
#include <linux/linkage.h>

.globl syscall_table
	.section .text
	.align	2

syscall_table:
	.long	sys_restart_syscall		;0
	.long	sys_exit
	.long	sys_fork
	.long	sys_read
	.long	sys_write
	.long	sys_open
	.long	sys_close
	.long	sys_waitpid
	.long	sys_creat
	.long	sys_link
	.long	sys_unlink		;10
	.long	sys_execve
	.long	sys_chdir
	.long	sys_time
	.long	sys_mknod
	.long	sys_chmod
	.long	sys_lchown
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_lseek
	.long	sys_getpid		;20
	.long	sys_mount
	.long	sys_umount
	.long	sys_setuid
	.long	sys_getuid
	.long	sys_stime
	.long	sys_ptrace
	.long	sys_alarm
	.long	sys_ni_syscall
	.long	sys_pause
	.long	sys_utime		;30
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_access
	.long	sys_nice
	.long	sys_ni_syscall
	.long	sys_sync
	.long	sys_kill
	.long	sys_rename
	.long	sys_mkdir
	.long	sys_rmdir		;40
	.long	sys_dup
	.long	sys_pipe
	.long	sys_times
	.long	sys_ni_syscall
	.long	sys_brk
	.long	sys_setgid
	.long	sys_getgid
	.long	sys_signal
	.long	sys_geteuid
	.long	sys_getegid		;50
	.long	sys_acct
	.long	sys_umount
	.long	sys_ni_syscall
	.long	sys_ioctl
	.long	sys_fcntl
	.long	sys_ni_syscall
	.long	sys_setpgid
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_umask		;60
	.long	sys_chroot
	.long	sys_ustat
	.long	sys_dup2
	.long	sys_getppid
	.long	sys_getpgrp
	.long	sys_setsid
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_ni_syscall		;70
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_setrlimit
	.long	sys_getrlimit
	.long	sys_getrusage
	.long	sys_gettimeofday
	.long	sys_settimeofday
	.long	sys_getgroups		;80
	.long	sys_setgroups
	.long	sys_ni_syscall
	.long	sys_symlink
	.long	sys_ni_syscall
	.long	sys_readlink
	.long	sys_uselib
	.long	sys_swapon
	.long	sys_reboot
	.long	sys_ni_syscall
	.long	sys_mmap		;90
	.long	sys_munmap
	.long	sys_truncate
	.long	sys_ftruncate
	.long	sys_fchmod
	.long	sys_fchown
	.long	sys_getpriority
	.long	sys_setpriority
	.long	sys_ni_syscall
	.long	sys_statfs
	.long	sys_fstatfs		;100
	.long	sys_ni_syscall
	.long	sys_socketcall
	.long	sys_syslog
	.long	sys_setitimer
	.long	sys_getitimer
	.long	sys_newstat
	.long	sys_newlstat
	.long	sys_newfstat
	.long	sys_ni_syscall
	.long	sys_ni_syscall		;110
	.long	sys_vhangup
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_wait4
	.long	sys_swapoff
	.long	sys_sysinfo
	.long	sys_ipc
	.long	sys_fsync
	.long	sys_ni_syscall
	.long	sys_clone		;120
	.long	sys_setdomainname
	.long	sys_ni_syscall
	.long	sys_cacheflush
	.long	sys_adjtimex
	.long	sys_mprotect
	.long	sys_sigprocmask
	.long	sys_ni_syscall
	.long	sys_init_module
	.long	sys_delete_module
	.long	sys_ni_syscall		;130
	.long	sys_quotactl
	.long	sys_getpgid
	.long	sys_fchdir
	.long	sys_bdflush
	.long	sys_sysfs
	.long	sys_personality
	.long	sys_ni_syscall
	.long	sys_setfsuid
	.long	sys_setfsgid
	.long	sys_llseek		;140
	.long	sys_getdents
	.long	sys_select
	.long	sys_flock
	.long	sys_msync
	.long	sys_readv
	.long	sys_writev
	.long	sys_getsid
	.long	sys_fdatasync
	.long	sys_sysctl
	.long	sys_mlock		;150
	.long	sys_munlock
	.long	sys_mlockall
	.long	sys_munlockall
	.long	sys_sched_setparam
	.long	sys_sched_getparam
	.long	sys_sched_setscheduler
	.long	sys_sched_getscheduler
	.long	sys_sched_yield
	.long	sys_sched_get_priority_max
	.long	sys_sched_get_priority_min		;160
	.long	sys_sched_rr_get_interval
	.long	sys_nanosleep
	.long	sys_mremap
	.long	sys_setresuid
	.long	sys_getresuid
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_poll
	.long	sys_ni_syscall
	.long	sys_setresgid		;170
	.long	sys_getresgid
	.long	sys_prctl
	.long	sys_rt_sigreturn
	.long	sys_rt_sigaction
	.long	sys_rt_sigprocmask
	.long	sys_rt_sigpending
	.long	sys_rt_sigtimedwait
	.long	sys_rt_sigqueueinfo
	.long	sys_rt_sigsuspend
	.long	sys_pread64		;180
	.long	sys_pwrite64
	.long	sys_chown
	.long	sys_getcwd
	.long	sys_capget
	.long	sys_capset
	.long	sys_sigaltstack
	.long	sys_sendfile
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_vfork		;190
	.long	sys_getrlimit
	.long	sys_mmap2
	.long	sys_truncate64
	.long	sys_ftruncate64
	.long	sys_stat64
	.long	sys_lstat64
	.long	sys_fstat64
	.long	sys_lchown
	.long	sys_getuid
	.long	sys_getgid		;200
	.long	sys_geteuid
	.long	sys_getegid
	.long	sys_setreuid
	.long	sys_setregid
	.long	sys_getgroups
	.long	sys_setgroups
	.long	sys_fchown
	.long	sys_setresuid
	.long	sys_getresuid
	.long	sys_setresgid		;210
	.long	sys_getresgid
	.long	sys_chown
	.long	sys_setuid
	.long	sys_setgid
	.long	sys_setfsuid
	.long	sys_setfsgid
	.long	sys_pivot_root
	.long	sys_mincore
	.long	sys_madvise
; skip madvise1
	.long	sys_getdents64		;220
	.long	sys_fcntl64
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_gettid
	.long	sys_readahead
	.long	sys_setxattr
	.long	sys_lsetxattr
	.long	sys_fsetxattr
	.long	sys_getxattr
	.long	sys_lgetxattr		;230
	.long	sys_fgetxattr
	.long	sys_listxattr
	.long	sys_llistxattr
	.long	sys_flistxattr
	.long	sys_removexattr
	.long	sys_lremovexattr
	.long	sys_fremovexattr
	.long	sys_tkill
	.long	sys_sendfile64
	.long	sys_futex		;240
	.long	sys_sched_setaffinity
	.long	sys_sched_getaffinity
	.long	sys_ni_syscall
	.long	sys_ni_syscall
	.long	sys_io_setup
	.long	sys_io_destroy
	.long	sys_io_getevents
	.long	sys_io_submit
	.long	sys_io_cancel
	.long	sys_fadvise64		;250
	.long	sys_ni_syscall
	.long	sys_exit_group
	.long	sys_lookup_dcookie
	.long	sys_epoll_create
	.long	sys_epoll_ctl
	.long	sys_epoll_wait
	.long	sys_remap_file_pages
	.long	sys_set_tid_address
	.long	sys_timer_create
	.long	sys_timer_settime		;260
	.long	sys_timer_gettime
	.long	sys_timer_getoverrun
	.long	sys_timer_delete
	.long	sys_clock_settime
	.long	sys_clock_gettime
	.long	sys_clock_getres
	.long	sys_clock_nanosleep
	.long	sys_statfs64
	.long	sys_fstatfs64
	.long	sys_tgkill		;270
	.long	sys_utimes
	.long	sys_fadvise64_64
	.long	sys_ni_syscall
	.long	sys_mbind
	.long	sys_get_mempolicy
	.long	sys_set_mempolicy
	.long	sys_mq_open
	.long	sys_mq_unlink
	.long	sys_mq_timedsend
	.long	sys_mq_timedreceive		;280
	.long	sys_mq_notify
	.long	sys_mq_getsetattr
	.long	sys_kexec_load
	.long	sys_waitid
	.long	sys_ni_syscall
	.long	sys_add_key
	.long	sys_request_key
	.long	sys_keyctl
	.long	sys_ioprio_set
	.long	sys_ioprio_get		;290
	.long	sys_inotify_init
	.long	sys_inotify_add_watch
	.long	sys_inotify_rm_watch
	.long	sys_migrate_pages
	.long	sys_openat
	.long	sys_mkdirat
	.long	sys_mknodat
	.long	sys_fchownat
	.long	sys_futimesat
	.long	sys_fstatat64		;300
	.long	sys_unlinkat
	.long	sys_renameat
	.long	sys_linkat
	.long	sys_symlinkat
	.long	sys_readlinkat
	.long	sys_fchmodat
	.long	sys_faccessat
	.long	sys_pselect6
	.long	sys_ppoll
	.long	sys_unshare		;310
	.long	sys_set_robust_list
	.long	sys_get_robust_list
	.long	sys_splice
	.long	sys_sync_file_range
	.long	sys_tee
	.long	sys_vmsplice
	.long	sys_move_pages
	.long	sys_getcpu
	.long	sys_epoll_pwait
	.long	sys_utimensat		;320
	.long	sys_signalfd
	.long	sys_timerfd_create
	.long	sys_eventfd
	.long	sys_fallocate
	.long	sys_timerfd_settime
	.long	sys_timerfd_gettime
	.long	sys_signalfd4
	.long	sys_eventfd2
	.long	sys_epoll_create1
	.long	sys_dup3		;330
	.long	sys_pipe2
	.long	sys_inotify_init1
	.long	sys_preadv
	.long	sys_pwritev
	.long	sys_rt_tgsigqueueinfo
	.long	sys_perf_event_open

sys_vfork:
	add	#12,r0,r1
	bra	rx_vfork

sys_clone:
	add	#12,r0,r1
	bra	rx_clone

sys_sigaltstack:
	add	#12,r0,r3
	bra	rx_sigaltstack

sys_rt_sigreturn:	
	add	#12,r0,r1
	bra	rx_rt_sigreturn

	.end
