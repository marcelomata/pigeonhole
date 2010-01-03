/* Copyright (c) 2002-2010 Dovecot Sieve authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-settings.h"
#include "sieve-error.h"
#include "sieve-extensions.h"
#include "sieve-interpreter.h"

#include "ext-spamvirustest-common.h"

#include <sys/types.h>
#include <regex.h>
#include <ctype.h>

/*
 * Extension data
 */

enum ext_spamvirustest_status_type {
	EXT_SPAMVIRUSTEST_STATUS_TYPE_VALUE,
	EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN,
	EXT_SPAMVIRUSTEST_STATUS_TYPE_YESNO,
};

struct ext_spamvirustest_header_spec {
	const char *header_name;
	regex_t regexp;
	bool regexp_match;
};

struct ext_spamvirustest_data {
	pool_t pool;

	struct ext_spamvirustest_header_spec status_header;
	struct ext_spamvirustest_header_spec max_header;

	enum ext_spamvirustest_status_type status_type;

	float max_value;
	const char *yes_string;
};

/*
 * Regexp utility
 */

static bool _regexp_compile
(regex_t *regexp, const char *data, const char **error_r)
{
	size_t errsize;
	int ret;

	*error_r = "";

	if ( (ret=regcomp(regexp, data, REG_EXTENDED)) == 0 ) {
		return TRUE;
	}

	errsize = regerror(ret, regexp, NULL, 0); 

	if ( errsize > 0 ) {
		char *errbuf = t_malloc(errsize);

		(void)regerror(ret, regexp, errbuf, errsize);
	 
		/* We don't want the error to start with a capital letter */
		errbuf[0] = i_tolower(errbuf[0]);

		*error_r = errbuf;
	}

	return FALSE;
}

static const char *_regexp_match_get_value
(const char *string, int index, regmatch_t pmatch[], int nmatch)
{
	if ( index > -1 && index < nmatch && pmatch[index].rm_so != -1 ) {
		return t_strndup(string + pmatch[index].rm_so, 
						pmatch[index].rm_eo - pmatch[index].rm_so);
	}
	return NULL;
}

/*
 * Configuration parser
 */

static bool ext_spamvirustest_header_spec_parse
(struct ext_spamvirustest_header_spec *spec, pool_t pool, const char *data, 
	const char **error_r)
{
	const char *p;
	const char *regexp_error;

	if ( *data == '\0' ) {
		*error_r = "empty header specification";
		return FALSE;
	}

	/* Parse header name */

	p = data;

	while ( *p == ' ' || *p == '\t' ) p++;
	while ( *p != ':' && *p != '\0' && *p != ' ' && *p != '\t' ) p++;

	if ( *p == '\0' ) {
		spec->header_name = p_strdup(pool, data);
		return TRUE;
	}

	spec->header_name = p_strdup_until(pool, data, p);
	while ( *p == ' ' || *p == '\t' ) p++;

	if ( p == '\0' ) {
		spec->regexp_match = FALSE;
		return TRUE;
	}

	/* Parse and compile regular expression */

	if ( *p != ':' ) {
		*error_r = t_strdup_printf("expecting ':', but found '%c'", *p);
		return FALSE;
	}
	p++;

	spec->regexp_match = TRUE;
	if ( !_regexp_compile(&spec->regexp, p, &regexp_error) ) {
		*error_r = t_strdup_printf("failed to compile regular expression '%s': " 
			"%s", p, regexp_error);
		return FALSE;
	}

	return TRUE;
}

static void ext_spamvirustest_header_spec_free
(struct ext_spamvirustest_header_spec *spec)
{
	regfree(&spec->regexp);
}

