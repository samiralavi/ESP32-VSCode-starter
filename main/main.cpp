/**
 * @file main.cpp
 * @brief Main entry point for the firmware.
 */

#include "app.h"

#include <cli.hpp>
#include <format.hpp>
#include <logger.hpp>

#include <esp_event.h>
#include <esp_log.h>
#include <wifi_provisioning/manager.h>

#include <chrono>
#include <stdio.h>
#include <string>
#include <thread>

constexpr char TAG[] = "main";

using namespace std::chrono;
using namespace std::chrono_literals;

void run_cli() {
  {
    fmt::print("Starting cli example!\n");
    //! [cli example]
    auto root_menu = std::make_unique<cli::Menu>("cli");
    root_menu->Insert(
        "hello", [](std::ostream &out) { out << "Hello world!\n"; },
        "Print hello world");
    root_menu->Insert(
        "hello_everysession",
        [](std::ostream &) {
          cli::Cli::cout() << "Hello everybody" << std::endl;
        },
        "Print hello everybody on all open sessions");
    root_menu->Insert(
        "answer",
        [](std::ostream &out, int x) { out << "The answer is: " << x << "\n"; },
        "Print the answer to Life, the Universe and Everything ");
    root_menu->Insert(
        "color",
        [](std::ostream &out) {
          out << "Colors ON\n";
          cli::SetColor();
        },
        "Enable colors in the cli");
    root_menu->Insert(
        "nocolor",
        [](std::ostream &out) {
          out << "Colors OFF\n";
          cli::SetNoColor();
        },
        "Disable colors in the cli");

    auto sub_menu = std::make_unique<cli::Menu>("sub");
    sub_menu->Insert(
        "hello", [](std::ostream &out) { out << "Hello, submenu world\n"; },
        "Print hello world in the submenu");
    sub_menu->Insert(
        "demo", [](std::ostream &out) { out << "This is a sample!\n"; },
        "Print a demo string");

    auto sub_sub_menu = std::make_unique<cli::Menu>("subsub");
    sub_sub_menu->Insert(
        "hello", [](std::ostream &out) { out << "Hello, subsubmenu world\n"; },
        "Print hello world in the sub-submenu");
    sub_menu->Insert(std::move(sub_sub_menu));

    root_menu->Insert(std::move(sub_menu));

    cli::Cli cli(std::move(root_menu));
    cli.ExitAction(
        [](auto &out) { out << "Goodbye and thanks for all the fish.\n"; });

    espp::Cli input(cli);
    input.SetInputHistorySize(10);
    input.Start();

    // if we've gotten here, the cli has finished it's session, let's print the
    // commands that were run:
    fmt::print("Commands executed: {}\n", input.GetInputHistory());

    //! [cli example]
  }

  fmt::print("Cli example complete!\n");

  while (true) {
    std::this_thread::sleep_for(1s);
  }
}
/**
 * @brief Entry point of the application.
 *
 * This function is the entry point of the firmware WiFi application.
 * It initializes the necessary components and starts the application.
 */
extern "C" void app_main(void) {
  espp::Logger logger({.tag = TAG, .level = espp::Logger::Verbosity::DEBUG});

  App::get_instance();

  // run_cli();
}
