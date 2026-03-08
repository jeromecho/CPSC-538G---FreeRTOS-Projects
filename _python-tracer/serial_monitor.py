import serial
import serial.tools.list_ports
import pandas as pd
import io
import sys
import os
import time
import signal

import plotly.graph_objects as go

# --- CONFIGURATION ---
BAUD_RATE = 115200  # Standard Pico SDK baud rate


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
    """Parses the CSV string and generates an annotated Gantt chart."""
    try:
        df = pd.read_csv(io.StringIO(csv_data))

        def get_task_name(row):
            t_type = row["TASK_TYPE"]
            t_id = row["TASK_ID"]
            if t_type == 0:
                return "Idle Task"
            elif t_type == 1:
                return f"Periodic {t_id}"
            elif t_type == 2:
                return f"Aperiodic {t_id}"
            elif t_type == 3:
                return "System Task"
            return f"Unknown ({t_type}-{t_id})"

        df["TASK_NAME"] = df.apply(get_task_name, axis=1)

        df_switches = df[df["EVENT"].isin([0, 6])].copy()
        exec_bars = []
        active_ins = {}

        state_mapping = {
            0: "eRunning",
            1: "eReady",
            2: "eBlocked",
            3: "eSuspended",
            4: "eDeleted",
            5: "eInvalid",
        }

        for _, row in df_switches.iterrows():
            t_name = row["TASK_NAME"]

            if row["EVENT"] == 0:  # Switch IN
                active_ins[t_name] = row
            elif row["EVENT"] == 6:  # Switch OUT
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
                            "TASK_STATE": state_mapping.get(
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

        # Map Event IDs
        event_mapping = {
            0: ("Switch In", "triangle-right", "lightgreen"),
            1: ("Release", "star", "green"),
            2: ("Take Semaphore", "triangle-down", "red"),
            3: ("Give Semaphore", "triangle-up", "blue"),
            4: ("SRP Blocked", "x", "orange"),
            5: ("Deadline Missed", "hexagram", "darkred"),
            6: ("Switch Out", "triangle-left", "pink"),
            7: ("Update Priorities", "diamond", "purple"),
            8: ("Deprioritized", "triangle-down-open", "gray"),
            9: ("Priority Set", "triangle-up-open", "gold"),
            10: ("Task Done", "circle-dot", "teal"),
            11: ("Rescheduled", "star-open", "mediumseagreen"),
        }

        fig = go.Figure()

        # Add execution bars
        if not df_exec.empty:
            for task_name in df_exec["TASK_NAME"].unique():
                df_task = df_exec[df_exec["TASK_NAME"] == task_name].copy()

                df_task["US_DURATION"] = (
                    df_task["ABS_TIME_END"] - df_task["ABS_TIME_START"]
                )
                df_task["HOVER_TEXT"] = (
                    "<b>"
                    + df_task["TASK_NAME"]
                    + " (Executing)</b><br>"
                    + "Start Tick: "
                    + df_task["TIMESTAMP"].astype(str)
                    + "<br>"
                    + "Duration: "
                    + df_task["DURATION"].astype(str)
                    + " ticks ("
                    + df_task["US_DURATION"].astype(str)
                    + " µs)<br>"
                    + "Abs Start: "
                    + df_task["ABS_TIME_START"].astype(str)
                    + " µs<br>"
                    + "Deadline: "
                    + df_task["DEADLINE"].astype(str)
                    + "<br>"
                    + "RTOS Priority: "
                    + df_task["PRIORITY"].astype(str)
                    + "<br>"
                    + "Task State: "
                    + df_task["TASK_STATE"].astype(str)
                    + "<br>"
                    + "Preempt Lvl: "
                    + df_task["PREEMPT_LVL"].astype(str)
                    + "<br>"
                    + "Sys Ceiling: "
                    + df_task["CEILING"].astype(str)
                )

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
                hover_texts = df_evt.apply(
                    lambda r: (
                        f"<b>{evt_name}</b><br>"
                        f"Task: {r['TASK_NAME']}<br>"
                        f"Tick: {r['TIMESTAMP']}<br>"
                        f"Abs Time: {r['ABS_TIME']} µs<br>"
                        f"Deadline: {r['DEADLINE']}<br>"
                        f"RTOS Priority: {r['PRIORITY']}<br>"
                        f"Task State: {state_mapping.get(r['TASK_STATE'], 'Unknown')}<br>"
                        f"Preempt Level: {r['PREEMPT_LVL']}<br>"
                        f"System Ceiling: {r['CEILING']}<br>"
                        f"Resource ID: {r['RESOURCE']}"
                    ),
                    axis=1,
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
                            capturing = True
                            trace_buffer = [line]
                            print("\n[Monitor] Trace start detected. Capturing data...")
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
