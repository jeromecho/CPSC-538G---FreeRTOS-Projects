import io
import os
import signal
import sys
import time
import argparse

import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plotly.colors import qualitative
import serial
import serial.tools.list_ports

from test_data import TASK_TYPES, TraceEvent

BAUD_RATE = 115200
TRACE_RUN_BANNERS = (
    "Running EDF Test",
    "Running SRP Test",
    "Running CBS Test",
    "Running SMP Test",
)
EXPECTED_HEADERS = {
    "TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE",
    "TIMESTAMP,EVENT,ABS_TIME,CORE,CORE_SEQ,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE",
}

DEFAULT_TRACE_TITLE = "FreeRTOS Scheduling Trace"

UINT32_MAX = 4294967295

BACKGROUND_TASK_PREFIXES = ("Idle Task", "System Task")

TASK_COLOR_PALETTE = qualitative.Plotly + qualitative.Dark24 + qualitative.Alphabet

EVENT_STYLE = {
    TraceEvent.TRACE_RELEASE: ("Release", "star", "#2ca02c"),
    TraceEvent.TRACE_SWITCH_IN: ("Switch In", "triangle-right", "#3cb44b"),
    TraceEvent.TRACE_SWITCH_OUT: ("Switch Out", "triangle-left", "#e6194b"),
    TraceEvent.TRACE_DONE: ("Task Done", "circle", "#008080"),
    TraceEvent.TRACE_RESCHEDULED: ("Rescheduled", "diamond", "#3cb371"),
    TraceEvent.TRACE_PREPARING_CONTEXT_SWITCH: ("Prepare Context Switch", "diamond-open", "#911eb4"),
    TraceEvent.TRACE_SUSPENDED: ("Suspended", "triangle-down-open", "#808080"),
    TraceEvent.TRACE_RESUMED: ("Resumed", "triangle-up-open", "#f2c300"),
    TraceEvent.TRACE_DEADLINE_MISS: ("Deadline Miss", "x", "#d62728"),
    TraceEvent.TRACE_SRP_BLOCK: ("SRP Block", "cross", "#ff8c00"),
    TraceEvent.TRACE_ADMISSION_FAILED: ("Admission Failed", "hexagram", "#b22222"),
    TraceEvent.TRACE_SEMAPHORE_TAKE: ("Semaphore Take", "triangle-down", "#dc143c"),
    TraceEvent.TRACE_SEMAPHORE_GIVE: ("Semaphore Give", "triangle-up", "#1f77b4"),
    # CBS Events
    TraceEvent.TRACE_BUDGET_RUN_OUT: ("CBS Budget Run Out", "hourglass", "teal"),
}


def force_quit(signum, frame):
    print("\n[Monitor] Force quitting instantly...")
    os._exit(0)


def select_serial_port():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("\n[Error] No serial ports found! Please plug in your device and try again.")
        sys.exit(1)

    default_port = None
    for port in ports:
        if "usbmodem" in port.device.lower() or "usbmodem" in port.description.lower():
            default_port = port.device
            break

    if default_port:
        while True:
            choice = input(f"\nFound likely USB device: {default_port}\nUse this device? [Y/n]: ").strip().lower()
            if choice in ["", "y", "yes"]:
                return default_port
            if choice in ["n", "no"]:
                break
            print("Please enter 'y' or 'n'.")

    print("\nAvailable serial ports:")
    for i, port in enumerate(ports):
        print(f"[{i}] {port.device} - {port.description}")

    while True:
        try:
            selection = input(f"\nEnter the number of the port to use (0-{len(ports) - 1}): ").strip()
            index = int(selection)
            if 0 <= index < len(ports):
                return ports[index].device
            print("Invalid selection. Please choose a number from the list.")
        except ValueError:
            print("Please enter a valid number.")


def detect_run_title(line):
    for banner in TRACE_RUN_BANNERS:
        if line.startswith(banner):
            return line.replace("Running ", "", 1)
    return None


def get_task_name(row):
    task_type = int(row["TASK_TYPE"])
    task_id = int(row["TASK_ID"])
    core = int(row["CORE"])

    base_name = TASK_TYPES.get(task_type, f"Unknown ({task_type})")
    if task_type in [1, 2]:
        # Keep same logical task ID split by core so task-centric lanes stay independent in SMP.
        return f"{base_name} {task_id + 1:03d} C{core}"
    if task_type in [0, 3]:
        return f"{base_name} C{core}"
    return base_name


