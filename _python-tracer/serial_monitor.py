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


def plot_rtos_trace(csv_data):
    try:
        df = pd.read_csv(io.StringIO(csv_data))

        def get_task_name(row):
            t_type = row["TASK_TYPE"]
            t_id = row["TASK_ID"]
            base_name = TASK_TYPES.get(t_type, f"Unknown ({t_type})")

            # Append ID only for periodic/aperiodic tasks
            if t_type in [1, 2]:
                return f"{base_name} {t_id}"
            return base_name

        df["TASK_NAME"] = df.apply(get_task_name, axis=1)

        # 1. Calculate Execution Bars
        df_switches = df[
            df["EVENT"].isin([TraceEvent.TRACE_SWITCH_IN, TraceEvent.TRACE_SWITCH_OUT])
        ].copy()
        exec_bars = []
        active_ins = {}

        for _, row in df_switches.iterrows():
            t_name = row["TASK_NAME"]

            if row["EVENT"] == TraceEvent.TRACE_SWITCH_IN:
                active_ins[t_name] = row
            elif row["EVENT"] == TraceEvent.TRACE_SWITCH_OUT:
                if t_name in active_ins:
                    current_in = active_ins[t_name]
                    exec_bars.append(
                        {
                            "TASK_NAME": current_in["TASK_NAME"],
                            "TIMESTAMP": current_in["TIMESTAMP"],
                            "END_TIMESTAMP": row["TIMESTAMP"],
                            "DURATION": row["TIMESTAMP"] - current_in["TIMESTAMP"],
                            "ABS_TIME_START": current_in["ABS_TIME"],
                            "ABS_TIME_END": row["ABS_TIME"],
                            "PRIORITY": current_in["PRIORITY"],
                            "TASK_STATE": TASK_STATES.get(
                                current_in["TASK_STATE"], "Unknown"
                            ),
                            "DEADLINE": current_in["DEADLINE"],
                            "PREEMPT_LVL": current_in["PREEMPT_LVL"],
                            "CEILING": current_in["CEILING"],
                            "RESOURCE": current_in["RESOURCE"],
                        }
                    )
                    del active_ins[t_name]

        df_exec = pd.DataFrame(exec_bars)
        df_events = df.copy()

        # Map Event IDs to visual markers
        event_mapping = {
            TraceEvent.TRACE_RELEASE: ("Release", "star", "green"),
            TraceEvent.TRACE_SWITCH_IN: ("Switch In", "triangle-right", "lightgreen"),
            TraceEvent.TRACE_SWITCH_OUT: ("Switch Out", "triangle-left", "pink"),
            TraceEvent.TRACE_DONE: ("Task Done", "circle-dot", "teal"),
            TraceEvent.TRACE_RESCHEDULED: (
                "Rescheduled",
                "star-open",
                "mediumseagreen",
            ),
            TraceEvent.TRACE_UPDATING_PRIORITIES: (
                "Update Priorities",
                "diamond",
                "purple",
            ),
            TraceEvent.TRACE_DEPRIORITIZED: (
                "Deprioritized",
                "triangle-down-open",
                "gray",
            ),
            TraceEvent.TRACE_PRIORITY_SET: ("Priority Set", "triangle-up-open", "gold"),
            TraceEvent.TRACE_DEADLINE_MISS: ("Deadline Miss", "hexagram", "darkred"),
            TraceEvent.TRACE_SRP_BLOCK: ("SRP Blocked", "x", "orange"),
            TraceEvent.TRACE_SEMAPHORE_TAKE: ("Take Semaphore", "triangle-down", "red"),
            TraceEvent.TRACE_SEMAPHORE_GIVE: ("Give Semaphore", "triangle-up", "blue"),
        }

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
            for task_name in df_exec["TASK_NAME"].unique():
                df_task = df_exec[df_exec["TASK_NAME"] == task_name].copy()

                df_task["US_DURATION"] = (
                    df_task["ABS_TIME_END"] - df_task["ABS_TIME_START"]
                )

                # Apply the dynamic builder function
                df_task["HOVER_TEXT"] = df_task.apply(build_bar_hover, axis=1)

                fig.add_trace(
                    go.Bar(
                        base=df_task["TIMESTAMP"],
                        x=df_task["DURATION"],
                        y=df_task["TASK_NAME"],
                        orientation="h",
                        name=task_name,
                        marker=dict(line=dict(width=1, color="black")),
                        hovertext=df_task["HOVER_TEXT"],
                        hoverinfo="text",
                    )
                )

        # Add event markers
        for event_id, (evt_name, symbol, color) in event_mapping.items():
            df_evt = df_events[df_events["EVENT"] == event_id]
            if not df_evt.empty:
                # Apply the dynamic builder function, passing in the mapped event name
                hover_texts = df_evt.apply(
                    lambda r: build_marker_hover(r, evt_name), axis=1
                )

                fig.add_trace(
                    go.Scatter(
                        x=df_evt["TIMESTAMP"],
                        y=df_evt["TASK_NAME"],
                        mode="markers",
                        name=evt_name,
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
            title="FreeRTOS EDF+SRP Scheduling Trace",
            xaxis_title="System Ticks",
            yaxis_title="Tasks",
            barmode="overlay",
            hovermode="closest",
            legend_title="Events & Tasks",
        )
        fig.update_yaxes(categoryorder="category descending")

        fig.show()
    except Exception as e:
        print(f"\n[Error] Failed to plot data: {e}")


def main():
    signal.signal(signal.SIGINT, force_quit)
    selected_port = select_serial_port()

    capturing = False
    trace_buffer = []

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
                            plot_rtos_trace(csv_string)
                            trace_buffer = []
                            continue

                        elif capturing:
                            trace_buffer.append(line)

        except serial.SerialException as e:
            time.sleep(1)
        except Exception as e:
            print(f"\n[Error] Unexpected error: {e}")
            time.sleep(1)


if __name__ == "__main__":
    main()
