import serial
import serial.tools.list_ports
import pandas as pd
import io
import sys
import os
import time
import signal
from enum import IntEnum

import plotly.graph_objects as go

# --- CONFIGURATION ---
BAUD_RATE = 115200
EXPECTED_HEADERS = "TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE"

# C-Header Maximums for N/A masking
UINT32_MAX = 4294967295
UINT8_MAX = 255


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

TASK_STATES = {
    0: "eRunning",
    1: "eReady",
    2: "eBlocked",
    3: "eSuspended",
    4: "eDeleted",
    5: "eInvalid",
}

# --- GLOBAL EVENT CONFIGURATION ---
# Format: EventID: ("Legend Name", "Symbol", "Color", visible_by_default, y_offset)
# y_offset shifts markers vertically (0.0 is center, -0.4 is top of the bar, +0.4 is the bottom)
# fmt: off
EVENT_CONFIG = {
    # Group: Context Switching (Centered)
    TraceEvent.TRACE_SWITCH_IN: ("Switch In", "triangle-right", "lightgreen", True, -0.4),
    TraceEvent.TRACE_SWITCH_OUT: ("Switch Out", "triangle-left", "pink", True, -0.4),
    
    # Group: Task Lifecycles (Shifted Up)
    TraceEvent.TRACE_RELEASE: ("Release", "star", "green", True, 0),
    TraceEvent.TRACE_RESCHEDULED: ("Rescheduled", "star-open", "mediumseagreen", True, 0),
    TraceEvent.TRACE_DONE: ("Task Done", "circle-dot", "teal", True, 0.2),
    TraceEvent.TRACE_DEADLINE_MISS: ("Deadline Miss", "hexagram", "darkred", True, 0.0),
    
    # Absolute Deadline (Centered, uses large marker size in code)
    "ABSOLUTE_DEADLINE": ("Task Deadline", "arrow-bar-down", "black", True, 0.4),

    # Group: Priority Changes (Shifted Down)
    TraceEvent.TRACE_UPDATING_PRIORITIES: ("Update Priorities", "diamond", "purple", True, 0),
    TraceEvent.TRACE_PRIORITY_SET: ("Priority Set", "triangle-up-open", "gold", False, 0.2),
    TraceEvent.TRACE_DEPRIORITIZED: ("Deprioritized", "triangle-down-open", "gray", False, 0.2),

    # Group: Resources (Shifted further to the edges)
    TraceEvent.TRACE_SRP_BLOCK: ("SRP Blocked", "x", "orange", True, 0.0),
    TraceEvent.TRACE_SEMAPHORE_TAKE: ("Take Semaphore", "triangle-down", "red", True, -0.2),
    TraceEvent.TRACE_SEMAPHORE_GIVE: ("Give Semaphore", "triangle-up", "blue", True, -0.2),
}
# fmt: on


def force_quit(signum, frame):
    print("\n[Monitor] Force quitting instantly...")
    os._exit(0)


def select_serial_port():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print(
            "\n[Error] No serial ports found! Please plug in your device and try again."
        )
        sys.exit(1)

    default_port = None
    for port in ports:
        if "usbmodem" in port.device.lower() or "usbmodem" in port.description.lower():
            default_port = port.device
            break

    if default_port:
        while True:
            choice = (
                input(
                    f"\nFound likely USB device: {default_port}\nUse this device? [Y/n]: "
                )
                .strip()
                .lower()
            )
            if choice in ["", "y", "yes"]:
                return default_port
            elif choice in ["n", "no"]:
                break
            else:
                print("Please enter 'y' or 'n'.")

    print("\nAvailable serial ports:")
    for i, port in enumerate(ports):
        print(f"[{i}] {port.device} - {port.description}")

    while True:
        try:
            selection = input(
                f"\nEnter the number of the port to use (0-{len(ports) - 1}): "
            ).strip()
            index = int(selection)
            if 0 <= index < len(ports):
                return ports[index].device
            else:
                print("Invalid selection. Please choose a number from the list.")
        except ValueError:
            print("Please enter a valid number.")


