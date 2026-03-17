import os
import sys
import time
import shutil
import subprocess
import re
import serial
import serial.tools.list_ports
import tty
import termios
import argparse
from enum import IntEnum

# --- CONFIGURATION ---
BAUD_RATE = 115200
PROJECT_CONFIG_PATH = "Standard/ProjectConfig.h"  # Path to your config header
BUILD_CMD = ["cmake", "--build", "build"]  # Matches your CMake setup
UF2_OUTPUT = "build/Standard/main_blinky.uf2"
PICO_DRIVE = "/Volumes/RPI-RP2"  # macOS Pico Bootloader path


class TraceEvent(IntEnum):
    TRACE_RELEASE = 0
    TRACE_SWITCH_IN = 1
    TRACE_SWITCH_OUT = 2
    TRACE_DONE = 3
    TRACE_RESCHEDULED = 4
    TRACE_UPDATING_PRIORITIES = 5
    TRACE_DEPRIORITIZED = 6
    TRACE_PRIORITY_SET = 7
    TRACE_DEADLINE_MISS = 8
    TRACE_SRP_BLOCK = 9
    TRACE_SEMAPHORE_TAKE = 10
    TRACE_SEMAPHORE_GIVE = 11


TASK_TYPES = {
    0: "Idle Task",
    1: "Periodic",
    2: "Aperiodic",
    3: "System Task",
}

