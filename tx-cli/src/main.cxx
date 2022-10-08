#include <tx/tx.hxx>

#include <fmt/format.h>

#include <cstdlib>
#include <memory_resource>
#include <ranges>
#include <span>
#include <string_view>

namespace tx {

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

void run_repl(VM& tvm) {
    Chunk chunk;
    const size_t line = 123;
    const double constant = 1.2;
    const double constant2 = 3.4;
    const double constant3 = 5.6;
    chunk.write_constant(tvm, constant, line);
    chunk.write_constant(tvm, constant2, line);
    chunk.write(tvm, OpCode::ADD, line);
    chunk.write_constant(tvm, constant3, line);
    chunk.write(tvm, OpCode::DIVIDE, line);
    chunk.write(tvm, OpCode::NEGATE, line);
    chunk.write(tvm, OpCode::RETURN, line);
    disassemble_chunk(chunk, "test chunk");
    tvm.interpret(chunk);
    chunk.destroy(tvm);
}

void print_title() noexcept {
    fmt::print(FMT_STRING("Tx version {}\n"), tx::VERSION);
}

void print_usage() noexcept {
    fmt::print(usage_str);
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
            if (arg == "--help") { args_help = true; }
            else if (arg == "--version") { args_version = true; }
            else if (arg == "-D") {
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
                    return EXIT_SUCCESS;
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
                    return EXIT_SUCCESS;
                }
            }
            else if (arg == "-c" or arg == "--command") {
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
    // fmt::print("file: {}\n", args_file_path);
    // fmt::print("cmd: {}\n", args_command);
    // fmt::print("use_stdin: {}\n", args_use_stdin);
    // fmt::print("args: ");
    // std::string_view sep;
    // for(const auto* arg: args_rest_of_args) {
    //     fmt::print("{}'{}'", sep, arg);
    //     sep = ", ";
    // }
    // fmt::print("\n");

    std::pmr::unsynchronized_pool_resource mem_res;
    tx::VM tvm(args_options, &mem_res);
    if (args_file_path.empty()) {
        tx::run_repl(tvm);
    } else {
        (void) args_use_stdin;
        fmt::print(
            FMT_STRING(
                "UNIMPLEMENTED: shoud execute '{}'\n"
            ),
            args_file_path
        );
        // tx::run_file(tvm, file_path);
    }
    return EXIT_SUCCESS;
}
