#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "../src/controller.h"

SCENARIO("Controller", "[Controller]") {
    using namespace std::literals;
    GIVEN("Controller and TV") {
        TV tv;
        std::istringstream input;
        std::ostringstream output;
        Menu menu{input, output};
        Controller controller{tv, menu};

        auto run_menu_command = [&menu, &input](std::string command) {
            input.str(std::move(command));
            input.clear();
            menu.Run();
        };
        auto expect_output = [&output](std::string_view expected) {
            // В g++ 10.3 не реализован метод ostringstream::view(), поэтому приходится
            // использовать метод str()
            // Также в conan есть баг, из-за которого Catch2 не подхватывает поддержку string_view:
            // https://github.com/conan-io/conan-center-index/issues/13993
            // Поэтому expected преобразуется к строке, чтобы обойти ошибку компиляции
            CHECK(output.str() == std::string{expected});
        };
        auto expect_extra_arguments_error = [&expect_output](std::string_view command) {
            expect_output("Error: the "s.append(command).append(
                " command does not require any arguments\n"sv));
        };
        auto expect_empty_output = [&expect_output] {
            expect_output({});
        };

        WHEN("The TV is turned off") {
            AND_WHEN("Info command is entered without arguments") {
                run_menu_command("Info"s);

                THEN("output contains info that TV is turned off") {
                    expect_output("TV is turned off\n"s);
                }
            }

            AND_WHEN("Info command is entered with some arguments") {
                run_menu_command("Info some extra arguments");

                THEN("Error message is printed") {
                    expect_extra_arguments_error("Info"s);
                }
            }

            AND_WHEN("Info command has trailing spaces") {
                run_menu_command("Info  "s);

                THEN("output contains information that TV is turned off") {
                    expect_output("TV is turned off\n"s);
                }
            }

            AND_WHEN("TurnOn command is entered without arguments") {
                run_menu_command("TurnOn"s);

                THEN("TV is turned on") {
                    CHECK(tv.IsTurnedOn());
                    expect_empty_output();
                }
            }

            AND_WHEN("TurnOn command is entered with some arguments") {
                run_menu_command("TurnOn some args"s);

                THEN("the error message is printed and TV is not turned on") {
                    CHECK(!tv.IsTurnedOn());
                    expect_extra_arguments_error("TurnOn"s);
                }
            }
            
            AND_WHEN("SelectChannel is entered") {
                run_menu_command("SelectChannel 42"s);
                THEN("error is printed") {
                    expect_output("TV is turned off\n"s);
                }
            }

            AND_WHEN("SelectPreviousChannel is entered") {
                run_menu_command("SelectPreviousChannel"s);
                THEN("error is printed") {
                    expect_output("TV is turned off\n"s);
                }
            }
        }

        WHEN("The TV is turned on") {
            tv.TurnOn();
            AND_WHEN("TurnOff command is entered without arguments") {
                run_menu_command("TurnOff"s);

                THEN("TV is turned off") {
                    CHECK(!tv.IsTurnedOn());
                    expect_empty_output();
                }
            }

            AND_WHEN("TurnOff command is entered with some arguments") {
                run_menu_command("TurnOff some args");

                THEN("the error message is printed and TV is not turned off") {
                    CHECK(tv.IsTurnedOn());
                    expect_extra_arguments_error("TurnOff"s);
                }
            }

            AND_WHEN("Info command is entered without arguments") {
                tv.SelectChannel(12);
                run_menu_command("Info"s);

                THEN("current channel is printed") {
                    expect_output("TV is turned on\nChannel number is 12\n"s);
                }
            }

            AND_WHEN("SelectChannel is entered with a valid channel") {
                run_menu_command("SelectChannel 42"s);
                THEN("channel is changed") {
                    CHECK(tv.GetChannel() == 42);
                    expect_empty_output();
                }
            }

            AND_WHEN("SelectChannel is entered with an out of range channel") {
                run_menu_command("SelectChannel 142"s);
                THEN("error is printed") {
                    CHECK(tv.GetChannel() == 1);
                    expect_output("Channel is out of range\n"s);
                }
            }

            AND_WHEN("SelectChannel is entered with an invalid channel") {
                run_menu_command("SelectChannel abc"s);
                THEN("error is printed") {
                    CHECK(tv.GetChannel() == 1);
                    expect_output("Invalid channel\n"s);
                }
            }

            AND_WHEN("SelectPreviousChannel is entered without arguments") {
                tv.SelectChannel(42);
                run_menu_command("SelectPreviousChannel"s);
                THEN("channel is changed") {
                    CHECK(tv.GetChannel() == 1);
                    expect_empty_output();
                }
            }

            AND_WHEN("SelectPreviousChannel is entered with arguments") {
                run_menu_command("SelectPreviousChannel abc"s);
                THEN("error is printed") {
                    CHECK(tv.GetChannel() == 1);
                    expect_extra_arguments_error("SelectPreviousChannel"s);
                }
            }
        }
    }
}