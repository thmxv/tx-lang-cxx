#include "tx/tx.hxx"

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
      -D all              Enable all debug options
      -D print-tokens     Print tokens during compilation
      -D print-bytecode   Print bytecode after compilation
      -D trace-execution  Trace bytecode execution
      -D trace-gc         Trace garbage collection
  -c,--command TXT  Execute command passed as argument.
  file TXT          Read script to ececute from file.
  -                 Read script to execute from the standard input.
  --                Stop parsing the following arguments as options.
  arguments TXT...  Argument to pass to executed script/command.
)";

constexpr std::string_view usage_debug_str =
    R"(Allowed debug options: 'all', 'print-tokens', 'print-bytecode', 
  'trace-execution', 'trace-gc'.
)";

constexpr std::string_view greeting_str =
    R"((o)>    Tx v{}
//\     MIT License, Copyright (C) 2022-2023 Xavier Thomas
V_/_    https://github.com/thmxv/tx-lang 
)";

void print_title() noexcept { fmt::print(FMT_STRING("Tx v{}\n"), tx::VERSION); }

void print_greeting() noexcept {
    fmt::print(FMT_STRING(greeting_str), tx::VERSION);
}

void print_usage() noexcept { fmt::print(FMT_STRING(usage_str)); }

void print_debug_usage() noexcept {
    fmt::print(stderr, FMT_STRING(usage_debug_str));
}

struct ArgsOptions {
    bool help = false;
    bool version = false;
    bool use_stdin = false;
    const char* file_path = nullptr;
    std::string_view command;
    std::span<const char*> rest_of_args;
    tx::VMOptions vm_options;
};

constexpr std::optional<ArgsOptions>
parse_arguments(int argc, const char** argv) {
    const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};
    ArgsOptions result;
    std::size_t idx = 1;
    for (; idx < args.size(); ++idx) {
        const auto arg = std::string_view{args[idx]};
        if (arg == "--help") {
            result.help = true;
        } else if (arg == "--version") {
            result.version = true;
        } else if (arg == "-D") {
            ++idx;
            if (idx >= args.size()) {
                fmt::print(
                    stderr,
                    FMT_STRING("Expecting debug option argument after '-D'.\n")
                );
                print_debug_usage();
                tx::print_usage();
                return std::nullopt;
            }
            const auto opt = std::string_view{args[idx]};
            if (opt == "trace-execution") {
                result.vm_options.trace_execution = true;
            } else if (opt == "print-tokens") {
                result.vm_options.print_tokens = true;
            } else if (opt == "print-bytecode") {
                result.vm_options.print_bytecode = true;
            } else if (opt == "trace-gc") {
                result.vm_options.trace_gc = true;
            } else if (opt == "all") {
                result.vm_options.print_bytecode = true;
                result.vm_options.print_tokens = true;
                result.vm_options.trace_execution = true;
                result.vm_options.trace_gc = true;
            } else {
                fmt::print(
                    stderr,
                    FMT_STRING("Unexpected debug option argument '{}'.\n"),
                    opt
                );
                print_debug_usage();
                tx::print_usage();
                return std::nullopt;
            }
        } else if (arg == "-c" or arg == "--command") {
            ++idx;
            if (idx >= args.size()) {
                fmt::print(
                    stderr,
                    FMT_STRING("Expecting command argument after '-c'.\n")
                );
                tx::print_usage();
                return std::nullopt;
            }
            result.command = std::string_view{args[idx++]};
            break;
        } else if (arg == "-") {
            result.use_stdin = true;
            ++idx;
            break;
        } else if (arg == "--") {
            ++idx;
            break;
        } else {
            result.file_path = args[idx];
            ++idx;
            break;
        }
    }
    result.rest_of_args = args.subspan(idx);
    return result;
}

[[nodiscard]] std::string read_file(const char* path) noexcept {
    gsl::owner<std::FILE*> file = std::fopen(path, "rb");
    if (file == nullptr) {
        fmt::print(
            stderr,
            FMT_STRING("Could not open file \"{:s}\": {}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        exit(ExitCode::IO_ERROR);
    }
    // NOTE: getc/ferror to detect is file is directory
    if ((void)std::getc(file); std::ferror(file) != 0) {
        fmt::print(
            stderr,
            FMT_STRING("Could not read file \"{:s}\": {}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        (void)std::fclose(file);
        exit(ExitCode::IO_ERROR);
    }
    auto is_error = std::fseek(file, 0L, SEEK_END);
    if (is_error != 0) {
        fmt::print(
            stderr,
            FMT_STRING("Could not seek in file \"{:s}\": {}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        (void)std::fclose(file);
        exit(ExitCode::IO_ERROR);
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
        exit(ExitCode::IO_ERROR);
    }
    if (bytes_read < file_size) {
        fmt::print(
            stderr,
            FMT_STRING("Could not read file \"{:s}\": {}\n"),
            path,
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::strerror(errno)
        );
        exit(ExitCode::IO_ERROR);
    }
    return result;
}

void run_file(VM& tvm, const char* path) {
    const auto source = read_file(path);
    const InterpretResult result = tvm.interpret(path, source);
    if (result == InterpretResult::COMPILE_ERROR) {
        exit(ExitCode::DATA_ERROR);
    }
    if (result == InterpretResult::RUNTIME_ERROR) {
        exit(ExitCode::SOFTWARE_INTERNAL_ERROR);
    }
}

void run_repl(VM& tvm) {
    print_greeting();
    std::array<char, REPL_LINE_MAX_LEN> line{};
    while (true) {
        fmt::print(FMT_STRING("\n> "));
        if (std::fgets(line.begin(), line.size(), stdin) == nullptr) {
            fmt::print(FMT_STRING("\n"));
            break;
        }
        const std::string_view source{line.data()};
        (void)tvm.interpret("<stdin>", source);
    }
}

}  // namespace tx

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char** argv) noexcept {
    auto args_opt = tx::parse_arguments(argc, argv);
    if (!args_opt.has_value()) {
        return to_underlying(tx::ExitCode::USAGE_ERROR);
    }
    auto args = *args_opt;
    if (args.help) {
        tx::print_title();
        tx::print_usage();
        return to_underlying(tx::ExitCode::SUCCESS);
    }
    if (args.version) {
        fmt::print(FMT_STRING("{}"), tx::VERSION);
        return to_underlying(tx::ExitCode::SUCCESS);
    }

    std::pmr::unsynchronized_pool_resource mem_res;
    std::pmr::memory_resource* mem_res_ptr = nullptr;
    // TODO make option
    if constexpr (tx::IS_DEBUG_BUILD) {
        mem_res_ptr = std::pmr::get_default_resource();
    } else {
        mem_res_ptr = &mem_res;
    }
    tx::VM tvm(args.vm_options, mem_res_ptr);
    if (args.file_path == nullptr) {
        tvm.get_options().allow_pointer_to_source_content = false;
        tvm.get_options().allow_global_redefinition = true;
        tvm.get_options().allow_end_compile_with_undefined_global = true;
        tx::run_repl(tvm);
    } else {
        if (args.use_stdin) {
            fmt::print(
                FMT_STRING("UNIMPLEMENTED: Cannot read from <stdin> yet.\n")
            );
        } else {
            tx::run_file(tvm, args.file_path);
        }
    }
    return to_underlying(tx::ExitCode::SUCCESS);
}