static bool ext_spamvirustest_parse_strlen_value
(const char *str_value, float *value_r, const char **error_r)
{
	const char *p = str_value;
	char ch = *p;

	if ( *str_value == '\0' ) {
		*error_r = "empty value";		
		return FALSE;
	}
	
	while ( *p == ch ) p++;

	if ( *p != '\0' ) {
		*error_r = t_strdup_printf(
			"different character '%c' encountered in strlen value",
			*p);
		return FALSE;
	}

	*value_r = ( p - str_value );

	return TRUE;
}

static bool ext_spamvirustest_parse_decimal_value
(const char *str_value, float *value_r, const char **error_r)
{
	const char *p = str_value;
	float value;
	float sign = 1;
	int digits;

	if ( *p == '\0' ) {
		*error_r = "empty value";		
		return FALSE;
	}

	if ( *p == '+' || *p == '-' ) {
		if ( *p == '-' )
			sign = -1;
		
		p++;
	}

	value = 0;
	digits = 0;
	while ( i_isdigit(*p) ) {
		value = value*10 + (*p-'0');
		if ( digits++ > 4 ) {
			*error_r = t_strdup_printf
				("decimal value has too many digits before radix point: %s", 
					str_value);
			return FALSE;	
		}
		p++;
	}

	if ( *p == '.' || *p == ',' ) {
		float radix = .1;
		p++;

		digits = 0;
		while ( i_isdigit(*p) ) {
			value = value + (*p-'0')*radix;

			if ( digits++ > 4 ) {
				*error_r = t_strdup_printf
					("decimal value has too many digits after radix point: %s", 
						str_value);
				return FALSE;
			}
			radix /= 10;
			p++;
		}
	}

	if ( *p != '\0' ) {
		*error_r = t_strdup_printf
			("invalid decimal point value: %s", str_value);
		return FALSE;
	}

	*value_r = value * sign;

	return TRUE;
}

/*
 * Extension initialization
 */

static bool ext_spamvirustest_config_load
(struct ext_spamvirustest_data *ext_data, const char *ext_name, 
	const char *status_header, const char *max_header, 
	const char *status_type, const char *max_value)
{
	const char *error;

	if ( !ext_spamvirustest_header_spec_parse
		(&ext_data->status_header, ext_data->pool, status_header, &error) ) {
		sieve_sys_error("%s: invalid status header specification "
			"'%s': %s", ext_name, status_header, error);
		return FALSE;
	}

	if ( max_header != NULL && !ext_spamvirustest_header_spec_parse
		(&ext_data->max_header, ext_data->pool, max_header, &error) ) {
		sieve_sys_error("%s: invalid max header specification "
			"'%s': %s", ext_name, max_header, error);
		return FALSE;
	}

	if ( status_type == NULL || strcmp(status_type, "value") == 0 ) {
		ext_data->status_type = EXT_SPAMVIRUSTEST_STATUS_TYPE_VALUE;
	} else if ( strcmp(status_type, "strlen") == 0 ) {
		ext_data->status_type = EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN;
	} else if ( strcmp(status_type, "yesno") == 0 ) {
		ext_data->status_type = EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN;
	} else {
		sieve_sys_error("%s: invalid status type '%s'", ext_name, status_type);
		return FALSE;
	}

	if ( max_value != NULL ) {
		switch ( ext_data->status_type ) {
		case EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN:
		case EXT_SPAMVIRUSTEST_STATUS_TYPE_VALUE:	
			if ( !ext_spamvirustest_parse_decimal_value
				(max_value, &ext_data->max_value, &error) ) {
				sieve_sys_error("%s: invalid max value specification "
					"'%s': %s", ext_name, max_value, error);
				return FALSE;
			}
			break;
		case EXT_SPAMVIRUSTEST_STATUS_TYPE_YESNO:
			ext_data->yes_string = p_strdup(ext_data->pool, max_value);
			ext_data->max_value = 1;
			break;
		}	
	}

	return TRUE;
}

static void ext_spamvirustest_config_free(struct ext_spamvirustest_data *ext_data)
{
	ext_spamvirustest_header_spec_free(&ext_data->status_header);
	ext_spamvirustest_header_spec_free(&ext_data->max_header);
}