def plot_rtos_trace(csv_data, plot_title):
    try:
        df = pd.read_csv(io.StringIO(csv_data))

        def get_task_name(row):
            t_type = row["TASK_TYPE"]
            t_id = row["TASK_ID"] + 1
            base_name = TASK_TYPES.get(t_type, f"Unknown ({t_type})")

            # Append ID only for periodic/aperiodic tasks
            if t_type in [1, 2]:
                return f"{base_name} {t_id:03}"
            return base_name

        df["TASK_NAME"] = df.apply(get_task_name, axis=1)

        # Sort tasks so they appear in a consistent logical order
        unique_tasks = sorted(df["TASK_NAME"].unique())
        task_y_map = {task: i for i, task in enumerate(unique_tasks)}

        # 1. Calculate Execution Bars
        # We rely on the original file order so SWITCH_OUT correctly precedes SWITCH_IN on identical ticks
        df_sorted = df.copy()
        exec_bars = []
        active_task = None
        current_segment_start = None
        current_in_row = None

        # Track the active resource per task (None means normal execution)
        task_resource = {task: None for task in unique_tasks}

        for _, row in df_sorted.iterrows():
            t_name = row["TASK_NAME"]
            event = row["EVENT"]
            timestamp = row["TIMESTAMP"]

            if event == TraceEvent.TRACE_SWITCH_IN:
                active_task = t_name
                current_segment_start = timestamp
                current_in_row = row

            elif event == TraceEvent.TRACE_SWITCH_OUT:
                if (
                    active_task == t_name
                    and current_segment_start is not None
                    and current_in_row is not None
                ):
                    exec_bars.append(
                        {
                            "TASK_NAME": t_name,
                            "TIMESTAMP": current_segment_start,
                            "END_TIMESTAMP": timestamp,
                            "DURATION": timestamp - current_segment_start,
                            "ABS_TIME_START": current_in_row["ABS_TIME"],
                            "ABS_TIME_END": row["ABS_TIME"],
                            "PRIORITY": current_in_row["PRIORITY"],
                            "TASK_STATE": TASK_STATES.get(
                                current_in_row["TASK_STATE"], "Unknown"
                            ),
                            "DEADLINE": current_in_row["DEADLINE"],
                            "PREEMPT_LVL": current_in_row["PREEMPT_LVL"],
                            "CEILING": current_in_row["CEILING"],
                            "RESOURCE": task_resource[t_name],  # Tracked resource
                        }
                    )
                    active_task = None
                    current_segment_start = None

            elif event == TraceEvent.TRACE_SEMAPHORE_TAKE:
                task_resource[t_name] = row["RESOURCE"]

                if (
                    active_task == t_name
                    and current_segment_start is not None
                    and current_in_row is not None
                ):
                    # Split bar: Close normal segment
                    if timestamp > current_segment_start:
                        exec_bars.append(
                            {
                                "TASK_NAME": t_name,
                                "TIMESTAMP": current_segment_start,
                                "END_TIMESTAMP": timestamp,
                                "DURATION": timestamp - current_segment_start,
                                "ABS_TIME_START": current_in_row["ABS_TIME"],
                                "ABS_TIME_END": row["ABS_TIME"],
                                "PRIORITY": current_in_row["PRIORITY"],
                                "TASK_STATE": TASK_STATES.get(
                                    current_in_row["TASK_STATE"], "Unknown"
                                ),
                                "DEADLINE": current_in_row["DEADLINE"],
                                "PREEMPT_LVL": current_in_row["PREEMPT_LVL"],
                                "CEILING": current_in_row["CEILING"],
                                "RESOURCE": None,  # Was normal
                            }
                        )
                    current_segment_start = timestamp
                    current_in_row = row

            elif event == TraceEvent.TRACE_SEMAPHORE_GIVE:
                held_res = task_resource[t_name]
                task_resource[t_name] = None

                if (
                    active_task == t_name
                    and current_segment_start is not None
                    and current_in_row is not None
                ):
                    # Split bar: Close resource segment
                    if timestamp > current_segment_start:
                        exec_bars.append(
                            {
                                "TASK_NAME": t_name,
                                "TIMESTAMP": current_segment_start,
                                "END_TIMESTAMP": timestamp,
                                "DURATION": timestamp - current_segment_start,
                                "ABS_TIME_START": current_in_row["ABS_TIME"],
                                "ABS_TIME_END": row["ABS_TIME"],
                                "PRIORITY": current_in_row["PRIORITY"],
                                "TASK_STATE": TASK_STATES.get(
                                    current_in_row["TASK_STATE"], "Unknown"
                                ),
                                "DEADLINE": current_in_row["DEADLINE"],
                                "PREEMPT_LVL": current_in_row["PREEMPT_LVL"],
                                "CEILING": current_in_row["CEILING"],
                                "RESOURCE": held_res,  # Was holding resource
                            }
                        )
                    current_segment_start = timestamp
                    current_in_row = row

        # Handle edge case: Trace ends while a task is still active
        if (
            active_task
            and current_segment_start is not None
            and current_in_row is not None
        ):
            last_timestamp = df["TIMESTAMP"].max()
            last_abs_time = df["ABS_TIME"].max()
            exec_bars.append(
                {
                    "TASK_NAME": active_task,
                    "TIMESTAMP": current_segment_start,
                    "END_TIMESTAMP": last_timestamp,
                    "DURATION": last_timestamp - current_segment_start,
                    "ABS_TIME_START": current_in_row["ABS_TIME"],
                    "ABS_TIME_END": last_abs_time,
                    "PRIORITY": current_in_row["PRIORITY"],
                    "TASK_STATE": TASK_STATES.get(
                        current_in_row["TASK_STATE"], "Unknown"
                    ),
                    "DEADLINE": current_in_row["DEADLINE"],
                    "PREEMPT_LVL": current_in_row["PREEMPT_LVL"],
                    "CEILING": current_in_row["CEILING"],
                    "RESOURCE": task_resource[active_task],
                }
            )

        df_exec = pd.DataFrame(exec_bars)
        df_events = df.copy()

        # --- Dynamic Hover Text Builders ---
        def build_bar_hover(r):
            lines = [
                f"<b>{r['TASK_NAME']} (Executing)</b>",
                f"Start Tick: {r['TIMESTAMP']}",
                f"Duration: {r['DURATION']} ticks ({r['US_DURATION']} µs)",
                f"Abs Start: {r['ABS_TIME_START']} µs",
            ]
            if r["DEADLINE"] != UINT32_MAX:
                lines.append(f"Deadline: {r['DEADLINE']}")
            if r["PRIORITY"] != UINT32_MAX:
                lines.append(f"RTOS Priority: {r['PRIORITY']}")
            lines.append(f"Task State: {r['TASK_STATE']}")
            if r["PREEMPT_LVL"] != UINT32_MAX:
                lines.append(f"Preempt Lvl: {r['PREEMPT_LVL']}")
            if r["CEILING"] != UINT32_MAX:
                lines.append(f"Sys Ceiling: {r['CEILING']}")
            return "<br>".join(lines)

        def build_marker_hover(r, evt_name):
            lines = [
                f"<b>{evt_name}</b>",
                f"Task: {r['TASK_NAME']}",
                f"Tick: {r['TIMESTAMP']}",
                f"Abs Time: {r['ABS_TIME']} µs",
            ]
            if r["DEADLINE"] != UINT32_MAX:
                lines.append(f"Deadline: {r['DEADLINE']}")
            if r["PRIORITY"] != UINT32_MAX:
                lines.append(f"RTOS Priority: {r['PRIORITY']}")
            lines.append(f"Task State: {TASK_STATES.get(r['TASK_STATE'], 'Unknown')}")
            if r["PREEMPT_LVL"] != UINT32_MAX:
                lines.append(f"Preempt Level: {r['PREEMPT_LVL']}")
            if r["CEILING"] != UINT32_MAX:
                lines.append(f"System Ceiling: {r['CEILING']}")
            if r["RESOURCE"] != UINT8_MAX:
                lines.append(f"Resource ID: {r['RESOURCE']}")
            return "<br>".join(lines)

        # -----------------------------------

        fig = go.Figure()

        # Add execution bars
        if not df_exec.empty:
            # Color map for resources (Matches your reference image style)
            res_color_map = [
                "#DE6046",  # Red
                "#656EF2",  # Blue
                "#F2A667",  # Yellow
                "#A167F2",  # Magenta
                "#63D0EF",  # Light blue
            ]

            for task_name in df_exec["TASK_NAME"].unique():
                df_task = df_exec[df_exec["TASK_NAME"] == task_name].copy()

                df_task["US_DURATION"] = (
                    df_task["ABS_TIME_END"] - df_task["ABS_TIME_START"]
                )

                # Assign colors dynamically: base color is green; map resources to specific colors
                def get_bar_color(res_id):
                    if pd.isna(res_id) or res_id is None or res_id == UINT8_MAX:
                        return "#5CC99A"
                    return res_color_map[int(res_id) % len(res_color_map)]

                df_task["BAR_COLOR"] = df_task["RESOURCE"].apply(get_bar_color)

                # Apply the dynamic builder function
                df_task["HOVER_TEXT"] = df_task.apply(build_bar_hover, axis=1)

                fig.add_trace(
                    go.Bar(
                        base=df_task["TIMESTAMP"],
                        x=df_task["DURATION"],
                        y=df_task["TASK_NAME"].map(task_y_map),
                        orientation="h",
                        name=task_name,
                        marker=dict(
                            line=dict(width=1, color="black"),
                            color=df_task["BAR_COLOR"],
                        ),
                        hovertext=df_task["HOVER_TEXT"],
                        hoverinfo="text",
                        showlegend=False,
                    )
                )

        # Add event markers
        for event_id, (
            evt_name,
            symbol,
            color,
            default_visible,
            y_offset,
        ) in EVENT_CONFIG.items():
            # --- Special Case: Absolute Deadlines ---
            if event_id == "ABSOLUTE_DEADLINE":
                df_evt = df_events[df_events["DEADLINE"] != UINT32_MAX].drop_duplicates(
                    subset=["TASK_NAME", "DEADLINE"]
                )
                x_col = "DEADLINE"

                if not df_evt.empty:
                    hover_texts = df_evt.apply(
                        lambda r: (
                            f"<b>{evt_name}</b><br>Task: {r['TASK_NAME']}<br>Tick: {r['DEADLINE']}"
                        ),
                        axis=1,
                    )
            else:
                df_evt = df_events[df_events["EVENT"] == event_id]
                x_col = "TIMESTAMP"

                if not df_evt.empty:
                    hover_texts = df_evt.apply(
                        lambda r: build_marker_hover(r, evt_name), axis=1
                    )

            # Draw the trace if we found data
            if not df_evt.empty:
                visibility = True if default_visible else "legendonly"

                # Apply mapping and the configured offset
                numeric_y = df_evt["TASK_NAME"].map(task_y_map) + y_offset

                fig.add_trace(
                    go.Scatter(
                        x=df_evt[x_col],
                        y=numeric_y,  # Pass the adjusted numeric coordinates
                        mode="markers",
                        name=evt_name,
                        visible=visibility,
                        marker=dict(
                            symbol=symbol,
                            size=12,
                            color=color,
                            line=dict(width=1, color="black"),
                        ),
                        hovertext=hover_texts,
                        hoverinfo="text",
                    )
                )

        fig.update_layout(
            title=plot_title,
            xaxis_title="System Ticks",
            yaxis=dict(
                title="Tasks",
                tickmode="array",
                tickvals=list(task_y_map.values()),
                ticktext=list(task_y_map.keys()),
                autorange="reversed",  # Keeps highest priority/first tasks at the top
            ),
            barmode="overlay",
            hovermode="closest",
            legend_title="Events & Tasks",
        )

        fig.show()
    except Exception as e:
        print(f"\n[Error] Failed to plot data: {e}")


