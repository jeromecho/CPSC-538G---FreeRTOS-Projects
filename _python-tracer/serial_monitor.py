import serial
import serial.tools.list_ports
import pandas as pd
import io
import sys
import time

import plotly.graph_objects as go

# --- CONFIGURATION ---
BAUD_RATE = 115200  # Standard Pico SDK baud rate


def select_serial_port():
    """Scans for USB devices and prompts the user to select one."""
    ports = serial.tools.list_ports.comports()

    if not ports:
        print("\n[Error] No serial ports found! Please plug in your device and try again.")
        sys.exit(1)

    # 1. Look for a default "usbmodem" device
    default_port = None
    for port in ports:
        # Check both the device path (macOS/Linux) and description (Windows)
        if "usbmodem" in port.device.lower() or "usbmodem" in port.description.lower():
            default_port = port.device
            break

    # 2. If a default is found, offer a quick Y/n confirmation
    if default_port:
        while True:
            choice = input(f"\nFound likely USB device: {default_port}\nUse this device? [Y/n]: ").strip().lower()
            if choice in ["", "y", "yes"]:
                return default_port
            elif choice in ["n", "no"]:
                break  # Break out to show the full list
            else:
                print("Please enter 'y' or 'n'.")

    # 3. Present a list of all available devices if no default was used
    print("\nAvailable serial ports:")
    for i, port in enumerate(ports):
        print(f"[{i}] {port.device} - {port.description}")

    while True:
        try:
            selection = input(f"\nEnter the number of the port to use (0-{len(ports) - 1}): ").strip()
            index = int(selection)
            if 0 <= index < len(ports):
                return ports[index].device
            else:
                print("Invalid selection. Please choose a number from the list.")
        except ValueError:
            print("Please enter a valid number.")


def plot_rtos_trace(csv_data):
    """Parses the expanded CSV string and generates an annotated Gantt chart."""
    try:
        # 1. Load the data
        df = pd.read_csv(io.StringIO(csv_data))

        # 2. Helper to dynamically generate task names based on type and ID
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

        # 3. Split data into Execution Time (Bars) and Point Events (Markers)
        # Event 0 is TRACE_EVT_SWITCH_IN
        df_exec = df[df["EVENT"] == 0].copy()
        df_events = df[df["EVENT"] != 0].copy()

        # Calculate duration for the execution bars
        df_exec["END_TIMESTAMP"] = df_exec["TIMESTAMP"].shift(-1)
        df_exec = df_exec.dropna(subset=["END_TIMESTAMP"]).copy()
        df_exec["DURATION"] = df_exec["END_TIMESTAMP"] - df_exec["TIMESTAMP"]

        # 4. Map Event IDs to symbols, colors, and readable names
        event_mapping = {
            1: ("Release", "star", "green"),
            2: ("Take Semaphore", "triangle-down", "red"),
            3: ("Give Semaphore", "triangle-up", "blue"),
            4: ("SRP Blocked", "x", "orange"),
            5: ("Deadline Missed", "hexagram", "darkred"),
        }

        # 5. Build the interactive figure
        fig = go.Figure()

        # Add execution bars (Gantt chart base)
        for task_name in df_exec["TASK_NAME"].unique():
            df_task = df_exec[df_exec["TASK_NAME"] == task_name].copy()

            # Create a rich tooltip for the bars
            df_task["HOVER_TEXT"] = (
                "<b>"
                + df_task["TASK_NAME"]
                + "</b><br>"
                + "Start Tick: "
                + df_task["TIMESTAMP"].astype(str)
                + "<br>"
                + "Duration: "
                + df_task["DURATION"].astype(str)
                + " ticks<br>"
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
                    hoverinfo="text",  # Force Plotly to use our custom HTML text
                )
            )

        # Add event markers on top
        for event_id, (evt_name, symbol, color) in event_mapping.items():
            df_evt = df_events[df_events["EVENT"] == event_id]
            if not df_evt.empty:
                # Build rich HTML hover text showing all the expanded struct data
                hover_texts = df_evt.apply(
                    lambda r: (
                        f"<b>{evt_name}</b><br>"
                        f"Time: {r['TIMESTAMP']}<br>"
                        f"Resource ID: {r['RESOURCE']}<br>"
                        f"System Ceiling: {r['CEILING']}<br>"
                        f"Preempt Level: {r['PREEMPT_LVL']}<br>"
                        f"Deadline: {r['DEADLINE']}"
                    ),
                    axis=1,
                )

                fig.add_trace(
                    go.Scatter(
                        x=df_evt["TIMESTAMP"],
                        y=df_evt["TASK_NAME"],
                        mode="markers",
                        name=evt_name,
                        marker=dict(symbol=symbol, size=14, color=color, line=dict(width=1, color="black")),
                        hovertext=hover_texts,
                        hoverinfo="text",
                    )
                )

        # 6. Layout formatting
        fig.update_layout(
            title="FreeRTOS EDF+SRP Scheduling Trace",
            xaxis_title="System Ticks",
            yaxis_title="Tasks",
            barmode="overlay",  # Crucial: allows bars to sit on the same line
            hovermode="closest",
            legend_title="Events & Tasks",
        )
        # Ensure Y-axis is sorted logically
        fig.update_yaxes(categoryorder="category descending")

        fig.show()
    except Exception as e:
        print(f"\n[Error] Failed to plot data: {e}")


def main():
    selected_port = select_serial_port()

    capturing = False
    trace_buffer = []

    print(f"\nLooking for {selected_port} at {BAUD_RATE} baud... (Press Ctrl+C to exit)")

    while True:
        try:
            # Attempt to open the serial port
            with serial.Serial(selected_port, BAUD_RATE, timeout=1) as ser:
                print("\n[Monitor] Connected to Pico!")

                while True:
                    # Read line, decode to string, strip whitespace/newlines
                    line = ser.readline().decode("utf-8", errors="replace").strip()

                    if line:
                        # Echo everything to the console exactly as it comes in
                        print(line)

                        # 1. Start Capturing Condition
                        if line == "TIMESTAMP,EVENT,TASK_TYPE,TASK_ID,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE":
                            capturing = True
                            trace_buffer = [line]
                            print("\n[Monitor] Trace start detected. Capturing data...")
                            continue  # Skip the rest of the loop for this line

                        # 2. Stop Capturing Condition
                        elif line == "--- END OF TRACE ---" and capturing:
                            print(f"\n[Monitor] Trace ended. Captured {len(trace_buffer) - 1} records. Plotting...")
                            capturing = False

                            # Join the captured lines and plot
                            csv_string = "\n".join(trace_buffer)
                            plot_rtos_trace(csv_string)

                            # Reset the buffer for the next run
                            trace_buffer = []
                            continue

                        # 3. Data Collection Condition
                        elif capturing:
                            # If we are currently capturing, just append the line.
                            # We don't need to strictly check for digits/commas anymore
                            # because we explicitly look for "--- END OF TRACE ---" to stop.
                            trace_buffer.append(line)

        except serial.SerialException as e:
            # If Errno 6 happens (device disconnects), we end up here.
            # We sleep for a second and then the outer while loop tries to reconnect.
            time.sleep(1)
        except KeyboardInterrupt:
            print("\n[Monitor] Exiting gracefully...")
            sys.exit(0)
        except Exception as e:
            print(f"\n[Error] Unexpected error: {e}")
            time.sleep(1)


if __name__ == "__main__":
    main()
