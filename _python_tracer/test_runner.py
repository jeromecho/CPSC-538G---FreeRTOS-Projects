import sys
import time
import argparse
import serial

# Import definitions from our data module
from test_data import TEST_CASES, TraceEvent, TASK_TYPES

# Import utilities from our environment module
from pico_env import (
    C_GREEN,
    C_RED,
    C_YELLOW,
    C_RESET,
    BAUD_RATE,
    print_status,
    clear_status,
    check_port_availability,
    auto_detect_port,
    patch_config_file,
    compile_and_flash,
    get_binary_memory_usage,
)

# Define which tests to run. Leave empty to run all tests
TESTS_TO_RUN: list[str] = [  #
    "EDF9",
    # "SRP1",
    # "SRP2",
    # "SRP3",
    # "SRP4",
    # "SRP5",
    # "SRP6",
    # "SRP7",
    # "SRP8",
    # "SRP9",
]

TRACE_RUN_BANNERS = (
    "Running EDF Test",
    "Running SRP Test",
    "Running SMP Test",
    "Running CBS Test",
)

BACKGROUND_TASK_PREFIXES = ("Idle Task", "System Task")

SUITE_PREFIXES = (
    ("SMP", "SMP"),
    ("SRP", "SRP"),
    ("EDF", "EDF"),
    ("CBS", "CBS"),
)


def get_task_name(t_type, t_id, core=None):
    base_name = TASK_TYPES.get(t_type, f"Unknown ({t_type})")
    if t_type in [1, 2]:
        return f"{base_name} {(t_id + 1):02d}"
    if core is not None and t_type in [0, 3]:
        return f"{base_name} C{core}"
    return base_name


def infer_suite_label(test_id):
    upper_test_id = test_id.upper()
    for prefix, label in SUITE_PREFIXES:
        if prefix in upper_test_id:
            return label
    return "EDF"


def build_expected_boot_name(test_id, test_case):
    suite_label = test_case.get("suite", infer_suite_label(test_id))
    test_num = "".join(filter(str.isdigit, test_id))
    return f"Results for {suite_label} Test {test_num}"


def detect_run_banner(line):
    for banner in TRACE_RUN_BANNERS:
        if line.startswith(banner):
            return line.replace("Running ", "", 1)
    return None


def parse_trace_record(line):
    parts = line.split(",")
    if len(parts) < 5:
        return None

    try:
        if len(parts) >= 13:
            core = int(parts[3])
            task_type = int(parts[5])
            task_id = int(parts[6])
        else:
            core = None
            task_type = int(parts[3])
            task_id = int(parts[4])

        return {
            "tick": int(parts[0]),
            "event": TraceEvent(int(parts[1])),
            "task_name": get_task_name(task_type, task_id, core),
            "raw": line,
        }
    except ValueError:
        return None


def stream_test_output(port, expected_boot_name):
    parsed_logs = []
    output_log_raw = ""
    capturing = False
    found_boot_name = False

    with serial.Serial(port, BAUD_RATE, timeout=1.0) as ser:
        start_time = time.time()
        while time.time() - start_time < 5.0:
            elapsed = time.time() - start_time
            state_msg = "Capturing traces..." if capturing else "Waiting for boot name..."
            print_status(f"[Monitor] attached to {port}. {state_msg} ({elapsed:.1f}s)")

            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            output_log_raw += line + "\n"

            if not found_boot_name and expected_boot_name in line:
                found_boot_name = True

            if "Assertion" in line or "failed" in line:
                return {
                    "status": "assertion",
                    "line": line,
                    "logs": parsed_logs,
                    "capturing": capturing,
                    "found_boot_name": found_boot_name,
                    "raw": output_log_raw,
                }

            if "TIMESTAMP" in line:
                capturing = True
                continue

            if line == "--- END OF TRACE ---":
                break

            if capturing:
                parsed_record = parse_trace_record(line)
                if parsed_record is not None:
                    parsed_logs.append(parsed_record)

    return {
        "status": "ok",
        "logs": parsed_logs,
        "capturing": capturing,
        "found_boot_name": found_boot_name,
        "raw": output_log_raw,
    }


def validate_expected_boot_name(found_boot_name, expected_boot_name, mem_usage):
    if found_boot_name:
        return True

    print(f"{C_RED}❌ FAILED: Did not detect '{expected_boot_name}' on boot.{C_RESET}")
    return False