bool ext_spamvirustest_load(const struct sieve_extension *ext, void **context)
{
	struct sieve_instance *svinst = ext->svinst;
	struct ext_spamvirustest_data *ext_data;
	const char *status_header, *max_header, *status_type, *max_value;
	const char *ext_name;
	pool_t pool;

	if ( *context != NULL ) {
		ext_spamvirustest_unload(ext);
	}

	/* FIXME: 
	 *   Prevent loading of both spamtest and spamtestplus: let these share 
	 *   contexts.
	 */

	if ( sieve_extension_is(ext, spamtest_extension) || 
		sieve_extension_is(ext, spamtestplus_extension) ) {
		ext_name = spamtest_extension.name;
	} else {
		ext_name = sieve_extension_name(ext);
	}

	/* Get settings */

	status_header = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_status_header", NULL));
	max_header = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_max_header", NULL));
	status_type = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_status_type", NULL));
	max_value = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_max_value", NULL));

	/* Verify settings */

	if ( status_header == NULL ) {
		return TRUE;
	}

	if ( max_header != NULL && max_value != NULL ) {
		sieve_sys_error("%s: sieve_%s_max_header and sieve_%s_max_value "
			"cannot both be configured", ext_name, ext_name, ext_name);
		return TRUE;
	}

	if ( max_header == NULL && max_value == NULL ) {
		sieve_sys_error("%s: none of sieve_%s_max_header or sieve_%s_max_value "
			"is configured", ext_name, ext_name, ext_name);
		return TRUE;
	}

	/* Pre-process configuration */

	pool = pool_alloconly_create("spamvirustest_data", 512);
	ext_data = p_new(pool, struct ext_spamvirustest_data, 1);
	ext_data->pool = pool;

	if ( !ext_spamvirustest_config_load
		(ext_data, ext_name, status_header, max_header, status_type, max_value) ) {
		sieve_sys_warning("%s: extension not configured, "
			"tests will always match against \"0\"", ext_name);
		ext_spamvirustest_config_free(ext_data);
		pool_unref(&ext_data->pool);
	} else { 
		*context = (void *) ext_data;
	}

	return TRUE;
}

void ext_spamvirustest_unload(const struct sieve_extension *ext)
{
	struct ext_spamvirustest_data *ext_data = 
		(struct ext_spamvirustest_data *) ext->context;
	
	ext_spamvirustest_header_spec_free(&ext_data->status_header);
	ext_spamvirustest_header_spec_free(&ext_data->max_header);
	
	pool_unref(&ext_data->pool);
}

/*
 * Score extraction
 */