def build_execution_segments(df):
    segments = []
    active_by_core = {}

    for _, row in df.iterrows():
        event = row["EVENT"]
        core = int(row["CORE"])
        task_name = row["TASK_NAME"]

        if event == TraceEvent.TRACE_SWITCH_IN:
            active_by_core[core] = row
            continue

        if event == TraceEvent.TRACE_DEADLINE_MISS:
            in_row = active_by_core.get(core)
            if in_row is not None and in_row["TASK_NAME"] == task_name:
                start_tick = int(in_row["TIMESTAMP"])
                end_tick = int(row["TIMESTAMP"])
                if end_tick >= start_tick:
                    segments.append(
                        {
                            "CORE": core,
                            "TASK_NAME": task_name,
                            "START_TICK": start_tick,
                            "END_TICK": end_tick,
                            "DURATION": end_tick - start_tick,
                            "INCLUSIVE_END": False,
                            "ABS_START": int(in_row["ABS_TIME"]),
                            "ABS_END": int(row["ABS_TIME"]),
                            "DEADLINE": int(in_row["DEADLINE"]),
                            "PRIORITY": int(in_row["PRIORITY"]),
                        }
                    )
                    del active_by_core[core]
            continue

        if event != TraceEvent.TRACE_SWITCH_OUT:
            continue

        in_row = active_by_core.get(core)
        if in_row is None:
            continue

        if in_row["TASK_NAME"] != task_name:
            continue

        start_tick = int(in_row["TIMESTAMP"])
        end_tick = int(row["TIMESTAMP"])
        if end_tick < start_tick:
            continue

        segments.append(
            {
                "CORE": core,
                "TASK_NAME": task_name,
                "START_TICK": start_tick,
                "END_TICK": end_tick,
                "DURATION": end_tick - start_tick,
                "ABS_START": int(in_row["ABS_TIME"]),
                "ABS_END": int(row["ABS_TIME"]),
                "DEADLINE": int(in_row["DEADLINE"]),
                "PRIORITY": int(in_row["PRIORITY"]),
            }
        )
        del active_by_core[core]

    return pd.DataFrame(segments)


def is_background_task_name(task_name):
    return task_name.startswith(BACKGROUND_TASK_PREFIXES)


def build_task_color_map(task_names):
    task_colors = {}
    foreground_names = [name for name in task_names if not is_background_task_name(name)]

    for index, task_name in enumerate(foreground_names):
        task_colors[task_name] = TASK_COLOR_PALETTE[index % len(TASK_COLOR_PALETTE)]

    for task_name in task_names:
        if task_name.startswith("Idle Task"):
            task_colors[task_name] = "#b0bec5"
        elif task_name.startswith("System Task"):
            task_colors[task_name] = "#78909c"

    return task_colors


def build_marker_display_x(df, df_exec):
    return df["TIMESTAMP"].astype(float).copy()


def parse_trace_dataframe(csv_data):
    df = pd.read_csv(io.StringIO(csv_data))

    if "CORE" not in df.columns:
        df["CORE"] = 0
    if "CORE_SEQ" not in df.columns:
        df["CORE_SEQ"] = range(len(df))

    df["EVENT"] = df["EVENT"].map(TraceEvent)
    df["TASK_NAME"] = df.apply(get_task_name, axis=1)

    df_sorted = df.sort_values(by=["ABS_TIME", "CORE", "CORE_SEQ"], kind="stable").copy()
    df_exec = build_execution_segments(df_sorted)

    return df, df_exec


def ordered_task_names(df, df_exec):
    task_meta = (
        df.groupby("TASK_NAME", as_index=False)
        .agg(
            CORE_MIN=("CORE", "min"),
            TASK_TYPE_MIN=("TASK_TYPE", "min"),
            TASK_ID_MIN=("TASK_ID", "min"),
            FIRST_TICK=("TIMESTAMP", "min"),
        )
        .set_index("TASK_NAME")
        .to_dict("index")
    )

    task_type_rank = {
        1: 0,  # periodic
        2: 1,  # aperiodic
        3: 2,  # system
        0: 3,  # idle
    }

    def sort_key(name):
        meta = task_meta.get(name, {})
        t_type = int(meta.get("TASK_TYPE_MIN", 99))
        return (
            int(meta.get("CORE_MIN", 99)),
            task_type_rank.get(t_type, 9),
            int(meta.get("TASK_ID_MIN", 9999)),
            int(meta.get("FIRST_TICK", UINT32_MAX)),
            name,
        )

    names = sorted(df["TASK_NAME"].unique(), key=sort_key)
    foreground = [name for name in names if not (name.startswith("Idle Task") or name.startswith("System Task"))]
    background = [name for name in names if (name.startswith("Idle Task") or name.startswith("System Task"))]
    return foreground + background


