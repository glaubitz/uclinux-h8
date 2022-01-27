#include <linux/sched.h>
#include <linux/sched/task_stack.h>
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
restore_sigcontext(struct sigcontext __user *sc,
		   unsigned long *r1)
{
	struct pt_regs *regs = current_pt_regs();
	unsigned int err = 0;

	/* Always make any pending restarted system calls return -EINTR */
	current->restart_block.fn = do_no_restart_syscall;

#define COPY(x)		       err |= __get_user(regs->x, &sc->sc_##x)
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

asmlinkage int sys_rt_sigreturn(void)
{
	struct pt_regs *regs = current_pt_regs();
	struct rt_sigframe *frame = (struct rt_sigframe *)(regs->usp - 4);
	sigset_t set;
	unsigned long r1;

	if (!access_ok(frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	set_current_blocked(&set);

	if (restore_sigcontext(&frame->uc.uc_mcontext, &r1))
		goto badframe;

	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;

	return r1;

badframe:
	force_sig(SIGSEGV);
	return 0;
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

/*
 * Determine which stack to use..
 */
static inline void __user *
get_sigframe(struct ksignal *ksig, unsigned long sp, size_t frame_size)
{
	/* This is the X/Open sanctioned signal stack switching.  */
	if (ksig->ka.sa.sa_flags & SA_ONSTACK) {
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

static int setup_rt_frame(struct ksignal *ksig, sigset_t *set,
			  struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	int err = 0;

	frame = get_sigframe(ksig, regs->usp, sizeof(*frame));

	if (!access_ok(frame, sizeof(*frame)))
		return -EFAULT;

	if (ksig->ka.sa.sa_flags & SA_SIGINFO)
		err |= copy_siginfo_to_user(&frame->info, &ksig->info);

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user(current->sas_ss_sp, (u8 *)&frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->usp),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, set->sig[0]);
	err |= raw_copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		return -EFAULT;

	/* setup retcode */
	err |= __put_user(*((u64 *)&__rt_retcode), (u64 *)frame->retcode);
	err |= __put_user(frame->retcode, &(frame->pretcode));
	if (err)
		return -EFAULT;

	/* Set up registers for signal handler */
	regs->usp = (unsigned long)frame;
	regs->r[1] = ksig->sig;	/* Arg for signal handler */
	regs->r[2] = (unsigned long)&frame->info;
	regs->r[3] = (unsigned long)&frame->uc;
	regs->pc = (unsigned long)ksig->ka.sa.sa_handler;

	return 0 ;

}


static void
handle_restart(struct pt_regs *regs, struct k_sigaction *ka)
{
	/* check for system call restart.. */
	switch (regs->r[1]) {
	case -ERESTARTNOHAND:
		if (!ka)
			goto do_restart;
		regs->r[1] = -EINTR;
		break;
	case -ERESTART_RESTARTBLOCK:
		if (!ka) {
			regs->r[1] = __NR_restart_syscall;
			regs->pc -= 1;
		} else
			regs->r[1] = -EINTR;
		break;
	case -ERESTARTSYS:
		if (!(ka->sa.sa_flags & SA_RESTART)) {
			regs->r[1] = -EINTR;
			break;
		}
		fallthrough;
	case -ERESTARTNOINTR:
do_restart:
		regs->pc -= 1;
		break;
	}
}

static void
handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	sigset_t *oldset = sigmask_to_save();
	int ret;

	/* Are we from a system call? */
	if (regs->vec >= 0x1000)
		handle_restart(regs, &ksig->ka);

	ret = setup_rt_frame(ksig, oldset, regs);

	signal_setup_done(ret, ksig, 0);
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 */
static void do_signal(struct pt_regs *regs)
{
	struct ksignal ksig;

	if (get_signal(&ksig)) {
		/* Whee!  Actually deliver the signal.  */
		handle_signal(&ksig, regs);
		return;
	}
	/* Did we come from a system call? */
	if (regs->vec >= 0x1000)
		handle_restart(regs, NULL);

	/* If there's no signal to deliver, we just restore the saved mask.  */
	restore_saved_sigmask();
}

/*
 * notification of userspace execution resumption
 * - triggered by current->work.notify_resume
 */
asmlinkage void do_notify_resume(struct pt_regs *regs, u32 thread_info_flags)
{
	/* deal with pending signal delivery */
	if (thread_info_flags & _TIF_SIGPENDING)
		do_signal(regs);

	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
	}
}
