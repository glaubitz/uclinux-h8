#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/profile.h>
#include <linux/timex.h>
#include <linux/sched.h>
#include <linux/clockchips.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <asm/clock.h>

void __init rx_clk_init(void (*tick)(void));

/* common tick interrupt handler */
void rx_timer_tick(void)
{
	if (current->pid)
		profile_tick(CPU_PROFILING);
	update_process_times(user_mode(get_irq_regs()));
	xtime_update(1);
	update_process_times(user_mode(get_irq_regs()));
}

/* dummy RTC access */
void read_persistent_clock(struct timespec *tv)
{
	tv->tv_sec = mktime(2000, 1, 1, 0, 0, 0);
	tv->tv_nsec = 0;
}

int update_persistent_clock(struct timespec now)
{
	return 0;
}

void __init time_init(void)
{
	rx_clk_init(rx_timer_tick);
	clk_init();
}
