#pragma once

#include <initializer_list>
#include <span>
#include <string_view>

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
int execvpe(std::span<const std::string_view> argv);

inline int execvpe(std::initializer_list<std::string_view> argv)
{
    return execvpe(
        std::span<const std::string_view>(argv.begin(), argv.size()));
}

} // namespace estoraged
