#pragma once

#include <stdplus/zstring_view.hpp>

#include <initializer_list>
#include <span>

namespace estoraged
{

/** @brief Executes command argv[0] with arguments argv[1..].
 *  @param[in] argv - Argument vector where argv[0] is the command to run.
 *  @return A return value similar to the return value of the system() C library
 *   function (see also
 *   https://pubs.opengroup.org/onlinepubs/7908799/xsh/system.html):
 *   - -1 if argv is empty or if fork() fails. errno will be set.
 *   - 127 if the command couldn't be started.
 *   - 128 + signal if the created process has been terminated by a signal.
 *   - 0..126 if the command has been executed. The returned value is the exit
 *     code of the command that has been run.
 */
int runCommand(std::span<const stdplus::zstring_view> argv);

inline int runCommand(std::initializer_list<stdplus::zstring_view> argv)
{
    return runCommand(
        std::span<const stdplus::zstring_view>(argv.begin(), argv.size()));
}

} // namespace estoraged
