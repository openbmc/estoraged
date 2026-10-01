#include "runCommand.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace estoraged_test
{

/*
 * Restores the default disposition of a signal for the lifetime of this object.
 * Signals that are ignored (SIG_IGN) remain ignored across execve() and a
 * non-interactive shell cannot reset a signal that was ignored when it was
 * started. Hence, if the test runner was started with e.g. SIGINT ignored, as
 * happens for background jobs, "kill -INT $$" would not terminate the shell.
 */
class ScopedDefaultSignal
{
  public:
    explicit ScopedDefaultSignal(int signum) : signum(signum)
    {
        struct sigaction dfl{};
        dfl.sa_handler = SIG_DFL;
        sigemptyset(&dfl.sa_mask);
        EXPECT_EQ(sigaction(signum, &dfl, &old), 0);
    }

    ~ScopedDefaultSignal()
    {
        sigaction(signum, &old, nullptr);
    }

    ScopedDefaultSignal(const ScopedDefaultSignal&) = delete;
    ScopedDefaultSignal& operator=(const ScopedDefaultSignal&) = delete;
    ScopedDefaultSignal(ScopedDefaultSignal&&) = delete;
    ScopedDefaultSignal& operator=(ScopedDefaultSignal&&) = delete;

  private:
    int signum;
    struct sigaction old{};
};

TEST(RunCommandTest, EmptyArgv)
{
    std::span<const stdplus::zstring_view> emptyArgv;
    EXPECT_EQ(estoraged::runCommand(emptyArgv), -1);
    EXPECT_EQ(errno, EINVAL);
}

TEST(RunCommandTest, SuccessCommand)
{
    std::vector<stdplus::zstring_view> argv = {"true"};
    EXPECT_EQ(estoraged::runCommand(argv), 0);
}

TEST(RunCommandTest, FailureCommand)
{
    std::vector<stdplus::zstring_view> argv = {"false"};
    EXPECT_NE(estoraged::runCommand(argv), 0);
}

TEST(RunCommandTest, CommandWithArguments)
{
    std::vector<stdplus::zstring_view> argv = {"echo", "test"};
    EXPECT_EQ(estoraged::runCommand(argv), 0);
}

TEST(RunCommandTest, ArgumentWithSpacesAndShellMetachars)
{
    std::vector<stdplus::zstring_view> argv = {"test", "arg with spaces; false",
                                               "=", "arg with spaces; false"};
    EXPECT_EQ(estoraged::runCommand(argv), 0);
}

TEST(RunCommandTest, SpecificExitStatus)
{
    std::vector<stdplus::zstring_view> argv = {"sh", "-c", "exit 42"};
    EXPECT_EQ(estoraged::runCommand(argv), 42);
}

TEST(RunCommandTest, EnvironmentInheritance)
{
    const char* envName = "ESTORAGED_TEST_ENV_VAR";
    const char* envVal = "test_value_12345";
    ASSERT_EQ(setenv(envName, envVal, 1), 0);

    std::vector<stdplus::zstring_view> argv = {
        "sh", "-c", R"(test "$ESTORAGED_TEST_ENV_VAR" = "test_value_12345")"};
    EXPECT_EQ(estoraged::runCommand(argv), 0);

    unsetenv(envName);
}

TEST(RunCommandTest, TerminatedBySignalInt)
{
    ScopedDefaultSignal dflInt(SIGINT);
    EXPECT_EQ(estoraged::runCommand({"sh", "-c", "kill -INT $$"}),
              128 + SIGINT);
}

TEST(RunCommandTest, TerminatedBySignalKill)
{
    EXPECT_EQ(estoraged::runCommand({"sh", "-c", "kill -KILL $$"}),
              128 + SIGKILL);
}

TEST(RunCommandTest, TerminatedBySignalTerm)
{
    ScopedDefaultSignal dflTerm(SIGTERM);
    EXPECT_EQ(estoraged::runCommand({"sh", "-c", "kill -TERM $$"}),
              128 + SIGTERM);
}

TEST(RunCommandTest, CloseRangeFileDescriptors)
{
    std::array<int, 2> pipeFds();
    ASSERT_EQ(pipe(pipeFds.data()), 0);
    EXPECT_EQ(fcntl(pipeFds[0], F_SETFD, 0), 0);
    EXPECT_EQ(fcntl(pipeFds[1], F_SETFD, 0), 0);

    std::string readFdStr = std::to_string(pipeFds[0]);
    std::string checkCmd = "test ! -e /proc/self/fd/" + readFdStr;
    std::vector<stdplus::zstring_view> argv = {"sh", "-c", checkCmd};
    EXPECT_EQ(estoraged::runCommand(argv), 0);

    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST(RunCommandTest, NonExistentCommand)
{
    std::vector<stdplus::zstring_view> argv = {"/nonexistent_command_12345"};
    EXPECT_EQ(estoraged::runCommand(argv), 127);
}

TEST(RunCommandTest, NonExecutableCommand)
{
    std::vector<stdplus::zstring_view> argv = {"/dev/null"};
    EXPECT_EQ(estoraged::runCommand(argv), 127);
}

} // namespace estoraged_test
