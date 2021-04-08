#include "embedded_cli.h"
#include "CliMock.h"

#include <catch.hpp>

void setVectorString(std::vector<char> &buffer, const std::string &str) {
    buffer.reserve(str.size() + 1);
    std::copy(str.begin(), str.end(), buffer.begin());
    buffer[str.size()] = '\0';
}

void runTestsForCli(EmbeddedCli *cli);

TEST_CASE("EmbeddedCli", "[cli]") {
    EmbeddedCli *cli = embeddedCliNewDefault();

    REQUIRE(cli != nullptr);

    runTestsForCli(cli);

    embeddedCliFree(cli);
}

TEST_CASE("EmbeddedCli. Static allocation", "[cli]") {
    EmbeddedCliConfig *config = embeddedCliDefaultConfig();

    SECTION("Can't create from small buffer") {
        std::vector<uint8_t> data(16);
        config->cliBuffer = data.data();
        config->cliBufferSize = 16;
        REQUIRE(embeddedCliNew(config) == nullptr);
    }

    std::vector<uint8_t> data(1024);
    config->cliBuffer = data.data();
    config->cliBufferSize = 1024;
    EmbeddedCli *cli = embeddedCliNew(config);

    REQUIRE(cli != nullptr);

    runTestsForCli(cli);

    embeddedCliFree(cli);
}

TEST_CASE("EmbeddedCli. Tokens", "[cli][token]") {
    std::vector<char> buffer;
    buffer.resize(30, '!');
    buffer.resize(32, '\0');

    SECTION("Tokenize simple string") {
        setVectorString(buffer, "a b c");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(buffer[0] == 'a');
        REQUIRE(buffer[1] == '\0');
        REQUIRE(buffer[2] == 'b');
        REQUIRE(buffer[3] == '\0');
        REQUIRE(buffer[4] == 'c');
        REQUIRE(buffer[5] == '\0');
        REQUIRE(buffer[6] == '\0');
    }

    SECTION("Tokenize string with duplicating separators") {
        setVectorString(buffer, "   a  b    c   ");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(buffer[0] == 'a');
        REQUIRE(buffer[1] == '\0');
        REQUIRE(buffer[2] == 'b');
        REQUIRE(buffer[3] == '\0');
        REQUIRE(buffer[4] == 'c');
        REQUIRE(buffer[5] == '\0');
        REQUIRE(buffer[6] == '\0');
    }

    SECTION("Tokenize string with long tokens") {
        setVectorString(buffer, "abcd ef");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(buffer[0] == 'a');
        REQUIRE(buffer[1] == 'b');
        REQUIRE(buffer[2] == 'c');
        REQUIRE(buffer[3] == 'd');
        REQUIRE(buffer[4] == '\0');
        REQUIRE(buffer[5] == 'e');
        REQUIRE(buffer[6] == 'f');
        REQUIRE(buffer[7] == '\0');
        REQUIRE(buffer[8] == '\0');
    }

    SECTION("Tokenize string of separators") {
        setVectorString(buffer, "      ");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(buffer[0] == '\0');
        REQUIRE(buffer[1] == '\0');
    }

    SECTION("Tokenize empty string") {
        setVectorString(buffer, "");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(buffer[0] == '\0');
        REQUIRE(buffer[1] == '\0');
    }

    SECTION("Tokenize null") {
        embeddedCliTokenizeArgs(nullptr);
    }

    SECTION("Get tokens") {
        setVectorString(buffer, "abcd efg");
        embeddedCliTokenizeArgs(buffer.data());

        const char *tok0 = embeddedCliGetToken(buffer.data(), 0);
        const char *tok1 = embeddedCliGetToken(buffer.data(), 1);
        const char *tok2 = embeddedCliGetToken(buffer.data(), 2);

        REQUIRE(tok0 != nullptr);
        REQUIRE(tok1 != nullptr);
        REQUIRE(tok2 == nullptr);

        REQUIRE(std::string(tok0) == "abcd");
        REQUIRE(std::string(tok1) == "efg");
    }

    SECTION("Get tokens from empty string") {
        setVectorString(buffer, "");
        embeddedCliTokenizeArgs(buffer.data());

        const char *tok0 = embeddedCliGetToken(buffer.data(), 0);

        REQUIRE(tok0 == nullptr);
    }

    SECTION("Get token from null string") {
        const char *tok0 = embeddedCliGetToken(nullptr, 0);

        REQUIRE(tok0 == nullptr);
    }

    SECTION("Get token count") {
        setVectorString(buffer, "a b c");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(embeddedCliGetTokenCount(buffer.data()) == 3);
    }

    SECTION("Get token count from empty string") {
        setVectorString(buffer, "");
        embeddedCliTokenizeArgs(buffer.data());

        REQUIRE(embeddedCliGetTokenCount(buffer.data()) == 0);
    }

    SECTION("Get token count for null string") {
        REQUIRE(embeddedCliGetTokenCount(nullptr) == 0);
    }
}

