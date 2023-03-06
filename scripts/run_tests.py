#!/usr/bin/env python3

from argparse import ArgumentParser
from os import listdir
from os.path import (
    abspath,
    dirname,
    isdir,
    join,
    realpath,
    relpath,
    splitext,
    isfile,
)
import re
from subprocess import Popen, PIPE
import sys

import term

# Runs the tests.

parser = ArgumentParser()
parser.add_argument("cli_app")
parser.add_argument("suite_root")
parser.add_argument("--no_colors", action="store_true")
parser.add_argument("filter", nargs="?")
args = parser.parse_args(sys.argv[1:])

BUILD_DIR = dirname(dirname(realpath(__file__)))
CLI_APP = args.cli_app
# TESTS_DIR = join(BUILD_DIR, "test_suite")
TESTS_DIR = args.suite_root

CLI_APP_WITH_EXT = CLI_APP
# if platform.system() == "Windows":
#     CLI_APP_WITH_EXT += ".exe"

if not isfile(CLI_APP_WITH_EXT):
    print("File not found: " + CLI_APP)
    sys.exit(1)

# Regex's for source files
OUTPUT_EXPECT = re.compile(r"# expect: ?(.*)")
HELP_EXPECT = re.compile(r"# ?(Help: .*)")
ERROR_EXPECT = re.compile(r"# (Syntax error.+)")
ERROR_LINE_EXPECT = re.compile(r"# \[(\d+)\] (Syntax error.+)")
RUNTIME_ERROR_EXPECT = re.compile(r"# (Runtime error:.+)")
NONTEST_RE = re.compile(r"# nontest")
# Regex's for program output
SYNTAX_ERROR_RE = re.compile(r"(Syntax error.+)")
SYNTAX_ERROR_LOC_RE = re.compile(r"  (.*?):(\d+):(\d+)")
SYNTAX_ERROR_HINT_RE = re.compile(r"  \d* \| .*")
STACK_TRACE_RE = re.compile(r"  \[(.*?):(\d+)\] in .*")

passed = 0
failed = 0
num_skipped = 0
expectations = 0

interpreter = None


def green(txt):
    if args and args.no_colors:
        return txt
    return term.green(txt)


def red(txt):
    if args and args.no_colors:
        return txt
    return term.red(txt)


def yellow(txt):
    if args and args.no_colors:
        return txt
    return term.yellow(txt)


def gray(txt):
    if args and args.no_colors:
        return txt
    return term.gray(txt)


