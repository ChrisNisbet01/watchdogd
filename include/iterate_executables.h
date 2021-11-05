#pragma once

#include <stdbool.h>

void
iterate_executable_files(
	char const * const pattern,
	bool (*cb)(char const * filename, void * user_ctx),
	void * user_ctx);