void runTestsForCli(EmbeddedCli *cli) {
    CliMock mock(cli);
    auto &commands = mock.getReceivedCommands();

    SECTION("Test single command") {
        for (int i = 0; i < 50; ++i) {
            mock.sendLine("set led 1 " + std::to_string(i));

            embeddedCliProcess(cli);

            REQUIRE(commands.size() == i + 1);
            REQUIRE(commands.back().name == "set");
            REQUIRE(commands.back().args == ("led 1 " + std::to_string(i)));
        }
    }

    SECTION("Test sending by parts") {
        mock.sendStr("set ");
        embeddedCliProcess(cli);
        REQUIRE(commands.empty());

        mock.sendStr("led 1");
        embeddedCliProcess(cli);
        REQUIRE(commands.empty());

        mock.sendLine(" 1");
        embeddedCliProcess(cli);
        REQUIRE(!commands.empty());

        REQUIRE(commands.back().name == "set");
        REQUIRE(commands.back().args == "led 1 1");
    }

    SECTION("Test sending multiple commands") {
        for (int i = 0; i < 3; ++i) {
            mock.sendLine("set led 1 " + std::to_string(i));
        }
        embeddedCliProcess(cli);

        REQUIRE(commands.size() == 3);
        for (int i = 0; i < 3; ++i) {
            REQUIRE(commands[i].name == "set");
            REQUIRE(commands[i].args == ("led 1 " + std::to_string(i)));
        }
    }

    SECTION("Test buffer overflow recovery") {
        for (int i = 0; i < 100; ++i) {
            mock.sendLine("set led 1 " + std::to_string(i));
        }
        embeddedCliProcess(cli);
        REQUIRE(commands.size() < 100);
        commands.clear();

        mock.sendLine("set led 1 150");
        embeddedCliProcess(cli);
        REQUIRE(commands.size() == 1);
        REQUIRE(commands.back().name == "set");
        REQUIRE(commands.back().args == "led 1 150");
    }

    SECTION("Test removing some chars") {
        mock.sendLine("s\bget led\b\b\bjack 1\b56\b");

        embeddedCliProcess(cli);

        REQUIRE(commands.back().name == "get");
        REQUIRE(commands.back().args == "jack 5");
    }

    SECTION("Test removing all chars") {
        mock.sendLine("set\b\b\b\b\bget led");

        embeddedCliProcess(cli);

        REQUIRE(commands.back().name == "get");
        REQUIRE(commands.back().args == "led");
    }

    SECTION("Test printing") {
        SECTION("Print with no command input") {
            embeddedCliPrint(cli, "test print");

            REQUIRE(mock.getRawOutput() == "test print\r\n");
        }

        SECTION("Print with intermediate command") {
            mock.sendStr("some cmd");

            embeddedCliProcess(cli);

            embeddedCliPrint(cli, "print");

            REQUIRE(mock.getOutput() == "print\r\nsome cmd");
        }
    }

    SECTION("Unknown command handling") {

        SECTION("Providing unknown command") {
            // unknown commands are only possible when onCommand callback is not set
            cli->onCommand = nullptr;
            mock.sendLine("get led");

            embeddedCliProcess(cli);

            REQUIRE(mock.getRawOutput().find("Unknown command") != std::string::npos);
        }

        SECTION("Providing known command without binding") {
            embeddedCliAddBinding(cli, {
                    "get",
                    nullptr,
                    false,
                    nullptr,
                    nullptr
            });

            mock.sendLine("get led");

            embeddedCliProcess(cli);

            REQUIRE(!commands.empty());
        }

        SECTION("Providing known command with binding") {
            mock.addCommandBinding("get");

            mock.sendLine("get led");

            embeddedCliProcess(cli);

            REQUIRE(commands.empty());
            auto &cmds = mock.getReceivedKnownCommands();
            REQUIRE_FALSE(cmds.empty());
            REQUIRE(cmds.back().name == "get");
            REQUIRE(cmds.back().args == "led");
        }
    }

    SECTION("Help command handling") {
        SECTION("Calling help with bindings") {
            mock.addCommandBinding("get", "Get specific parameter");
            mock.addCommandBinding("set", "Set specific parameter");

            mock.sendLine("help");

            embeddedCliProcess(cli);

            REQUIRE(commands.empty());
            REQUIRE(mock.getRawOutput().find("get") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("Get specific parameter") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("set") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("Set specific parameter") != std::string::npos);
        }

        SECTION("Calling help for known command") {
            mock.addCommandBinding("get", "Get specific parameter");
            mock.addCommandBinding("set", "Set specific parameter");

            mock.sendLine("help get");

            embeddedCliProcess(cli);

            REQUIRE(commands.empty());
            REQUIRE(mock.getRawOutput().find("get") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("Get specific parameter") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("set") == std::string::npos);
            REQUIRE(mock.getRawOutput().find("Set specific parameter") == std::string::npos);
        }

        SECTION("Calling help for unknown command") {
            mock.addCommandBinding("set", "Set specific parameter");

            mock.sendLine("help get");

            embeddedCliProcess(cli);

            REQUIRE(commands.empty());
            REQUIRE(mock.getRawOutput().find("get") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("Unknown") != std::string::npos);
        }

        SECTION("Calling help for command without help") {
            mock.addCommandBinding("get");

            mock.sendLine("help get");

            embeddedCliProcess(cli);

            REQUIRE(commands.empty());
            REQUIRE(mock.getRawOutput().find("get") != std::string::npos);
            REQUIRE(mock.getRawOutput().find("No help") != std::string::npos);
        }

        SECTION("Calling help with multiple arguments") {
            mock.addCommandBinding("get");

            mock.sendLine("help get set");

            embeddedCliProcess(cli);

            REQUIRE(commands.empty());
            REQUIRE(mock.getRawOutput().find("Command \"help\" receives one or zero arguments") != std::string::npos);
            REQUIRE(mock.getRawOutput().rfind("get") < 10);
        }
    }

    SECTION("Autocomplete") {
        mock.addCommandBinding("get");
        mock.addCommandBinding("set");
        mock.addCommandBinding("get-new");
        mock.addCommandBinding("reset-first");
        mock.addCommandBinding("reset-second");

        SECTION("Autocomplete when single candidate") {
            mock.sendStr("s\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getRawOutput() == "set ");
        }

        SECTION("Submit autocompleted command") {
            mock.sendLine("s\t");

            embeddedCliProcess(cli);

            REQUIRE(!mock.getReceivedKnownCommands().empty());
            REQUIRE(mock.getReceivedKnownCommands().back().name == "set");
        }

        SECTION("Submit autocompleted command when multiple candidates") {
            mock.sendLine("g\t");

            embeddedCliProcess(cli);

            REQUIRE(!mock.getReceivedKnownCommands().empty());
            REQUIRE(mock.getReceivedKnownCommands().back().name == "get");
        }

        SECTION("Autocomplete help command") {
            mock.sendStr("h\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getRawOutput() == "help ");
        }

        SECTION("Autocomplete when multiple candidates with common prefix") {
            mock.sendStr("g\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getOutput() == "get");
        }

        SECTION("Autocomplete when multiple candidates with common prefix and not common suffix") {
            mock.sendStr("r\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getOutput() == "reset-");
        }

        SECTION("Autocomplete when multiple candidates and one is help") {
            mock.addCommandBinding("hello");

            mock.sendStr("hel\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getOutput() == "help\r\nhello\r\nhel");
        }

        SECTION("Autocomplete when multiple candidates without common prefix") {
            mock.sendStr("get\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getOutput() == "get\r\nget-new\r\nget");
        }

        SECTION("Autocomplete when no candidates") {
            mock.sendStr("m\t");

            embeddedCliProcess(cli);

            REQUIRE(mock.getRawOutput() == "m");
        }
    }
}
