/* Copyright (c) 2002-2008 Dovecot Sieve authors, see the included COPYING file 
 */
 
#include "lib.h"
#include "str.h"
#include "str-sanitize.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-ast.h"
#include "sieve-commands.h"
#include "sieve-validator.h"

#include "ext-enotify-limits.h"
#include "ext-enotify-common.h"

#include <ctype.h>

/*
 * Notify capability
 */

static const char *ext_notify_get_methods_string(void);

const struct sieve_extension_capabilities notify_capabilities = {
	"notify",
	ext_notify_get_methods_string
};

/*
 * Notify method registry
 */
 
static ARRAY_DEFINE(ext_enotify_methods, const struct sieve_enotify_method *); 

void ext_enotify_methods_init(void)
{
	p_array_init(&ext_enotify_methods, default_pool, 4);

	sieve_enotify_method_register(&mailto_notify);
}

void ext_enotify_methods_deinit(void)
{
	array_free(&ext_enotify_methods);
}

void sieve_enotify_method_register(const struct sieve_enotify_method *method) 
{
	array_append(&ext_enotify_methods, &method, 1);
}

const struct sieve_enotify_method *ext_enotify_method_find
(const char *identifier) 
{
	unsigned int meth_count, i;
	const struct sieve_enotify_method *const *methods;
	 
	methods = array_get(&ext_enotify_methods, &meth_count);
		
	for ( i = 0; i < meth_count; i++ ) {
		if ( strcasecmp(methods[i]->identifier, identifier) == 0 ) {
			return methods[i];
		}
	}
	
	return NULL;
}

static const char *ext_notify_get_methods_string(void)
{
	unsigned int meth_count, i;
	const struct sieve_enotify_method *const *methods;
	string_t *result = t_str_new(128);
	 
	methods = array_get(&ext_enotify_methods, &meth_count);
		
	if ( meth_count > 0 ) {
		str_append(result, methods[0]->identifier);
		
		for ( i = 1; i < meth_count; i++ ) {
			str_append_c(result, ' ');
			str_append(result, methods[i]->identifier);
		}
		
		return str_c(result);
	}
	
	return NULL;
}

/*
 * URI validation
 */
 
static const char *ext_enotify_uri_scheme_parse(const char **uri_p)
{
	string_t *scheme = t_str_new(EXT_ENOTIFY_MAX_SCHEME_LEN);
	const char *p = *uri_p;
	unsigned int len = 0;
	
	/* RFC 3968:
	 *
	 *   scheme  = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	 *
	 * FIXME: we do not allow '%' in schemes. Is this correct?
	 */
	 
	if ( !i_isalpha(*p) )
		return NULL;
		
	str_append_c(scheme, *p);
	p++;
		
	while ( *p != '\0' && len < EXT_ENOTIFY_MAX_SCHEME_LEN ) {
			
		if ( !i_isalnum(*p) && *p != '+' && *p != '-' && *p != '.' )
			break;
	
		str_append_c(scheme, *p);
		p++;
		len++;
	}
	
	if ( *p != ':' )
		return NULL;
	p++;
	
	*uri_p = p;
	return str_c(scheme);
}

bool ext_enotify_uri_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument *arg)
{
	const char *uri = sieve_ast_argument_strc(arg);
	const char *scheme;
	const struct sieve_enotify_method *method;
	
	if ( (scheme=ext_enotify_uri_scheme_parse(&uri)) == NULL ) {
		sieve_argument_validate_error(valdtr, arg, 
			"invalid scheme part for method URI '%s'", 
			str_sanitize(sieve_ast_argument_strc(arg), 80));
		return FALSE;
	}
	
	if ( (method=ext_enotify_method_find(scheme)) == NULL ) {
		sieve_argument_validate_error(valdtr, arg, 
			"invalid notify method '%s'", scheme);
		return FALSE;
	}
	
	if ( method->validate_uri != NULL )
		return method->validate_uri(valdtr, arg, uri);
		
	return TRUE;
}

