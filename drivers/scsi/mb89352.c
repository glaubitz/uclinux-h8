/*
 * MB89352 SPC driver
 */

#include <linux/module.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/printk.h>

#include "scsi.h"
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_spi.h>
#include <scsi/scsi_eh.h>
#include <asm/scsi_spc.h>

/* number of queueable commands
   (unless we support more than 1 cmd_per_lun this should do) */
#define SPC_MAXQUEUE 7

#define BDID (0x01)
#define SCTL (0x03)
#define SCMD (0x05)
#define INTS (0x09)
#define PSNS (0x0b)
#define SDGC (0x0b)
#define SSTS (0x0d)
#define SERR (0x0f)
#define PCTL (0x11)
#define MBC  (0x13)
#define DREG (0x15)
#define TEMP (0x17)
#define TCH  (0x19)
#define TCM  (0x1b)
#define TCL  (0x1d)

/* Some macros to manipulate ports and their bits */

#define SETPORT(VAL, PORT)	outb( (VAL), shpnt->base + (PORT) )

#define GETPORT(PORT)		inb( shpnt->base + PORT )


/* DEFINES */

#define	DO_LOCK(flags)		spin_lock_irqsave(&QLOCK,flags)
#define	DO_UNLOCK(flags)	spin_unlock_irqrestore(&QLOCK,flags)

#define LEAD		"(scsi%d:%d:%d) "
#define INFO_LEAD	KERN_INFO	LEAD
#define CMDINFO(cmd) \
			(cmd) ? ((cmd)->device->host->host_no) : -1, \
                        (cmd) ? ((cmd)->device->id & 0x0f) : -1, \
			(cmd) ? ((u8)(cmd)->device->lun & 0x07) : -1

/* SCSI signal IN */
#define SIG_IOI          0x01
#define SIG_CDI          0x02
#define SIG_MSGI         0x04
#define SIG_BSYI         0x08
#define SIG_SELI         0x10
#define SIG_ATNI         0x20
#define SIG_ACKI         0x40
#define SIG_REQI         0x80

/* SCSI Phases */
#define P_MASK       (SIG_MSGI|SIG_CDI|SIG_IOI)
#define P_DATAO      (0)
#define P_DATAI      (SIG_IOI)
#define P_CMD        (SIG_CDI)
#define P_STATUS     (SIG_CDI|SIG_IOI)
#define P_MSGO       (SIG_MSGI|SIG_CDI)
#define P_MSGI       (SIG_MSGI|SIG_CDI|SIG_IOI)

#define SELTIME 128
#define SELSTART 3

static inline void
CMD_INC_RESID(struct scsi_cmnd *cmd, int inc)
{
	scsi_set_resid(cmd, scsi_get_resid(cmd) + inc);
}

#define DELAY_DEFAULT 1000

enum {
	not_issued	= 0x0001,	/* command not yet issued */
	selecting	= 0x0002,	/* target is being selected */
	identified	= 0x0004,	/* IDENTIFY was sent */
	disconnected	= 0x0008,	/* target disconnected */
	completed	= 0x0010,	/* target sent COMMAND COMPLETE */
	aborted		= 0x0020,	/* ABORT was sent */
	resetted	= 0x0040,	/* BUS DEVICE RESET was sent */
	spiordy		= 0x0080,	/* waiting for SPIORDY to raise */
	aborting	= 0x0200,	/* ABORT is pending */
	resetting	= 0x0400,	/* BUS DEVICE RESET is pending */
	check_condition = 0x0800,	/* requesting sense after CHECK CONDITION */
};

MODULE_AUTHOR("Yoshinori Sato <ysato@users.sourceforge.jp>");
MODULE_DESCRIPTION("MB89352 SCSI driver");
MODULE_LICENSE("GPL");

static struct scsi_host_template spc_driver_template;

/*
 * internal states of the host
 *
 */
enum spc_state {
	idle=0,
	unknown,
	seldo,
	seldi,
	selto,
	busfree,
	msgo,
	cmd,
	msgi,
	status,
	datai,
	datao,
	parerr,
	rsti,
	maxstate
};

/*
 * current state information of the host
 *
 */
struct spc_hostdata {
	struct scsi_cmnd *issue_SC;
		/* pending commands to issue */

	struct scsi_cmnd *current_SC;
		/* current command on the bus */

	struct scsi_cmnd *disconnected_SC;
		/* commands that disconnected */

	struct scsi_cmnd *done_SC;
		/* command that was completed */

	spinlock_t lock;
		/* host lock */

#if defined(SPC_STAT)
	int	      total_commands;
	int	      disconnections;
	int	      busfree_without_any_action;
	int	      busfree_without_old_command;
	int	      busfree_without_new_command;
	int	      busfree_without_done_command;
	int	      busfree_with_check_condition;
	int	      count[maxstate];
	int	      count_trans[maxstate];
	unsigned long time[maxstate];
#endif

	int commands;		/* current number of commands */

	int reconnect;		/* disconnection allowed */
	int parity;		/* parity checking enabled */
	int delay;		/* reset out delay */
	int ext_trans;		/* extended translation enabled */

	int service;		/* bh needs to be run */
	int in_intr;		/* bh is running */

	/* current state,
	   previous state,
	   last state different from current state */
	enum spc_state state, prevstate, laststate;

	int target;
		/* reconnecting target */

	int cmd_i;
		/* number of sent bytes of current command */

	int msgi_len;
		/* number of received message bytes */
	unsigned char msgi[256];
		/* received message bytes */

	int msgo_i, msgo_len;
		/* number of sent bytes and length of current messages */
	unsigned char msgo[256];
		/* pending messages */

	int data_len;
		/* number of sent/received bytes in dataphase */
	struct device dev;
	struct work_struct wq;
	struct Scsi_Host *host;
	int sctl;
	int ints;
};


/*
 * host specific command extension
 *
 */
struct spc_scdata {
	struct scsi_cmnd *next;	/* next sc in queue */
	struct completion *done;/* semaphore to block on */
	struct scsi_eh_save ses;
};

/* access macros for hostdata */

#define HOSTDATA(shpnt)		((struct spc_hostdata *) &shpnt->hostdata)

#define CURRENT_SC		(HOSTDATA(shpnt)->current_SC)
#define DONE_SC			(HOSTDATA(shpnt)->done_SC)
#define ISSUE_SC		(HOSTDATA(shpnt)->issue_SC)
#define DISCONNECTED_SC		(HOSTDATA(shpnt)->disconnected_SC)
#define QLOCK			(HOSTDATA(shpnt)->lock)
#define QLOCKER			(HOSTDATA(shpnt)->locker)
#define QLOCKERL		(HOSTDATA(shpnt)->lockerl)