def validate_admission_failure(parsed_logs, test_case, mem_usage):
    expected_failure_task = test_case.get("expected_admission_failure")
    actual_admission_failures = [log for log in parsed_logs if log["event"] == TraceEvent.TRACE_ADMISSION_FAILED]

    if expected_failure_task:
        if not actual_admission_failures:
            print(f"    {C_RED}❌ FAILED: Expected admission failure for '{expected_failure_task}', but it did not occur.{C_RESET}")
            return False

        failed_task = actual_admission_failures[0]["task_name"]
        if failed_task != expected_failure_task:
            print(f"    {C_RED}❌ FAILED: Expected '{expected_failure_task}' to fail admission, but '{failed_task}' failed instead.{C_RESET}")
            return False

        return True

    if actual_admission_failures:
        failed_task = actual_admission_failures[0]["task_name"]
        print(f"    {C_RED}❌ FAILED: Unexpected admission failure for task '{failed_task}'.{C_RESET}")
        return False

    return True


def validate_trace_events(parsed_logs, test_case):
    if test_case.get("ignore_traces", False):
        return True

    expected_set = set()
    for exp_task, events in test_case.get("expected_events", {}).items():
        for exp_tick, exp_event in events:
            expected_set.add((exp_tick, exp_task, exp_event))

    all_passed = True
    sorted_expected_events = sorted(expected_set, key=lambda x: (x[0], x[1]))

    for exp_tick, exp_task, exp_event in sorted_expected_events:
        event_name = TraceEvent(exp_event).name
        found = any(log["tick"] == exp_tick and log["task_name"] == exp_task and log["event"] == exp_event for log in parsed_logs)

        if not found:
            print(f"    {C_RED}❌ MISSING: Tick {exp_tick:04d} | {exp_task} | {event_name}{C_RESET}")
            all_passed = False

    for log in parsed_logs:
        is_background = any(log["task_name"].startswith(name) for name in BACKGROUND_TASK_PREFIXES)
        if not is_background and log["event"] in {TraceEvent.TRACE_SWITCH_IN, TraceEvent.TRACE_SWITCH_OUT}:
            actual_event_tuple = (log["tick"], log["task_name"], log["event"])
            if actual_event_tuple not in expected_set:
                event_name = TraceEvent(log["event"]).name
                print(f"    {C_RED}❌ UNEXPECTED: Tick {log['tick']:04d} | {log['task_name']} | {event_name}{C_RESET}")
                all_passed = False

    return all_passed


def print_test_result(test_id, test_name, passed, mem_usage):
    status = f"{C_GREEN}[PASS]{C_RESET}" if passed else f"{C_RED}[FAIL]{C_RESET}"
    text_str = f"{mem_usage['text']:,} B" if mem_usage else "N/A"
    data_str = f"{mem_usage['data']:,} B" if mem_usage else "N/A"
    bss_str = f"{mem_usage['bss']:,} B" if mem_usage else "N/A"
    print(f"{status:<17} | {test_id:<6} | {text_str:<10} | {data_str:<10} | {bss_str:<10} | {test_name}")


def summarize_results(test_results, passed_count):
    print("\n" + "=" * 95)
    print(f"  TEST SUITE OVERVIEW: {passed_count}/{len(test_results)} PASSED")
    print("=" * 95)
    print(f"{'STATUS':<8} | {'ID':<6} | {'.TEXT':<10} | {'.DATA':<10} | {'.BSS':<10} | {'TEST NAME'}")
    print("-" * 95)

    for t_id, t_name, passed, mem in test_results:
        print_test_result(t_id, t_name, passed, mem)

    print("=" * 95 + "\n")