# --- TEST DEFINITIONS ---
TEST_CASES = {
    # EDF TESTS
    "EDF1": {
        "name": "Smoke Test for Periodic Tasks",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 1},
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (1, TraceEvent.TRACE_SWITCH_OUT),
                #
                (2, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                #
                (4, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_SWITCH_OUT),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                #
                (10, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "EDF2": {
        "name": "Mark's Proposed EDF Smoke Test",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 2},
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                #
                (12, TraceEvent.TRACE_RELEASE),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (16, TraceEvent.TRACE_SWITCH_OUT),
                #
                (18, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_SWITCH_OUT),
                #
                (16, TraceEvent.TRACE_RELEASE),
                (16, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                #
                (9, TraceEvent.TRACE_RELEASE),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_SWITCH_OUT),
                #
                (18, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (23, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "EDF3": {
        "name": "100 Tasks NON-ADMISSIBLE",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 3},
        "expected_admission_failure": "Admission failed for: EDF Test 3, Task 33",
        "expected_events": {},
    },
    "EDF4": {
        "name": "100 Tasks ADMISSIBLE",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 4},
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF5": {
        "name": "Admissible by utilization",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 5},
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF6": {
        "name": "Non-admissible by utilization",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 6},
        "expected_admission_failure": "Admission failed for: EDF Test 6, Task 9",
        "expected_events": {},
    },
    "EDF7": {
        "name": "Admissible by processor demand",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 7},
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF8": {
        "name": "Non-admissible by processor demand",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 8},
        "expected_admission_failure": "Admission failed for: EDF Test 8, Task 2",
        "expected_events": {},
    },
    "EDF9": {
        "name": "Admissible drop-in",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 9},
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (160, TraceEvent.TRACE_SWITCH_OUT),
                #
                (800, TraceEvent.TRACE_RELEASE),
                (800, TraceEvent.TRACE_SWITCH_IN),
                (960, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (800, TraceEvent.TRACE_RELEASE),
                (960, TraceEvent.TRACE_SWITCH_IN),
                (1200, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "EDF10": {
        "name": "Inadmissible drop-in",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 10},
        "expected_admission_failure": "Admission failed for: EDF Test 10, Task 2",
        "expected_events": {},
    },
    "EDF11": {
        "name": "Missed deadline",
        "flags": {"USE_EDF": 1, "USE_SRP": 0, "TEST_NR": 11},
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_SWITCH_OUT),
                #
                (120, TraceEvent.TRACE_RELEASE),
                (120, TraceEvent.TRACE_SWITCH_IN),
                (170, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (120, TraceEvent.TRACE_SWITCH_OUT),
                (170, TraceEvent.TRACE_SWITCH_IN),
                (201, TraceEvent.TRACE_DEADLINE_MISS),
            ],
        },
    },
    # SRP TESTS
    "SRP1": {
        "name": "Complex Multi-Resource SRP Validation",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 1},
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (40, TraceEvent.TRACE_RELEASE),
                (101, TraceEvent.TRACE_SWITCH_IN),
                (131, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (131, TraceEvent.TRACE_SWITCH_IN),
                (181, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (101, TraceEvent.TRACE_SWITCH_OUT),
                (181, TraceEvent.TRACE_SWITCH_IN),
                (200, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "SRP2": {
        "name": "Complex Multi-Resource SRP Validation",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 2},
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (400, TraceEvent.TRACE_RELEASE),
                (482, TraceEvent.TRACE_SWITCH_IN),
                (845, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (279, TraceEvent.TRACE_RELEASE),
                (279, TraceEvent.TRACE_SWITCH_IN),
                (482, TraceEvent.TRACE_SWITCH_OUT),
                (845, TraceEvent.TRACE_SWITCH_IN),
                (937, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (150, TraceEvent.TRACE_RELEASE),
                (251, TraceEvent.TRACE_SWITCH_IN),
                (279, TraceEvent.TRACE_SWITCH_OUT),
                (937, TraceEvent.TRACE_SWITCH_IN),
                (1201, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 04": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (251, TraceEvent.TRACE_SWITCH_OUT),
                (1201, TraceEvent.TRACE_SWITCH_IN),
                (1293, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "SRP3": {
        "name": "Simple execution with Stack Sharing disabled",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 3},
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_SWITCH_OUT),
                (100, TraceEvent.TRACE_SWITCH_IN),
                (150, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (150, TraceEvent.TRACE_SWITCH_IN),
                (250, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "SRP4": {
        "name": "Simple execution with Stack Sharing enabled",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 4},
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_SWITCH_OUT),
                (100, TraceEvent.TRACE_SWITCH_IN),
                (150, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (150, TraceEvent.TRACE_SWITCH_IN),
                (250, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    # Test 5 and 6 should be 25 tasks running sequentially. Haven't bothered defining the results here, as long as they compile they shoud be fine. They really aren't very different from tests 3 and 4
    "SRP5": {
        "name": "Stack Sharing - Disabled",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 5},
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SRP6": {
        "name": "Stack Sharing - Enabled",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 6},
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SRP7": {
        "name": "Admission Control - Pass (Implicit Deadlines)",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 7},
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SRP8": {
        "name": "Admission Control - Fail (Implicit Deadlines)",
        "flags": {"USE_EDF": 1, "USE_SRP": 1, "TEST_NR": 8},
        "expected_admission_failure": "Admission failed for: SRP Test 8, Task 3",
        "expected_events": {},
    },
}

# Define which tests to run (1-indexed based on their order in TEST_CASES).
# Leave empty [] to run ALL tests. Example: [1, 3] runs Test 1 and Test 3.
TESTS_TO_RUN: list[str] = [  #
    # "SRP3",
    # "SRP7",
    # "SRP8",
]

# --- HELPER FUNCTIONS ---


def get_task_name(t_type, t_id):
    base_name = TASK_TYPES.get(t_type, f"Unknown ({t_type})")
    if t_type in [1, 2]:
        return f"{base_name} {(t_id + 1):02d}"
    return base_name


def wait_for_keypress(prompt):
    print(f"\n{prompt}")
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        char = sys.stdin.read(1)

        if char == "\x03":
            raise KeyboardInterrupt
        if char == "\x04":
            raise EOFError
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    print("Continuing...\n")


def auto_detect_port():
    """Automatically finds the Pico's serial port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "usbmodem" in port.device.lower() or "pico" in port.description.lower():
            return port.device
    return None


def patch_config_file(flags):
    """Updates ProjectConfig.h with the test's required flags."""
    with open(PROJECT_CONFIG_PATH, "r") as file:
        content = file.read()

    for flag, value in flags.items():
        pattern = rf"(#define\s+{flag}\s+)\d+"
        replacement = rf"\g<1>{value}"
        content = re.sub(pattern, replacement, content)

    with open(PROJECT_CONFIG_PATH, "w") as file:
        file.write(content)


def compile_and_flash():
    """Compiles the project and copies the UF2 to the Pico."""
    print("[Build] Running CMake...")
    result = subprocess.run(BUILD_CMD, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"❌ Build Failed:\n{result.stderr}")
        return False

    wait_for_keypress(
        "BOOTSEL Mode: Hold BOOTSEL, press RUN/RESET, release BOOTSEL.\nThen press ANY KEY to flash..."
    )

    print(f"[Flash] Waiting for Pico drive to mount at {PICO_DRIVE}...")

    # Poll for the drive to appear (Timeout after 10 seconds)
    timeout = 10.0
    start_wait = time.time()
    while not os.path.exists(PICO_DRIVE):
        if time.time() - start_wait > timeout:
            print(f"❌ Pico drive did not appear after {timeout} seconds!")
            return False
        time.sleep(0.5)  # Check twice a second

    print("[Flash] Drive found! Copying UF2 to Pico...")

    max_retries = 5
    for attempt in range(max_retries):
        try:
            shutil.copy(UF2_OUTPUT, PICO_DRIVE)
            break  # If copy succeeds, break out of the loop
        except PermissionError as e:
            if attempt < max_retries - 1:
                print(
                    f"    [Flash] OS locked drive (Permission Denied). Retrying in 0.5s... ({attempt + 1}/{max_retries})"
                )
                time.sleep(0.5)
            else:
                print(f"❌ Failed to copy after {max_retries} attempts: {e}")
                return False
        except IOError as e:
            # IOErrors usually mean the drive fully disconnected while copying
            print(f"❌ Error during copy (Drive disconnected): {e}")
            return False

    print("[Flash] Copy complete. Waiting for Pico to reboot...")
    time.sleep(2.5)  # Give the Pico time to flash itself and restart USB CDC
    return True


def run_test(test_id, test_case):
    patch_config_file(test_case["flags"])
    if not compile_and_flash():
        return False

    port = auto_detect_port()
    if not port:
        print(
            "❌ Could not detect Pico serial port after flashing. Waiting 2s and retrying..."
        )
        time.sleep(2)
        port = auto_detect_port()
        if not port:
            return False

    print(f"[Monitor] Attached to {port}. Waiting for traces...")

    parsed_logs = []
    output_log_raw = ""
    capturing = False

    # --- Strict Name Validation Setup ---
    test_type = "SRP" if "SRP" in test_id.upper() else "EDF"
    test_num = "".join(filter(str.isdigit, test_id))  # Extracts "10" from "EDF10"
    expected_boot_name = f"Running {test_type} Test {test_num}"
    found_boot_name = False
    # ------------------------------------

    try:
        with serial.Serial(port, BAUD_RATE, timeout=1.0) as ser:
            start_time = time.time()

            while time.time() - start_time < 5.0:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                output_log_raw += line + "\n"

                # 1. Check Boot Name
                if not found_boot_name and expected_boot_name in line:
                    print(f"    [BOOT] Confirmed: {line}")
                    found_boot_name = True

                # 2. Check Admission Failure string (if we expect one)
                if test_case.get("expected_admission_failure"):
                    if test_case["expected_admission_failure"] in line:
                        print(f"    [ADMISSION] Caught expected failure: {line}")
                        return found_boot_name  # Ensure it also printed the boot name!

                # 3. Handle Assertions
                if "Assertion" in line or "failed" in line:
                    print(f"    ❌ [CRITICAL ASSERTION CAUGHT] {line}")
                    return False

                # 4. Handle Trace CSV Mode
                if "TIMESTAMP" in line:
                    capturing = True
                    continue
                elif line == "--- END OF TRACE ---":
                    break
                elif capturing:
                    parts = line.split(",")
                    if len(parts) >= 5:
                        try:
                            tick = int(parts[0])
                            event = int(parts[1])
                            task_type = int(parts[3])
                            task_id = int(parts[4])
                            task_name = get_task_name(task_type, task_id)

                            parsed_logs.append(
                                {
                                    "tick": tick,
                                    "event": event,
                                    "task_name": task_name,
                                    "raw": line,
                                }
                            )
                        except ValueError:
                            pass

    except serial.SerialException as e:
        print(f"❌ Serial Error: {e}")
        return False

    # --- Pre-Validation Checks ---
    if not found_boot_name:
        print(
            f"❌ FAILED: Did not detect expected test name '{expected_boot_name}' on boot."
        )
        return False

    if test_case.get("expected_admission_failure"):
        print("❌ FAILED: Expected an admission failure, but it did not occur.")
        return False

    if test_case.get("ignore_traces", False):
        print("    [Validate] 'ignore_traces' is set. Skipping strict sequence checks.")
        return True

    # --- VALIDATE EXPECTED EVENTS ---
    expected_set = set()
    for exp_task, events in test_case.get("expected_events", {}).items():
        for exp_tick, exp_event in events:
            expected_set.add((exp_tick, exp_task, exp_event))

    print(f"[Validate] Checking {len(expected_set)} expected events...")
    all_passed = True

    sorted_expected_events = sorted(list(expected_set), key=lambda x: (x[0], x[1]))

    for exp_tick, exp_task, exp_event in sorted_expected_events:
        event_name = TraceEvent(exp_event).name
        found = False
        for log in parsed_logs:
            if (
                log["tick"] == exp_tick
                and log["task_name"] == exp_task
                and log["event"] == exp_event
            ):
                found = True
                break

        if not found:
            print(f"    ❌ MISSING: Tick {exp_tick:04d} | {exp_task} | {event_name}")
            all_passed = False

    print("[Validate] Checking for unexpected switch in and switch out events...")
    allowed_background_tasks = ["Idle Task", "System Task"]
    strict_policed_events = {TraceEvent.TRACE_SWITCH_IN, TraceEvent.TRACE_SWITCH_OUT}

    for log in parsed_logs:
        task_name = log["task_name"]
        event = log["event"]
        tick = log["tick"]

        if task_name not in allowed_background_tasks and event in strict_policed_events:
            actual_event_tuple = (tick, task_name, event)
            if actual_event_tuple not in expected_set:
                event_name = TraceEvent(event).name
                print(
                    f"    ❌ UNEXPECTED EVENT: Tick {tick:04d} | {task_name} | {event_name}"
                )
                all_passed = False

    return all_passed


# --- MAIN RUNNER ---
if __name__ == "__main__":
    import argparse

    try:
        # fmt: off
        parser = argparse.ArgumentParser(description="Pico RTOS Hardware-in-the-Loop Test Runner")
        parser.add_argument("tests", metavar="ID", type=str, nargs="*", help="List of test IDs to run (e.g., EDF1 SRP2). Overrides TESTS_TO_RUN.",)
        parser.add_argument("-l", "--list", action="store_true", help="List all available tests and exit.",)
        # fmt: on

        args = parser.parse_args()

        # Handle the --list flag
        if args.list:
            print("=== AVAILABLE TESTS ===")
            for test_id, test_data in TEST_CASES.items():
                print(f"[{test_id}] {test_data['name']}")
            sys.exit(0)

        # Determine which list of tests to use (CLI overrides hardcoded)
        active_tests = args.tests if args.tests else TESTS_TO_RUN
        active_tests = [t.upper() for t in active_tests]

        # Validate that requested test IDs actually exist
        valid_ids = [k.upper() for k in TEST_CASES.keys()]
        invalid_ids = [t for t in active_tests if t not in valid_ids]
        if invalid_ids:
            print(f"❌ Invalid test IDs requested: {', '.join(invalid_ids)}")
            print(f"Valid IDs are: {', '.join(TEST_CASES.keys())}")
            sys.exit(1)

        print("=== STARTING PICO RTOS TEST SUITE ===")
        passed_count = 0

        # Filter the test cases
        tests_to_execute = []
        for test_id, test_data in TEST_CASES.items():
            if not active_tests or test_id.upper() in active_tests:
                tests_to_execute.append((test_id, test_data))

        if not tests_to_execute:
            print("❌ No valid tests selected. Exiting.")
            sys.exit(1)

        # Run the filtered tests
        for i, (test_id, test_data) in enumerate(tests_to_execute):
            print("\n========================================")
            print(
                f"  Running [{test_id}] {test_data['name']} ({i + 1}/{len(tests_to_execute)})"
            )
            print("========================================")

            test_passed = run_test(test_id, test_data)
            if test_passed:
                passed_count += 1
                print("✅ TEST PASSED!")
            else:
                print("❌ TEST FAILED!")

        print(
            f"\n=== TEST SUITE COMPLETE: {passed_count}/{len(tests_to_execute)} PASSED ==="
        )

    except KeyboardInterrupt, EOFError:
        print("\n\n[Test Runner] Aborted by user. Exiting cleanly.")
        sys.exit(0)
