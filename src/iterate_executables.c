#include "iterate_executables.h"

#include <glob.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void
iterate_files_in_directory(
    char const * const directory_pattern,
    bool (*cb)(char const * filename, void * user_ctx),
    void * const user_ctx)
{
    glob_t globbuf;

    if (directory_pattern == NULL)
    {
        goto done;
    }

    memset(&globbuf, 0, sizeof globbuf);

    glob(directory_pattern, 0, NULL, &globbuf);

    for (size_t i = 0; i < globbuf.gl_pathc; i++)
    {
        if (!cb(globbuf.gl_pathv[i], user_ctx))
        {
            break;
        }
    }

    globfree(&globbuf);

done:
    return;
}

static bool
is_executable(char const * const filename)
{
	struct stat sb;

	return stat(filename, &sb) == 0 && (sb.st_mode & S_IXUSR) != 0;
}

struct iterate_exe_st
{
    void * user_ctx;
    bool (*cb)(char const * filename, void * user_ctx);
};

static bool
iterate_exes_cb(char const * filename, void * user_ctx)
{
    struct iterate_exe_st const * const exe_ctx = user_ctx;
    bool continue_iteration;

    if (is_executable(filename))
    {
        continue_iteration = exe_ctx->cb(filename, exe_ctx->user_ctx);
    }
    else
    {
        continue_iteration = true;
    }

    return continue_iteration;
}

void
iterate_executable_files(
	char const * const pattern,
	bool (*cb)(char const * filename, void * user_ctx),
	void * const user_ctx)
{
    struct iterate_exe_st ctx =
    {
        .cb = cb,
        .user_ctx = user_ctx
    };

    iterate_files_in_directory(pattern, iterate_exes_cb, &ctx);
}