#define STATE			(HOSTDATA(shpnt)->state)
#define PREVSTATE		(HOSTDATA(shpnt)->prevstate)
#define LASTSTATE		(HOSTDATA(shpnt)->laststate)

#define RECONN_TARGET		(HOSTDATA(shpnt)->target)

#define CMD_I			(HOSTDATA(shpnt)->cmd_i)

#define MSGO(i)			(HOSTDATA(shpnt)->msgo[i])
#define MSGO_I			(HOSTDATA(shpnt)->msgo_i)
#define MSGOLEN			(HOSTDATA(shpnt)->msgo_len)
#define ADDMSGO(x)		(MSGOLEN<256 ? (void)(MSGO(MSGOLEN++)=x) : spc_error(shpnt,"MSGO overflow"))

#define MSGI(i)			(HOSTDATA(shpnt)->msgi[i])
#define MSGILEN			(HOSTDATA(shpnt)->msgi_len)
#define ADDMSGI(x)		(MSGILEN<256 ? (void)(MSGI(MSGILEN++)=x) : spc_error(shpnt,"MSGI overflow"))

#define DATA_LEN		(HOSTDATA(shpnt)->data_len)

#define DELAY			(HOSTDATA(shpnt)->delay)
#define EXT_TRANS		(HOSTDATA(shpnt)->ext_trans)
#define RECONNECT		(HOSTDATA(shpnt)->reconnect)
#define PARITY			(HOSTDATA(shpnt)->parity)

#define HOSTIOPORT		(HOSTDATA(shpnt)->start)

#define SCDATA(SCpnt)		((struct spc_scdata *) (SCpnt)->host_scribble)
#define SCNEXT(SCpnt)		SCDATA(SCpnt)->next
#define SCSEM(SCpnt)		SCDATA(SCpnt)->done

#define SG_ADDRESS(buffer)	((char *) sg_virt((buffer)))

/* state handling */
static void seldi_run(struct Scsi_Host *shpnt);
static void seldo_run(struct Scsi_Host *shpnt);
static void selto_run(struct Scsi_Host *shpnt);
static void busfree_run(struct Scsi_Host *shpnt);

static void msgo_init(struct Scsi_Host *shpnt);
static void msgo_run(struct Scsi_Host *shpnt);
static void msgo_end(struct Scsi_Host *shpnt);

static void cmd_init(struct Scsi_Host *shpnt);
static void cmd_run(struct Scsi_Host *shpnt);
static void cmd_end(struct Scsi_Host *shpnt);

static void datai_init(struct Scsi_Host *shpnt);
static void datai_run(struct Scsi_Host *shpnt);
static void datai_end(struct Scsi_Host *shpnt);

static void datao_init(struct Scsi_Host *shpnt);
static void datao_run(struct Scsi_Host *shpnt);
static void datao_end(struct Scsi_Host *shpnt);

static void status_run(struct Scsi_Host *shpnt);

static void msgi_run(struct Scsi_Host *shpnt);
static void msgi_end(struct Scsi_Host *shpnt);

static void parerr_run(struct Scsi_Host *shpnt);
static void rsti_run(struct Scsi_Host *shpnt);

static void is_complete(struct Scsi_Host *shpnt);

/*
 * driver states
 *
 */
static struct {
	char		*name;
	void		(*init)(struct Scsi_Host *);
	void		(*run)(struct Scsi_Host *);
	void		(*end)(struct Scsi_Host *);
	int		spio;
} states[] = {
	{ "idle",	NULL,		NULL,		NULL,		0},
	{ "unknown",	NULL,		NULL,		NULL,		0},
	{ "seldo",	NULL,		seldo_run,	NULL,		0},
	{ "seldi",	NULL,		seldi_run,	NULL,		0},
	{ "selto",	NULL,		selto_run,	NULL,		0},
	{ "busfree",	NULL,		busfree_run,	NULL,		0},
	{ "msgo",	msgo_init,	msgo_run,	msgo_end,	1},
	{ "cmd",	cmd_init,	cmd_run,	cmd_end,	1},
	{ "msgi",	NULL,		msgi_run,	msgi_end,	1},
	{ "status",	NULL,		status_run,	NULL,		1},
	{ "datai",	datai_init,	datai_run,	datai_end,	0},
	{ "datao",	datao_init,	datao_run,	datao_end,	0},
	{ "parerr",	NULL,		parerr_run,	NULL,		0},
	{ "rsti",	NULL,		rsti_run,	NULL,		0},
};

/* setup & interrupt */
static irqreturn_t spc_intr(int irq, void *dev_id);
static void spc_error(struct Scsi_Host *shpnt, char *msg);
static void done(struct Scsi_Host *shpnt, int error);

/* diagnostics */
static void show_command(struct scsi_cmnd * ptr);
static void show_queues(struct Scsi_Host *shpnt);


/*
 *  queue services:
 *
 */
static inline void append_SC(struct scsi_cmnd **SC, struct scsi_cmnd *new_SC)
{
	struct scsi_cmnd *end;

	SCNEXT(new_SC) = NULL;
	if (!*SC)
		*SC = new_SC;
	else {
		for (end = *SC; SCNEXT(end); end = SCNEXT(end))
			;
		SCNEXT(end) = new_SC;
	}
}

static inline struct scsi_cmnd *remove_first_SC(struct scsi_cmnd ** SC)
{
	struct scsi_cmnd *ptr;

	ptr = *SC;
	if (ptr) {
		*SC = SCNEXT(*SC);
		SCNEXT(ptr)=NULL;
	}
	return ptr;
}

static inline struct scsi_cmnd *remove_lun_SC(struct scsi_cmnd ** SC, int target, int lun)
{
	struct scsi_cmnd *ptr, *prev;

	for (ptr = *SC, prev = NULL;
	     ptr && ((ptr->device->id != target) || (ptr->device->lun != lun));
	     prev = ptr, ptr = SCNEXT(ptr))
	     ;

	if (ptr) {
		if (prev)
			SCNEXT(prev) = SCNEXT(ptr);
		else
			*SC = SCNEXT(ptr);

		SCNEXT(ptr)=NULL;
	}

	return ptr;
}

static inline struct scsi_cmnd *remove_SC(struct scsi_cmnd **SC, struct scsi_cmnd *SCp)
{
	struct scsi_cmnd *ptr, *prev;

	for (ptr = *SC, prev = NULL;
	     ptr && SCp!=ptr;
	     prev = ptr, ptr = SCNEXT(ptr))
	     ;

	if (ptr) {
		if (prev)
			SCNEXT(prev) = SCNEXT(ptr);
		else
			*SC = SCNEXT(ptr);

		SCNEXT(ptr)=NULL;
	}

	return ptr;
}

