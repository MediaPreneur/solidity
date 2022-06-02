#!/usr/bin/env python3
# pylint: disable=consider-using-enumerate, import-error

import re
import os
import sys
import getopt
import tempfile
from getkey import getkey


def parse_call(call):
    function = ''
    arguments = ""
    results = ""
    if search := re.search(
        r'// (.*):(.*)\s->\s(.*)', call, re.MULTILINE | re.DOTALL
    ):
        function = search[1]
        arguments = search[2]
        results = search[3]
        if results.find("#") != -1:
            results = results[:results.find("#")]
    elif search := re.search(
        r'// (.*)(.*)\s->\s(.*)', call, re.MULTILINE | re.DOTALL
    ):
        function = search[1]
        arguments = search[2]
        results = search[3]
        if results.find("#") != -1:
            results = results[:results.find("#")]
    if function.find("wei") >= 0:
        function = function[:function.find(",")]
    return function.strip(), arguments.strip(), results.strip()


def colorize(left, right, index):
    red = "\x1b[31m"
    yellow = "\x1b[33m"
    reset = "\x1b[0m"
    colors = [red, yellow]
    color = colors[index % len(colors)]
    function, _arguments, _results = parse_call(right)
    left = left.replace("compileAndRun", f"{color}compileAndRun{reset}")
    right = right.replace("constructor", f"{color}constructor{reset}")
    if function:
        left = left.replace(function, color + function + reset)
        right = right.replace(function, color + function + reset)
    if left.find(function):
        bottom = " " * (left.find(function) - 4) + right
    else:
        bottom = f" {right}"
    return f"    {left}" + "\n" + bottom


def get_checks(content, sol_file_path):
    constructors = []
    checks = []
    for line in content.split("\n"):
        line = line.strip()
        if line.startswith("compileAndRun"):
            constructors.append(line)
        if line.startswith("ABI_CHECK") or line.startswith("BOOST_REQUIRE"):
            checks.append(line)
    with open(sol_file_path, "r", encoding='utf8') as sol_file:
        sol_constructors = []
        sol_checks = []
        inside_expectations = False
        for line in sol_file.readlines():
            if line.startswith("// constructor()"):
                sol_constructors.append(line)
            elif inside_expectations and line.startswith("// "):
                sol_checks.append(line)
            if line.startswith("// ----"):
                inside_expectations = True
        sol_file.close()
        if len(constructors) == len(sol_constructors) == 1:
            checks.insert(0, constructors[0])
            sol_checks.insert(0, sol_constructors[0])
        return checks, sol_checks


def show_test(name, content, sol_file_path, current_test, test_count):
    with tempfile.NamedTemporaryFile(delete=False) as cpp_file:
        cpp_file.write(content.encode())
        cpp_file.close()

        os.system("clear")
        print(f"{str(current_test)} / {str(test_count)} - {name}" + "\n")
        diff_env = os.getenv('DIFF', "/usr/local/bin/colordiff -a -d -w -y -W 200 ")
        os.system(f"{diff_env} {cpp_file.name} {sol_file_path}")
        os.unlink(cpp_file.name)
        print("\n")

        checks, sol_checks = get_checks(content, sol_file_path)

        if len(checks) == len(sol_checks):
            for i in range(len(checks)):
                print(colorize(checks[i].strip(), sol_checks[i].strip(), i))
        else:
            print("warning: check count not matching. this should not happen!")

        what = ""
        print("\nContinue? (ENTER) Abort? (ANY OTHER KEY)")
        while what != '\n':
            what = getkey()
            if what != '\n':
                sys.exit(0)
        print()


def get_tests(e2e_path):
    return [
        f.replace(".sol", "")
        for f in os.listdir(e2e_path)
        if f.endswith(".sol")
    ]


def process_input_file(e2e_path, input_file, interactive):
    tests = get_tests(e2e_path)
    with open(input_file, "r", encoding='utf8') as cpp_file:
        inside_test = False
        test_name = ""
        inside_extracted_test = False
        new_lines = 0
        count = 0
        test_content = ""
        for line in cpp_file.readlines():
            if test := re.search(
                r'BOOST_AUTO_TEST_CASE\((.*)\)', line, re.M | re.I
            ):
                test_name = test[1]
                inside_test = True
                inside_extracted_test = inside_test & (test_name in tests)
                if inside_extracted_test:
                    count = count + 1

            if interactive and inside_extracted_test:
                test_content = test_content + line

            if not inside_extracted_test:
                new_lines = new_lines + 1 if line == "\n" else 0
                if not interactive and new_lines <= 1:
                    sys.stdout.write(line)

            if line == "}\n":
                if interactive and inside_extracted_test:
                    show_test(
                        test_name,
                        test_content.strip(),
                        f"{e2e_path}/{test_name}.sol",
                        count,
                        len(tests),
                    )

                    test_content = ""
                inside_test = False
        cpp_file.close()
        sys.stdout.flush()


def main(argv):
    interactive = False
    input_file = None
    try:
        opts, _args = getopt.getopt(argv, "if:")
    except getopt.GetoptError:
        print("./remove-testcases.py [-i] [-f <full path to SolidityEndToEndTest.cpp>]")
        sys.exit(1)

    for opt, arg in opts:
        if opt == '-i':
            interactive = True
        elif opt in '-f':
            input_file = arg

    base_path = os.path.dirname(__file__)

    if not input_file:
        input_file = f"{base_path}/../../test/libsolidity/SolidityEndToEndTest.cpp"

    e2e_path = f"{base_path}/../../test/libsolidity/semanticTests/extracted"

    process_input_file(e2e_path, input_file, interactive)


if __name__ == "__main__":
    main(sys.argv[1:])