class Test:
    def __init__(self, path):
        self.path = path
        self.output = []
        self.compile_errors = {}
        self.runtime_error_line = 0
        self.runtime_error_message = None
        self.exit_code = 0
        self.failures = []

    def parse(self):
        global num_skipped
        global expectations

        line_num = 1
        with open(self.path, "r") as file:
            for line in file:
                match = OUTPUT_EXPECT.search(line)
                if match:
                    self.output.append((match.group(1), line_num))
                    expectations += 1

                match = HELP_EXPECT.search(line)
                if match:
                    self.output.append((match.group(1), line_num))
                    expectations += 1

                match = ERROR_EXPECT.search(line)
                if match:
                    self.compile_errors[match.group(1)] = (self.path, line_num)

                    # If we expect a compile error, it should exit with
                    # EX_DATAERR.
                    self.exit_code = 65
                    expectations += 1

                match = ERROR_LINE_EXPECT.search(line)
                if match:
                    self.compile_errors[match.group(2)] = (self.path, match.group(1))

                    # If we expect a compile error, it should exit with
                    # EX_DATAERR.
                    self.exit_code = 65
                    expectations += 1

                match = RUNTIME_ERROR_EXPECT.search(line)
                if match:
                    self.runtime_error_line = line_num
                    self.runtime_error_message = match.group(1)
                    # If we expect a runtime error, it should exit with
                    # EX_SOFTWARE.
                    self.exit_code = 70
                    expectations += 1

                match = NONTEST_RE.search(line)
                if match:
                    # Not a test file at all, so ignore it.
                    return False

                line_num += 1

            # If we got here, it's a valid test.
            return True

    def run(self):
        # Invoke the interpreter and run the test.
        args = [CLI_APP_WITH_EXT]
        args.append(self.path)
        proc = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)

        out, err = proc.communicate()
        self.validate(proc.returncode, out, err)

    def validate(self, exit_code, out, err):
        if self.compile_errors and self.runtime_error_message:
            self.fail("Test error: Cannot expect both compile and runtime errors.")
            return

        try:
            out = out.decode("utf-8").replace("\r\n", "\n")
            err = err.decode("utf-8").replace("\r\n", "\n")
        except:
            self.fail("Error decoding output.")

        error_lines = err.split("\n")

        # Validate that an expected runtime error occurred.
        if self.runtime_error_message:
            self.validate_runtime_error(error_lines)
        else:
            self.validate_compile_errors(error_lines)

        self.validate_exit_code(exit_code, error_lines)
        self.validate_output(out)

    def validate_runtime_error(self, error_lines):
        if len(error_lines) < 2:
            self.fail(
                'Expected runtime error "{0}" and got none.', self.runtime_error_message
            )
            return

        # Skip any compile errors. This can happen if there is a compile
        # error in a module loaded by the module being tested.
        line = 0
        while SYNTAX_ERROR_RE.search(error_lines[line]):
            line += 1

        if error_lines[line] != self.runtime_error_message:
            self.fail(
                'Expected runtime error "{0}" and got:', self.runtime_error_message
            )
            self.fail(error_lines[line])

        # Make sure the stack trace has the right line. Skip over any
        # lines that come from builtin libraries.
        match = False
        stack_lines = error_lines[line + 1 :]
        for stack_line in stack_lines:
            match = STACK_TRACE_RE.search(stack_line)
            if match:
                break

        if not match:
            self.fail("Expected stack trace and got:")
            for stack_line in stack_lines:
                self.fail(stack_line)
        else:
            file_path = match.group(1)
            stack_line = int(match.group(2))
            if file_path != self.path:
                self.fail(
                    "Expected runtime error on file {0} but was on file {1}.",
                    self.path,
                    file_path
                )
            if stack_line != self.runtime_error_line:
                self.fail(
                    "Expected runtime error on line {0} but was on line {1}.",
                    self.runtime_error_line,
                    stack_line,
                )

    def validate_compile_errors(self, error_lines):
        # Validate that every compile error was expected.
        found_errors = set()
        num_unexpected = 0
        index = 0
        while index < len(error_lines):
            line = error_lines[index]
            line_index = index
            index += 1
            match = SYNTAX_ERROR_RE.search(line)
            if match:
                error = match.group(1)
                match2 = SYNTAX_ERROR_LOC_RE.search(error_lines[index])
                if match2:
                    index += 1
                    while SYNTAX_ERROR_HINT_RE.search(error_lines[index]):
                        index += 1
                    if error in self.compile_errors:
                        expected = self.compile_errors[error]
                        # col_num = match2.group(3)
                        if match2.group(1) == expected[0] and (
                            int(match2.group(2)) == int(expected[1])
                        ):
                            found_errors.add(error)
                            continue
                if num_unexpected < 10:
                    self.fail("Unexpected error:")
                    # self.fail(line)
                    while line_index < index:
                        self.fail(error_lines[line_index])
                        line_index += 1
                num_unexpected += 1
            elif line != "":
                if num_unexpected < 10:
                    self.fail("Unexpected output on stderr:")
                    self.fail(line)
                num_unexpected += 1

        if num_unexpected > 10:
            self.fail("(truncated " + str(num_unexpected - 10) + " more...)")

        # Validate that every expected error occurred.
        for error in self.compile_errors.keys() - found_errors:
            self.fail(
                "Missing expected error: [{0}] {1}",
                self.compile_errors[error][1],
                error,
            )

    def validate_exit_code(self, exit_code, error_lines):
        if exit_code == self.exit_code:
            return

        if len(error_lines) > 10:
            error_lines = error_lines[0:10]
            error_lines.append("(truncated...)")
        self.fail(
            "Expected return code {0} and got {1}. Stderr:", self.exit_code, exit_code
        )
        self.failures += error_lines

    def validate_output(self, out):
        # Remove the trailing last empty line.
        out_lines = out.split("\n")
        if out_lines[-1] == "":
            del out_lines[-1]

        index = 0
        for line in out_lines:
            if index >= len(self.output):
                self.fail('Got output "{0}" when none was expected.', line)
            elif self.output[index][0] != line:
                self.fail(
                    'Expected output "{0}" on line {1} and got "{2}".',
                    self.output[index][0],
                    self.output[index][1],
                    line,
                )
            index += 1

        while index < len(self.output):
            self.fail(
                'Missing expected output "{0}" on line {1}.',
                self.output[index][0],
                self.output[index][1],
            )
            index += 1

    def fail(self, message, *args):
        if args:
            message = message.format(*args)
        self.failures.append(message)


def walk(dir, callback):
    """
    Walks [dir], and executes [callback] on each file.
    """

    dir = abspath(dir)
    for file in listdir(dir):
        nfile = join(dir, file)
        if isdir(nfile):
            walk(nfile, callback)
        else:
            callback(nfile)


def run_script(path):
    if "benchmark" in path:
        return

    global passed
    global failed
    global num_skipped

    if splitext(path)[1] != ".tx":
        return

    # Check if we are just running a subset of the tests.
    if args and args.filter:
        this_test = relpath(path, TESTS_DIR)
        if not this_test.startswith(args.filter):
            return

    # Make a nice short path relative to the working directory.

    # Normalize it to use "/" since, among other things, the interpreters
    # expect the argument to use that.
    path = relpath(path).replace("\\", "/")
    printable_path = path.split("/")[-1]

    # Update the status line.
    term.print_line(
        "Passed: {} Failed: {} Skipped: {} {}".format(
            green(passed),
            red(failed),
            yellow(num_skipped),
            gray("({})".format(printable_path)),
        )
    )

    # Read the test and parse out the expectations.
    test = Test(path)

    if not test.parse():
        # It's a skipped or non-test file.
        return

    test.run()

    # Display the results.
    if len(test.failures) == 0:
        passed += 1
    else:
        failed += 1
        term.print_line(term.red("FAIL") + ": " + path)
        print("")
        for failure in test.failures:
            print("    " + term.pink(failure))
        print("")


walk(TESTS_DIR, run_script)
term.print_line()

if failed == 0:
    print(
        "All {} tests passed ({} expectations).".format(
            green(passed), str(expectations)
        )
    )
else:
    print(
        "{} tests passed. {} tests failed.".format(
            green(passed),
            red(failed),
        )
    )

if failed != 0:
    sys.exit(1)