static void run(struct work_struct *work);
/*
 *  Queue a command and setup interrupts for a free bus.
 */
static int spc_internal_queue(struct scsi_cmnd *SCpnt, struct completion *complete,
		int phase, void (*done)(struct scsi_cmnd *))
{
	struct Scsi_Host *shpnt = SCpnt->device->host;
	unsigned long flags;

	SCpnt->scsi_done	= done;
	SCpnt->SCp.phase	= not_issued | phase;
	SCpnt->SCp.Status	= 0x1; /* Ilegal status by SCSI standard */
	SCpnt->SCp.Message	= 0;
	SCpnt->SCp.have_data_in	= 0;
	SCpnt->SCp.sent_command	= 0;

	if(SCpnt->SCp.phase & (resetting|check_condition)) {
		if (!SCpnt->host_scribble || SCSEM(SCpnt) || SCNEXT(SCpnt)) {
			scmd_printk(KERN_ERR, SCpnt, "cannot reuse command\n");
			return FAILED;
		}
	} else {
		SCpnt->host_scribble = kmalloc(sizeof(struct spc_scdata), GFP_ATOMIC);
		if(!SCpnt->host_scribble) {
			scmd_printk(KERN_ERR, SCpnt, "allocation failed\n");
			return FAILED;
		}
	}

	SCNEXT(SCpnt)		= NULL;
	SCSEM(SCpnt)		= complete;

	/* setup scratch area
	   SCp.ptr              : buffer pointer
	   SCp.this_residual    : buffer length
	   SCp.buffer           : next buffer
	   SCp.buffers_residual : left buffers in list
	   SCp.phase            : current state of the command */

	if ((phase & resetting) || !scsi_sglist(SCpnt)) {
		SCpnt->SCp.ptr           = NULL;
		SCpnt->SCp.this_residual = 0;
		scsi_set_resid(SCpnt, 0);
		SCpnt->SCp.buffer           = NULL;
		SCpnt->SCp.buffers_residual = 0;
	} else {
		scsi_set_resid(SCpnt, scsi_bufflen(SCpnt));
		SCpnt->SCp.buffer           = scsi_sglist(SCpnt);
		SCpnt->SCp.ptr              = SG_ADDRESS(SCpnt->SCp.buffer);
		SCpnt->SCp.this_residual    = SCpnt->SCp.buffer->length;
		SCpnt->SCp.buffers_residual = scsi_sg_count(SCpnt) - 1;
	}

	DO_LOCK(flags);

#if defined(SPC_STAT)
	HOSTDATA(shpnt)->total_commands++;
#endif

	HOSTDATA(shpnt)->commands++;

	append_SC(&ISSUE_SC, SCpnt);

	DO_UNLOCK(flags);
	SETPORT(GETPORT(SCTL) | 0x01, SCTL);
	if(!HOSTDATA(shpnt)->in_intr)
		busfree_run(shpnt);

	return 0;
}

/*
 *  queue a command
 *
 */
static int spc_queue_lck(struct scsi_cmnd *SCpnt, void (*done)(struct scsi_cmnd *))
{
	return spc_internal_queue(SCpnt, NULL, 0, done);
}

static DEF_SCSI_QCMD(spc_queue)

/*
 *
 */
static void reset_done(struct scsi_cmnd *SCpnt)
{
	if(SCSEM(SCpnt)) {
		complete(SCSEM(SCpnt));
	} else {
		printk(KERN_ERR "spc: reset_done w/o completion\n");
	}
}

/*
 *  Abort a command
 *
 */
static int spc_abort(struct scsi_cmnd *SCpnt)
{
	struct Scsi_Host *shpnt = SCpnt->device->host;
	struct scsi_cmnd *ptr;
	unsigned long flags;

	DO_LOCK(flags);

	ptr=remove_SC(&ISSUE_SC, SCpnt);

	if(ptr) {
		HOSTDATA(shpnt)->commands--;
		DO_UNLOCK(flags);

		kfree(SCpnt->host_scribble);
		SCpnt->host_scribble=NULL;

	}
	SETPORT(0x40, SCTL);
	SETPORT(HOSTDATA(shpnt)->sctl, SCTL);

	DO_UNLOCK(flags);

	/*
	 * FIXME:
	 * for current command: queue ABORT for message out and raise ATN
	 * for disconnected command: pseudo SC with ABORT message or ABORT on reselection?
	 *
	 */

	return SUCCESS;
}

/*
 * Reset a device
 *
 */
static int spc_device_reset(struct scsi_cmnd * SCpnt)
{
	struct Scsi_Host *shpnt = SCpnt->device->host;
	DECLARE_COMPLETION(done);
	int ret, issued, disconnected;
	unsigned char old_cmd_len = SCpnt->cmd_len;
	unsigned long flags;
	unsigned long timeleft;

	if(CURRENT_SC==SCpnt) {
		scmd_printk(KERN_ERR, SCpnt, "cannot reset current device\n");
		return FAILED;
	}

	DO_LOCK(flags);
	issued       = remove_SC(&ISSUE_SC, SCpnt) == NULL;
	disconnected = issued && remove_SC(&DISCONNECTED_SC, SCpnt);
	DO_UNLOCK(flags);

	SCpnt->cmd_len         = 0;

	spc_internal_queue(SCpnt, &done, resetting, reset_done);

	timeleft = wait_for_completion_timeout(&done, 100*HZ);
	if (!timeleft) {
		/* remove command from issue queue */
		DO_LOCK(flags);
		remove_SC(&ISSUE_SC, SCpnt);
		DO_UNLOCK(flags);
	}

	SCpnt->cmd_len         = old_cmd_len;

	DO_LOCK(flags);

	if(SCpnt->SCp.phase & resetted) {
		HOSTDATA(shpnt)->commands--;
		kfree(SCpnt->host_scribble);
		SCpnt->host_scribble=NULL;

		ret = SUCCESS;
	} else {
		/* requeue */
		if(!issued) {
			append_SC(&ISSUE_SC, SCpnt);
		} else if(disconnected) {
			append_SC(&DISCONNECTED_SC, SCpnt);
		}

		ret = FAILED;
	}

	DO_UNLOCK(flags);
	return ret;
}