const char *ext_spamvirustest_get_value
(const struct sieve_runtime_env *renv, const struct sieve_extension *ext,
	 bool percent)
{
	static const char *VALUE_FAILED = "0";
	struct ext_spamvirustest_data *ext_data = 
		(struct ext_spamvirustest_data *) ext->context;	
	struct ext_spamvirustest_header_spec *status_header, *max_header;
	const struct sieve_message_data *msgdata = renv->msgdata;
	const char *ext_name = sieve_extension_name(ext);
	regmatch_t match_values[2];
	const char *header_value, *error;
	const char *status = NULL, *max = NULL, *yes = NULL;
	float status_value, max_value;
	int value;

	/* 
	 * Check whether extension is properly configured 
	 */
	if ( ext_data == NULL ) {
		sieve_runtime_trace(renv, "%s: extension not configured", ext_name);
		return VALUE_FAILED;
	}

	status_header = &ext_data->status_header;
	max_header = &ext_data->max_header;

	/*
	 * Get max status value
	 */	

	if ( max_header->header_name != NULL ) {
		/* Get header from message */
		if ( mail_get_first_header_utf8
			(msgdata->mail, max_header->header_name, &header_value) < 0 ||
			header_value == NULL ) {
			sieve_runtime_trace(renv, "%s: header '%s' not found in message", 
				ext_name, max_header->header_name);
			return VALUE_FAILED;
		}

		if ( max_header->regexp_match ) {
			/* Execute regex */
			if ( regexec(&max_header->regexp, header_value, 2, match_values, 0) 
				!= 0 ) {
				sieve_runtime_trace(renv, "%s: regexp for header '%s' did not match "
					"on value '%s'", ext_name, max_header->header_name, header_value);
				return VALUE_FAILED;
			}

			max = _regexp_match_get_value(header_value, 1, match_values, 2);
			if ( max == NULL ) {
				sieve_runtime_trace(renv, "%s: regexp did not return match value "
					"for string '%s'", ext_name, header_value);
				return VALUE_FAILED;
			}
		} else {
			max = header_value;
		}

		switch ( ext_data->status_type ) {
		case EXT_SPAMVIRUSTEST_STATUS_TYPE_VALUE:	
		case EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN:
			if ( !ext_spamvirustest_parse_decimal_value(max, &max_value, &error) ) {
				sieve_runtime_trace(renv, "%s: failed to parse maximum value: %s", 
					ext_name, error);
				return VALUE_FAILED;
			}
			break;
		case EXT_SPAMVIRUSTEST_STATUS_TYPE_YESNO:
			yes = max;
			max_value = 1;
			break;
		}	
	} else {
		yes = ext_data->yes_string;
		max_value = ext_data->max_value;
	}

	if ( max_value == 0 ) {
		sieve_runtime_trace(renv, "%s: max value is 0", ext_name);
		return VALUE_FAILED;
	}

	/*
	 * Get status value
	 */

	/* Get header from message */
	if ( mail_get_first_header_utf8
		(msgdata->mail, status_header->header_name, &header_value) < 0 ||
		header_value == NULL ) {
		sieve_runtime_trace(renv, "%s: header '%s' not found in message", 
			ext_name, status_header->header_name);
		return VALUE_FAILED;
	}

	/* Execute regex */
	if ( status_header->regexp_match ) {
		if ( regexec(&status_header->regexp, header_value, 2, match_values, 0) 
			!= 0 ) {
			sieve_runtime_trace(renv, "%s: regexp for header '%s' did not match "
				"on value '%s'", ext_name, status_header->header_name, header_value);
			return VALUE_FAILED;
		}

		status = _regexp_match_get_value(header_value, 1, match_values, 2);
		if ( status == NULL ) {
			sieve_runtime_trace(renv, "%s: regexp did not return match value "
				"for string '%s'", ext_name, header_value);
			return VALUE_FAILED;
		}
	} else {
		status = header_value;
	}

	switch ( ext_data->status_type ) {
	case EXT_SPAMVIRUSTEST_STATUS_TYPE_VALUE:
		if ( !ext_spamvirustest_parse_decimal_value(status, &status_value, &error) 
			) {
			sieve_runtime_trace(renv, "%s: failed to parse status value '%s': %s", 
				ext_name, status, error);
			return VALUE_FAILED;
		}
		break;
	case EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN:
		if ( !ext_spamvirustest_parse_strlen_value(status, &status_value, &error) 
			) {
			sieve_runtime_trace(renv, "%s: failed to parse status value '%s': %s", 
				ext_name, status, error);
			return VALUE_FAILED;
		}
		break;
	case EXT_SPAMVIRUSTEST_STATUS_TYPE_YESNO:
		if ( strcmp(status, yes) == 0 )
			status_value = 1;
		else
			status_value = 0;
		break;
	}
	
	/* Calculate value */
	if ( status_value < 0 ) {
		value = 1;
	} else {
		if ( percent )
			value = (status_value / max_value) * 99 + 1;
		else
			value = (status_value / max_value) * 9 + 1;
	}

	return t_strdup_printf("%d", value);;
}


