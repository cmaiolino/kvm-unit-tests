// SPDX-License-Identifier: GPL-2.0-only
/*
 * SBI verification
 *
 * Copyright (C) 2023, Ventana Micro Systems Inc., Andrew Jones <ajones@ventanamicro.com>
 */
#include <libcflat.h>
#include <stdlib.h>
#include <asm/sbi.h>

static void help(void)
{
	puts("Test SBI\n");
	puts("An environ must be provided where expected values are given.\n");
}

static int env_is_defined(const char *env)
{

	if (!getenv(env)) {
		report_skip("mvendorid: missing MVENDORID environment variable");
		return 0;
	}

	return 1;
}

static void gen_report(struct sbiret *ret, long expected)
{
	report(!ret->error, "no sbi.error");

	if (ret->value == expected)
		report(true, "expected sbi.value");
	else
		report(false, "expected sbi.value: %ld - Got: %ld",
		       expected, ret->value);
}

static void check_base(void)
{
	struct sbiret ret;
	long expected;

	report_prefix_push("base");

	if (env_is_defined("MVENDORID")) {
		report_prefix_push("mvendorid");
		expected = strtol(getenv("MVENDORID"), NULL, 0);

		ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_GET_MVENDORID,
				0, 0, 0, 0, 0, 0);

		gen_report(&ret, expected);
		report_prefix_pop();
	}

	if (env_is_defined("PROBE_EXT")) {
		report_prefix_push("probe_ext");
		expected = strtol(getenv("PROBE_EXT"), NULL, 0);

		ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT,
				SBI_EXT_BASE, 0, 0, 0, 0, 0);

		gen_report(&ret, expected);
		report_prefix_pop();
	}

	if (env_is_defined("MARCHID")) {
		report_prefix_push("marchid");
		expected = strtol(getenv("MARCHID"), NULL, 0);

		ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT,
				SBI_EXT_BASE_GET_MARCHID, 0, 0, 0, 0, 0);

		gen_report(&ret, expected);
		report_prefix_pop();
	}

	report_prefix_pop();
}

int main(int argc, char **argv)
{

	if (argc > 1 && !strcmp(argv[1], "-h")) {
		help();
		exit(0);
	}

	report_prefix_push("sbi");
	check_base();

	return report_summary();
}