static void free_hard_reset_SCs(struct Scsi_Host *shpnt, struct scsi_cmnd **SCs)
{
	struct scsi_cmnd *ptr;

	ptr=*SCs;
	while(ptr) {
		struct scsi_cmnd *next;

		if(SCDATA(ptr)) {
			next = SCNEXT(ptr);
		} else {
			scmd_printk(KERN_DEBUG, ptr,
				    "queue corrupted at %p\n", ptr);
			next = NULL;
		}

		if (!ptr->device->soft_reset) {
			remove_SC(SCs, ptr);
			HOSTDATA(shpnt)->commands--;
			kfree(ptr->host_scribble);
			ptr->host_scribble=NULL;
		}

		ptr = next;
	}
}

/*
 * Reset the bus
 *
 */
static int spc_bus_reset_host(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *shpnt = cmd->device->host;
	unsigned long flags;

	DO_LOCK(flags);

	free_hard_reset_SCs(shpnt, &ISSUE_SC);
	free_hard_reset_SCs(shpnt, &DISCONNECTED_SC);

	SETPORT(0x00, SCTL);
	SETPORT(0x10, SCMD);
	mdelay(256);
	SETPORT(0x00, SCMD);
	mdelay(256);
	SETPORT(0x01, SCTL);
	DO_UNLOCK(flags);

	return SUCCESS;
}

/*
 * Reset the host (bus and controller)
 *
 */
static int spc_host_reset_host(struct Scsi_Host *shpnt)
{
	SETPORT(0x40, SCTL);
	SETPORT(0x01, SCTL);

	return SUCCESS;
}

/*
 * Reset the host (bus and controller)
 *
 */
static int spc_host_reset(struct scsi_cmnd *SCpnt)
{
	spc_host_reset_host(SCpnt->device->host);
	spc_bus_reset_host(SCpnt);
	return SUCCESS;
}

/*
 * Return the "logical geometry"
 *
 */
static int spc_biosparam(struct scsi_device *sdev, struct block_device *bdev,
		sector_t capacity, int *info_array)
{
	info_array[0] = 64;
	info_array[1] = 32;
	info_array[2] = (unsigned long)capacity / (64 * 32);

	return 0;
}

/*
 *  Internal done function
 *
 */
static void done(struct Scsi_Host *shpnt, int error)
{
	if (CURRENT_SC) {
		if(DONE_SC)
			scmd_printk(KERN_ERR, CURRENT_SC,
				    "there's already a completed command %p "
				    "- will cause abort\n", DONE_SC);

		DONE_SC = CURRENT_SC;
		CURRENT_SC = NULL;
		DONE_SC->result = error;
	} else
		printk(KERN_ERR "spc: done() called outside of command\n");
}

/*
 * Run service completions on the card with interrupts enabled.
 *
 */
static void run(struct work_struct *work)
{
	struct spc_hostdata *hd;
	
	hd = container_of(work, struct spc_hostdata, wq);
	is_complete(hd->host);
}

/*
 * Interrupt handler
 *
 */
static irqreturn_t spc_intr(int irqno, void *dev_id)
{
	struct Scsi_Host *shpnt = dev_id;
	unsigned long flags;

	DO_LOCK(flags);
	SETPORT(GETPORT(SCTL) & 0xfe, SCTL);
	if( HOSTDATA(shpnt)->service==0) {
		HOSTDATA(shpnt)->service=1;
		HOSTDATA(shpnt)->ints = GETPORT(INTS);
		SETPORT(HOSTDATA(shpnt)->ints, INTS);
		
		/* Poke the BH handler */
		INIT_WORK(&HOSTDATA(shpnt)->wq, run);
		schedule_work(&HOSTDATA(shpnt)->wq);
	}
	DO_UNLOCK(flags);

	return IRQ_HANDLED;
}

/*
 * busfree phase
 * - handle completition/disconnection/error of current command
 * - start selection for next command (if any)
 */
static void busfree_run(struct Scsi_Host *shpnt)
{
	unsigned long flags;
#if defined(SPC_STAT)
	int action=0;
#endif

	if(CURRENT_SC) {
#if defined(SPC_STAT)
		action++;
#endif
		if(CURRENT_SC->SCp.phase & completed) {
			/* target sent COMMAND COMPLETE */
			done(shpnt, (CURRENT_SC->SCp.Status & 0xff) | ((CURRENT_SC->SCp.Message & 0xff) << 8) | (DID_OK << 16));

		} else if(CURRENT_SC->SCp.phase & aborted) {
			done(shpnt, (CURRENT_SC->SCp.Status & 0xff) | ((CURRENT_SC->SCp.Message & 0xff) << 8) | (DID_ABORT << 16));

		} else if(CURRENT_SC->SCp.phase & resetted) {
			done(shpnt, (CURRENT_SC->SCp.Status & 0xff) | ((CURRENT_SC->SCp.Message & 0xff) << 8) | (DID_RESET << 16));

		} else if(CURRENT_SC->SCp.phase & disconnected) {
			/* target sent DISCONNECT */
#if defined(SPC_STAT)
			HOSTDATA(shpnt)->disconnections++;
#endif
			append_SC(&DISCONNECTED_SC, CURRENT_SC);
			CURRENT_SC->SCp.phase |= 1 << 16;
			CURRENT_SC = NULL;

		} else {
			done(shpnt, DID_ERROR << 16);
		}
#if defined(SPC_STAT)
	} else {
		HOSTDATA(shpnt)->busfree_without_old_command++;
#endif
	}

	DO_LOCK(flags);

	if(DONE_SC) {
#if defined(SPC_STAT)
		action++;
#endif

		if(DONE_SC->SCp.phase & check_condition) {
			struct scsi_cmnd *cmd = HOSTDATA(shpnt)->done_SC;
			struct spc_scdata *sc = SCDATA(cmd);

			scsi_eh_restore_cmnd(cmd, &sc->ses);

			cmd->SCp.Status = SAM_STAT_CHECK_CONDITION;

			HOSTDATA(shpnt)->commands--;
		} else if(DONE_SC->SCp.Status==SAM_STAT_CHECK_CONDITION) {
#if defined(SPC_STAT)
			HOSTDATA(shpnt)->busfree_with_check_condition++;
#endif

			if(!(DONE_SC->SCp.phase & not_issued)) {
				struct spc_scdata *sc;
				struct scsi_cmnd *ptr = DONE_SC;
				DONE_SC=NULL;

				sc = SCDATA(ptr);
				/* It was allocated in spc_internal_queue? */
				BUG_ON(!sc);
				scsi_eh_prep_cmnd(ptr, &sc->ses, NULL, 0, ~0);

				DO_UNLOCK(flags);
				spc_internal_queue(ptr, NULL, check_condition, ptr->scsi_done);
				DO_LOCK(flags);
			}
		}

		if(DONE_SC && DONE_SC->scsi_done) {
			struct scsi_cmnd *ptr = DONE_SC;
			DONE_SC=NULL;

			HOSTDATA(shpnt)->commands--;

			if(ptr->scsi_done != reset_done) {
				kfree(ptr->host_scribble);
				ptr->host_scribble=NULL;
			}

			DO_UNLOCK(flags);
			ptr->scsi_done(ptr);
			DO_LOCK(flags);
		}

		DONE_SC=NULL;
#if defined(SPC_STAT)
	} else {
		HOSTDATA(shpnt)->busfree_without_done_command++;
#endif
	}

	if(ISSUE_SC)
		CURRENT_SC = remove_first_SC(&ISSUE_SC);

	DO_UNLOCK(flags);

	if(CURRENT_SC) {
#if defined(SPC_STAT)
		action++;
#endif
		CURRENT_SC->SCp.phase |= selecting;

		/* clear selection timeout */
		SETPORT(0x10, INTS);

		SETPORT(0x60, SCMD);
		SETPORT(GETPORT(BDID) | 1 << CURRENT_SC->device->id, TEMP);
		SETPORT(0x00, PCTL);
		SETPORT(SELTIME >> 8, TCH);
		SETPORT(SELTIME & 0xff, TCM);
		SETPORT(SELSTART, TCL);
		SETPORT(HOSTDATA(shpnt)->sctl = (GETPORT(SCTL) |
						 (PARITY?0x08:0x00) | 0x01), SCTL);
		SETPORT(0x20, SCMD);
	} else {
#if defined(SPC_STAT)
		HOSTDATA(shpnt)->busfree_without_new_command++;
#endif
		HOSTDATA(shpnt)->sctl = GETPORT(SCTL) |
			((DISCONNECTED_SC ? 0x04 : 0x00) & 0xfe);
		SETPORT(0x00, PCTL);
	}

#if defined(SPC_STAT)
	if(!action)
		HOSTDATA(shpnt)->busfree_without_any_action++;
#endif
}

