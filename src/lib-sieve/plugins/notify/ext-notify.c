/* Copyright (c) 2002-2017 Pigeonhole authors, see the included COPYING file
 */

/* Extension notify
 * ----------------
 *
 * Authors: Stephan Bosch
 * Specification: draft-ietf-sieve-notify-00.txt
 * Implementation: full, but deprecated; provided for backwards compatibility
 * Status: testing
 *
 */

#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-actions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"

#include "ext-notify-common.h"

/*
 * Operations
 */

const struct sieve_operation_def *ext_notify_operations[] = {
	&notify_old_operation,
	&denotify_operation
};

/*
 * Extension
 */

static bool ext_notify_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def notify_extension = {
	.name = "notify",
	.validator_load = ext_notify_validator_load,
	SIEVE_EXT_DEFINE_OPERATIONS(ext_notify_operations)
};

/*
 * Extension validation
 */

static bool ext_notify_validator_check_conflict
	(const struct sieve_extension *ext,
		struct sieve_validator *valdtr, void *context,
		struct sieve_ast_argument *require_arg,
		const struct sieve_extension *ext_other,
		bool required);
static bool ext_notify_validator_validate
	(const struct sieve_extension *ext,
		struct sieve_validator *valdtr, void *context,
		struct sieve_ast_argument *require_arg,
		bool required);

const struct sieve_validator_extension notify_validator_extension = {
	.ext = &notify_extension,
	.check_conflict = ext_notify_validator_check_conflict,
	.validate = ext_notify_validator_validate	
};

static bool ext_notify_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register validator extension to check for conflict with enotify */
	sieve_validator_extension_register
		(valdtr, ext, &notify_validator_extension, NULL);
	return TRUE;
}

static bool ext_notify_validator_check_conflict
(const struct sieve_extension *ext ATTR_UNUSED,
	struct sieve_validator *valdtr, void *context ATTR_UNUSED,
	struct sieve_ast_argument *require_arg,
	const struct sieve_extension *ext_other,
	bool required ATTR_UNUSED)
{
	/* Check for conflict with enotify */
	if ( sieve_extension_name_is(ext_other, "enotify") ) {
		sieve_argument_validate_error(valdtr, require_arg,
			"the (deprecated) notify extension cannot be used "
			"together with the enotify extension");
		return FALSE;
	}

	return TRUE;
}

static bool ext_notify_validator_validate
(const struct sieve_extension *ext,
	struct sieve_validator *valdtr, void *context ATTR_UNUSED,
	struct sieve_ast_argument *require_arg ATTR_UNUSED,
	bool required ATTR_UNUSED)
{
	/* No conflicts: register new commands */
	sieve_validator_register_command(valdtr, ext, &cmd_notify_old);
	sieve_validator_register_command(valdtr, ext, &cmd_denotify);
	return TRUE;
}
