#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/utsname.h>
#include <linux/ipc.h>
#include <linux/linkage.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>

/*
 * Do a system call from kernel instead of calling sys_execve so we
 * end up with proper pt_regs.
 */
int kernel_execve(const char *filename, const char *const argv[], const char *const envp[])
{
	register long __r15 __asm__ ("r15") = __NR_execve;
	register long __r1 __asm__ ("r1") = (long) filename;
	register long __r2 __asm__ ("r2") = (long) argv;
	register long __r3 __asm__ ("r3") = (long) envp;
	__asm__ __volatile__ ("int #0x08" : "=r" (__r1)
			: "r" (__r15), "r" (__r1), "r" (__r2), "r" (__r3)
			: "memory");
	return __r1;
}

asmlinkage int sys_cacheflush(unsigned long addr, unsigned long len, int op)
{
	/* Not affected and always success */
	return 0;
}

asmlinkage long sys_mmap(unsigned long addr, unsigned long len,
			  unsigned long prot, unsigned long flags,
			  unsigned long fd, unsigned long pgoff)
{
	if (pgoff & ~PAGE_MASK)
		return -EINVAL;

	return do_mmap_pgoff(NULL, addr, len, prot, flags, pgoff >> PAGE_SHIFT);
}

asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
			  unsigned long prot, unsigned long flags,
			  unsigned long fd, unsigned long pgoff)
{
	int error = -EBADF;
	struct file * file = NULL;

	/*
	 * The shift for mmap2 is constant, regardless of PAGE_SIZE
	 * setting.
	 */
	if (pgoff & ((1 << (PAGE_SHIFT - 12)) - 1))
		return -EINVAL;

	pgoff >>= PAGE_SHIFT - 12;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
out:
	return error;
}