/*
 * Selection done (OUT)
 * - queue IDENTIFY message and SDTR to selected target for message out
 *   (ATN asserted automagically via ENAUTOATNO in busfree())
 */
static void seldo_run(struct Scsi_Host *shpnt)
{
	CURRENT_SC->SCp.phase &= ~(selecting|not_issued);

	if (HOSTDATA(shpnt)->ints & 0x20) {
		scmd_printk(KERN_ERR, CURRENT_SC,
			    "spc: passing bus free condition\n");
		done(shpnt, DID_NO_CONNECT << 16);
		return;
	}

	ADDMSGO(IDENTIFY(RECONNECT, CURRENT_SC->device->lun));

	if (CURRENT_SC->SCp.phase & aborting) {
		ADDMSGO(ABORT);
	} else if (CURRENT_SC->SCp.phase & resetting) {
		ADDMSGO(BUS_DEVICE_RESET);
	}
}

/*
 * Selection timeout
 * - return command to mid-level with failure cause
 *
 */
static void selto_run(struct Scsi_Host *shpnt)
{
	SETPORT(0x00, TEMP);
	SETPORT(0x00, SCMD);
	if (!CURRENT_SC)
		return;

	CURRENT_SC->SCp.phase &= ~selecting;

	if (CURRENT_SC->SCp.phase & aborted)
		done(shpnt, DID_ABORT << 16);
	else
		/* ARBITRATION won, but SELECTION failed */
		done(shpnt, DID_NO_CONNECT << 16);
	busfree_run(shpnt);
}

/*
 * Selection in done
 * - put current command back to issue queue
 *   (reconnection of a disconnected nexus instead
 *    of successful selection out)
 *
 */
static void seldi_run(struct Scsi_Host *shpnt)
{
	int selid;
	int target;
	unsigned long flags;

	if(CURRENT_SC) {
		if(!(CURRENT_SC->SCp.phase & not_issued))
			scmd_printk(KERN_ERR, CURRENT_SC,
				    "command should not have been issued yet\n");

		DO_LOCK(flags);
		append_SC(&ISSUE_SC, CURRENT_SC);
		DO_UNLOCK(flags);

		CURRENT_SC = NULL;
	}

	if (!DISCONNECTED_SC)
		return;

	RECONN_TARGET=-1;

	selid = GETPORT(TEMP) & ~(1 << shpnt->this_id);

	if (selid==0) {
		shost_printk(KERN_INFO, shpnt,
			     "target id unknown (%02x)\n", selid);
		return;
	}

	target = ffs(selid);
	if(selid & ~(1 << target)) {
		shost_printk(KERN_INFO, shpnt,
			     "multiple targets reconnected (%02x)\n", selid);
	}


	SETPORT((1 <<shpnt->this_id) | (1 << target), TEMP);

	RECONN_TARGET=target;
}

/*
 * message in phase
 * - handle initial message after reconnection to identify
 *   reconnecting nexus
 * - queue command on DISCONNECTED_SC on DISCONNECT message
 * - set completed flag on COMMAND COMPLETE
 *   (other completition code moved to busfree_run)
 * - handle response to SDTR
 * - clear synchronous transfer agreements on BUS RESET
 *
 * FIXME: what about SAVE POINTERS, RESTORE POINTERS?
 *
 */
