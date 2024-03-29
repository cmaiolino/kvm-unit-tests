/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Tests guarded storage support.
 *
 * Copyright 2018 IBM Corp.
 *
 * Authors:
 *    Martin Schwidefsky <schwidefsky@de.ibm.com>
 *    Janosch Frank <frankja@linux.ibm.com>
 */
#include <libcflat.h>
#include <asm/asm-offsets.h>
#include <asm/page.h>
#include <asm/facility.h>
#include <asm/interrupt.h>
#include <asm-generic/barrier.h>
#include <gs.h>

static volatile int guarded = 0;
static struct gs_cb gs_cb;
static struct gs_epl gs_epl;
static unsigned long gs_area = 0x2000000;

void gs_handler(struct gs_cb *this_cb);

static inline unsigned long load_guarded(unsigned long *p)
{
	unsigned long v;

	asm(".insn rxy,0xe3000000004c, %0,%1"
	    : "=d" (v)
	    : "m" (*p)
	    : "r14", "memory");
	return v;
}

/* guarded-storage event handler and finally it calls gs_handler */
extern void gs_handler_asm(void);
	asm (          ".macro	STGSC	args:vararg\n"
		"	.insn	rxy,0xe30000000049,\\args\n"
		"	.endm\n"
		"	.globl	gs_handler_asm\n"
		"gs_handler_asm:\n"
		"	lgr	%r14,%r15\n"				/* Save current stack address in r14 */
		".Lgs_handler_frame = 16*8+32+" xstr(STACK_FRAME_SIZE) "\n"
		"	aghi	%r15,-(.Lgs_handler_frame)\n"		/* Allocate stack frame */
		"	stmg	%r0,%r13,192(%r15)\n"			/* Store regs to save area */
		"	stg	%r14,312(%r15)\n"
		"	la	%r2," xstr(STACK_FRAME_SIZE) "(%r15)\n"	/* Store gscb address in this_cb */
		"	STGSC	%r0," xstr(STACK_FRAME_SIZE) "(%r15)\n"
		"	lg	%r14,24(%r2)\n"				/* Get GSEPLA from GSCB*/
		"	lg	%r14,40(%r14)\n"			/* Get GSERA from GSEPL*/
		"	stg	%r14,304(%r15)\n"			/* Store GSERA in r14 of reg save area */
		"	brasl	%r14,gs_handler\n"			/* Jump to gs_handler */
		"	lmg	%r0,%r15,192(%r15)\n"			/* Restore regs */
		"	aghi	%r14, 6\n"				/* Add lgg instr len to GSERA */
		"	br	%r14\n"					/* Jump to next instruction after lgg */
		".size gs_handler_asm,.-gs_handler_asm\n"
	);

void gs_handler(struct gs_cb *this_cb)
{
	guarded = 1;
	struct gs_epl *gs_epl = (struct gs_epl *) this_cb->gs_epl_a;
	printf("gs_handler called for %016lx at %016lx\n",
	       gs_epl->gs_eir, gs_epl->gs_eia);
}

/* Test if load guarded gets intercepted. */
static void test_load(void)
{
	unsigned long v;

	guarded = 0;
	v = load_guarded(&gs_area);
	report(guarded, "load guarded %ld", v);
	guarded = 0;
}

/* Test gs instructions without enablement resulting in an exception */
static void test_special(void)
{
	report_prefix_push("disabled gs");
	report_prefix_push("load gs");
	expect_pgm_int();
	load_gs_cb(&gs_cb);
	check_pgm_int_code(PGM_INT_CODE_SPECIAL_OPERATION);
	report_prefix_pop();

	report_prefix_push("store gs");
	expect_pgm_int();
	store_gs_cb(&gs_cb);
	check_pgm_int_code(PGM_INT_CODE_SPECIAL_OPERATION);
	report_prefix_pop();

	report_prefix_pop();
}

static void init(void)
{
	/* Enable control bit for gs */
	ctl_set_bit(2, CTL2_GUARDED_STORAGE);

	/* Setup gs registers to guard the gs_area */
	gs_cb.gsd = gs_area | 25;

	/* Check all 512kb slots for events */
	gs_cb.gssm = 0xffffffffffffffffULL;
	gs_cb.gs_epl_a =  (unsigned long) &gs_epl;

	/* Register handler */
	gs_epl.gs_eha = (unsigned long) gs_handler_asm;
	load_gs_cb(&gs_cb);
}

int main(void)
{
	bool has_gs = test_facility(133);

	report_prefix_push("gs");
	if (!has_gs) {
		report_skip("Guarded storage is not available");
		goto done;
	}

	test_special();
	init();
	test_load();

done:
	report_prefix_pop();
	return report_summary();
}