def main():
    signal.signal(signal.SIGINT, force_quit)
    selected_port = select_serial_port()

    capturing = False
    trace_buffer = []

    # Default title just in case it misses the printout
    current_title = "[Name of test not printed from microcontroller]"

    print(
        f"\nLooking for {selected_port} at {BAUD_RATE} baud... (Press Ctrl+C to exit)"
    )

    while True:
        try:
            with serial.Serial(selected_port, BAUD_RATE, timeout=0.1) as ser:
                print("\n[Monitor] Connected to Pico!")

                while True:
                    line = ser.readline().decode("utf-8", errors="replace").strip()

                    if line:
                        print(line)

                        # --- Catch the Test Name ---
                        if "Running EDF Test" in line or "Running SRP Test" in line:
                            current_title = line.replace("Running ", "")
                        # ---------------------------

                        if "TIMESTAMP" in line:
                            if line != EXPECTED_HEADERS:
                                print(
                                    f"\n[Monitor] Warning: Encountered header with unknown format.\nExpected: {EXPECTED_HEADERS}\nGot:      {line}"
                                )
                            else:
                                capturing = True
                                trace_buffer = [line]
                                print(
                                    "\n[Monitor] Trace start detected. Capturing data..."
                                )
                            continue

                        elif line == "--- END OF TRACE ---" and capturing:
                            print(
                                f"\n[Monitor] Trace ended. Captured {len(trace_buffer) - 1} records. Plotting..."
                            )
                            capturing = False
                            csv_string = "\n".join(trace_buffer)

                            # Pass the captured title to the plot!
                            plot_rtos_trace(csv_string, current_title)

                            trace_buffer = []
                            current_title = "FreeRTOS EDF+SRP Scheduling Trace"  # Reset for the next run
                            continue

                        elif capturing:
                            trace_buffer.append(line)

        except serial.SerialException:
            time.sleep(1)
        except Exception as e:
            print(f"\n[Error] Unexpected error: {e}")
            time.sleep(1)


if __name__ == "__main__":
    main()