static void msgi_run(struct Scsi_Host *shpnt)
{
	SETPORT(7, PCTL);
	for(;;) {
		int sstat1 = HOSTDATA(shpnt)->ints;

		if(sstat1 & 0x28)
			return;

		if (!(GETPORT(PSNS) & 0x80))
			return;
		
		ADDMSGI(GETPORT(TEMP));
		SETPORT(0xe0, SCMD);
		while ((GETPORT(PSNS) & 0x80));
		SETPORT(0xc0, SCMD);
		while ((GETPORT(PSNS) & 0x40));

		if(!CURRENT_SC) {
			if(LASTSTATE!=seldi) {
				shost_printk(KERN_ERR, shpnt,
					     "message in w/o current command"
					     " not after reselection\n");
			}

			/*
			 * Handle reselection
			 */
			if(!(MSGI(0) & IDENTIFY_BASE)) {
				shost_printk(KERN_ERR, shpnt,
					     "target didn't identify after reselection\n");
				continue;
			}

			CURRENT_SC = remove_lun_SC(&DISCONNECTED_SC, RECONN_TARGET, MSGI(0) & 0x3f);

			if (!CURRENT_SC) {
				show_queues(shpnt);
				shost_printk(KERN_ERR, shpnt,
					     "no disconnected command"
					     " for target %d/%d\n",
					     RECONN_TARGET, MSGI(0) & 0x3f);
				continue;
			}

			CURRENT_SC->SCp.Message = MSGI(0);
			CURRENT_SC->SCp.phase &= ~disconnected;

			MSGILEN=0;

			/* next message if any */
			continue;
		}

		CURRENT_SC->SCp.Message = MSGI(0);

		switch (MSGI(0)) {
		case DISCONNECT:
			if (!RECONNECT)
				scmd_printk(KERN_WARNING, CURRENT_SC,
					    "target was not allowed to disconnect\n");

			CURRENT_SC->SCp.phase |= disconnected;
			break;

		case COMMAND_COMPLETE:
			CURRENT_SC->SCp.phase |= completed;
			break;

		case MESSAGE_REJECT:
			scmd_printk(KERN_INFO, CURRENT_SC,
				    "inbound message (MESSAGE REJECT)\n");
			break;

		case SAVE_POINTERS:
			break;

		case RESTORE_POINTERS:
			break;

		case EXTENDED_MESSAGE:
			if(MSGILEN<2 || MSGILEN<MSGI(1)+2) {
				/* not yet completed */
				continue;
			}

			switch (MSGI(2)) {

			case EXTENDED_MODIFY_DATA_POINTER:
			case EXTENDED_EXTENDED_IDENTIFY:
			case EXTENDED_WDTR:
			default:
				ADDMSGO(MESSAGE_REJECT);
				break;
			}
			break;
		}

		MSGILEN=0;
	}
}

static void msgi_end(struct Scsi_Host *shpnt)
{
	if(MSGILEN>0)
		scmd_printk(KERN_WARNING, CURRENT_SC,
			    "target left before message completed (%d)\n",
			    MSGILEN);

	if (MSGOLEN > 0 && !(HOSTDATA(shpnt)->ints & 0x20))
		SETPORT(0x40, SCMD);
}

/*
 * message out phase
 *
 */
static void msgo_init(struct Scsi_Host *shpnt)
{
	if(MSGOLEN==0) {
		scmd_printk(KERN_INFO, CURRENT_SC,
			    "unexpected MESSAGE OUT phase; rejecting\n");
		ADDMSGO(MESSAGE_REJECT);
	} else
		SETPORT(0x06, PCTL);
}

/*
 * message out phase
 *
 */
static void msgo_run(struct Scsi_Host *shpnt)
{
	while(MSGO_I<MSGOLEN) {
		if (!(GETPORT(PSNS) & 0x80))
			return;

		if (MSGO_I==MSGOLEN-1) {
			/* Leave MESSAGE OUT after transfer */
			SETPORT(0x40, SCMD);
		}


		if (MSGO(MSGO_I) & IDENTIFY_BASE)
			CURRENT_SC->SCp.phase |= identified;

		if (MSGO(MSGO_I)==ABORT)
			CURRENT_SC->SCp.phase |= aborted;

		if (MSGO(MSGO_I)==BUS_DEVICE_RESET)
			CURRENT_SC->SCp.phase |= resetted;

		SETPORT(MSGO(MSGO_I++), TEMP);
		SETPORT(0xe0, SCMD);
		while ((GETPORT(PSNS) & 0x80));
		SETPORT(0xc0, SCMD);
		while ((GETPORT(PSNS) & 0x40));
	}
}

static void msgo_end(struct Scsi_Host *shpnt)
{
	if(MSGO_I<MSGOLEN) {
		scmd_printk(KERN_ERR, CURRENT_SC,
			    "message sent incompletely (%d/%d)\n",
			    MSGO_I, MSGOLEN);
	}

	MSGO_I  = 0;
	MSGOLEN = 0;
}

/*
 * command phase
 *
 */
static void cmd_init(struct Scsi_Host *shpnt)
{
	if (CURRENT_SC->SCp.sent_command) {
		scmd_printk(KERN_ERR, CURRENT_SC,
			    "command already sent\n");
		done(shpnt, DID_ERROR << 16);
		return;
	}
		SETPORT(0x02, PCTL);

	CMD_I=0;
}

/*
 * command phase
 *
 */
static void cmd_run(struct Scsi_Host *shpnt)
{
	while(CMD_I<CURRENT_SC->cmd_len) {
		if (!(GETPORT(PSNS) & 0x80))
			return;

		SETPORT(CURRENT_SC->cmnd[CMD_I++], TEMP);
		SETPORT(0xe0, SCMD);
		while ((GETPORT(PSNS) & 0x80));
		SETPORT(0xc0, SCMD);
		while ((GETPORT(PSNS) & 0x40));
	}
}

static void cmd_end(struct Scsi_Host *shpnt)
{
	if(CMD_I<CURRENT_SC->cmd_len)
		scmd_printk(KERN_ERR, CURRENT_SC,
			    "command sent incompletely (%d/%d)\n",
			    CMD_I, CURRENT_SC->cmd_len);
	else
		CURRENT_SC->SCp.sent_command++;
}

/*
 * status phase
 *
 */
static void status_run(struct Scsi_Host *shpnt)
{
	SETPORT(0x03, PCTL);
	if (!(GETPORT(PSNS) & 0x80))
		return;

	CURRENT_SC->SCp.Status = GETPORT(TEMP);
	SETPORT(0xe0, SCMD);
	while ((GETPORT(PSNS) & 0x80));
	SETPORT(0xc0, SCMD);
	while ((GETPORT(PSNS) & 0x40));
}

/*
 * data in phase
 *
 */
static void datai_init(struct Scsi_Host *shpnt)
{
	SETPORT(0x01, PCTL);
	DATA_LEN=0;
}

static void datai_run(struct Scsi_Host *shpnt)
{
	/*
	 * loop while the phase persists or the fifos are not empty
	 *
	 */
	do {

		SETPORT(CURRENT_SC->SCp.this_residual >> 16, TCH);
		SETPORT((CURRENT_SC->SCp.this_residual >> 8) & 0xff, TCM);
		SETPORT(CURRENT_SC->SCp.this_residual & 0xff, TCL);
		SETPORT(0x80, SCMD);

		for (;CURRENT_SC->SCp.this_residual > 0;
			CURRENT_SC->SCp.this_residual--) {
			*CURRENT_SC->SCp.ptr++ = GETPORT(DREG);
			DATA_LEN++;
		}
		if (CURRENT_SC->SCp.buffers_residual > 0) {
			CURRENT_SC->SCp.buffer = sg_next(CURRENT_SC->SCp.buffer);
			CURRENT_SC->SCp.this_residual = CURRENT_SC->SCp.buffer->length;
			CURRENT_SC->SCp.ptr = sg_virt(CURRENT_SC->SCp.buffer);
		}
		CURRENT_SC->SCp.buffers_residual--;
	} while(CURRENT_SC->SCp.buffers_residual >= 0);
}

