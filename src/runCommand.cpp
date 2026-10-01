#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "runCommand.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <span>
#include <string>
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

    // execvpe() expects a 'char* const*' argument vector. Copy the arguments
    // into mutable strings to avoid having to cast away constness. Do this
    // before fork() since memory allocation is not async-signal-safe.
    std::vector<std::string> args;
    args.reserve(argv.size());
    for (const auto& arg : argv)
    {
        args.emplace_back(arg.data(), arg.size());
    }

    std::vector<char*> argPtrs;
    argPtrs.reserve(args.size() + 1);
    for (auto& arg : args)
    {
        argPtrs.push_back(arg.data());
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
        ::execvpe(argPtrs[0], argPtrs.data(), environ);
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
