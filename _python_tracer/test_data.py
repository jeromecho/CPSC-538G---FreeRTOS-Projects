from enum import Enum
import re

TraceEvent = Enum(
    "TraceEvent",
    [
        "TRACE_RELEASE",
        "TRACE_SWITCH_IN",
        "TRACE_SWITCH_OUT",
        "TRACE_DONE",
        "TRACE_PREPARING_CONTEXT_SWITCH",
        "TRACE_SUSPENDED",
        "TRACE_RESUMED",
        "TRACE_DEADLINE_MISS",
        "TRACE_ADMISSION_FAILED",
        "TRACE_SEMAPHORE_TAKE",
        "TRACE_SEMAPHORE_GIVE",
        "TRACE_BUDGET_RUN_OUT",
        "TRACE_REMOVED_FROM_CORE",
        "TRACE_MIGRATED_TO_CORE",
        "TRACE_DEBUG_MARKER",
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
                (4, TraceEvent.TRACE_DONE),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_DONE),
                (10, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (1, TraceEvent.TRACE_DONE),
                (1, TraceEvent.TRACE_SWITCH_OUT),
                #
                (2, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_DONE),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                #
                (4, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_DONE),
                (5, TraceEvent.TRACE_SWITCH_OUT),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_DONE),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_DONE),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                #
                (10, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                # (11, TraceEvent.TRACE_DONE), # Doesn't happen since test ends first
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
                (2, TraceEvent.TRACE_DONE),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                #
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_DONE),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                #
                (12, TraceEvent.TRACE_RELEASE),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (16, TraceEvent.TRACE_DONE),
                (16, TraceEvent.TRACE_SWITCH_OUT),
                #
                (18, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_DONE),
                (20, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                #
                (8, TraceEvent.TRACE_RELEASE),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_DONE),
                (11, TraceEvent.TRACE_SWITCH_OUT),
                #
                (16, TraceEvent.TRACE_RELEASE),
                (16, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_DONE),
                (18, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_DONE),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                #
                (9, TraceEvent.TRACE_RELEASE),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_DONE),
                (14, TraceEvent.TRACE_SWITCH_OUT),
                #
                (18, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                # (23, TraceEvent.TRACE_DONE), # Doesn't happen since test ends first
                (23, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "EDF3": {
        "name": "100 Tasks NON-ADMISSIBLE",
        "expected_admission_failure": "Periodic 34",
        "ignore_traces": True,
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
        "ignore_traces": True,
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
        "ignore_traces": True,
        "expected_events": {},
    },
    "EDF9": {
        "name": "Admissible drop-in",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (160, TraceEvent.TRACE_DONE),
                (160, TraceEvent.TRACE_SWITCH_OUT),
                #
                (800, TraceEvent.TRACE_RELEASE),
                (800, TraceEvent.TRACE_SWITCH_IN),
                (960, TraceEvent.TRACE_DONE),
                (960, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (800, TraceEvent.TRACE_RELEASE),
                (960, TraceEvent.TRACE_SWITCH_IN),
                # (1360, TraceEvent.TRACE_DONE),  # Doesn't happen since test ends first
                (1360, TraceEvent.TRACE_SWITCH_OUT),
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
                (20, TraceEvent.TRACE_DONE),
                (20, TraceEvent.TRACE_SWITCH_OUT),
                #
                (100, TraceEvent.TRACE_RELEASE),
                (100, TraceEvent.TRACE_SWITCH_IN),
                (120, TraceEvent.TRACE_DONE),
                (120, TraceEvent.TRACE_SWITCH_OUT),
                #
                (200, TraceEvent.TRACE_RELEASE),
                (200, TraceEvent.TRACE_SWITCH_IN),
                (220, TraceEvent.TRACE_DONE),
                (220, TraceEvent.TRACE_SWITCH_OUT),
                #
                (300, TraceEvent.TRACE_RELEASE),
                (300, TraceEvent.TRACE_SWITCH_IN),
                (320, TraceEvent.TRACE_DONE),
                (320, TraceEvent.TRACE_SWITCH_OUT),
                #
                (400, TraceEvent.TRACE_RELEASE),
                (400, TraceEvent.TRACE_SWITCH_IN),
                (420, TraceEvent.TRACE_DONE),
                (420, TraceEvent.TRACE_SWITCH_OUT),
                #
                (500, TraceEvent.TRACE_RELEASE),
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
                (50, TraceEvent.TRACE_DONE),
                (50, TraceEvent.TRACE_SWITCH_OUT),
                #
                (120, TraceEvent.TRACE_RELEASE),
                (120, TraceEvent.TRACE_SWITCH_IN),
                (170, TraceEvent.TRACE_DONE),
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
                (100, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (130, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (130, TraceEvent.TRACE_DONE),
                (130, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (130, TraceEvent.TRACE_SWITCH_IN),
                (180, TraceEvent.TRACE_DONE),
                (180, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (0, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (100, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (100, TraceEvent.TRACE_SWITCH_OUT),
                (180, TraceEvent.TRACE_SWITCH_IN),
                (200, TraceEvent.TRACE_DONE),
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
                (481, TraceEvent.TRACE_SWITCH_IN),
                (574, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (619, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (664, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (709, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (754, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (799, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (844, TraceEvent.TRACE_DONE),
                (844, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (279, TraceEvent.TRACE_RELEASE),
                (279, TraceEvent.TRACE_SWITCH_IN),
                (372, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (481, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (481, TraceEvent.TRACE_SWITCH_OUT),
                (844, TraceEvent.TRACE_SWITCH_IN),
                (937, TraceEvent.TRACE_DONE),
                (937, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (150, TraceEvent.TRACE_RELEASE),
                (250, TraceEvent.TRACE_SWITCH_IN),
                (279, TraceEvent.TRACE_SWITCH_OUT),
                (937, TraceEvent.TRACE_SWITCH_IN),
                (998, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (1107, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (1200, TraceEvent.TRACE_DONE),
                (1200, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 04": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (93, TraceEvent.TRACE_SEMAPHORE_TAKE),
                (250, TraceEvent.TRACE_SEMAPHORE_GIVE),
                (250, TraceEvent.TRACE_SWITCH_OUT),
                (1200, TraceEvent.TRACE_SWITCH_IN),
                (1293, TraceEvent.TRACE_DONE),
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
                (150, TraceEvent.TRACE_DONE),
                (150, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (150, TraceEvent.TRACE_SWITCH_IN),
                (250, TraceEvent.TRACE_DONE),
                (250, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_DONE),
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
                (150, TraceEvent.TRACE_DONE),
                (150, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (20, TraceEvent.TRACE_RELEASE),
                (150, TraceEvent.TRACE_SWITCH_IN),
                (250, TraceEvent.TRACE_DONE),
                (250, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_DONE),
                (100, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
        "expected_less_bss_than": "SRP3",
    },
    # Test 5 and 6 should be 100 tasks running sequentially.
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
        "ignore_traces": True,
        "expected_events": {},
    },
    "SRP9": {
        "name": "Admission Control - Fail (Constrained Deadlines)",
        "expected_admission_failure": "Periodic 03",
        "ignore_traces": True,
        "expected_events": {},
    },
    # CBS TESTS
    "CBS1": {
        "name": "Smoke test (textbook pg.190): 1 periodic task with 2 aperiodic tasks; 1 CBS server",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_DONE),
                #
                (11, TraceEvent.TRACE_SWITCH_OUT),
                (14, TraceEvent.TRACE_RELEASE),
                (15, TraceEvent.TRACE_SWITCH_IN),
                (19, TraceEvent.TRACE_DONE),
                #
                (19, TraceEvent.TRACE_SWITCH_OUT),
                (21, TraceEvent.TRACE_RELEASE),
            ],
            "Aperiodic 01": [
                (3, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (13, TraceEvent.TRACE_RELEASE),
                (13, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (19, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_DONE),
                #
                (20, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS2": {
        "name": "Single aperiodic task running on CBS server",
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS3": {
        "name": "Multiple tasks queueing up to max capacity on 1 CBS server",
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (6, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (9, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (12, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (15, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (18, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (21, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (27, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (30, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (33, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (36, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (39, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (40, TraceEvent.TRACE_DONE),
                #
                (40, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS4": {
        "name": "Smoke Test #2: Different setup with 1 periodic task and 1 CBS server",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (5, TraceEvent.TRACE_RELEASE),
                (5, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_DONE),
                #
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (15, TraceEvent.TRACE_RELEASE),
                (15, TraceEvent.TRACE_SWITCH_IN),
                (17, TraceEvent.TRACE_DONE),
                #
                (17, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_DONE),
                #
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (25, TraceEvent.TRACE_RELEASE),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_DONE),
                #
                (27, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_RELEASE),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (5, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (10, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (17, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (20, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (20, TraceEvent.TRACE_SWITCH_OUT),
                (22, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_DONE),
                #
                (24, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS5": {
        "name": "Smoke Test #3: Multiple Periodic Tasks Running Alongside Single CBS Server",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (4, TraceEvent.TRACE_RELEASE),
                (5, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_DONE),
                #
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_RELEASE),
                (13, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_DONE),
                #
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (16, TraceEvent.TRACE_RELEASE),
                (16, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_DONE),
                #
                (18, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (21, TraceEvent.TRACE_SWITCH_IN),
                (23, TraceEvent.TRACE_DONE),
                #
                (23, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (26, TraceEvent.TRACE_DONE),
                #
                (26, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_RELEASE),
                (29, TraceEvent.TRACE_SWITCH_IN),
                (31, TraceEvent.TRACE_DONE),
                #
                (31, TraceEvent.TRACE_SWITCH_OUT),
                (32, TraceEvent.TRACE_RELEASE),
                (32, TraceEvent.TRACE_SWITCH_IN),
                (34, TraceEvent.TRACE_DONE),
                #
                (34, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (37, TraceEvent.TRACE_SWITCH_IN),
                (39, TraceEvent.TRACE_DONE),
                #
                (39, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_DONE),
                #
                (42, TraceEvent.TRACE_SWITCH_OUT),
                (44, TraceEvent.TRACE_RELEASE),
                (45, TraceEvent.TRACE_SWITCH_IN),
                (47, TraceEvent.TRACE_DONE),
                #
                (47, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
                (48, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_DONE),
                #
                (5, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_RELEASE),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (13, TraceEvent.TRACE_DONE),
                #
                (13, TraceEvent.TRACE_SWITCH_OUT),
                (16, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (21, TraceEvent.TRACE_DONE),
                #
                (21, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (26, TraceEvent.TRACE_SWITCH_IN),
                (29, TraceEvent.TRACE_DONE),
                #
                (29, TraceEvent.TRACE_SWITCH_OUT),
                (32, TraceEvent.TRACE_RELEASE),
                (34, TraceEvent.TRACE_SWITCH_IN),
                (37, TraceEvent.TRACE_DONE),
                #
                (37, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (45, TraceEvent.TRACE_DONE),
                #
                (45, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (15, TraceEvent.TRACE_SWITCH_IN),
                (16, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (16, TraceEvent.TRACE_SWITCH_OUT),
                (23, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (31, TraceEvent.TRACE_SWITCH_IN),
                (32, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (32, TraceEvent.TRACE_SWITCH_OUT),
                (39, TraceEvent.TRACE_SWITCH_IN),
                (40, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (40, TraceEvent.TRACE_SWITCH_OUT),
                (47, TraceEvent.TRACE_SWITCH_IN),
                (48, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (48, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS6": {
        "name": "Multiple Periodic Tasks Running Alongside 2 symmetric CBS servers",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_DONE),
                #
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_DONE),
                #
                (9, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_RELEASE),
                (13, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_DONE),
                #
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (18, TraceEvent.TRACE_RELEASE),
                (19, TraceEvent.TRACE_SWITCH_IN),
                (21, TraceEvent.TRACE_DONE),
                #
                (21, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_DONE),
                #
                (27, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_RELEASE),
                (31, TraceEvent.TRACE_SWITCH_IN),
                (33, TraceEvent.TRACE_DONE),
                #
                (33, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (37, TraceEvent.TRACE_SWITCH_IN),
                (39, TraceEvent.TRACE_DONE),
                #
                (39, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_RELEASE),
                (43, TraceEvent.TRACE_SWITCH_IN),
                (45, TraceEvent.TRACE_DONE),
                #
                (45, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
                (49, TraceEvent.TRACE_SWITCH_IN),
                (51, TraceEvent.TRACE_DONE),
                #
                (51, TraceEvent.TRACE_SWITCH_OUT),
                (54, TraceEvent.TRACE_RELEASE),
                (55, TraceEvent.TRACE_SWITCH_IN),
                (57, TraceEvent.TRACE_DONE),
                #
                (57, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (61, TraceEvent.TRACE_SWITCH_IN),
                (63, TraceEvent.TRACE_DONE),
                #
                (63, TraceEvent.TRACE_SWITCH_OUT),
                (66, TraceEvent.TRACE_RELEASE),
                (67, TraceEvent.TRACE_SWITCH_IN),
                (69, TraceEvent.TRACE_DONE),
                #
                (69, TraceEvent.TRACE_SWITCH_OUT),
                (72, TraceEvent.TRACE_RELEASE),
                (73, TraceEvent.TRACE_SWITCH_IN),
                (75, TraceEvent.TRACE_DONE),
                #
                (75, TraceEvent.TRACE_SWITCH_OUT),
                (78, TraceEvent.TRACE_RELEASE),
                (79, TraceEvent.TRACE_SWITCH_IN),
                (81, TraceEvent.TRACE_DONE),
                #
                (81, TraceEvent.TRACE_SWITCH_OUT),
                (84, TraceEvent.TRACE_RELEASE),
                (85, TraceEvent.TRACE_SWITCH_IN),
                (87, TraceEvent.TRACE_DONE),
                #
                (87, TraceEvent.TRACE_SWITCH_OUT),
                (90, TraceEvent.TRACE_RELEASE),
                (91, TraceEvent.TRACE_SWITCH_IN),
                (93, TraceEvent.TRACE_DONE),
                #
                (93, TraceEvent.TRACE_SWITCH_OUT),
                (96, TraceEvent.TRACE_RELEASE),
                (97, TraceEvent.TRACE_SWITCH_IN),
                (99, TraceEvent.TRACE_DONE),
                #
                (99, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (1, TraceEvent.TRACE_DONE),
                #
                (1, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_RELEASE),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_DONE),
                #
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_RELEASE),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_RELEASE),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (13, TraceEvent.TRACE_DONE),
                #
                (13, TraceEvent.TRACE_SWITCH_OUT),
                (15, TraceEvent.TRACE_RELEASE),
                (15, TraceEvent.TRACE_SWITCH_IN),
                (16, TraceEvent.TRACE_DONE),
                #
                (16, TraceEvent.TRACE_SWITCH_OUT),
                (18, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (19, TraceEvent.TRACE_DONE),
                #
                (19, TraceEvent.TRACE_SWITCH_OUT),
                (21, TraceEvent.TRACE_RELEASE),
                (21, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_DONE),
                #
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_DONE),
                #
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (27, TraceEvent.TRACE_RELEASE),
                (27, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_RELEASE),
                (30, TraceEvent.TRACE_SWITCH_IN),
                (31, TraceEvent.TRACE_DONE),
                #
                (31, TraceEvent.TRACE_SWITCH_OUT),
                (33, TraceEvent.TRACE_RELEASE),
                (33, TraceEvent.TRACE_SWITCH_IN),
                (34, TraceEvent.TRACE_DONE),
                #
                (34, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (36, TraceEvent.TRACE_SWITCH_IN),
                (37, TraceEvent.TRACE_DONE),
                #
                (37, TraceEvent.TRACE_SWITCH_OUT),
                (39, TraceEvent.TRACE_RELEASE),
                (39, TraceEvent.TRACE_SWITCH_IN),
                (40, TraceEvent.TRACE_DONE),
                #
                (40, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (43, TraceEvent.TRACE_DONE),
                #
                (43, TraceEvent.TRACE_SWITCH_OUT),
                (45, TraceEvent.TRACE_RELEASE),
                (45, TraceEvent.TRACE_SWITCH_IN),
                (46, TraceEvent.TRACE_DONE),
                #
                (46, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
                (48, TraceEvent.TRACE_SWITCH_IN),
                (49, TraceEvent.TRACE_DONE),
                #
                (49, TraceEvent.TRACE_SWITCH_OUT),
                (51, TraceEvent.TRACE_RELEASE),
                (51, TraceEvent.TRACE_SWITCH_IN),
                (52, TraceEvent.TRACE_DONE),
                #
                (52, TraceEvent.TRACE_SWITCH_OUT),
                (54, TraceEvent.TRACE_RELEASE),
                (54, TraceEvent.TRACE_SWITCH_IN),
                (55, TraceEvent.TRACE_DONE),
                #
                (55, TraceEvent.TRACE_SWITCH_OUT),
                (57, TraceEvent.TRACE_RELEASE),
                (57, TraceEvent.TRACE_SWITCH_IN),
                (58, TraceEvent.TRACE_DONE),
                #
                (58, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (60, TraceEvent.TRACE_SWITCH_IN),
                (61, TraceEvent.TRACE_DONE),
                #
                (61, TraceEvent.TRACE_SWITCH_OUT),
                (63, TraceEvent.TRACE_RELEASE),
                (63, TraceEvent.TRACE_SWITCH_IN),
                (64, TraceEvent.TRACE_DONE),
                #
                (64, TraceEvent.TRACE_SWITCH_OUT),
                (66, TraceEvent.TRACE_RELEASE),
                (66, TraceEvent.TRACE_SWITCH_IN),
                (67, TraceEvent.TRACE_DONE),
                #
                (67, TraceEvent.TRACE_SWITCH_OUT),
                (69, TraceEvent.TRACE_RELEASE),
                (69, TraceEvent.TRACE_SWITCH_IN),
                (70, TraceEvent.TRACE_DONE),
                #
                (70, TraceEvent.TRACE_SWITCH_OUT),
                (72, TraceEvent.TRACE_RELEASE),
                (72, TraceEvent.TRACE_SWITCH_IN),
                (73, TraceEvent.TRACE_DONE),
                #
                (73, TraceEvent.TRACE_SWITCH_OUT),
                (75, TraceEvent.TRACE_RELEASE),
                (75, TraceEvent.TRACE_SWITCH_IN),
                (76, TraceEvent.TRACE_DONE),
                #
                (76, TraceEvent.TRACE_SWITCH_OUT),
                (78, TraceEvent.TRACE_RELEASE),
                (78, TraceEvent.TRACE_SWITCH_IN),
                (79, TraceEvent.TRACE_DONE),
                #
                (79, TraceEvent.TRACE_SWITCH_OUT),
                (81, TraceEvent.TRACE_RELEASE),
                (81, TraceEvent.TRACE_SWITCH_IN),
                (82, TraceEvent.TRACE_DONE),
                #
                (82, TraceEvent.TRACE_SWITCH_OUT),
                (84, TraceEvent.TRACE_RELEASE),
                (84, TraceEvent.TRACE_SWITCH_IN),
                (85, TraceEvent.TRACE_DONE),
                #
                (85, TraceEvent.TRACE_SWITCH_OUT),
                (87, TraceEvent.TRACE_RELEASE),
                (87, TraceEvent.TRACE_SWITCH_IN),
                (88, TraceEvent.TRACE_DONE),
                #
                (88, TraceEvent.TRACE_SWITCH_OUT),
                (90, TraceEvent.TRACE_RELEASE),
                (90, TraceEvent.TRACE_SWITCH_IN),
                (91, TraceEvent.TRACE_DONE),
                #
                (91, TraceEvent.TRACE_SWITCH_OUT),
                (93, TraceEvent.TRACE_RELEASE),
                (93, TraceEvent.TRACE_SWITCH_IN),
                (94, TraceEvent.TRACE_DONE),
                #
                (94, TraceEvent.TRACE_SWITCH_OUT),
                (96, TraceEvent.TRACE_RELEASE),
                (96, TraceEvent.TRACE_SWITCH_IN),
                (97, TraceEvent.TRACE_DONE),
                #
                (97, TraceEvent.TRACE_SWITCH_OUT),
                (99, TraceEvent.TRACE_RELEASE),
                (99, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (5, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (11, TraceEvent.TRACE_SWITCH_OUT),
                (16, TraceEvent.TRACE_SWITCH_IN),
                (17, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (17, TraceEvent.TRACE_SWITCH_OUT),
                (22, TraceEvent.TRACE_SWITCH_IN),
                (23, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (23, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (29, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (29, TraceEvent.TRACE_SWITCH_OUT),
                (34, TraceEvent.TRACE_SWITCH_IN),
                (35, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (35, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (41, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (41, TraceEvent.TRACE_SWITCH_OUT),
                (46, TraceEvent.TRACE_SWITCH_IN),
                (47, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (47, TraceEvent.TRACE_SWITCH_OUT),
                (52, TraceEvent.TRACE_DONE),
                #
                (52, TraceEvent.TRACE_SWITCH_IN),
                (52, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (5, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (17, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (18, TraceEvent.TRACE_SWITCH_OUT),
                (23, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (29, TraceEvent.TRACE_SWITCH_IN),
                (30, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (30, TraceEvent.TRACE_SWITCH_OUT),
                (35, TraceEvent.TRACE_SWITCH_IN),
                (36, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (36, TraceEvent.TRACE_SWITCH_OUT),
                (41, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_SWITCH_OUT),
                (47, TraceEvent.TRACE_SWITCH_IN),
                (48, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (48, TraceEvent.TRACE_SWITCH_OUT),
                (52, TraceEvent.TRACE_SWITCH_IN),
                (53, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (53, TraceEvent.TRACE_DONE),
                #
                (53, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS7": {
        "name": "Smoke Test #4: Multiple Periodic Tasks Running Alongside 2 asymmetric CBS servers",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_DONE),
                #
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_RELEASE),
                (15, TraceEvent.TRACE_SWITCH_IN),
                (19, TraceEvent.TRACE_DONE),
                #
                (19, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (44, TraceEvent.TRACE_DONE),
                #
                (44, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
                (49, TraceEvent.TRACE_SWITCH_IN),
                (53, TraceEvent.TRACE_DONE),
                #
                (53, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (60, TraceEvent.TRACE_SWITCH_IN),
                (64, TraceEvent.TRACE_DONE),
                #
                (64, TraceEvent.TRACE_SWITCH_OUT),
                (72, TraceEvent.TRACE_RELEASE),
                (75, TraceEvent.TRACE_SWITCH_IN),
                (79, TraceEvent.TRACE_DONE),
                #
                (79, TraceEvent.TRACE_SWITCH_OUT),
                (84, TraceEvent.TRACE_RELEASE),
                (84, TraceEvent.TRACE_SWITCH_IN),
                (88, TraceEvent.TRACE_DONE),
                #
                (88, TraceEvent.TRACE_SWITCH_OUT),
                (96, TraceEvent.TRACE_RELEASE),
                (96, TraceEvent.TRACE_SWITCH_IN),
                (100, TraceEvent.TRACE_DONE),
                #
                (100, TraceEvent.TRACE_SWITCH_OUT),
                (108, TraceEvent.TRACE_RELEASE),
                (111, TraceEvent.TRACE_SWITCH_IN),
                (115, TraceEvent.TRACE_DONE),
                #
                (115, TraceEvent.TRACE_SWITCH_OUT),
                (120, TraceEvent.TRACE_RELEASE),
                (120, TraceEvent.TRACE_SWITCH_IN),
                (124, TraceEvent.TRACE_DONE),
                #
                (124, TraceEvent.TRACE_SWITCH_OUT),
                (132, TraceEvent.TRACE_RELEASE),
                (132, TraceEvent.TRACE_SWITCH_IN),
                (136, TraceEvent.TRACE_DONE),
                #
                (136, TraceEvent.TRACE_SWITCH_OUT),
                (144, TraceEvent.TRACE_RELEASE),
                (147, TraceEvent.TRACE_SWITCH_IN),
                (151, TraceEvent.TRACE_DONE),
                #
                (151, TraceEvent.TRACE_SWITCH_OUT),
                (156, TraceEvent.TRACE_RELEASE),
                (156, TraceEvent.TRACE_SWITCH_IN),
                (160, TraceEvent.TRACE_DONE),
                #
                (160, TraceEvent.TRACE_SWITCH_OUT),
                (168, TraceEvent.TRACE_RELEASE),
                (168, TraceEvent.TRACE_SWITCH_IN),
                (172, TraceEvent.TRACE_DONE),
                #
                (172, TraceEvent.TRACE_SWITCH_OUT),
                (180, TraceEvent.TRACE_RELEASE),
                (183, TraceEvent.TRACE_SWITCH_IN),
                (187, TraceEvent.TRACE_DONE),
                #
                (187, TraceEvent.TRACE_SWITCH_OUT),
                (192, TraceEvent.TRACE_RELEASE),
                (192, TraceEvent.TRACE_SWITCH_IN),
                (196, TraceEvent.TRACE_DONE),
                #
                (196, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Periodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_DONE),
                #
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (9, TraceEvent.TRACE_RELEASE),
                (9, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (18, TraceEvent.TRACE_RELEASE),
                (19, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_DONE),
                #
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (27, TraceEvent.TRACE_RELEASE),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (31, TraceEvent.TRACE_DONE),
                #
                (31, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (36, TraceEvent.TRACE_SWITCH_IN),
                (39, TraceEvent.TRACE_DONE),
                #
                (39, TraceEvent.TRACE_SWITCH_OUT),
                (45, TraceEvent.TRACE_RELEASE),
                (45, TraceEvent.TRACE_SWITCH_IN),
                (48, TraceEvent.TRACE_DONE),
                #
                (48, TraceEvent.TRACE_SWITCH_OUT),
                (54, TraceEvent.TRACE_RELEASE),
                (54, TraceEvent.TRACE_SWITCH_IN),
                (57, TraceEvent.TRACE_DONE),
                #
                (57, TraceEvent.TRACE_SWITCH_OUT),
                (63, TraceEvent.TRACE_RELEASE),
                (64, TraceEvent.TRACE_SWITCH_IN),
                (67, TraceEvent.TRACE_DONE),
                #
                (67, TraceEvent.TRACE_SWITCH_OUT),
                (72, TraceEvent.TRACE_RELEASE),
                (72, TraceEvent.TRACE_SWITCH_IN),
                (75, TraceEvent.TRACE_DONE),
                #
                (75, TraceEvent.TRACE_SWITCH_OUT),
                (81, TraceEvent.TRACE_RELEASE),
                (81, TraceEvent.TRACE_SWITCH_IN),
                (84, TraceEvent.TRACE_DONE),
                #
                (84, TraceEvent.TRACE_SWITCH_OUT),
                (90, TraceEvent.TRACE_RELEASE),
                (90, TraceEvent.TRACE_SWITCH_IN),
                (93, TraceEvent.TRACE_DONE),
                #
                (93, TraceEvent.TRACE_SWITCH_OUT),
                (99, TraceEvent.TRACE_RELEASE),
                (100, TraceEvent.TRACE_SWITCH_IN),
                (103, TraceEvent.TRACE_DONE),
                #
                (103, TraceEvent.TRACE_SWITCH_OUT),
                (108, TraceEvent.TRACE_RELEASE),
                (108, TraceEvent.TRACE_SWITCH_IN),
                (111, TraceEvent.TRACE_DONE),
                #
                (111, TraceEvent.TRACE_SWITCH_OUT),
                (117, TraceEvent.TRACE_RELEASE),
                (117, TraceEvent.TRACE_SWITCH_IN),
                (120, TraceEvent.TRACE_DONE),
                #
                (120, TraceEvent.TRACE_SWITCH_OUT),
                (126, TraceEvent.TRACE_RELEASE),
                (126, TraceEvent.TRACE_SWITCH_IN),
                (129, TraceEvent.TRACE_DONE),
                #
                (129, TraceEvent.TRACE_SWITCH_OUT),
                (135, TraceEvent.TRACE_RELEASE),
                (136, TraceEvent.TRACE_SWITCH_IN),
                (139, TraceEvent.TRACE_DONE),
                #
                (139, TraceEvent.TRACE_SWITCH_OUT),
                (144, TraceEvent.TRACE_RELEASE),
                (144, TraceEvent.TRACE_SWITCH_IN),
                (147, TraceEvent.TRACE_DONE),
                #
                (147, TraceEvent.TRACE_SWITCH_OUT),
                (153, TraceEvent.TRACE_RELEASE),
                (153, TraceEvent.TRACE_SWITCH_IN),
                (156, TraceEvent.TRACE_DONE),
                #
                (156, TraceEvent.TRACE_SWITCH_OUT),
                (162, TraceEvent.TRACE_RELEASE),
                (162, TraceEvent.TRACE_SWITCH_IN),
                (165, TraceEvent.TRACE_DONE),
                #
                (165, TraceEvent.TRACE_SWITCH_OUT),
                (171, TraceEvent.TRACE_RELEASE),
                (172, TraceEvent.TRACE_SWITCH_IN),
                (175, TraceEvent.TRACE_DONE),
                #
                (175, TraceEvent.TRACE_SWITCH_OUT),
                (180, TraceEvent.TRACE_RELEASE),
                (180, TraceEvent.TRACE_SWITCH_IN),
                (183, TraceEvent.TRACE_DONE),
                #
                (183, TraceEvent.TRACE_SWITCH_OUT),
                (189, TraceEvent.TRACE_RELEASE),
                (189, TraceEvent.TRACE_SWITCH_IN),
                (192, TraceEvent.TRACE_DONE),
                #
                (192, TraceEvent.TRACE_SWITCH_OUT),
                (198, TraceEvent.TRACE_RELEASE),
                (198, TraceEvent.TRACE_SWITCH_IN),
                (200, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (9, TraceEvent.TRACE_SWITCH_OUT),
                (22, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (35, TraceEvent.TRACE_SWITCH_IN),
                (36, TraceEvent.TRACE_SWITCH_OUT),
                (39, TraceEvent.TRACE_SWITCH_IN),
                (40, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (40, TraceEvent.TRACE_SWITCH_OUT),
                (53, TraceEvent.TRACE_SWITCH_IN),
                (54, TraceEvent.TRACE_SWITCH_OUT),
                (57, TraceEvent.TRACE_SWITCH_IN),
                (58, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (58, TraceEvent.TRACE_DONE),
                #
                (58, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_DONE),
                #
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (31, TraceEvent.TRACE_SWITCH_IN),
                (35, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (35, TraceEvent.TRACE_SWITCH_OUT),
                (44, TraceEvent.TRACE_SWITCH_IN),
                (45, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_SWITCH_IN),
                (49, TraceEvent.TRACE_DONE),
                #
                (49, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS8": {
        "name": "Multiple (2) symmetric CBS servers in isolation",
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (1, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (1, TraceEvent.TRACE_SWITCH_OUT),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (5, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (21, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (21, TraceEvent.TRACE_SWITCH_OUT),
                (22, TraceEvent.TRACE_SWITCH_IN),
                (23, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (23, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (26, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (27, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (41, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (41, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_DONE),
                #
                (42, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (5, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (21, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (23, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (26, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (26, TraceEvent.TRACE_SWITCH_OUT),
                (27, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (41, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_SWITCH_OUT),
                (43, TraceEvent.TRACE_DONE),
                #
                (43, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (43, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS9": {
        "name": "Multiple (2) asymmetric CBS servers in isolation",
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (1, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (1, TraceEvent.TRACE_SWITCH_OUT),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (6, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (6, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (26, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (27, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (28, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (43, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (43, TraceEvent.TRACE_DONE),
                #
                (43, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (25, TraceEvent.TRACE_DONE),
                #
                (25, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_DONE),
                #
                (42, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS10": {
        "name": "Multiple (3) asymmetric CBS servers in isolation",
        "expected_admission_failure": None,
        "expected_events": {
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (1, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (1, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (9, TraceEvent.TRACE_DONE),
                #
                (9, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (26, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (26, TraceEvent.TRACE_SWITCH_OUT),
                (29, TraceEvent.TRACE_SWITCH_IN),
                (30, TraceEvent.TRACE_SWITCH_IN),
                (30, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (30, TraceEvent.TRACE_SWITCH_OUT),
                (31, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (32, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (32, TraceEvent.TRACE_DONE),
                #
                (32, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (41, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_DONE),
                #
                (42, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (1, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (26, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_DONE),
                #
                (30, TraceEvent.TRACE_SWITCH_IN),
                (30, TraceEvent.TRACE_SWITCH_OUT),
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (52, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (52, TraceEvent.TRACE_DONE),
                #
                (52, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (22, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (29, TraceEvent.TRACE_DONE),
                #
                (29, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (60, TraceEvent.TRACE_SWITCH_IN),
                (62, TraceEvent.TRACE_DONE),
                #
                (62, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS11": {
        "name": "Multiple (2) CBS servers running alongside 1 periodic task",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_RELEASE),
                (6, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_DONE),
                #
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_RELEASE),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_DONE),
                #
                (14, TraceEvent.TRACE_SWITCH_OUT),
                (18, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_DONE),
                #
                (20, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (26, TraceEvent.TRACE_DONE),
                #
                (26, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_RELEASE),
                (30, TraceEvent.TRACE_SWITCH_IN),
                (32, TraceEvent.TRACE_DONE),
                #
                (32, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (36, TraceEvent.TRACE_SWITCH_IN),
                (38, TraceEvent.TRACE_DONE),
                #
                (38, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (44, TraceEvent.TRACE_DONE),
                #
                (44, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
                (48, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_DONE),
                #
                (50, TraceEvent.TRACE_SWITCH_OUT),
                (54, TraceEvent.TRACE_RELEASE),
                (54, TraceEvent.TRACE_SWITCH_IN),
                (56, TraceEvent.TRACE_DONE),
                #
                (56, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (60, TraceEvent.TRACE_SWITCH_IN),
                (62, TraceEvent.TRACE_DONE),
                #
                (62, TraceEvent.TRACE_SWITCH_OUT),
                (66, TraceEvent.TRACE_RELEASE),
                (66, TraceEvent.TRACE_SWITCH_IN),
                (68, TraceEvent.TRACE_DONE),
                #
                (68, TraceEvent.TRACE_SWITCH_OUT),
                (72, TraceEvent.TRACE_RELEASE),
                (72, TraceEvent.TRACE_SWITCH_IN),
                (74, TraceEvent.TRACE_DONE),
                #
                (74, TraceEvent.TRACE_SWITCH_OUT),
                (78, TraceEvent.TRACE_RELEASE),
                (78, TraceEvent.TRACE_SWITCH_IN),
                (80, TraceEvent.TRACE_DONE),
                #
                (80, TraceEvent.TRACE_SWITCH_OUT),
                (84, TraceEvent.TRACE_RELEASE),
                (84, TraceEvent.TRACE_SWITCH_IN),
                (86, TraceEvent.TRACE_DONE),
                #
                (86, TraceEvent.TRACE_SWITCH_OUT),
                (90, TraceEvent.TRACE_RELEASE),
                (90, TraceEvent.TRACE_SWITCH_IN),
                (92, TraceEvent.TRACE_DONE),
                #
                (92, TraceEvent.TRACE_SWITCH_OUT),
                (96, TraceEvent.TRACE_RELEASE),
                (96, TraceEvent.TRACE_SWITCH_IN),
                (98, TraceEvent.TRACE_DONE),
                #
                (98, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (9, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (10, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (10, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (26, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (27, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (29, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (30, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (30, TraceEvent.TRACE_SWITCH_OUT),
                (32, TraceEvent.TRACE_DONE),
                #
                (32, TraceEvent.TRACE_SWITCH_IN),
                (32, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (44, TraceEvent.TRACE_SWITCH_IN),
                (45, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (45, TraceEvent.TRACE_DONE),
                #
                (45, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (6, TraceEvent.TRACE_DONE),
                #
                (6, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (27, TraceEvent.TRACE_DONE),
                #
                (27, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_DONE),
                #
                (42, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS12": {
        "name": "Multiple (3) CBS servers running alongside 1 periodic task",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (2, TraceEvent.TRACE_DONE),
                #
                (2, TraceEvent.TRACE_SWITCH_OUT),
                (6, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_DONE),
                #
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_RELEASE),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_DONE),
                #
                (14, TraceEvent.TRACE_SWITCH_OUT),
                (18, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_DONE),
                #
                (20, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_RELEASE),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (27, TraceEvent.TRACE_DONE),
                #
                (27, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_RELEASE),
                (31, TraceEvent.TRACE_SWITCH_IN),
                (33, TraceEvent.TRACE_DONE),
                #
                (33, TraceEvent.TRACE_SWITCH_OUT),
                (36, TraceEvent.TRACE_RELEASE),
                (36, TraceEvent.TRACE_SWITCH_IN),
                (38, TraceEvent.TRACE_DONE),
                #
                (38, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (44, TraceEvent.TRACE_DONE),
                #
                (44, TraceEvent.TRACE_SWITCH_OUT),
                (48, TraceEvent.TRACE_RELEASE),
                (48, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_DONE),
                #
                (50, TraceEvent.TRACE_SWITCH_OUT),
                (54, TraceEvent.TRACE_RELEASE),
                (54, TraceEvent.TRACE_SWITCH_IN),
                (56, TraceEvent.TRACE_DONE),
                #
                (56, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (60, TraceEvent.TRACE_SWITCH_IN),
                (62, TraceEvent.TRACE_DONE),
                #
                (62, TraceEvent.TRACE_SWITCH_OUT),
                (66, TraceEvent.TRACE_RELEASE),
                (66, TraceEvent.TRACE_SWITCH_IN),
                (68, TraceEvent.TRACE_DONE),
                #
                (68, TraceEvent.TRACE_SWITCH_OUT),
                (72, TraceEvent.TRACE_RELEASE),
                (72, TraceEvent.TRACE_SWITCH_IN),
                (74, TraceEvent.TRACE_DONE),
                #
                (74, TraceEvent.TRACE_SWITCH_OUT),
                (78, TraceEvent.TRACE_RELEASE),
                (78, TraceEvent.TRACE_SWITCH_IN),
                (80, TraceEvent.TRACE_DONE),
                #
                (80, TraceEvent.TRACE_SWITCH_OUT),
                (84, TraceEvent.TRACE_RELEASE),
                (84, TraceEvent.TRACE_SWITCH_IN),
                (86, TraceEvent.TRACE_DONE),
                #
                (86, TraceEvent.TRACE_SWITCH_OUT),
                (90, TraceEvent.TRACE_RELEASE),
                (90, TraceEvent.TRACE_SWITCH_IN),
                (92, TraceEvent.TRACE_DONE),
                #
                (92, TraceEvent.TRACE_SWITCH_OUT),
                (96, TraceEvent.TRACE_RELEASE),
                (96, TraceEvent.TRACE_SWITCH_IN),
                (98, TraceEvent.TRACE_DONE),
                #
                (98, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (2, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (11, TraceEvent.TRACE_SWITCH_OUT),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (15, TraceEvent.TRACE_DONE),
                #
                (15, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (27, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (33, TraceEvent.TRACE_SWITCH_IN),
                (34, TraceEvent.TRACE_SWITCH_IN),
                (34, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (34, TraceEvent.TRACE_SWITCH_OUT),
                (35, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (36, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (36, TraceEvent.TRACE_SWITCH_OUT),
                (38, TraceEvent.TRACE_DONE),
                #
                (38, TraceEvent.TRACE_SWITCH_IN),
                (38, TraceEvent.TRACE_SWITCH_OUT),
                (40, TraceEvent.TRACE_RELEASE),
                (40, TraceEvent.TRACE_SWITCH_IN),
                (41, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_SWITCH_OUT),
                (44, TraceEvent.TRACE_DONE),
                #
                (44, TraceEvent.TRACE_SWITCH_IN),
                (44, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 02": [
                (0, TraceEvent.TRACE_RELEASE),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (5, TraceEvent.TRACE_SWITCH_OUT),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (22, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (22, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (30, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (30, TraceEvent.TRACE_SWITCH_OUT),
                (34, TraceEvent.TRACE_DONE),
                #
                (34, TraceEvent.TRACE_SWITCH_IN),
                (34, TraceEvent.TRACE_SWITCH_OUT),
                (50, TraceEvent.TRACE_RELEASE),
                (50, TraceEvent.TRACE_SWITCH_IN),
                (52, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (52, TraceEvent.TRACE_DONE),
                #
                (52, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 03": [
                (0, TraceEvent.TRACE_RELEASE),
                (5, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_RELEASE),
                (22, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (30, TraceEvent.TRACE_SWITCH_IN),
                (31, TraceEvent.TRACE_DONE),
                #
                (31, TraceEvent.TRACE_SWITCH_OUT),
                (60, TraceEvent.TRACE_RELEASE),
                (62, TraceEvent.TRACE_SWITCH_IN),
                (64, TraceEvent.TRACE_DONE),
                #
                (64, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS13": {
        "name": "1 CBS Server, 1 periodic task. Bandwidth is high but load of aperiodic tasks is low. (no deadline miss)",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_DONE),
                #
                (11, TraceEvent.TRACE_SWITCH_OUT),
                (14, TraceEvent.TRACE_RELEASE),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_DONE),
                #
                (18, TraceEvent.TRACE_SWITCH_OUT),
                (21, TraceEvent.TRACE_RELEASE),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (5, TraceEvent.TRACE_DONE),
                #
                (5, TraceEvent.TRACE_SWITCH_OUT),
                (8, TraceEvent.TRACE_RELEASE),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (16, TraceEvent.TRACE_RELEASE),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (19, TraceEvent.TRACE_DONE),
                #
                (19, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
        "suite": "CBS",
    },
    "CBS14": {
        "name": "1 CBS Server, 1 periodic task. Bandwidth is high and load of aperiodic tasks is high. (deadline miss)",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_RELEASE),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_DEADLINE_MISS),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (12, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS15": {
        "name": "Bandwidth just under deadline miss threshold (Total Utilization < 100%)",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (11, TraceEvent.TRACE_DONE),
                #
                (11, TraceEvent.TRACE_SWITCH_OUT),
                (14, TraceEvent.TRACE_RELEASE),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (18, TraceEvent.TRACE_DONE),
                #
                (18, TraceEvent.TRACE_SWITCH_OUT),
                (21, TraceEvent.TRACE_RELEASE),
                (21, TraceEvent.TRACE_SWITCH_IN),
                (25, TraceEvent.TRACE_DONE),
                #
                (25, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_RELEASE),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (32, TraceEvent.TRACE_DONE),
                #
                (32, TraceEvent.TRACE_SWITCH_OUT),
                (35, TraceEvent.TRACE_RELEASE),
                (35, TraceEvent.TRACE_SWITCH_IN),
                (39, TraceEvent.TRACE_DONE),
                #
                (39, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (46, TraceEvent.TRACE_DONE),
                #
                (46, TraceEvent.TRACE_SWITCH_OUT),
                (49, TraceEvent.TRACE_RELEASE),
                (49, TraceEvent.TRACE_SWITCH_IN),
                (50, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (11, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (14, TraceEvent.TRACE_SWITCH_OUT),
                (18, TraceEvent.TRACE_SWITCH_IN),
                (21, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (21, TraceEvent.TRACE_SWITCH_OUT),
                (25, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (32, TraceEvent.TRACE_SWITCH_IN),
                (35, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (35, TraceEvent.TRACE_SWITCH_OUT),
                (39, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (42, TraceEvent.TRACE_SWITCH_OUT),
                (46, TraceEvent.TRACE_SWITCH_IN),
                (49, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (49, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS16": {
        "name": "Bandwidth at deadline miss threshold (Total Utilization = 100%)",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (3, TraceEvent.TRACE_SWITCH_IN),
                (7, TraceEvent.TRACE_RELEASE),
                (7, TraceEvent.TRACE_DONE),
                #
                (7, TraceEvent.TRACE_SWITCH_OUT),
                (10, TraceEvent.TRACE_SWITCH_IN),
                (14, TraceEvent.TRACE_RELEASE),
                (14, TraceEvent.TRACE_DONE),
                #
                (14, TraceEvent.TRACE_SWITCH_OUT),
                (17, TraceEvent.TRACE_SWITCH_IN),
                (21, TraceEvent.TRACE_RELEASE),
                (21, TraceEvent.TRACE_DONE),
                #
                (21, TraceEvent.TRACE_SWITCH_OUT),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_RELEASE),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (31, TraceEvent.TRACE_SWITCH_IN),
                (35, TraceEvent.TRACE_RELEASE),
                (35, TraceEvent.TRACE_DONE),
                #
                (35, TraceEvent.TRACE_SWITCH_OUT),
                (38, TraceEvent.TRACE_SWITCH_IN),
                (42, TraceEvent.TRACE_RELEASE),
                (42, TraceEvent.TRACE_DONE),
                #
                (42, TraceEvent.TRACE_SWITCH_OUT),
                (45, TraceEvent.TRACE_SWITCH_IN),
                (49, TraceEvent.TRACE_RELEASE),
                (49, TraceEvent.TRACE_DONE),
                #
                (49, TraceEvent.TRACE_SWITCH_IN),
                (49, TraceEvent.TRACE_SWITCH_OUT),
                (50, TraceEvent.TRACE_SWITCH_OUT),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (3, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (3, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_SWITCH_IN),
                (10, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (10, TraceEvent.TRACE_SWITCH_OUT),
                (14, TraceEvent.TRACE_SWITCH_IN),
                (17, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (17, TraceEvent.TRACE_SWITCH_OUT),
                (21, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (31, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (31, TraceEvent.TRACE_SWITCH_OUT),
                (35, TraceEvent.TRACE_SWITCH_IN),
                (38, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (38, TraceEvent.TRACE_SWITCH_OUT),
                (42, TraceEvent.TRACE_SWITCH_IN),
                (45, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (45, TraceEvent.TRACE_SWITCH_OUT),
                (49, TraceEvent.TRACE_DONE),
                #
                (49, TraceEvent.TRACE_SWITCH_IN),
                (49, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS17": {
        "name": "Bandwidth just over deadline miss threshold (Total Utilization > 100%)",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_RELEASE),
                (8, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_DONE),
                #
                (12, TraceEvent.TRACE_SWITCH_OUT),
                (14, TraceEvent.TRACE_RELEASE),
                (16, TraceEvent.TRACE_SWITCH_IN),
                (20, TraceEvent.TRACE_DONE),
                #
                (20, TraceEvent.TRACE_SWITCH_OUT),
                (21, TraceEvent.TRACE_RELEASE),
                (24, TraceEvent.TRACE_SWITCH_IN),
                (28, TraceEvent.TRACE_RELEASE),
                (28, TraceEvent.TRACE_DONE),
                #
                (28, TraceEvent.TRACE_SWITCH_OUT),
                (32, TraceEvent.TRACE_SWITCH_IN),
                (36, TraceEvent.TRACE_DEADLINE_MISS),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (8, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (8, TraceEvent.TRACE_SWITCH_OUT),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (16, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (16, TraceEvent.TRACE_SWITCH_OUT),
                (20, TraceEvent.TRACE_SWITCH_IN),
                (24, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (24, TraceEvent.TRACE_SWITCH_OUT),
                (28, TraceEvent.TRACE_SWITCH_IN),
                (32, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (32, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "CBS18": {
        "name": "100% server bandwidth",
        "expected_admission_failure": None,
        "expected_events": {
            "Periodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (0, TraceEvent.TRACE_SWITCH_IN),
                (4, TraceEvent.TRACE_DONE),
                #
                (4, TraceEvent.TRACE_SWITCH_OUT),
                (7, TraceEvent.TRACE_RELEASE),
                (12, TraceEvent.TRACE_SWITCH_IN),
                (15, TraceEvent.TRACE_DEADLINE_MISS),
            ],
            "Aperiodic 01": [
                (0, TraceEvent.TRACE_RELEASE),
                (4, TraceEvent.TRACE_SWITCH_IN),
                (12, TraceEvent.TRACE_BUDGET_RUN_OUT),
                (12, TraceEvent.TRACE_SWITCH_OUT),
            ],
        },
    },
    "SMP1": {
        "name": "Simple EDF test on both cores",
        "expected_admission_failure": None,
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (1, TraceEvent.TRACE_SWITCH_IN, 0),
                (2, TraceEvent.TRACE_SWITCH_OUT, 0),
                (3, TraceEvent.TRACE_SWITCH_IN, 0),
                (4, TraceEvent.TRACE_DONE, 0),
                (4, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (6, TraceEvent.TRACE_RELEASE, 0),
                (7, TraceEvent.TRACE_SWITCH_IN, 0),
                (8, TraceEvent.TRACE_SWITCH_OUT, 0),
                (9, TraceEvent.TRACE_SWITCH_IN, 0),
                (10, TraceEvent.TRACE_DONE, 0),
                (10, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (1, TraceEvent.TRACE_DONE, 0),
                (1, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (2, TraceEvent.TRACE_RELEASE, 0),
                (2, TraceEvent.TRACE_SWITCH_IN, 0),
                (3, TraceEvent.TRACE_DONE, 0),
                (3, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (4, TraceEvent.TRACE_RELEASE, 0),
                (4, TraceEvent.TRACE_SWITCH_IN, 0),
                (5, TraceEvent.TRACE_DONE, 0),
                (5, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (6, TraceEvent.TRACE_RELEASE, 0),
                (6, TraceEvent.TRACE_SWITCH_IN, 0),
                (7, TraceEvent.TRACE_DONE, 0),
                (7, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (8, TraceEvent.TRACE_RELEASE, 0),
                (8, TraceEvent.TRACE_SWITCH_IN, 0),
                (9, TraceEvent.TRACE_DONE, 0),
                (9, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (10, TraceEvent.TRACE_RELEASE, 0),
                (10, TraceEvent.TRACE_SWITCH_IN, 0),
                # (11, TraceEvent.TRACE_DONE), # Doesn't happen since test ends first
                (11, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            2: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (1, TraceEvent.TRACE_SWITCH_IN, 1),
                (2, TraceEvent.TRACE_SWITCH_OUT, 1),
                (3, TraceEvent.TRACE_SWITCH_IN, 1),
                (4, TraceEvent.TRACE_DONE, 1),
                (4, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (6, TraceEvent.TRACE_RELEASE, 1),
                (7, TraceEvent.TRACE_SWITCH_IN, 1),
                (8, TraceEvent.TRACE_SWITCH_OUT, 1),
                (9, TraceEvent.TRACE_SWITCH_IN, 1),
                (10, TraceEvent.TRACE_DONE, 1),
                (10, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
            3: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (1, TraceEvent.TRACE_DONE, 1),
                (1, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (2, TraceEvent.TRACE_RELEASE, 1),
                (2, TraceEvent.TRACE_SWITCH_IN, 1),
                (3, TraceEvent.TRACE_DONE, 1),
                (3, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (4, TraceEvent.TRACE_RELEASE, 1),
                (4, TraceEvent.TRACE_SWITCH_IN, 1),
                (5, TraceEvent.TRACE_DONE, 1),
                (5, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (6, TraceEvent.TRACE_RELEASE, 1),
                (6, TraceEvent.TRACE_SWITCH_IN, 1),
                (7, TraceEvent.TRACE_DONE, 1),
                (7, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (8, TraceEvent.TRACE_RELEASE, 1),
                (8, TraceEvent.TRACE_SWITCH_IN, 1),
                (9, TraceEvent.TRACE_DONE, 1),
                (9, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (10, TraceEvent.TRACE_RELEASE, 1),
                (10, TraceEvent.TRACE_SWITCH_IN, 1),
                # (11, TraceEvent.TRACE_DONE), # Doesn't happen since test ends first
                (11, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
        },
    },
    "SMP2": {
        "name": "Mark's Proposed EDF Smoke Test",
        "expected_admission_failure": None,
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (2, TraceEvent.TRACE_DONE, 0),
                (2, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (6, TraceEvent.TRACE_RELEASE, 0),
                (7, TraceEvent.TRACE_SWITCH_IN, 0),
                (9, TraceEvent.TRACE_DONE, 0),
                (9, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (12, TraceEvent.TRACE_RELEASE, 0),
                (14, TraceEvent.TRACE_SWITCH_IN, 0),
                (16, TraceEvent.TRACE_DONE, 0),
                (16, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (18, TraceEvent.TRACE_RELEASE, 0),
                (18, TraceEvent.TRACE_SWITCH_IN, 0),
                (20, TraceEvent.TRACE_DONE, 0),
                (20, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (2, TraceEvent.TRACE_SWITCH_IN, 0),
                (4, TraceEvent.TRACE_DONE, 0),
                (4, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (8, TraceEvent.TRACE_RELEASE, 0),
                (9, TraceEvent.TRACE_SWITCH_IN, 0),
                (11, TraceEvent.TRACE_DONE, 0),
                (11, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (16, TraceEvent.TRACE_RELEASE, 0),
                (16, TraceEvent.TRACE_SWITCH_IN, 0),
                (18, TraceEvent.TRACE_DONE, 0),
                (18, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            2: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (4, TraceEvent.TRACE_SWITCH_IN, 0),
                (7, TraceEvent.TRACE_DONE, 0),
                (7, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (9, TraceEvent.TRACE_RELEASE, 0),
                (11, TraceEvent.TRACE_SWITCH_IN, 0),
                (14, TraceEvent.TRACE_DONE, 0),
                (14, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (18, TraceEvent.TRACE_RELEASE, 0),
                (20, TraceEvent.TRACE_SWITCH_IN, 0),
                # (23, TraceEvent.TRACE_DONE), # Doesn't happen since test ends first
                (23, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            3: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (2, TraceEvent.TRACE_DONE, 1),
                (2, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (6, TraceEvent.TRACE_RELEASE, 1),
                (7, TraceEvent.TRACE_SWITCH_IN, 1),
                (9, TraceEvent.TRACE_DONE, 1),
                (9, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (12, TraceEvent.TRACE_RELEASE, 1),
                (14, TraceEvent.TRACE_SWITCH_IN, 1),
                (16, TraceEvent.TRACE_DONE, 1),
                (16, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (18, TraceEvent.TRACE_RELEASE, 1),
                (18, TraceEvent.TRACE_SWITCH_IN, 1),
                (20, TraceEvent.TRACE_DONE, 1),
                (20, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
            4: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (2, TraceEvent.TRACE_SWITCH_IN, 1),
                (4, TraceEvent.TRACE_DONE, 1),
                (4, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (8, TraceEvent.TRACE_RELEASE, 1),
                (9, TraceEvent.TRACE_SWITCH_IN, 1),
                (11, TraceEvent.TRACE_DONE, 1),
                (11, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (16, TraceEvent.TRACE_RELEASE, 1),
                (16, TraceEvent.TRACE_SWITCH_IN, 1),
                (18, TraceEvent.TRACE_DONE, 1),
                (18, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
            5: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (4, TraceEvent.TRACE_SWITCH_IN, 1),
                (7, TraceEvent.TRACE_DONE, 1),
                (7, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (9, TraceEvent.TRACE_RELEASE, 1),
                (11, TraceEvent.TRACE_SWITCH_IN, 1),
                (14, TraceEvent.TRACE_DONE, 1),
                (14, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (18, TraceEvent.TRACE_RELEASE, 1),
                (20, TraceEvent.TRACE_SWITCH_IN, 1),
                # (23, TraceEvent.TRACE_DONE), # Doesn't happen since test ends first
                (23, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
        },
    },
    "SMP3": {
        "name": "100 Tasks NON-ADMISSIBLE",
        "expected_admission_failure": "Periodic 34 C0",
        "ignore_traces": True,
        "expected_events": {},
    },
    "SMP4": {
        "name": "100 Tasks ADMISSIBLE",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SMP5": {
        "name": "Admissible by utilization",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SMP6": {
        "name": "Non-admissible by utilization",
        "expected_admission_failure": "Periodic 10 C0",
        "ignore_traces": True,
        "expected_events": {},
    },
    "SMP7": {
        "name": "Admissible by processor demand",
        "expected_admission_failure": None,
        "ignore_traces": True,
        "expected_events": {},
    },
    "SMP8": {
        "name": "Non-admissible by processor demand",
        "expected_admission_failure": "Periodic 02 C0",
        "ignore_traces": True,
        "expected_events": {},
    },
    "SMP9": {
        "name": "Admissible drop-in",
        "expected_admission_failure": None,
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (160, TraceEvent.TRACE_DONE, 0),
                (160, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (800, TraceEvent.TRACE_RELEASE, 0),
                (800, TraceEvent.TRACE_SWITCH_IN, 0),
                (960, TraceEvent.TRACE_DONE, 0),
                (960, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            2: [
                (800, TraceEvent.TRACE_RELEASE, 0),
                (960, TraceEvent.TRACE_SWITCH_IN, 0),
                # (1360, TraceEvent.TRACE_DONE),  # Doesn't happen since test ends first
                (1360, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (160, TraceEvent.TRACE_DONE, 1),
                (160, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (800, TraceEvent.TRACE_RELEASE, 1),
                (800, TraceEvent.TRACE_SWITCH_IN, 1),
                (960, TraceEvent.TRACE_DONE, 1),
                (960, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
            3: [
                (800, TraceEvent.TRACE_RELEASE, 1),
                (960, TraceEvent.TRACE_SWITCH_IN, 1),
                # (1360, TraceEvent.TRACE_DONE),  # Doesn't happen since test ends first
                (1360, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
        },
    },
    "SMP10": {
        "name": "Inadmissible drop-in",
        "expected_admission_failure": "Periodic 02 C0",
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (20, TraceEvent.TRACE_DONE, 0),
                (20, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (100, TraceEvent.TRACE_RELEASE, 0),
                (100, TraceEvent.TRACE_SWITCH_IN, 0),
                (120, TraceEvent.TRACE_DONE, 0),
                (120, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (200, TraceEvent.TRACE_RELEASE, 0),
                (200, TraceEvent.TRACE_SWITCH_IN, 0),
                (220, TraceEvent.TRACE_DONE, 0),
                (220, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (300, TraceEvent.TRACE_RELEASE, 0),
                (300, TraceEvent.TRACE_SWITCH_IN, 0),
                (320, TraceEvent.TRACE_DONE, 0),
                (320, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (400, TraceEvent.TRACE_RELEASE, 0),
                (400, TraceEvent.TRACE_SWITCH_IN, 0),
                (420, TraceEvent.TRACE_DONE, 0),
                (420, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (500, TraceEvent.TRACE_RELEASE, 0),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (20, TraceEvent.TRACE_DONE, 1),
                (20, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (100, TraceEvent.TRACE_RELEASE, 1),
                (100, TraceEvent.TRACE_SWITCH_IN, 1),
                (120, TraceEvent.TRACE_DONE, 1),
                (120, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (200, TraceEvent.TRACE_RELEASE, 1),
                (200, TraceEvent.TRACE_SWITCH_IN, 1),
                (220, TraceEvent.TRACE_DONE, 1),
                (220, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (300, TraceEvent.TRACE_RELEASE, 1),
                (300, TraceEvent.TRACE_SWITCH_IN, 1),
                (320, TraceEvent.TRACE_DONE, 1),
                (320, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (400, TraceEvent.TRACE_RELEASE, 1),
                (400, TraceEvent.TRACE_SWITCH_IN, 1),
                (420, TraceEvent.TRACE_DONE, 1),
                (420, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (500, TraceEvent.TRACE_RELEASE, 1),
            ],
        },
    },
    "SMP11": {
        "name": "Missed deadline",
        "expected_admission_failure": None,
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (50, TraceEvent.TRACE_DONE, 0),
                (50, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (120, TraceEvent.TRACE_RELEASE, 0),
                (120, TraceEvent.TRACE_SWITCH_IN, 0),
                (170, TraceEvent.TRACE_DONE, 0),
                (170, TraceEvent.TRACE_SWITCH_OUT, 0),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (50, TraceEvent.TRACE_SWITCH_IN, 0),
                (120, TraceEvent.TRACE_SWITCH_OUT, 0),
                (170, TraceEvent.TRACE_SWITCH_IN, 0),
                (201, TraceEvent.TRACE_DEADLINE_MISS, 0),
            ],
            2: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (50, TraceEvent.TRACE_DONE, 1),
                (50, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (120, TraceEvent.TRACE_RELEASE, 1),
                (120, TraceEvent.TRACE_SWITCH_IN, 1),
                (170, TraceEvent.TRACE_DONE, 1),
                (170, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
            3: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (50, TraceEvent.TRACE_SWITCH_IN, 1),
                (120, TraceEvent.TRACE_SWITCH_OUT, 1),
                (170, TraceEvent.TRACE_SWITCH_IN, 1),
            ],
        },
    },
    "SMP12": {
        "name": "Remove-from-core",
        "expected_admission_failure": None,
        "ignore_traces": False,
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (6, TraceEvent.TRACE_DONE, 0),
                (6, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (20, TraceEvent.TRACE_RELEASE, 0),
                (20, TraceEvent.TRACE_REMOVED_FROM_CORE, 0),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (6, TraceEvent.TRACE_DONE, 1),
                (6, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (20, TraceEvent.TRACE_RELEASE, 1),
                (20, TraceEvent.TRACE_SWITCH_IN, 1),
                (26, TraceEvent.TRACE_DONE, 1),
                (26, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
        },
    },
    "SMP13": {
        "name": "Migrate-to-core",
        "expected_admission_failure": None,
        "ignore_traces": False,
        "expected_events": {
            0: [
                (0, TraceEvent.TRACE_RELEASE, 0),
                (0, TraceEvent.TRACE_SWITCH_IN, 0),
                (5, TraceEvent.TRACE_DONE, 0),
                (5, TraceEvent.TRACE_SWITCH_OUT, 0),
                #
                (16, TraceEvent.TRACE_RELEASE, 0),
                (16, TraceEvent.TRACE_SWITCH_IN, 0),
                (20, TraceEvent.TRACE_SWITCH_OUT, 0),
                (20, TraceEvent.TRACE_REMOVED_FROM_CORE, 0),
                (20, TraceEvent.TRACE_MIGRATED_TO_CORE, 1),
                (32, TraceEvent.TRACE_RELEASE, 1),
                (34, TraceEvent.TRACE_SWITCH_IN, 1),
                (39, TraceEvent.TRACE_DONE, 1),
                (39, TraceEvent.TRACE_SWITCH_OUT, 1),
            ],
            1: [
                (0, TraceEvent.TRACE_RELEASE, 1),
                (0, TraceEvent.TRACE_SWITCH_IN, 1),
                (2, TraceEvent.TRACE_DONE, 1),
                (2, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (16, TraceEvent.TRACE_RELEASE, 1),
                (16, TraceEvent.TRACE_SWITCH_IN, 1),
                (18, TraceEvent.TRACE_DONE, 1),
                (18, TraceEvent.TRACE_SWITCH_OUT, 1),
                #
                (32, TraceEvent.TRACE_RELEASE, 1),
                (32, TraceEvent.TRACE_SWITCH_IN, 1),
                (34, TraceEvent.TRACE_DONE, 1),
                (34, TraceEvent.TRACE_SWITCH_OUT, 1),
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
