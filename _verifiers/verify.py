import csv
import io
from pprint import pprint

import collections

"""
If the `DONE` events appears for task_id in the events (for this tick), return true
"""
def is_task_done(task_id, events): 
    for parts in events:
        # Index 1: Event Type, Index 6: Task ID
        if parts[1] == "3" and parts[6] == task_id:
            return True
    return False

def verify_gedf_trace(log_text, exec_times, num_cores=2):
    IDLE_DEADLINE = 4294967295
    lines = [l for l in log_text.strip().split("\n") if "," in l and not l.startswith("TIMESTAMP")]
    
    ticks_data = collections.defaultdict(list)
    for entry in lines:
        parts = entry.split(",")
        try:
            ticks_data[int(parts[0])].append(parts)
        except (ValueError, IndexError):
            continue

    ready_queue = {}      # task_id -> deadline
    running_on_core = {}  # core_id -> [task_id, deadline]
    
    # Track execution progress: task_id -> ticks_run_since_release
    execution_progress = collections.defaultdict(int)
    
    violations = []
    last_tick = None

    for t in sorted(ticks_data.keys()):
        # --- PHASE 0: Accumulate Execution Time ---
        if last_tick is not None:
            elapsed = t - last_tick
            for core_id, (tid, _) in running_on_core.items():
                execution_progress[tid] += elapsed
        
        events = ticks_data[t]

        # --- PHASE 1: Update State Machine ---
        for parts in events:
            event_type, core, task_id, deadline = parts[1], parts[3], parts[6], int(parts[13])

            if deadline == IDLE_DEADLINE:
                if event_type == "2" and core in running_on_core and running_on_core[core][0] == task_id:
                    del running_on_core[core]
                continue

            if event_type == "0":  # Release/Arrival
                ready_queue[task_id] = deadline
                # Reset execution counter for the new job instance
                execution_progress[task_id] = 0
            
            elif event_type == "1":  # Run/Start
                running_on_core[core] = [task_id, deadline]
                ready_queue.pop(task_id, None)
            
            elif event_type == "2":  # Stop/Preempt
                if core in running_on_core and running_on_core[core][0] == task_id:
                    del running_on_core[core]
                # It returns to ready queue if it wasn't a completion
                # (Event 4 usually follows immediately in the same tick if finishing)
                if is_task_done(task_id, events):
                    ready_queue.pop(task_id, None)
                else:
                    ready_queue[task_id] = deadline
            
            elif event_type == "4":  # Finish/Complete
                actual_run_time = execution_progress[task_id]
                required_run_time = exec_times.get(task_id, 0)

                # ERROR CHECK: Did it run long enough?
                if actual_run_time != required_run_time:
                    violations.append(
                        f"Tick {t}: Execution Error. Task {task_id} marked DONE "
                        f"after {actual_run_time} ticks, but requires {required_run_time}."
                    )
                
                # Cleanup state
                if core in running_on_core and running_on_core[core][0] == task_id:
                    del running_on_core[core]
                ready_queue.pop(task_id, None)
                execution_progress[task_id] = 0 # Reset for safety

        """
        if t == 40:
            # 1. Filter events where Task ID (index 6) is '2'
            filtered = [entry for entry in events if entry[6] == '2']
            # 2. Print a header for clarity
            print(f"--- Events for Task 2 at Tick {t} ---")
            # 3. Use pprint to make the list of lists readable
            pprint(filtered)
        """

        # --- PHASE 2: Formal G-EDF Verification ---
        
        # A. Work Conserving Check
        if ready_queue and len(running_on_core) < num_cores:
            violations.append(
                f"Tick {t}: Work Conserving Violation. Cores: {len(running_on_core)}/{num_cores}. "
                f"Waiting: {list(ready_queue.keys())}"
            )

        # B. Priority (EDF) Check
        if ready_queue and running_on_core:
            min_ready_deadline = min(ready_queue.values())
            for core_id, (run_tid, run_deadline) in running_on_core.items():
                if run_deadline > min_ready_deadline:
                    violations.append(
                        f"Tick {t}: Priority Violation. Core {core_id} running Task {run_tid} (D={run_deadline}), "
                        f"but Task with D={min_ready_deadline} is waiting."
                    )
        
        last_tick = t

    return violations

# To run:
# results = verify_gedf_trace(trace_input)
# print("\n".join(results) if results else "Trace Verified: 100% G-EDF Compliant.")