def _build_marker_offset_map(df):
    events_present = sorted(df["EVENT"].dropna().unique(), key=lambda e: int(e.value))
    if not events_present:
        return {}

    if len(events_present) == 1:
        return {events_present[0]: 0.0}

    top = 0.36
    step = (2 * top) / (len(events_present) - 1)
    return {event: -top + (idx * step) for idx, event in enumerate(events_present)}


def _event_hover(row_data, event_label):
    lines = [
        f"<b>{event_label}</b>",
        f"Task: {row_data['TASK_NAME']}",
        f"Core: {int(row_data['CORE'])}",
        f"Tick: {int(row_data['TIMESTAMP'])}",
        f"Abs Time: {int(row_data['ABS_TIME'])} us",
    ]
    if int(row_data["DEADLINE"]) != UINT32_MAX:
        lines.append(f"Deadline: {int(row_data['DEADLINE'])}")
    if int(row_data["PRIORITY"]) != UINT32_MAX:
        lines.append(f"Priority: {int(row_data['PRIORITY'])}")
    return "<br>".join(lines)


def _add_event_markers(fig, df, y_map_func, marker_offsets, row=None, col=None, showlegend=True):
    events_present = sorted(df["EVENT"].dropna().unique(), key=lambda e: int(e.value))
    for event in events_present:
        df_evt = df[df["EVENT"] == event]
        if df_evt.empty:
            continue

        event_label, symbol, color = EVENT_STYLE.get(
            event,
            (event.name.replace("TRACE_", "").replace("_", " ").title(), "circle", "#444444"),
        )
        y_values = df_evt.apply(lambda r: y_map_func(r) + marker_offsets.get(event, 0.0), axis=1)

        trace = go.Scatter(
            x=df_evt["DISPLAY_X"],
            y=y_values,
            mode="markers",
            name=event_label,
            marker=dict(symbol=symbol, size=9, color=color, line=dict(color="black", width=1)),
            hovertext=df_evt.apply(lambda r: _event_hover(r, event_label), axis=1),
            hoverinfo="text",
            showlegend=showlegend,
        )

        if row is None or col is None:
            fig.add_trace(trace)
        else:
            fig.add_trace(trace, row=row, col=col)


def _add_execution_bars(fig, df_exec, y_map_func, task_colors, row=None, col=None):
    if df_exec.empty:
        return

    exec_rows = list(df_exec.iterrows())
    background_rows = [item for item in exec_rows if is_background_task_name(item[1]["TASK_NAME"])]
    foreground_rows = [item for item in exec_rows if not is_background_task_name(item[1]["TASK_NAME"])]

    for _, row_data in background_rows + foreground_rows:
        us_duration = row_data["ABS_END"] - row_data["ABS_START"]
        duration = int(row_data["DURATION"])
        hover_lines = [
            f"<b>{row_data['TASK_NAME']}</b>",
            f"Core: {row_data['CORE']}",
            f"Start Tick: {row_data['START_TICK']}",
            f"Duration: {row_data['DURATION']} ticks ({us_duration} us)",
            f"Abs Start: {row_data['ABS_START']} us",
        ]
        if row_data["DEADLINE"] != UINT32_MAX:
            hover_lines.append(f"Deadline: {row_data['DEADLINE']}")
        if row_data["PRIORITY"] != UINT32_MAX:
            hover_lines.append(f"Priority: {row_data['PRIORITY']}")

        trace = go.Bar(
            base=[row_data["START_TICK"]],
            x=[float(duration)],
            y=[y_map_func(row_data)],
            orientation="h",
            marker=dict(color=task_colors[row_data["TASK_NAME"]], line=dict(color="black", width=1)),
            hovertext=["<br>".join(hover_lines)],
            hoverinfo="text",
            showlegend=False,
        )

        if row is None or col is None:
            fig.add_trace(trace)
        else:
            fig.add_trace(trace, row=row, col=col)


