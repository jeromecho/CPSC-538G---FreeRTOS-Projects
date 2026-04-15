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
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_DONE),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (0, TraceEvent.TRACE_SWITCH_OUT),
                (0, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (2, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (4, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_DONE),
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
                (1, TraceEvent.TRACE_SWITCH_OUT),
                (1, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_RELEASE),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (15, TraceEvent.TRACE_DONE),
                #
                (18, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (19, TraceEvent.TRACE_SWITCH_OUT),
                (19, TraceEvent.TRACE_DONE),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_DONE),
                #
                (16, TraceEvent.TRACE_RELEASE),
                (16, TraceEvent.TRACE_SWITCH_IN),
                (17, TraceEvent.TRACE_SWITCH_OUT),
                (17, TraceEvent.TRACE_DONE),
            ],
            "Periodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (9, TraceEvent.TRACE_RELEASE),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (13, TraceEvent.TRACE_SWITCH_OUT),
                (13, TraceEvent.TRACE_DONE),
                #
                (18, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (22, TraceEvent.TRACE_DONE),
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
                (159, TraceEvent.TRACE_SWITCH_OUT),
                (159, TraceEvent.TRACE_DONE),
                #
                (800, TraceEvent.TRACE_RELEASE),
                (800, TraceEvent.TRACE_SWITCH_IN),
                (959, TraceEvent.TRACE_SWITCH_OUT),
                (959, TraceEvent.TRACE_DONE),
            ],
            "Periodic 02": [
                (800, TraceEvent.TRACE_RELEASE),
                (960, TraceEvent.TRACE_SWITCH_IN),
                (1359, TraceEvent.TRACE_SWITCH_OUT),
                (1359, TraceEvent.TRACE_DONE),
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
                (19, TraceEvent.TRACE_SWITCH_OUT),
                (19, TraceEvent.TRACE_DONE),
                #
                (100, TraceEvent.TRACE_RELEASE),
                (100, TraceEvent.TRACE_SWITCH_IN),
                (119, TraceEvent.TRACE_SWITCH_OUT),
                (119, TraceEvent.TRACE_DONE),
                #
                (200, TraceEvent.TRACE_RELEASE),
                (200, TraceEvent.TRACE_SWITCH_IN),
                (219, TraceEvent.TRACE_SWITCH_OUT),
                (219, TraceEvent.TRACE_DONE),
                #
                (300, TraceEvent.TRACE_RELEASE),
                (300, TraceEvent.TRACE_SWITCH_IN),
                (319, TraceEvent.TRACE_SWITCH_OUT),
                (319, TraceEvent.TRACE_DONE),
                #
                (400, TraceEvent.TRACE_RELEASE),
                (400, TraceEvent.TRACE_SWITCH_IN),
                (419, TraceEvent.TRACE_SWITCH_OUT),
                (419, TraceEvent.TRACE_DONE),
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
                (49, TraceEvent.TRACE_SWITCH_OUT),
                (49, TraceEvent.TRACE_DONE),
                #
                (120, TraceEvent.TRACE_RELEASE),
                (120, TraceEvent.TRACE_SWITCH_IN),
                (169, TraceEvent.TRACE_SWITCH_OUT),
                (169, TraceEvent.TRACE_DONE),
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
                (100, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (129, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (129, TraceEvent.TRACE_SWITCH_OUT),
                (129, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (130, TraceEvent.TRACE_SWITCH_IN),
                (179, TraceEvent.TRACE_SWITCH_OUT),
                (179, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (0, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (99, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (99, TraceEvent.TRACE_SWITCH_OUT),
                (180, TraceEvent.TRACE_SWITCH_IN),
                (199, TraceEvent.TRACE_SWITCH_OUT),
                (199, TraceEvent.TRACE_DONE),
            ],
        },
    },
    "SRP2": {
        "name": "Complex Multi-Resource SRP Validation",
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (400, TraceEvent.TRACE_RELEASE),
                (481, TraceEvent.TRACE_SWITCH_IN),
                (573, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (618, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (663, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (708, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (753, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (798, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (843, TraceEvent.TRACE_SWITCH_OUT),
                (843, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 02": [
                (279, TraceEvent.TRACE_RELEASE),
                (279, TraceEvent.TRACE_SWITCH_IN),
                (371, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (480, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (480, TraceEvent.TRACE_SWITCH_OUT),
                (844, TraceEvent.TRACE_SWITCH_IN),
                (936, TraceEvent.TRACE_SWITCH_OUT),
                (936, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 03": [
                (150, TraceEvent.TRACE_RELEASE),
                (250, TraceEvent.TRACE_SWITCH_IN),
                (279, TraceEvent.TRACE_SWITCH_OUT),
                (937, TraceEvent.TRACE_SWITCH_IN),
                (1199, TraceEvent.TRACE_SWITCH_OUT),
                (1199, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 04": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (92, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (249, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (249, TraceEvent.TRACE_SWITCH_OUT),
                (1200, TraceEvent.TRACE_SWITCH_IN),
                (1292, TraceEvent.TRACE_SWITCH_OUT),
                (1292, TraceEvent.TRACE_DONE),
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
                (149, TraceEvent.TRACE_SWITCH_OUT),
                (149, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (150, TraceEvent.TRACE_SWITCH_IN),
                (249, TraceEvent.TRACE_SWITCH_OUT),
                (249, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 03": [
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (99, TraceEvent.TRACE_SWITCH_OUT),
                (99, TraceEvent.TRACE_DONE),
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
                (149, TraceEvent.TRACE_SWITCH_OUT),
                (149, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (150, TraceEvent.TRACE_SWITCH_IN),
                (249, TraceEvent.TRACE_SWITCH_OUT),
                (249, TraceEvent.TRACE_DONE),
            ],
            "Aperiodic 03": [
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (99, TraceEvent.TRACE_SWITCH_OUT),
                (99, TraceEvent.TRACE_DONE),
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
    # SMP TESTS
    "SMP1": {
        "name": "Simple EDF test on both cores",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01 C0": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_DONE),
            ],
            "Periodic 02 C0": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (0, TraceEvent.TRACE_SWITCH_OUT),
                (0, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (2, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (4, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_DONE),
            ],
            "Periodic 01 C1": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_DONE),
            ],
            "Periodic 02 C1": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (0, TraceEvent.TRACE_SWITCH_OUT),
                (0, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (2, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (4, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_DONE),
            ],
        },
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
