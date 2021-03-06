/* Copyright (c) 2002-2017 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_SCRIPT_PRIVATE_H
#define __SIEVE_SCRIPT_PRIVATE_H

#include "sieve-common.h"
#include "sieve-script.h"

/*
 * Script object
 */

struct sieve_script_vfuncs {
	void (*destroy)(struct sieve_script *script);

	int (*open)
		(struct sieve_script *script, enum sieve_error *error_r);

	int (*get_stream)
		(struct sieve_script *script, struct istream **stream_r,
			enum sieve_error *error_r);
	
	/* binary */
	int (*binary_read_metadata)
		(struct sieve_script *_script, struct sieve_binary_block *sblock,
			sieve_size_t *offset);
	void (*binary_write_metadata)
		(struct sieve_script *script, struct sieve_binary_block *sblock);
	bool (*binary_dump_metadata)
		(struct sieve_script *script, struct sieve_dumptime_env *denv,
			struct sieve_binary_block *sblock, sieve_size_t *offset);
	struct sieve_binary *(*binary_load)
		(struct sieve_script *script, enum sieve_error *error_r);
	int (*binary_save)
		(struct sieve_script *script, struct sieve_binary *sbin,
			bool update, enum sieve_error *error_r);
	const char *(*binary_get_prefix)
		(struct sieve_script *script);

	/* management */
	int (*rename)
		(struct sieve_script *script, const char *newname);
	int (*delete)(struct sieve_script *script);
	int (*is_active)(struct sieve_script *script);
	int (*activate)(struct sieve_script *script);

	/* properties */
	int (*get_size)
		(const struct sieve_script *script, uoff_t *size_r);

	/* matching */
	bool (*equals)
		(const struct sieve_script *script, const struct sieve_script *other);
};

struct sieve_script {
	pool_t pool;
	unsigned int refcount;
	struct sieve_storage *storage;

	const char *driver_name;
	const struct sieve_script *script_class;
	struct sieve_script_vfuncs v;

	const char *name;
	const char *location;

	/* Stream */
	struct istream *stream;

	bool open:1;
};

void sieve_script_init
(struct sieve_script *script, struct sieve_storage *storage,
	const struct sieve_script *script_class, const char *location,
	const char *name);

/*
 * Built-in script drivers
 */

extern const struct sieve_script sieve_file_script;
extern const struct sieve_script sieve_dict_script;
extern const struct sieve_script sieve_ldap_script;

/*
 * Error handling
 */

void sieve_script_set_error
	(struct sieve_script *script, enum sieve_error error,
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_script_set_internal_error
	(struct sieve_script *script);
void sieve_script_set_critical
	(struct sieve_script *script, const char *fmt, ...)
		ATTR_FORMAT(2, 3);

void sieve_script_sys_error
	(struct sieve_script *script, const char *fmt, ...)
		ATTR_FORMAT(2, 3);
void sieve_script_sys_warning
	(struct sieve_script *script, const char *fmt, ...)
		ATTR_FORMAT(2, 3);
void sieve_script_sys_info
	(struct sieve_script *script, const char *fmt, ...)
		ATTR_FORMAT(2, 3);
void sieve_script_sys_debug
	(struct sieve_script *script, const char *fmt, ...)
		ATTR_FORMAT(2, 3);

/*
 * Script sequence
 */

void sieve_script_sequence_init
(struct sieve_script_sequence *seq, struct sieve_storage *storage);

#endif /* __SIEVE_SCRIPT_PRIVATE_H */
