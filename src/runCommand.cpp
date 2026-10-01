#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "runCommand.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <span>
#include <vector>

namespace estoraged
{

int runCommand(std::span<const stdplus::zstring_view> argv)
{
    if (argv.empty())
    {
        errno = EINVAL;
        return -1;
    }

    std::vector<const char*> argPtrs;
    argPtrs.reserve(argv.size() + 1);
    for (auto& s : argv)
    {
        argPtrs.push_back(s.data());
    }
    argPtrs.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0)
    {
        return -1;
    }

    if (pid == 0)
    {
        close_range(3, ~0U, 0);
        ::execvpe(argPtrs[0], const_cast<char* const*>(argPtrs.data()),
                  environ);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }
    return -1;
}

} // namespace estoraged
