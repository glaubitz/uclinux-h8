#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/personality.h>
#include <linux/freezer.h>
#include <linux/tracehook.h>
#include <asm/cacheflush.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

struct rt_sigframe
{
	u8 __user *pretcode;
	struct siginfo info;
	struct ucontext uc;
	u8 retcode[8];
};

static int
restore_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc,
		   unsigned long *r1)
{
	unsigned int err = 0;

	/* Always make any pending restarted system calls return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

#define COPY(x)		err |= __get_user(regs->x, &sc->sc_##x)
	COPY(r[0]); COPY(r[1]);
	COPY(r[2]); COPY(r[3]);
	COPY(r[4]); COPY(r[5]);
	COPY(r[6]); COPY(r[7]);
	COPY(r[8]); COPY(r[9]);
	COPY(r[10]); COPY(r[11]);
	COPY(r[12]); COPY(r[13]);
	COPY(r[14]); COPY(r[15]);
	COPY(psw); COPY(pc);
	COPY(usp);
#undef COPY
	regs->vec = 0;
	*r1 = regs->r[1];
	return err;
}


static int
setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
	         unsigned long mask)
{
	int err = 0;

#define COPY(x)	err |= __put_user(regs->x, &sc->sc_##x)
	COPY(r[0]); COPY(r[1]);
	COPY(r[2]); COPY(r[3]);
	COPY(r[4]); COPY(r[5]);
	COPY(r[6]); COPY(r[7]);
	COPY(r[8]); COPY(r[9]);
	COPY(r[10]); COPY(r[11]);
	COPY(r[12]); COPY(r[13]);
	COPY(r[14]); COPY(r[15]);
	COPY(psw); COPY(pc);
	COPY(usp);
#undef COPY

	err |= __put_user(mask, &sc->sc_mask);

	return err;
}

asmlinkage long rx_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	unsigned long result;
	sigset_t set;

	frame = (struct rt_sigframe __user *)(regs->usp - 4);
	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext, &result))
		goto badframe;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL, regs->usp) == -EFAULT)
		goto badframe;

	return result;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int rx_sigaltstack(const stack_t *uss, stack_t *uoss, 
			      struct pt_regs *regs)
{
	return do_sigaltstack(uss, uoss, regs->usp);
}

/*
 * Determine which stack to use..
 */
static inline void __user *
get_sigframe(struct k_sigaction *ka, unsigned long sp, size_t frame_size)
{
	/* This is the X/Open sanctioned signal stack switching.  */
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (sas_ss_flags(sp) == 0)
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	return (void __user *)((sp - frame_size) & -8ul);
}

#define SIMM16(n) (n) & 0x00ff, ((n) >> 8) & 0x00ff
static const u8 __rt_retcode[8] = {
	0xfb, 0xfa, SIMM16(__NR_rt_sigreturn),	/* mov.l #__NR_rt_sigreturn,r15 */
	0x75, 0x60, 0x08, 			/* int #0x08 */
	0x03};					/* nop (padding) */

static void setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			   sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	int err = 0;
	int signal;

	frame = get_sigframe(ka, regs->usp, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	signal = current_thread_info()->exec_domain
		&& current_thread_info()->exec_domain->signal_invmap
		&& sig < 32
		? current_thread_info()->exec_domain->signal_invmap[sig]
		: sig;

	err |= copy_siginfo_to_user(&frame->info, info);
	if (err)
		goto give_sigsegv;

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user(current->sas_ss_sp, (u8 *)&frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->usp),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		goto give_sigsegv;

	/* setup retcode */
	err |= __put_user(*((u64 *)&__rt_retcode), (u64 *)frame->retcode);
	err |= __put_user(frame->retcode, &(frame->pretcode));
	if (err)
		goto give_sigsegv;

	/* Set up registers for signal handler */
	regs->usp = (unsigned long)frame;
	regs->r[1] = signal;	/* Arg for signal handler */
	regs->r[2] = (unsigned long)&frame->info;
	regs->r[3] = (unsigned long)&frame->uc;
	regs->pc = (unsigned long)ka->sa.sa_handler;

	set_fs(USER_DS);

#if 0
	printk("SIG deliver (%s:%d): sp=%p pc=%p\n",
		current->comm, current->pid, frame, regs->pc);
#endif

	return;

give_sigsegv:
	force_sigsegv(sig, current);
}

/*
 * OK, we're invoking a handler
 */

static int 
handle_signal(unsigned long sig, struct k_sigaction *ka, siginfo_t *info,
	      sigset_t *oldset, struct pt_regs *regs, unsigned long saved_r1)
{
	/* Are we from a system call? */
	if (regs->vec >= 0x1000) {
		/* check for system call restart.. */
		switch (regs->r[1]) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->r[1] = -EINTR;
			break;
			
		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->r[1] = -EINTR;
				break;
			}
			/* fallthrough */
		case -ERESTARTNOINTR:
			regs->r[1] = saved_r1;
			regs->pc -= 3;
			break;
		}
	}
	/* Set up the stack frame */
	setup_rt_frame(sig, ka, info, oldset, regs);

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	spin_lock_irq(&current->sighand->siglock);
	sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
	if (!(ka->sa.sa_flags & SA_NODEFER))
		sigaddset(&current->blocked,sig);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
	return 0;
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 */
static int do_signal(struct pt_regs *regs, unsigned long saved_r1)
{
	siginfo_t info;
	int signr;
	sigset_t *oldset;
	struct k_sigaction ka;

	/*
	 * We want the common case to go fast, which
	 * is why we may in certain cases get here from
	 * kernel mode. Just return without doing anything
	 * if so.
	 */
	if (!user_mode(regs))
		return 1;

	if (try_to_freeze()) 
		goto no_signal;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		/* Re-enable any watchpoints before delivering the
		 * signal to user space. The processor register will
		 * have been cleared if the watchpoint triggered
		 * inside the kernel.
		 */

		/* Whee!  Actually deliver the signal.  */
		handle_signal(signr, &ka, &info, oldset, regs, saved_r1);
		return 1;
	}

 no_signal:
	/* Did we come from a system call? */
	if (regs->vec >= 0x1000) {
		/* Restart the system call - no handlers present */
		if (regs->r[1] == -ERESTARTNOHAND ||
		    regs->r[1] == -ERESTARTSYS ||
		    regs->r[1] == -ERESTARTNOINTR) {
			regs->r[1] = saved_r1;
			regs->pc -= 3;
		}
		if (regs->r[1] == -ERESTART_RESTARTBLOCK){
			regs->r[1] = saved_r1;
			regs->r[8] = __NR_restart_syscall;
			regs->pc -=3;
		}
	}
	return 0;
}

/*
 * notification of userspace execution resumption
 * - triggered by current->work.notify_resume
 */
void do_notify_resume(struct pt_regs *regs, unsigned long thread_info_flags,
		      unsigned long saved_r1)
{
	/* deal with pending signal delivery */
	if (thread_info_flags & _TIF_SIGPENDING)
		do_signal(regs, saved_r1);

	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}
