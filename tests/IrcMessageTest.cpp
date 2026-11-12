#include "IrcMessage.hpp"

#include <cstddef>
#include <iostream>
#include <string>

struct TestCase
{
    std::string input;
    bool success;
    const char *command;
    const char *params[4];
    std::size_t param_count;
};

int main()
{
    const TestCase cases[] = {
        {"NICK taro", true, "NICK", {"taro"}, 1},
        {"pRiVmSg #42 :hello world",
            true, "PRIVMSG", {"#42", "hello world"}, 2},
        {"PASS SeCrEt", true, "PASS", {"SeCrEt"}, 1},
        {"USER hugh 0 * :Hugh Example",
            true, "USER", {"hugh", "0", "*", "Hugh Example"}, 4},

        // 余分な区切り空白を読み飛ばす。
        {"  NICK   taro  ", true, "NICK", {"taro"}, 1},

        // 末尾引数の空白やコロンを保持する。
        {"PRIVMSG #42 : hello  world ",
            true, "PRIVMSG", {"#42", " hello  world "}, 2},
        {"PRIVMSG #42 :hello:world",
            true, "PRIVMSG", {"#42", "hello:world"}, 2},
        {"PRIVMSG #42 :", true, "PRIVMSG", {"#42", ""}, 2},

        // 引数の途中にあるコロンは通常の文字。
        {"PASS abc:def", true, "PASS", {"abc:def"}, 1},

        // 引数不足や未知のコマンドも、分解自体は成功する。
        {"PRIVMSG #42", true, "PRIVMSG", {"#42"}, 1},
        {"NICK", true, "NICK", {0}, 0},
        {"UNKNOWN value", true, "UNKNOWN", {"value"}, 1},
        {"001 taro :Welcome",
            true, "001", {"taro", "Welcome"}, 2},

        // 空行・構文不正。
        {"", false, "", {0}, 0},
        {"   ", false, "", {0}, 0},
        {":", false, "", {0}, 0},
        {"NI!CK taro", false, "", {0}, 0},
        {"12 taro", false, "", {0}, 0},
        {"NICK taro\r\n", false, "", {0}, 0},
        {"NICK taro\nJOIN #42", false, "", {0}, 0},
        {std::string("NICK\0 taro", 10), false, "", {0}, 0}
    };

    const std::size_t count = sizeof(cases) / sizeof(cases[0]);
    std::size_t failures = 0;
    IrcMessage message;

    for (std::size_t i = 0; i < count; ++i)
    {
        const TestCase &test = cases[i];

        // 成功時・失敗時とも、古い結果が残らないことを確認する。
        message.command = "OLD";
        message.params.clear();
        message.params.push_back("old parameter");

        const bool success = parseMessage(test.input, message);

        bool passed = success == test.success
            && message.command == test.command
            && message.params.size() == test.param_count;

        for (std::size_t j = 0; passed && j < test.param_count; ++j)
            passed = message.params[j] == test.params[j];

        if (!passed)
        {
            ++failures;
            std::cerr << "FAIL: case " << i + 1
                      << ", input=[" << test.input << "]"
                      << std::endl;
        }
    }

    std::cout << count - failures << "/" << count
              << " tests passed" << std::endl;

    return failures == 0 ? 0 : 1;
}
