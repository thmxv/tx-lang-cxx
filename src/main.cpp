#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <cstdlib>
#include <optional>

// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `tx::cmake`.
// You can modify the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char** argv) {
    CLI::App app{fmt::format(
        "{} version {}",
        tx::cmake::project_name,
        tx::cmake::project_version
    )};
    std::optional<std::string> message;
    app.add_option("-m,--message", message, "A message to print back out");
    bool show_version = false;
    app.add_flag("--version", show_version, "Show version information");
    // app.parse(argc, argv);
    CLI11_PARSE(app, argc, argv);
    if (show_version) {
        fmt::print("{}\n", tx::cmake::project_version);
        return EXIT_SUCCESS;
    }
    if (message) {
        fmt::print("Message: '{}'\n", *message);
    } else {
        fmt::print("No Message Provided :(\n");
    }
}