static void datai_end(struct Scsi_Host *shpnt)
{
	CMD_INC_RESID(CURRENT_SC, DATA_LEN);

}

/*
 * data out phase
 *
 */
static void datao_init(struct Scsi_Host *shpnt)
{
	SETPORT(0x07, PCTL);

	DATA_LEN = scsi_get_resid(CURRENT_SC);
}

static void datao_run(struct Scsi_Host *shpnt)
{
	/* until phase changes or all data sent */
	while(CURRENT_SC->SCp.this_residual>0) {
		SETPORT(CURRENT_SC->SCp.this_residual >> 16, TCH);
		SETPORT((CURRENT_SC->SCp.this_residual >> 8) & 0xff, TCM);
		SETPORT(CURRENT_SC->SCp.this_residual & 0xff, TCL);
		SETPORT(0x80, SCMD);

		while(CURRENT_SC->SCp.this_residual>0) {
			SETPORT(*CURRENT_SC->SCp.ptr++, DREG);
			CURRENT_SC->SCp.this_residual--;
			CMD_INC_RESID(CURRENT_SC, -1);
		}

		if(CURRENT_SC->SCp.this_residual==0 && CURRENT_SC->SCp.buffers_residual>0) {
			/* advance to next buffer */
			CURRENT_SC->SCp.buffers_residual--;
			CURRENT_SC->SCp.buffer++;
			CURRENT_SC->SCp.ptr           = SG_ADDRESS(CURRENT_SC->SCp.buffer);
			CURRENT_SC->SCp.this_residual = CURRENT_SC->SCp.buffer->length;
		}

	}
}

static void datao_end(struct Scsi_Host *shpnt)
{
}

/*
 * figure out what state we're in
 *
 */
static int update_state(struct Scsi_Host *shpnt)
{
	int dataphase=0;
	int ints = HOSTDATA(shpnt)->ints;

	PREVSTATE = STATE;
	STATE=unknown;

	if(ints  & 1) {
		STATE=rsti;
	} else if ((ints & 0x80) && PREVSTATE == busfree) {
		STATE=seldi;
		HOSTDATA(shpnt)->ints &= ~0x80;
	} else if((ints & 0x10) && CURRENT_SC && (CURRENT_SC->SCp.phase & selecting)) {
		STATE=seldo;
		HOSTDATA(shpnt)->ints &= ~0x10;
	} else if(ints & 0x04) {
		STATE=selto;
		HOSTDATA(shpnt)->ints &= ~0x40;
	} else if(ints & 0x20 || (GETPORT(PSNS) == 0x00)) {
		STATE=busfree;
		HOSTDATA(shpnt)->ints &= ~0x20;
	} else if(ints & 0x02) {
		STATE=parerr;
		HOSTDATA(shpnt)->ints &= ~0x02;
	} else if((GETPORT(SSTS) & 0xc0) == 0x80) {
		switch(GETPORT(PSNS) & P_MASK) {
		case P_MSGI:	STATE=msgi;	break;
		case P_MSGO:	STATE=msgo;	break;
		case P_DATAO:	STATE=datao;	dataphase=0; break;
		case P_DATAI:	STATE=datai;    dataphase=0; break;
		case P_STATUS:	STATE=status;	break;
		case P_CMD:	STATE=cmd;	break;
		}
	}

	if((ints & 0x40) && STATE!=seldi && !dataphase) {
		scmd_printk(KERN_INFO, CURRENT_SC, "reselection missed?");
	}

	if(STATE!=PREVSTATE) {
		LASTSTATE=PREVSTATE;
	}
	return dataphase;
}

/*
 * handle parity error
 *
 * FIXME: in which phase?
 *
 */
static void parerr_run(struct Scsi_Host *shpnt)
{
	scmd_printk(KERN_ERR, CURRENT_SC, "parity error\n");
	done(shpnt, DID_PARITY << 16);
}

/*
 * handle reset in
 *
 */
static void rsti_run(struct Scsi_Host *shpnt)
{
	struct scsi_cmnd *ptr;

	shost_printk(KERN_NOTICE, shpnt, "scsi reset in\n");

	ptr=DISCONNECTED_SC;
	while(ptr) {
		struct scsi_cmnd *next = SCNEXT(ptr);

		if (!ptr->device->soft_reset) {
			remove_SC(&DISCONNECTED_SC, ptr);

			kfree(ptr->host_scribble);
			ptr->host_scribble=NULL;

			ptr->result =  DID_RESET << 16;
			ptr->scsi_done(ptr);
		}

		ptr = next;
	}

	if(CURRENT_SC && !CURRENT_SC->device->soft_reset)
		done(shpnt, DID_RESET << 16 );
}


/*
 * bottom-half handler
 *
 */
static void is_complete(struct Scsi_Host *shpnt)
{
	int dataphase = 0;
	unsigned long flags;

	if(!shpnt)
		return;

	DO_LOCK(flags);

	if( HOSTDATA(shpnt)->service==0 )  {
		DO_UNLOCK(flags);
		return;
	}

	if(HOSTDATA(shpnt)->in_intr) {
		DO_UNLOCK(flags);
		/* spc_error never returns.. */
		spc_error(shpnt, "bottom-half already running!?");
	}
	HOSTDATA(shpnt)->in_intr++;
 	HOSTDATA(shpnt)->service = 0;
	DO_UNLOCK(flags);
	/*
	 * loop while there are interrupt conditions pending
	 *
	 */
	
	while(!dataphase) {
		dataphase = update_state(shpnt);
		/*
		 * end previous state
		 *
		 */
		if(PREVSTATE!=STATE && states[PREVSTATE].end)
			states[PREVSTATE].end(shpnt);
	
		/*
		 * initialize for new state
		 *
		 */
		if(PREVSTATE!=STATE && states[STATE].init)
			states[STATE].init(shpnt);
		
		/*
		 * handle current state
		 *
		 */
		if(states[STATE].run) {
			states[STATE].run(shpnt);
	
		} else {
			scmd_printk(KERN_ERR, CURRENT_SC,
			    "unexpected state (%x)\n", STATE);
			break;
		}
		if (STATE == busfree || STATE == selto)
			break;
		HOSTDATA(shpnt)->ints = GETPORT(INTS);
		SETPORT(HOSTDATA(shpnt)->ints, INTS);
	}
	/*
	 * setup controller to interrupt on
	 * the next expected condition and
	 * loop if it's already there
	 *
	 */
	DO_LOCK(flags);
#if defined(SPC_STAT)
	HOSTDATA(shpnt)->count[STATE]++;
	if(PREVSTATE!=STATE)
		HOSTDATA(shpnt)->count_trans[STATE]++;
	HOSTDATA(shpnt)->time[STATE] += jiffies-start;
#endif

	/*
	 * enable interrupts and leave bottom-half
	 *
	 */
	HOSTDATA(shpnt)->in_intr--;
	DO_UNLOCK(flags);
	SETPORT(HOSTDATA(shpnt)->sctl | 0x01, SCTL);
}


