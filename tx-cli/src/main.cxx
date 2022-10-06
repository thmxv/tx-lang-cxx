#include <tx/tx.hxx>

#include <fmt/format.h>

#include <cstdlib>
#include <span>
#include <string_view>

namespace tx {

void run_repl() {
    Chunk chunk;
    const size_t line = 123;
    const double constant = 1.2;
    chunk.write_constant(constant, line);
    chunk.write(OpCode::RETURN, line);
    disassemble_chunk(chunk, "test chunk");
    chunk.destroy();
}

}  // namespace tx

void print_usage() noexcept {
    fmt::print(
        "Tx version {}\n"
        "Usage:\n"
        "  tx [file] [arguments...]\n"
        "  tx --help | --version\n"
        "Options:\n"
        "  --help     Show this message and exit.\n"
        "  --version  Show version and exit.\n",
        tx::version
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
        fmt::print("{}", tx::version);
        return EXIT_SUCCESS;
    }

    if (argc == 1) {
        tx::run_repl();
    } else {
        const std::string_view file_path{args[1]};
        (void)file_path;
        // run_file
    }

    return EXIT_SUCCESS;
}
