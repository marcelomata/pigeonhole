/* Copyright (c) 2002-2009 Dovecot Sieve authors, see the included COPYING file
 */
 
#ifndef __EXT_DATE_COMMON_H
#define __EXT_DATE_COMMON_H

#include "sieve-common.h"

#include <time.h>

/*
 * Extension
 */
 
extern const struct sieve_extension date_extension;

bool ext_date_interpreter_load
	(const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED);

/* 
 * Tests
 */

extern const struct sieve_command date_test;
extern const struct sieve_command currentdate_test;
 
/*
 * Operations
 */

enum ext_date_opcode {
	EXT_DATE_OPERATION_DATE,
	EXT_DATE_OPERATION_CURRENTDATE
};

extern const struct sieve_operation date_operation;
extern const struct sieve_operation currentdate_operation;

/*
 * Zone string
 */

bool ext_date_parse_timezone(const char *zone, int *zone_offset_r);

/*
 * Current date
 */

time_t ext_date_get_current_date(const struct sieve_runtime_env *renv);

/*
 * Date part
 */

struct ext_date_part {
	const char *identifier;

	const char *(*get_string)(struct tm *tm, int zone_offset);
};

const char *ext_date_part_extract
	(const char *part, struct tm *tm, int zone_offset);

#endif /* __EXT_DATE_COMMON_H */