/*
 * Dump the current driver status and panic
 */
static void spc_error(struct Scsi_Host *shpnt, char *msg)
{
	shost_printk(KERN_EMERG, shpnt, "%s\n", msg);
	show_queues(shpnt);
	panic("spc panic\n");
}

/*
 * Show the command data of a command
 */
static void show_command(struct scsi_cmnd *ptr)
{
	scsi_print_command(ptr);
	scmd_printk(KERN_DEBUG, ptr,
		    "request_bufflen=%d; resid=%d; "
		    "phase |%s%s%s%s%s%s%s%s; next=0x%p",
		    scsi_bufflen(ptr), scsi_get_resid(ptr),
		    (ptr->SCp.phase & not_issued) ? "not issued|" : "",
		    (ptr->SCp.phase & selecting) ? "selecting|" : "",
		    (ptr->SCp.phase & identified) ? "identified|" : "",
		    (ptr->SCp.phase & disconnected) ? "disconnected|" : "",
		    (ptr->SCp.phase & completed) ? "completed|" : "",
		    (ptr->SCp.phase & spiordy) ? "spiordy|" : "",
		    (ptr->SCp.phase & aborted) ? "aborted|" : "",
		    (ptr->SCp.phase & resetted) ? "resetted|" : "",
		    (SCDATA(ptr)) ? SCNEXT(ptr) : NULL);
}

/*
 * Dump the queued data
 */
static void show_queues(struct Scsi_Host *shpnt)
{
	struct scsi_cmnd *ptr;
	unsigned long flags;

	DO_LOCK(flags);
	printk(KERN_DEBUG "\nqueue status:\nissue_SC:\n");
	for (ptr = ISSUE_SC; ptr; ptr = SCNEXT(ptr))
		show_command(ptr);
	DO_UNLOCK(flags);

	printk(KERN_DEBUG "current_SC:\n");
	if (CURRENT_SC)
		show_command(CURRENT_SC);
	else
		printk(KERN_DEBUG "none\n");

	printk(KERN_DEBUG "disconnected_SC:\n");
	for (ptr = DISCONNECTED_SC; ptr; ptr = SCDATA(ptr) ? SCNEXT(ptr) : NULL)
		show_command(ptr);

}

static int spc_adjust_queue(struct scsi_device *device)
{
	blk_queue_bounce_limit(device->request_queue, BLK_BOUNCE_HIGH);
	return 0;
}

static struct scsi_host_template spc_driver_template = {
	.module				= THIS_MODULE,
	.name				= "MB89352 (SPC) Driver",
	.queuecommand			= spc_queue,
	.eh_abort_handler		= spc_abort,
	.eh_device_reset_handler	= spc_device_reset,
	.eh_bus_reset_handler		= spc_bus_reset_host,
	.eh_host_reset_handler		= spc_host_reset,
	.bios_param			= spc_biosparam,
	.can_queue			= 1,
	.sg_tablesize			= SG_ALL,
	.slave_alloc			= spc_adjust_queue,
};

static int spc_probe(struct platform_device *pdev)
{
	struct Scsi_Host *shpnt;
	const struct resource *res;
	struct spc_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	shpnt = scsi_host_alloc(&spc_driver_template, sizeof(struct Scsi_Host));
	if (!shpnt)
		return -ENOMEM;
	
	memset(HOSTDATA(shpnt), 0, sizeof *HOSTDATA(shpnt));
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	shpnt->base   = res->start;
	shpnt->irq       = platform_get_irq(pdev, 0);

	spin_lock_init(&QLOCK);
	PARITY      = pdata->parity;
	DELAY       = pdata->delay;

	SETPORT(shpnt->this_id = pdata->scsiid, BDID);

	/* RESET OUT */
	SETPORT(0x80, SCTL);
	mdelay(256);
	SETPORT(0x00, SCTL);
	mdelay(256);

	printk(KERN_INFO "spc: io=0x%08lx, irq=%d, scsiid=%d\n",
	       shpnt->base,
	       shpnt->irq,
	       shpnt->this_id);

	if (!request_mem_region(res->start, res->end - res->start, "spc")) {
		printk(KERN_ERR "spc: mem %08x busy.\n", res->start);
		ret = -EBUSY;
		goto out_host_put1;
	}

	ret = request_irq(shpnt->irq, spc_intr, IRQF_SHARED, "spc", shpnt);
	if (ret) {
		printk(KERN_ERR "spc: irq %d busy. %d\n", shpnt->irq, ret);
		goto out_host_put2;
	}

	ret = scsi_add_host(shpnt, NULL);
	if (ret) {
		printk(KERN_ERR "spc%d: failed to add host.\n", shpnt->host_no);
		goto out_host_put3;
	}

	HOSTDATA(shpnt)->host = shpnt;
	platform_set_drvdata(pdev, shpnt);
	scsi_scan_host(shpnt);

	return 0;

out_host_put3:
	free_irq(shpnt->irq, shpnt);
out_host_put2:
	release_mem_region(res->start, res->end - res->start);
out_host_put1:
	scsi_host_put(shpnt);

	return ret;
}

static int spc_remove(struct platform_device *pdev)
{
	struct Scsi_Host *shpnt;

	shpnt = platform_get_drvdata(pdev);

	scsi_remove_host(shpnt);
	free_irq(shpnt->irq, shpnt);
	release_mem_region(shpnt->base, 0x20);
	scsi_host_put(shpnt);

	return 0;
}

static struct platform_driver spc_driver = {
	.probe = spc_probe,
	.remove = spc_remove,
	.driver = {
		.name = "spc",
	}
};

static int __init spc_init(void)
{
	return platform_driver_register(&spc_driver);
}

static void __exit spc_exit(void)
{
	platform_driver_unregister(&spc_driver);
}

module_init(spc_init);
module_exit(spc_exit);
