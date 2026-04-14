from enum import Enum
import re

TraceEvent = Enum(
    "TraceEvent",
    [
        "TRACE_RELEASE",
        "TRACE_SWITCH_IN",
        "TRACE_SWITCH_OUT",
        "TRACE_DONE",
        "TRACE_RESCHEDULED",
        "TRACE_UPDATING_PRIORITIES",
        "TRACE_DEPRIORITIZED",
        "TRACE_PRIORITY_SET",
        "TRACE_DEADLINE_MISS",
        "TRACE_SRP_BLOCK",
        "TRACE_ADMISSION_FAILED",
        "TRACE_SEMAPHORE_TAKE",
        "TRACE_SEMAPHORE_GIVE",
    ],
    start=0,
)

TASK_TYPES = {
    0: "Idle Task",
    1: "Periodic",
    2: "Aperiodic",
    3: "System Task",
}

TEST_SUITE_IDS = {
    "EDF": 1,
    "SRP": 2,
    "CBS": 3,
    "SMP": 4,
}


def build_test_flags(suite, test_nr, overrides=None):
    if suite not in TEST_SUITE_IDS:
        raise ValueError(f"Unknown suite '{suite}'")

    flags = {
        "TEST_SUITE": TEST_SUITE_IDS[suite],
        "TEST_NR": int(test_nr),
    }

    if overrides:
        flags.update(overrides)

    return flags


# --- TEST DEFINITIONS ---
TEST_CASES = {
    # EDF TESTS
    "EDF1": {
        "name": "Smoke Test for Periodic Tasks",
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
        "expected_admission_failure": "Periodic 34",
        "expected_events": {},
    },
    "EDF4": {
        "name": "100 Tasks ADMISSIBLE",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF5": {
        "name": "Admissible by utilization",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF6": {
        "name": "Non-admissible by utilization",
        "expected_admission_failure": "Periodic 10",
        "expected_events": {},
    },
    "EDF7": {
        "name": "Admissible by processor demand",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF8": {
        "name": "Non-admissible by processor demand",
        "expected_admission_failure": "Periodic 02",
        "expected_events": {},
    },
    "EDF9": {
        "name": "Admissible drop-in",
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
        "expected_admission_failure": "Periodic 02",
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_SWITCH_OUT),
                #
                (100, TraceEvent.TRACE_RELEASE),
                (100, TraceEvent.TRACE_SWITCH_IN),
                (120, TraceEvent.TRACE_SWITCH_OUT),
                #
                (200, TraceEvent.TRACE_RELEASE),
                (200, TraceEvent.TRACE_SWITCH_IN),
                (220, TraceEvent.TRACE_SWITCH_OUT),
                #
                (300, TraceEvent.TRACE_RELEASE),
                (300, TraceEvent.TRACE_SWITCH_IN),
                (320, TraceEvent.TRACE_SWITCH_OUT),
                #
                (400, TraceEvent.TRACE_RELEASE),
                (400, TraceEvent.TRACE_SWITCH_IN),
                (420, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "EDF11": {
        "name": "Missed deadline",
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
        "name": "Simple Single-Resource SRP Validation",
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
        "name": "Stack Sharing Disabled - Simple Execution",
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
        "name": "Stack Sharing Enabled - Simple Execution",
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
        "expected_less_bss_than": "SRP3",
    },
    # Test 5 and 6 should be 25 tasks running sequentially.
    # Haven't bothered defining the results here, as long as they compile they shoud be fine.
    # They really aren't very different from tests 3 and 4
    "SRP5": {
        "name": "Stack Sharing Disabled - 100 Tasks w/ 5 Preemption Levels",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SRP6": {
        "name": "Stack Sharing Enabled - 100 Tasks w/ 5 Preemption Levels",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
        "expected_less_bss_than": "SRP5",
    },
    "SRP7": {
        "name": "Admission Control - Pass (Implicit Deadlines)",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SRP8": {
        "name": "Admission Control - Fail (Implicit Deadlines)",
        "expected_admission_failure": "Periodic 03",
        "expected_events": {},
    },
    "SRP9": {
        "name": "Admission Control - Fail (Constrained Deadlines)",
        "expected_admission_failure": "Periodic 03",
        "expected_events": {},
    },
}

TEST_ID_PATTERN = re.compile(r"^(EDF|SRP|CBS|SMP)(\d+)$")
for test_id, test_case in TEST_CASES.items():
    match = TEST_ID_PATTERN.match(test_id)
    if not match:
        raise ValueError(f"Invalid test_id format: '{test_id}'. Must be Prefix + Number (e.g., SRP9).")

    suite = match.group(1)
    test_nr = int(match.group(2))

    test_case["suite"] = suite
    test_case["flags"] = build_test_flags(
        suite,
        test_nr,
        overrides=test_case.get("flags_override"),
    )
