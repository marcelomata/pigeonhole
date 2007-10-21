#ifndef __SIEVE_EXTENSIONS_H__
#define __SIEVE_EXTENSIONS_H__

#include "lib.h"
#include "sieve-validator.h"

struct sieve_extension {
	const char *name;
	
	bool (*validator_load)(struct sieve_validator *validator);
	bool (*generator_load)(struct sieve_generator *generator);
	
	bool (*opcode_dump)(struct sieve_interpreter *interpreter);
	bool (*opcode_execute)(struct sieve_interpreter *interpreter);
};

const struct sieve_extension *sieve_extension_acquire(const char *extension);

#endif /* __SIEVE_EXTENSIONS_H__ */