def run_test(test_id, test_case):
    patch_config_file(test_case["flags"])

    if not compile_and_flash():
        return False, None

    mem_usage = get_binary_memory_usage()

    port = auto_detect_port()
    port_connection_attempts = 5
    while not port:
        port_connection_attempts -= 1
        if port_connection_attempts == 0:
            clear_status()
            print(f"{C_RED}❌ Could not detect Pico serial port.{C_RESET}")
            return False, mem_usage

        print_status("[Monitor] Waiting for serial port to initialize...")
        time.sleep(0.5)
        port = auto_detect_port()

    expected_boot_name = build_expected_boot_name(test_id, test_case)

    try:
        trace_result = stream_test_output(port, expected_boot_name)

    except serial.SerialException as e:
        clear_status()
        print(f"{C_RED}❌ Serial Error: {e}{C_RESET}")
        return False, mem_usage

    clear_status()

    if trace_result["status"] == "assertion":
        print(f"    {C_RED}❌ [CRITICAL ASSERTION] {trace_result['line']}{C_RESET}")
        return False, mem_usage

    if not validate_expected_boot_name(trace_result["found_boot_name"], expected_boot_name, mem_usage):
        return False, mem_usage

    if not validate_admission_failure(trace_result["logs"], test_case, mem_usage):
        return False, mem_usage

    if test_case.get("ignore_traces", False):
        return True, mem_usage

    all_passed = validate_trace_events(trace_result["logs"], test_case)
    return all_passed, mem_usage


if __name__ == "__main__":
    try:
        parser = argparse.ArgumentParser(description="Pico RTOS Hardware-in-the-Loop Test Runner")
        parser.add_argument(
            "tests",
            metavar="ID",
            type=str,
            nargs="*",
            help="List of test IDs to run (e.g., EDF1 SRP2). Overrides TESTS_TO_RUN.",
        )
        parser.add_argument(
            "-l",
            "--list",
            action="store_true",
            help="List all available tests and exit.",
        )

        args = parser.parse_args()

        if args.list:
            print("=== AVAILABLE TESTS ===")
            for test_id, test_data in TEST_CASES.items():
                print(f"[{test_id}] {test_data['name']}")
            sys.exit(0)

        check_port_availability()

        active_tests = [t.upper() for t in (args.tests if args.tests else TESTS_TO_RUN)]
        valid_test_ids = {k.upper() for k in TEST_CASES.keys()}
        invalid_ids = [t for t in active_tests if t not in valid_test_ids]

        if invalid_ids:
            print(f"{C_RED}❌ Invalid test IDs requested: {', '.join(invalid_ids)}{C_RESET}")
            sys.exit(1)

        tests_to_execute = [(tid, tdata) for tid, tdata in TEST_CASES.items() if not active_tests or tid.upper() in active_tests]

        if not tests_to_execute:
            print(f"{C_RED}❌ No valid tests selected. Exiting.{C_RESET}")
            sys.exit(1)

        test_results = []
        passed_count = 0

        for i, (test_id, test_data) in enumerate(tests_to_execute):
            print(f"\n{C_YELLOW}▶ Running [{test_id}] {test_data['name']} ({i + 1}/{len(tests_to_execute)}){C_RESET}")

            test_passed, mem_usage = run_test(test_id, test_data)

            if test_passed and mem_usage:
                rel_check_id = test_data.get("expected_less_bss_than")
                if rel_check_id:
                    ref_mem = next(
                        (past_mem for past_t_id, _, _, past_mem in test_results if past_t_id == rel_check_id),
                        None,
                    )

                    if ref_mem:
                        if mem_usage["bss"] < ref_mem["bss"]:
                            diff = ref_mem["bss"] - mem_usage["bss"]
                            print(f"    {C_GREEN}✓ MEMORY CHECK: Saved {diff:,} Bytes compared to {rel_check_id}.{C_RESET}")
                        else:
                            print(f"    {C_RED}❌ MEMORY FAIL: .BSS ({mem_usage['bss']:,} B) is NOT less than {rel_check_id} ({ref_mem['bss']:,} B).{C_RESET}")
                            test_passed = False
                    else:
                        print(f"    {C_YELLOW}⚠ Skipped memory check: Reference test '{rel_check_id}' was not run in this session.{C_RESET}")

            if test_passed:
                passed_count += 1
                print(f"{C_GREEN}✅ [{test_id}] PASSED{C_RESET}")
            else:
                print(f"{C_RED}❌ [{test_id}] FAILED{C_RESET}")

            test_results.append((test_id, test_data["name"], test_passed, mem_usage))

        summarize_results(test_results, passed_count)

    except KeyboardInterrupt, EOFError:
        clear_status()
        print(f"\n\n{C_YELLOW}[Test Runner] Aborted by user. Exiting cleanly.{C_RESET}")
        sys.exit(0)
