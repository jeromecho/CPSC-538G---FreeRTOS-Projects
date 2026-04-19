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
    # "FP1",
    #
    # "EDF1",
    # "EDF2",
    # "EDF3",
    # "EDF4",
    # "EDF5",
    # "EDF6",
    # "EDF7",
    # "EDF8",
    # "EDF9",
    # "EDF10",
    # "EDF11",
    #
    # "SRP1",
    # "SRP2",
    # "SRP3",
    # "SRP4",
    # "SRP5",
    # "SRP6",
    # "SRP7",
    # "SRP8",
    # "SRP9",
    #
    # "CBS1",
    # "CBS2",
    # "CBS3",
    # "CBS4",
    # "CBS5",
    # "CBS6",
    # "CBS7",
    # "CBS8",
    # "CBS9",
    # "CBS10",
    # "CBS11",
    # "CBS12",
    # "CBS13",
    # "CBS14",
    # "CBS15",
    # "CBS16",
    # "CBS17",
    # "CBS18",
    #
    # "SMP1",
    # "SMP2",
    # "SMP3",
    # "SMP4",
    # "SMP5",
    # "SMP6",
    # "SMP7",
    # "SMP8",
    # "SMP9",
    # "SMP10",
    # "SMP11",
    # "SMP12",
    # "SMP13",
    # "SMP14",
    # "SMP15",
    # "SMP16",
    # "SMP17",
]

TRACE_RUN_BANNERS = (
    "Running EDF Test",
    "Running SRP Test",
    "Running SMP Test",
    "Running CBS Test",
    "Running FP Test",
)

BACKGROUND_TASK_PREFIXES = ("Idle Task", "System Task")

SUITE_PREFIXES = (
    ("FP", "FP"),
    ("SMP", "SMP"),
    ("SRP", "SRP"),
    ("EDF", "EDF"),
    ("CBS", "CBS"),
)

STRICT_POLICED_EVENTS = {  #
    TraceEvent.TRACE_RELEASE,
    TraceEvent.TRACE_SWITCH_IN,
    TraceEvent.TRACE_SWITCH_OUT,
    TraceEvent.TRACE_DONE,
    TraceEvent.TRACE_DEADLINE_MISS,
    TraceEvent.TRACE_SEMAPHORE_TAKE,
    TraceEvent.TRACE_SEMAPHORE_GIVE,
    TraceEvent.TRACE_BUDGET_RUN_OUT,
}


def get_task_name(t_type, t_id, core=None, include_core_for_realtime=False):
    base_name = TASK_TYPES.get(t_type, f"Unknown ({t_type})")
    if t_type in [1, 2]:
        if include_core_for_realtime and core is not None:
            return f"{base_name} {(t_id + 1):02d} C{core}"
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


def parse_trace_record(line, include_core_for_realtime=False):
    parts = line.split(",")
    if len(parts) < 5:
        return None

    try:
        if len(parts) >= 15:
            core = int(parts[3])
            task_type = int(parts[5])
            task_id = int(parts[6])
            task_uid = int(parts[14])
        elif len(parts) >= 13:
            core = int(parts[3])
            task_type = int(parts[5])
            task_id = int(parts[6])
            task_uid = task_id
        else:
            core = None
            task_type = int(parts[3])
            task_id = int(parts[4])
            task_uid = task_id

        return {
            "tick": int(parts[0]),
            "event": TraceEvent(int(parts[1])),
            "core": core,
            "task_uid": task_uid,
            "task_name": get_task_name(task_type, task_id, core, include_core_for_realtime),
            "raw": line,
        }
    except ValueError:
        return None


def normalize_expected_core(expected_core):
    if expected_core is None:
        return None

    if isinstance(expected_core, int):
        return expected_core

    if isinstance(expected_core, str):
        core_text = expected_core.strip().upper()
        if core_text.startswith("C"):
            core_text = core_text[1:]
        return int(core_text)

    return int(expected_core)


def matches_expected_task_reference(log, expected_task_ref):
    if isinstance(expected_task_ref, int):
        return int(log["task_uid"]) == expected_task_ref

    if isinstance(expected_task_ref, str):
        stripped_ref = expected_task_ref.strip()
        if stripped_ref.isdigit():
            return int(log["task_uid"]) == int(stripped_ref)
        return log["task_name"] == expected_task_ref

    return int(log["task_uid"]) == int(expected_task_ref)