def build_trace_figure(df, df_exec, title, view):
    core_labels = sorted(df["CORE"].unique())
    lane_names = {core: f"Core {int(core)}" for core in core_labels}
    core_to_y = {core: idx for idx, core in enumerate(core_labels)}
    marker_offsets = _build_marker_offset_map(df)
    df_plot = df.copy()
    df_plot["DISPLAY_X"] = build_marker_display_x(df_plot, df_exec)
    task_colors = build_task_color_map(ordered_task_names(df, df_exec))

    if view == "combined":
        unique_tasks = list(task_colors.keys())
        task_to_y = {task: idx for idx, task in enumerate(unique_tasks)}

        fig = make_subplots(
            rows=2,
            cols=1,
            shared_xaxes=True,
            vertical_spacing=0.08,
            subplot_titles=("Core Lanes", "Task-Centric Timeline"),
        )

        _add_execution_bars(
            fig,
            df_exec,
            y_map_func=lambda r: core_to_y[r["CORE"]],
            task_colors=task_colors,
            row=1,
            col=1,
        )
        _add_event_markers(
            fig,
            df_plot,
            y_map_func=lambda r: core_to_y[int(r["CORE"])],
            marker_offsets=marker_offsets,
            row=1,
            col=1,
            showlegend=True,
        )

        _add_execution_bars(
            fig,
            df_exec,
            y_map_func=lambda r: task_to_y[r["TASK_NAME"]],
            task_colors=task_colors,
            row=2,
            col=1,
        )
        _add_event_markers(
            fig,
            df_plot,
            y_map_func=lambda r: task_to_y[r["TASK_NAME"]],
            marker_offsets=marker_offsets,
            row=2,
            col=1,
            showlegend=False,
        )

        fig.update_yaxes(
            title_text="Core",
            row=1,
            col=1,
            tickmode="array",
            tickvals=list(core_to_y.values()),
            ticktext=[lane_names[c] for c in core_labels],
        )
        fig.update_yaxes(
            title_text="Task",
            row=2,
            col=1,
            tickmode="array",
            tickvals=list(task_to_y.values()),
            ticktext=unique_tasks,
        )
        fig.update_xaxes(title_text="System Ticks", row=2, col=1)

        fig.update_layout(title=f"{title} (Combined)", barmode="overlay", hovermode="closest", height=900)
        return fig

    fig = go.Figure()
    _add_execution_bars(fig, df_exec, y_map_func=lambda r: core_to_y[r["CORE"]], task_colors=task_colors)
    _add_event_markers(fig, df_plot, y_map_func=lambda r: core_to_y[int(r["CORE"])], marker_offsets=marker_offsets)

    fig.update_layout(
        title=title,
        xaxis_title="System Ticks",
        yaxis=dict(
            title="Core Lanes",
            tickmode="array",
            tickvals=list(core_to_y.values()),
            ticktext=[lane_names[c] for c in core_labels],
        ),
        barmode="overlay",
        hovermode="closest",
    )
    return fig


def render_trace_plot(csv_data, title, view):
    df, df_exec = parse_trace_dataframe(csv_data)
    fig = build_trace_figure(df, df_exec, title, view)
    fig.show()


def parse_args():
    parser = argparse.ArgumentParser(description="SMP trace monitor and plotter")
    parser.add_argument(
        "--view",
        choices=["lanes", "combined"],
        default="combined",
        help="Visualization mode: combined (core lanes + task timeline) or lanes (core-only).",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    signal.signal(signal.SIGINT, force_quit)
    selected_port = select_serial_port()

    capturing = False
    trace_buffer = []
    current_title = DEFAULT_TRACE_TITLE

    print(f"\nLooking for {selected_port} at {BAUD_RATE} baud... (Press Ctrl+C to exit)")

    while True:
        try:
            with serial.Serial(selected_port, BAUD_RATE, timeout=0.1) as ser:
                print("\n[Monitor] Connected to Pico!")

                while True:
                    line = ser.readline().decode("utf-8", errors="replace").strip()

                    if not line:
                        continue

                    print(line)

                    banner_title = detect_run_title(line)
                    if banner_title is not None:
                        current_title = banner_title

                    if "TIMESTAMP" in line:
                        if line not in EXPECTED_HEADERS:
                            print(f"\n[Monitor] Warning: Encountered header with unknown format.\nExpected one of: {EXPECTED_HEADERS}\nGot:      {line}")
                        else:
                            capturing = True
                            trace_buffer = [line]
                            print("\n[Monitor] Trace start detected. Capturing data...")
                        continue

                    if line == "--- END OF TRACE ---" and capturing:
                        capturing = False
                        csv_string = "\n".join(trace_buffer)
                        print(f"\n[Monitor] Trace ended. Captured {len(trace_buffer) - 1} records. Plotting...")
                        render_trace_plot(csv_string, current_title, args.view)
                        trace_buffer = []
                        current_title = DEFAULT_TRACE_TITLE
                        continue

                    if capturing:
                        trace_buffer.append(line)

        except serial.SerialException:
            time.sleep(1)
        except Exception as err:
            print(f"\n[Error] Unexpected error: {err}")
            time.sleep(1)


if __name__ == "__main__":
    main()
