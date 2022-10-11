#include "tx/exit_codes.hxx"
#include <tx/tx.hxx>

#include <fmt/format.h>

#include <cerrno>
#include <cstdlib>
#include <memory_resource>
#include <ranges>
#include <span>
#include <string_view>

namespace tx {

static constexpr std::size_t REPL_LINE_MAX_LEN = 1024;

constexpr std::string_view usage_str =
    R"(Usage:
  tx [OPTIONS] [-c cmd | file | -] [--] [arguments...]
  tx --help | --version
Options:
  --help            Show this message and exit.
  --version         Show version and exit.
  -D TXT            Set debug specific option(s). (only works on debug builds)
                    The following options are available:
                      -D trace-execution  Trace bytecode execution
                      -D print-bytecode   Print bytecode after compilation
                      -D trace-gc         Trace garbage collection
                      -D stress-gc        Run garbage collector on every 
                                            allocation
  -c,--command TXT  Execute command passed as argument.
  file TXT          Read script to ececute from file.
  -                 Read script to execute from the standard input.
  --                Stop parsing the following arguments as options.
  arguments TXT...  Argument to pass to executed script/command.)";

void print_title() noexcept {
    fmt::print(FMT_STRING("Tx version {}\n"), tx::VERSION);
}

void print_usage() noexcept { fmt::print(usage_str); }

[[nodiscard]] std::string read_file(const char* path) noexcept {
    gsl::owner<std::FILE*> file = std::fopen(path, "rb");
    if (file == nullptr) {
        fmt::print(
            stderr,
            FMT_STRING("Could not open file \"{:s}\":{}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        std::exit(EXT_IO_ERROR);
    }
    auto is_error = std::fseek(file, 0L, SEEK_END);
    if (is_error != 0) {
        fmt::print(
            stderr,
            FMT_STRING("Could not seek in file \"{:s}\":{}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        (void)std::fclose(file);
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        std::exit(EXT_IO_ERROR);
    }
    const auto file_size = static_cast<std::size_t>(std::ftell(file));
    std::rewind(file);
    std::string result;
    result.resize(static_cast<std::size_t>(file_size));
    const auto bytes_read = std::fread(
        result.data(),
        sizeof(char),
        static_cast<std::size_t>(file_size),
        file
    );
    is_error = std::fclose(file);
    if (is_error != 0) {
        fmt::print(
            stderr,
            FMT_STRING("Could not close file \"{:s}\": {}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        std::exit(EXT_IO_ERROR);
    }
    if (bytes_read < file_size) {
        fmt::print(
            stderr,
            FMT_STRING("Could not read file \"{:s}\": {}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        std::exit(EXT_IO_ERROR);
    }
    return result;
}

void run_repl(VM& tvm) {
    print_title();
    std::array<char, REPL_LINE_MAX_LEN> line{};
    while (true) {
        fmt::print("> ");
        if (std::fgets(line.begin(), line.size(), stdin) == nullptr) {
            fmt::print("\n");
            break;
        }
        (void)tvm.interpret({line.data()});
    }
}

void run_file(VM& tvm, const char* path) {
    const auto source = read_file(path);
    const InterpretResult result = tvm.interpret(source);
    if (result == InterpretResult::COMPILE_ERROR) {
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        std::exit(EXT_DATA_ERROR);
    }
    if (result == InterpretResult::RUNTIME_ERROR) {
        // NOLINTNEXTLINE
        std::exit(EXT_SOFTWARE_INTERNAL_ERROR);
    }
}

}  // namespace tx

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char** argv) noexcept {
    const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};
    bool args_help = false;
    bool args_version = false;
    bool args_use_stdin = false;
    std::string_view args_file_path;
    std::string_view args_command;
    std::span<const char*> args_rest_of_args;
    tx::VMOptions args_options;

    {
        std::size_t idx = 1;
        for (; idx < args.size(); ++idx) {
            const auto arg = std::string_view{args[idx]};
            if (arg == "--help") {
                args_help = true;
            } else if (arg == "--version") {
                args_version = true;
            } else if (arg == "-D") {
                ++idx;
                if (idx >= args.size()) {
                    fmt::print(
                        stderr,
                        "Expecting debug option argument after '-D'. "
                        "Allowed debug options: "
                        "'trace-execution', "
                        "'print-bytecode', "
                        "'trace-gc', "
                        "'stress-gc'.\n"
                    );
                    tx::print_usage();
                    return tx::EXT_USAGE_ERROR;
                }
                const auto opt = std::string_view{args[++idx]};
                if (opt == "trace-execution") {
                    args_options.trace_execution = true;
                } else if (opt == "print-bytecode") {
                    args_options.print_bytecode = true;
                } else if (opt == "trace-gc") {
                    args_options.trace_gc = true;
                } else if (opt == "stress-gc") {
                    args_options.stress_gc = true;
                } else {
                    fmt::print(
                        stderr,
                        FMT_STRING("Unexpected debug option argument '{}'. "
                                   "Allowed debug options: "
                                   "'trace-execution', "
                                   "'print-bytecode', "
                                   "'trace-gc', "
                                   "'stress-gc'.\n"),
                        opt
                    );
                    tx::print_usage();
                    return tx::EXT_USAGE_ERROR;
                }
            } else if (arg == "-c" or arg == "--command") {
                args_command = std::string_view{args[++idx]};
                ++idx;
                break;
            } else if (arg == "-") {
                args_use_stdin = true;
                ++idx;
                break;
            } else if (arg == "--") {
                ++idx;
                break;
            } else {
                args_file_path = arg;
                ++idx;
                break;
            }
        }
        args_rest_of_args = args.subspan(idx);
    }

    if (args_help) {
        tx::print_title();
        tx::print_usage();
        return EXIT_SUCCESS;
    }
    if (args_version) {
        fmt::print(FMT_STRING("{}"), tx::VERSION);
        return EXIT_SUCCESS;
    }

    std::pmr::unsynchronized_pool_resource mem_res;
    tx::VM tvm(args_options, &mem_res);
    if (args_file_path.empty()) {
        tx::run_repl(tvm);
    } else {
        (void)args_use_stdin;
        fmt::print(
            FMT_STRING("UNIMPLEMENTED: shoud execute '{}'\n"),
            args_file_path
        );
        // tx::run_file(tvm, file_path);
    }
    return EXIT_SUCCESS;
}
