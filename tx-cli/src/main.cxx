#include <tx/tx.hxx>

#include <fmt/format.h>

#include <cstdlib>
#include <span>
#include <string_view>

namespace tx {

void run_repl() {
    VM vm_{};
    Chunk chunk;
    const size_t line = 123;
    const double constant = 1.2;
    const double constant2 = 3.4;
    const double constant3 = 5.6;
    chunk.write_constant(constant, line);
    chunk.write_constant(constant2, line);
    chunk.write(OpCode::ADD, line);
    chunk.write_constant(constant3, line);
    chunk.write(OpCode::DIVIDE, line);
    chunk.write(OpCode::NEGATE, line);
    chunk.write(OpCode::RETURN, line);
    disassemble_chunk(chunk, "test chunk");
    vm_.interpret(chunk);
    chunk.destroy();
}

}  // namespace tx

void print_usage() noexcept {
    fmt::print(
        "Tx version {}\n"
        "Usage:\n"
        "  tx [options] [file] [arguments...]\n"
        "  tx --help | --version\n"
        "Options:\n"
        "  --help     Show this message and exit.\n"
        "  --version  Show version and exit.\n"
        "  -X opt     Set inplementation-specific option. The following"
        "             options are available:"
        "             -X trace-execution  Trace bytecode execution"
        "             -X print-bytecode   Print bytecode after compilation"
        "             -X trace-gc         Print bytecode after compilation"
        "             -X stress-gc        Print bytecode after compilation",
        tx::VERSION
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
        fmt::print("{}", tx::VERSION);
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
