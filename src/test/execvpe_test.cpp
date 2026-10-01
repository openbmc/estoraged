#include "execvpe.hpp"

#include <fcntl.h>
#include <unistd.h>

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

TEST(ExecvpeTest, EmptyArgv)
{
    std::span<const std::string_view> emptyArgv;
    EXPECT_EQ(estoraged::execvpe(emptyArgv), -1);
    EXPECT_EQ(errno, EINVAL);
}

TEST(ExecvpeTest, SuccessCommand)
{
    std::vector<std::string_view> argv = {"true"};
    EXPECT_EQ(estoraged::execvpe(argv), 0);
}

TEST(ExecvpeTest, FailureCommand)
{
    std::vector<std::string_view> argv = {"false"};
    EXPECT_NE(estoraged::execvpe(argv), 0);
}

TEST(ExecvpeTest, CommandWithArguments)
{
    std::vector<std::string_view> argv = {"echo", "test"};
    EXPECT_EQ(estoraged::execvpe(argv), 0);
}

TEST(ExecvpeTest, ArgumentWithSpacesAndShellMetachars)
{
    std::vector<std::string_view> argv = {"test", "arg with spaces; false", "=",
                                          "arg with spaces; false"};
    EXPECT_EQ(estoraged::execvpe(argv), 0);
}

TEST(ExecvpeTest, SpecificExitStatus)
{
    std::vector<std::string_view> argv = {"sh", "-c", "exit 42"};
    EXPECT_EQ(estoraged::execvpe(argv), 42);
}

TEST(ExecvpeTest, EnvironmentInheritance)
{
    const char* envName = "ESTORAGED_TEST_ENV_VAR";
    const char* envVal = "test_value_12345";
    ASSERT_EQ(setenv(envName, envVal, 1), 0);

    std::vector<std::string_view> argv = {
        "sh", "-c", "test \"$ESTORAGED_TEST_ENV_VAR\" = \"test_value_12345\""};
    EXPECT_EQ(estoraged::execvpe(argv), 0);

    unsetenv(envName);
}

TEST(ExecvpeTest, TerminatedBySignalInt)
{
    EXPECT_EQ(estoraged::execvpe({"bash", "-c", "kill -SIGINT $$"}),
              128 + SIGINT);
}

TEST(ExecvpeTest, TerminatedBySignalKill)
{
    EXPECT_EQ(estoraged::execvpe({"bash", "-c", "kill -SIGKILL $$"}),
              128 + SIGKILL);
}

TEST(ExecvpeTest, TerminatedBySignalTerm)
{
    EXPECT_EQ(estoraged::execvpe({"bash", "-c", "kill -SIGTERM $$"}),
              128 + SIGTERM);
}

TEST(ExecvpeTest, CloseRangeFileDescriptors)
{
    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);
    EXPECT_EQ(fcntl(pipeFds[0], F_SETFD, 0), 0);
    EXPECT_EQ(fcntl(pipeFds[1], F_SETFD, 0), 0);

    std::string readFdStr = std::to_string(pipeFds[0]);
    std::string checkCmd = "test ! -e /proc/self/fd/" + readFdStr;
    std::vector<std::string_view> argv = {"sh", "-c", checkCmd};
    EXPECT_EQ(estoraged::execvpe(argv), 0);

    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST(ExecvpeTest, NonExistentCommand)
{
    std::vector<std::string_view> argv = {"/nonexistent_command_12345"};
    EXPECT_EQ(estoraged::execvpe(argv), 127);
}

TEST(ExecvpeTest, NonExecutableCommand)
{
    std::vector<std::string_view> argv = {"/dev/null"};
    EXPECT_EQ(estoraged::execvpe(argv), 127);
}

} // namespace estoraged_test
