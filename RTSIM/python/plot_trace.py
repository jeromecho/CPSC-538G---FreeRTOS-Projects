import matplotlib
matplotlib.use('macosx')  # Use the native Mac window manager
import matplotlib.pyplot as plt

import re
import argparse
import sys
from collections import defaultdict
import mplcursors  # <--- New Import

def get_display_name(task_name):
    """Helper to maintain consistent row grouping."""
    cbs_match = re.match(r"(CBS\d+)_a\d+", task_name, re.IGNORECASE)
    if cbs_match:
        return f"{cbs_match.group(1)} (Aperiodic Pool)"
    return task_name

def parse_and_plot(file_path, end_time, save_plot=False):
    tasks_intervals = defaultdict(list)
    active_starts = {}
    deadline_misses = []  # Store (time, task_name, display_name)
    
    # Updated pattern to include "missed"
    pattern = re.compile(r"\[Time:(\d+)\]\s+([\w.]+)\s+(arrived|scheduled|descheduled|ended|missed)")

    try:
        with open(file_path, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if not match: continue
                
                time = int(match.group(1))
                task_name = match.group(2)
                event = match.group(3)
                display_name = get_display_name(task_name)

                # --- NEW: DEADLINE MISS LOGIC ---
                if event == "missed":
                    print(f"!!! Deadline Miss Detected at {time} for {task_name}. Stopping parse.")
                    deadline_misses.append((time, task_name, display_name))
                    
                    # Close out any currently running task at the time of the miss
                    if task_name in active_starts:
                        start_time = active_starts.pop(task_name)
                        tasks_intervals[display_name].append((start_time, time - start_time, task_name))
                    
                    break # STOP adding additional bars immediately
                # --------------------------------

                if event == "scheduled":
                    active_starts[task_name] = time
                elif event in ["descheduled", "ended"]:
                    if task_name in active_starts:
                        start_time = active_starts.pop(task_name)
                        duration = time - start_time
                        if duration > 0:
                            tasks_intervals[display_name].append((start_time, duration, task_name))

        # Only extend tasks to end_time if no miss occurred, otherwise limit to miss time
        final_cutoff = deadline_misses[0][0] if deadline_misses else end_time
        for t_name, start_time in active_starts.items():
            duration = final_cutoff - start_time
            if duration > 0:
                display_name = get_display_name(t_name)
                tasks_intervals[display_name].append((start_time, duration, t_name))

    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found."); sys.exit(1)

    sorted_task_names = sorted(tasks_intervals.keys(), 
                               key=lambda x: (0 if 'cbs' in x.lower() else 1, 
                                              0 if x.lower().startswith('p') else 1, x))

    fig, ax = plt.subplots(figsize=(14, 7))
    rects = []
    
    # Create mapping for Y-axis placement
    task_to_y = {name: i*10 + 5 for i, name in enumerate(reversed(sorted_task_names))}

    for name in sorted_task_names:
        y_pos = task_to_y[name]
        intervals = tasks_intervals[name]
        for start, dur, task_id in intervals:
            rect = ax.barh(y_pos, dur, left=start, height=6, 
                          color='tab:blue', edgecolor='black', alpha=0.8)
            rect[0].task_id = task_id
            rect[0].duration = dur
            rects.append(rect[0])

    # --- NEW: PLOT DEADLINE MISS MARKERS ---
    for m_time, m_task, m_display in deadline_misses:
        if m_display in task_to_y:
            ax.plot(m_time, task_to_y[m_display], marker='h', color='red', 
                    markersize=15, markeredgecolor='black', label="Deadline Miss")
            # Optional: Add text label
            ax.text(m_time, task_to_y[m_display] + 4, "MISS", color='red', 
                    fontweight='bold', ha='center')

    ax.set_xlabel('Time (Ticks)', fontweight='bold')
    ax.set_yticks(list(task_to_y.values()))
    ax.set_yticklabels(task_to_y.keys(), fontweight='bold')
    ax.set_title(f'RTSim Trace: {file_path}', pad=20)
    
    # Set X-limit to show just past the miss or the specified end_time
    ax.set_xlim(0, final_cutoff + 2)
    ax.grid(True, axis='x', linestyle='--', alpha=0.5)

    cursor = mplcursors.cursor(rects, hover=True)
    
    @cursor.connect("add")
    def on_add(sel):
        sel.annotation.set_text(f"Task: {sel.artist.task_id}\nDuration: {sel.artist.duration}")
        sel.annotation.get_bbox_patch().set(fc="white", alpha=0.9, boxstyle="round,pad=0.5")

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot RTSim trace files.")
    parser.add_argument("file", help="Path to the trace.txt file")
    parser.add_argument("--end_time", help="End Time")
    parser.add_argument("--save", action="store_true", help="Save the plot as a PNG file")
    
    args = parser.parse_args()
    parse_and_plot(args.file, int(args.end_time), args.save)