/* Copyright (c) 2002-2008 Dovecot Sieve authors, see the included COPYING file 
 */

#ifndef __SIEVE_TYPES_H
#define __SIEVE_TYPES_H

#include "lib.h"

#include <stdio.h>

#define SIEVE_VERSION "0.0.1"
#define SIEVE_IMPLEMENTATION "Dovecot Sieve " SIEVE_VERSION

/* Enable runtime trace functionality */
#define SIEVE_RUNTIME_TRACE

struct sieve_script;
struct sieve_binary;

/* The mail message + envelope data */

struct sieve_message_data {
	struct mail *mail;
	const char *return_path;
	const char *to_address;
	const char *auth_user;
	const char *id;
};

/* Environment for currently executing script */

struct sieve_script_env {
	/* Mail-related */
	const char *inbox;
	struct mail_namespace *namespaces;
	
	/* System-related */
	const char *username;
	const char *hostname;
	const char *postmaster_address;
		
	/* Callbacks */
	
	/* Interface for sending mail */
	void *(*smtp_open)
		(const char *destination, const char *return_path, FILE **file_r);
	bool (*smtp_close)(void *handle);
	
	/* Interface for marking and checking duplicates */
	int (*duplicate_check)(const void *id, size_t id_size, const char *user);
	void (*duplicate_mark)(const void *id, size_t id_size,
                    const char *user, time_t time);
};	

/*
 * Execution exit codes
 */

enum sieve_execution_exitcode {
    SIEVE_EXEC_OK          = 1,
    SIEVE_EXEC_FAILURE     = 0,
    SIEVE_EXEC_BIN_CORRUPT = -1,
	SIEVE_EXEC_KEEP_FAILED = -2
};

#endif /* __SIEVE_TYPES_H */