def format_expected_task_reference(expected_task_ref):
    if isinstance(expected_task_ref, int):
        return f"UID {expected_task_ref}"
    return str(expected_task_ref)


def matches_expected_event(log, expected_task_ref, exp_tick, exp_event, exp_core=None):
    if log["tick"] != exp_tick or log["event"] != exp_event:
        return False

    if not matches_expected_task_reference(log, expected_task_ref):
        return False

    if exp_core is not None and int(log["core"]) != exp_core:
        return False

    return True


def stream_test_output(port, expected_boot_name, include_core_for_realtime=False):
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
                parsed_record = parse_trace_record(line, include_core_for_realtime)
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
    expected_failure_spec = test_case.get("expected_admission_failure")
    actual_admission_failures = [log for log in parsed_logs if log["event"] == TraceEvent.TRACE_ADMISSION_FAILED]

    if expected_failure_spec:
        # Normalize expected_failure_spec to a list of UIDs
        if isinstance(expected_failure_spec, list):
            expected_uids = expected_failure_spec
        else:
            # Single UID or string (backward compatibility)
            expected_uids = [expected_failure_spec]

        if not actual_admission_failures:
            expected_desc = " or ".join(str(uid) for uid in expected_uids)
            print(f"    {C_RED}❌ FAILED: Expected admission failure for UID {expected_desc}, but it did not occur.{C_RESET}")
            return False

        failed_log = actual_admission_failures[0]
        failed_uid = failed_log.get("task_uid")

        # Check if the failed UID matches any of the expected options
        if failed_uid is not None and failed_uid not in expected_uids:
            expected_desc = " or ".join(str(uid) for uid in expected_uids)
            print(f"    {C_RED}❌ FAILED: Expected UID {expected_desc} to fail admission, but UID {failed_uid} failed instead.{C_RESET}")
            return False

        return True

    if actual_admission_failures:
        failed_uid = actual_admission_failures[0].get("task_uid")
        print(f"    {C_RED}❌ FAILED: Unexpected admission failure for UID {failed_uid}.{C_RESET}")
        return False

    return True


def validate_trace_events(parsed_logs, test_case):
    if test_case.get("ignore_traces", False):
        return True

    expected_entries = []
    for exp_task, events in test_case.get("expected_events", {}).items():
        for event_spec in events:
            if len(event_spec) == 2:
                exp_tick, exp_event = event_spec
                exp_core = None
            elif len(event_spec) == 3:
                exp_tick, exp_event, exp_core = event_spec
                exp_core = normalize_expected_core(exp_core)
            else:
                raise ValueError(f"Expected events must be 2- or 3-tuples, got {event_spec!r} for task {exp_task!r}")

            expected_entries.append((exp_task, exp_tick, exp_event, exp_core))

    all_passed = True
    sorted_expected_events = sorted(expected_entries, key=lambda x: (x[1], str(x[0])))

    for exp_task, exp_tick, exp_event, exp_core in sorted_expected_events:
        event_name = TraceEvent(exp_event).name
        found = any(matches_expected_event(log, exp_task, exp_tick, exp_event, exp_core) for log in parsed_logs)

        if not found:
            core_suffix = f" | C{exp_core}" if exp_core is not None else ""
            print(f"    {C_RED}❌ MISSING: Tick {exp_tick:04d} | {format_expected_task_reference(exp_task)}{core_suffix} | {event_name}{C_RESET}")
            all_passed = False

    for log in parsed_logs:
        is_background = any(log["task_name"].startswith(name) for name in BACKGROUND_TASK_PREFIXES)
        if not is_background and log["event"] in STRICT_POLICED_EVENTS:
            if not any(matches_expected_event(log, exp_task, exp_tick, exp_event, exp_core) for exp_task, exp_tick, exp_event, exp_core in expected_entries):
                event_name = TraceEvent(log["event"]).name
                unexpected_task = f"UID {int(log['task_uid'])} ({log['task_name']})"
                print(f"    {C_RED}❌ UNEXPECTED: Tick {log['tick']:04d} | {unexpected_task} | {event_name}{C_RESET}")
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
    include_core_for_realtime = test_case.get("suite") == "SMP"

    try:
        trace_result = stream_test_output(port, expected_boot_name, include_core_for_realtime)

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
