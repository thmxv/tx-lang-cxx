#include <fmt/format.h>
#include <string_view>

#include <cstdlib>
#include <span>

// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `tx::cmake`.
// You can modify the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hxx>

void print_usage() noexcept {
    fmt::print(
        "Tx version {}\n"
        "Usage:\n"
        "  tx [file] [arguments...]\n"
        "  tx --help | --version\n"
        "Options:\n"
        "  --help     Show this message and exit.\n"
        "  --version  Show version and exit.\n",
        tx::cmake::project_version
    );
}

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char** argv) noexcept {
    const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};

    if (argc >= 2 && args[1] == std::string_view{"--help"}) {
        print_usage();
        return EXIT_SUCCESS;
    }

    if (argc >= 2 && args[1] == std::string_view{"--version"}) {
        fmt::print("{}", tx::cmake::project_version);
        return EXIT_SUCCESS;
    }

    if (argc == 1) {
        // run_repl
    } else {
        const std::string_view file_path{args[1]};
        (void)file_path;
        // run_file
    }

    return EXIT_SUCCESS;
}
