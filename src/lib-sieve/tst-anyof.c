#include "sieve-commands.h"
#include "sieve-commands-private.h"
#include "sieve-validator.h"

bool tst_anyof_validate(struct sieve_validator *validator, struct sieve_command_context *tst) 
{
	/* Check envelope test syntax (optional tags are registered above):
	 *   anyof <tests: test-list>   
	 */
	if ( !sieve_validate_command_arguments(validator, tst, 0, NULL) ||
		!sieve_validate_command_subtests(validator, tst, 2) ) 
		return FALSE;
	
	return TRUE;
}

bool tst_anyof_generate	
	(struct sieve_generator *generator, struct sieve_command_context *ctx,
		struct sieve_jumplist *jumps, bool jump_true)
{
	struct sieve_ast_node *test;
	struct sieve_jumplist true_jumps;
	
	if ( !jump_true ) {
		/* Prepare jumplist */
		sieve_jumplist_init(&true_jumps);
	}
	
	test = sieve_ast_test_first(ctx->ast_node);
	while ( test != NULL ) {	
		/* If this test list must jump on true, all sub-tests can simply add their jumps
		 * to the caller's jump list, otherwise this test redirects all true jumps to the 
		 * end of the currently generated code. This is just after a final jump to the false
		 * case 
		 */
		if ( !jump_true ) 
			sieve_generate_test(generator, test, &true_jumps, TRUE);
		else
			sieve_generate_test(generator, test, jumps, TRUE);
		
		test = sieve_ast_test_next(test);
	}	
	
	if ( !jump_true ) {
		/* All tests failed, jump to case FALSE */
		sieve_generator_emit_core_opcode(generator, SIEVE_OPCODE_JMP);
		sieve_jumplist_add(jumps, sieve_generator_emit_offset(generator, 0));
		
		/* All true exits jump here */
		sieve_jumplist_resolve(&true_jumps, generator);
	}
		
	return TRUE;
}
