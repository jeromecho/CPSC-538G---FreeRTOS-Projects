import csv
import io
from enum import IntEnum
from pprint import pprint


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
    TRACE_BUDGET_RUN_OUT = 12

    # This method controls how the enum appears in lists/pprint
    def __repr__(self):
        return f"TraceEvent.{self.name}"


TASK_TYPES = {
    0: "Idle Task",
    1: "Periodic",
    2: "Aperiodic",
    3: "System Task",
}


def parse_trace_log(raw_log: str):
    # Mapping of (TASK_TYPE_ID, TASK_ID) -> list of (timestamp, event)
    events_map = {}

    # Filter for specific events requested
    target_events = {
        TraceEvent.TRACE_RELEASE,
        TraceEvent.TRACE_SWITCH_IN,
        TraceEvent.TRACE_SWITCH_OUT,
        TraceEvent.TRACE_DEADLINE_MISS,
    }

    # Locate the CSV data between the markers
    lines = raw_log.strip().split("\n")
    start_idx = 0
    for i, line in enumerate(lines):
        if "TIMESTAMP,EVENT" in line:
            start_idx = i + 1
            break

    # Skip "Monitor" or non-data lines
    data_lines = [l for l in lines[start_idx:] if l and l[0].isdigit()]

    reader = csv.reader(data_lines)

    for row in reader:
        if not row:
            continue

        timestamp = int(row[0])
        event_id = int(row[1])
        task_type_id = int(row[3])
        task_id = int(row[4])

        # We only care about Periodic (1) and Aperiodic (2) tasks for expected_events
        if task_type_id not in [1, 2]:
            continue

        if event_id in target_events:
            task_name = f"{TASK_TYPES[task_type_id]} {(task_id+1):02d}"

            if task_name not in events_map:
                events_map[task_name] = []

            # Append as (timestamp, TraceEvent)
            events_map[task_name].append((timestamp, TraceEvent(event_id)))

    return events_map


# Example Usage:
log_data = """
--- TEST COMPLETE ---
Traces captured: 21
TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE

[Monitor] Trace start detected. Capturing data...
0,0,603379,1,0,1,3,255,4294967295,4294967295,7
0,0,603445,2,0,1,3,255,4294967295,4294967295,8
0,5,603485,3,255,4294967295,5,255,4294967295,4294967295,4294967295
0,7,603519,1,0,2,1,255,4294967295,4294967295,7
0,1,603672,3,255,4294967295,5,255,4294967295,4294967295,4294967295
0,2,603798,3,255,4294967295,5,255,4294967295,4294967295,4294967295
0,1,603816,1,0,2,0,255,4294967295,4294967295,7
4,5,607752,3,255,4294967295,5,255,4294967295,4294967295,4294967295
4,6,607767,1,0,1,0,255,4294967295,4294967295,7
4,7,607778,2,0,2,1,255,4294967295,4294967295,8
4,3,607785,1,0,1,0,255,4294967295,4294967295,7
4,2,607790,1,0,1,0,255,4294967295,4294967295,7
4,1,607796,2,0,2,0,255,4294967295,4294967295,8
7,0,610718,1,0,1,3,255,4294967295,4294967295,14
12,12,615713,2,0,2,0,255,4294967295,4294967295,16
12,5,615721,3,255,4294967295,5,255,4294967295,4294967295,4294967295
12,6,615727,2,0,1,0,255,4294967295,4294967295,16
12,7,615732,1,0,2,1,255,4294967295,4294967295,14
12,2,615739,2,0,1,0,255,4294967295,4294967295,16
12,1,615742,1,0,2,0,255,4294967295,4294967295,14
15,8,618714,1,0,2,0,255,4294967295,4294967295,14
--- END OF TRACE ---
"""

parsed_results = parse_trace_log(log_data)

pprint(parsed_results, sort_dicts=False)
