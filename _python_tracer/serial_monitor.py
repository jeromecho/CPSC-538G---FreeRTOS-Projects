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
    "Running SMP (Partitioned) Test",
)
EXPECTED_HEADERS = {
    "TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE",
    "TIMESTAMP,EVENT,ABS_TIME,CORE,CORE_SEQ,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE",
    "TIMESTAMP,EVENT,ABS_TIME,CORE,CORE_SEQ,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,DEBUG_CODE,CEILING,PREEMPT_LVL,DEADLINE",
    "TIMESTAMP,EVENT,ABS_TIME,CORE,CORE_SEQ,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,DEBUG_CODE,CEILING,PREEMPT_LVL,DEADLINE,TASK_UID",
}

DEFAULT_TRACE_TITLE = "FreeRTOS Scheduling Trace"
SYNTHETIC_TICK_BOUNDARY_US = 1000

UINT8_MAX = 255
UINT32_MAX = 4294967295

BACKGROUND_TASK_PREFIXES = ("Idle Task", "System Task")

TASK_COLOR_PALETTE = qualitative.Plotly + qualitative.Dark24 + qualitative.Alphabet


EVENT_STYLE = {
    TraceEvent.TRACE_RELEASE: ("Release", "star", "#2ca02c"),
    TraceEvent.TRACE_SWITCH_IN: ("Switch In", "triangle-right", "#3cb44b"),
    TraceEvent.TRACE_SWITCH_OUT: ("Switch Out", "triangle-left", "#e6194b"),
    TraceEvent.TRACE_DONE: ("Task Done", "circle", "#008080"),
    TraceEvent.TRACE_PREPARING_CONTEXT_SWITCH: ("Prepare Context Switch", "diamond-open", "#911eb4"),
    TraceEvent.TRACE_SUSPENDED: ("Suspended", "triangle-down-open", "#808080"),
    TraceEvent.TRACE_RESUMED: ("Resumed", "triangle-up-open", "#f2c300"),
    TraceEvent.TRACE_DEADLINE_MISS: ("Deadline Miss", "x", "#d62728"),
    TraceEvent.TRACE_ADMISSION_FAILED: ("Admission Failed", "hexagram", "#b22222"),
    TraceEvent.TRACE_SEMAPHORE_TAKE: ("Semaphore Take", "triangle-down", "#dc143c"),
    TraceEvent.TRACE_SEMAPHORE_GIVE: ("Semaphore Give", "triangle-up", "#1f77b4"),
    # CBS Events
    TraceEvent.TRACE_BUDGET_RUN_OUT: ("CBS Budget Run Out", "hourglass", "teal"),
    # SMP Events
    TraceEvent.TRACE_REMOVED_FROM_CORE: ("Removed From Core", "x-thin", "#ff7f0e"),
    TraceEvent.TRACE_MIGRATED_TO_CORE: ("Migrated To Core", "cross-thin", "#17becf"),
    TraceEvent.TRACE_DEBUG_MARKER: ("Debug Marker", "circle-open-dot", "#000000"),
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
    task_uid = int(row["TASK_UID"]) if "TASK_UID" in row and int(row["TASK_UID"]) != UINT32_MAX else int(row["TASK_ID"])
    core = int(row["CORE"])

    base_name = TASK_TYPES.get(task_type, f"Unknown ({task_type})")
    if task_type in [1, 2]:
        return f"{base_name} {task_uid + 1:03d}"
    if task_type in [0, 3]:
        return f"{base_name} C{core}"
    return base_name


def get_task_name_without_uid(row):
    task_type = int(row["TASK_TYPE"])
    task_id = int(row["TASK_ID"])
    core = int(row["CORE"])

    base_name = TASK_TYPES.get(task_type, f"Unknown ({task_type})")
    if task_type in [1, 2]:
        return f"{base_name} {task_id + 1:02d}"
    if task_type in [0, 3]:
        return f"{base_name} C{core}"
    return base_name


def build_execution_segments(df):
    segment_columns = [
        "CORE",
        "TASK_LABEL",
        "TASK_UID",
        "START_TICK",
        "END_TICK",
        "DURATION",
        "INCLUSIVE_END",
        "ABS_START",
        "ABS_END",
        "DEADLINE",
        "PRIORITY",
    ]

    segments = []
    active_by_core = {}

    for _, row in df.iterrows():
        event = row["EVENT"]
        core = int(row["CORE"])
        task_name = row["TASK_LABEL"]

        if event == TraceEvent.TRACE_SWITCH_IN:
            active_by_core[core] = row
            continue

        if event == TraceEvent.TRACE_DEADLINE_MISS:
            in_row = active_by_core.get(core)
            if in_row is not None and in_row["TASK_LABEL"] == task_name:
                start_tick = int(in_row["TIMESTAMP"])
                end_tick = int(row["TIMESTAMP"])
                if end_tick >= start_tick:
                    segments.append(
                        {
                            "CORE": core,
                            "TASK_LABEL": task_name,
                            "TASK_UID": int(in_row["TASK_UID"]),
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

        if in_row["TASK_LABEL"] != task_name:
            continue

        start_tick = int(in_row["TIMESTAMP"])
        end_tick = int(row["TIMESTAMP"])
        if end_tick < start_tick:
            continue

        segments.append(
            {
                "CORE": core,
                "TASK_LABEL": task_name,
                "TASK_UID": int(in_row["TASK_UID"]),
                "START_TICK": start_tick,
                "END_TICK": end_tick,
                "DURATION": end_tick - start_tick,
                "INCLUSIVE_END": True,
                "ABS_START": int(in_row["ABS_TIME"]),
                "ABS_END": int(row["ABS_TIME"]),
                "DEADLINE": int(in_row["DEADLINE"]),
                "PRIORITY": int(in_row["PRIORITY"]),
            }
        )
        del active_by_core[core]

    return pd.DataFrame(segments, columns=segment_columns)


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


def _core_pattern_for_task_view(core):
    # Keep task color stable and use 45-degree diagonal hatching for core 1.
    if core == 1:
        return dict(shape="/", fgcolor="rgba(0,0,0,0.55)", solidity=0.22)
    return dict(shape="")


def build_marker_display_x(df, use_time_axis):
    if use_time_axis:
        return df["ELAPSED_US"].astype(float).copy()
    return df["TIMESTAMP"].astype(float).copy()


def get_execution_display_values(row_data, use_time_axis):
    if use_time_axis:
        start_value = int(row_data.get("ELAPSED_START_US", row_data["ABS_START"]))
        end_value = int(row_data.get("ELAPSED_END_US", row_data["ABS_END"]))
        return start_value, end_value, end_value - start_value

    start_value = int(row_data["START_TICK"])
    end_value = int(row_data["END_TICK"])
    return start_value, end_value, end_value - start_value


def parse_trace_dataframe(csv_data):
    df = pd.read_csv(io.StringIO(csv_data))

    if "CORE" not in df.columns:
        df["CORE"] = 0
    if "CORE_SEQ" not in df.columns:
        df["CORE_SEQ"] = range(len(df))
    if "DEBUG_CODE" not in df.columns:
        df["DEBUG_CODE"] = UINT32_MAX
    has_stable_uid = "TASK_UID" in df.columns
    if not has_stable_uid:
        df["TASK_UID"] = df["TASK_ID"]

    zero_abs_time_mask = df["ABS_TIME"] == 0
    if zero_abs_time_mask.any():
        dropped = int(zero_abs_time_mask.sum())
        print(f"[Monitor] Warning: Dropped {dropped} trace row(s) with ABS_TIME=0.")
        df = df.loc[~zero_abs_time_mask].copy()

    trace_start_us = int(df["ABS_TIME"].min())
    df["EVENT"] = df["EVENT"].map(TraceEvent)
    df["ELAPSED_US"] = df["ABS_TIME"] - trace_start_us
    df["TASK_NAME"] = df.apply(get_task_name, axis=1)
    if has_stable_uid:
        df["TASK_LABEL"] = df["TASK_NAME"]
    else:
        df["TASK_LABEL"] = df.apply(get_task_name_without_uid, axis=1)

    df_sorted = df.sort_values(by=["ABS_TIME", "CORE", "CORE_SEQ"], kind="stable").copy()
    df_exec = build_execution_segments(df_sorted)
    if not df_exec.empty:
        df_exec["ELAPSED_START_US"] = df_exec["ABS_START"] - trace_start_us
        df_exec["ELAPSED_END_US"] = df_exec["ABS_END"] - trace_start_us

    return df, df_exec


def ordered_task_names(df, df_exec):
    task_meta = (
        df.groupby("TASK_LABEL", as_index=False)
        .agg(
            CORE_MIN=("CORE", "min"),
            TASK_TYPE_MIN=("TASK_TYPE", "min"),
            TASK_ID_MIN=("TASK_UID", "min"),
            FIRST_TICK=("TIMESTAMP", "min"),
        )
        .set_index("TASK_LABEL")
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

    names = sorted(df["TASK_LABEL"].unique(), key=sort_key)
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


def _event_hover(row_data, event_label, event):
    elapsed_us = int(row_data.get("ELAPSED_US", row_data["ABS_TIME"]))
    lines = [
        f"<b>{event_label}</b>",
        f"Task: {row_data['TASK_LABEL']}",
        f"Core: {int(row_data['CORE'])}",
        f"Tick: {int(row_data['TIMESTAMP'])}",
        f"Time: {elapsed_us} us",
    ]
    if int(row_data["TASK_UID"]) != UINT32_MAX:
        lines.append(f"Task UID: {int(row_data['TASK_UID'])}")
    if int(row_data["DEADLINE"]) != UINT32_MAX:
        lines.append(f"Deadline: {int(row_data['DEADLINE'])}")
    if int(row_data["PRIORITY"]) != UINT32_MAX:
        lines.append(f"Priority: {int(row_data['PRIORITY'])}")
    if event == TraceEvent.TRACE_DEBUG_MARKER and "DEBUG_CODE" in row_data and int(row_data["DEBUG_CODE"]) != UINT8_MAX:
        lines.append(f"Debug Code: {int(row_data['DEBUG_CODE'])}")
    return "<br>".join(lines)


def _add_event_markers(
    fig,
    df,
    y_map_func,
    marker_offsets,
    row=None,
    col=None,
    showlegend=True,
    x_column="DISPLAY_X",
    visible=True,
):
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
            x=df_evt[x_column],
            y=y_values,
            mode="markers",
            name=event_label,
            marker=dict(symbol=symbol, size=9, color=color, line=dict(color="black", width=1)),
            hovertext=df_evt.apply(lambda r: _event_hover(r, event_label, event), axis=1),
            hoverinfo="text",
            showlegend=showlegend,
            visible=visible,
        )

        if row is None or col is None:
            fig.add_trace(trace)
        else:
            fig.add_trace(trace, row=row, col=col)


def _add_execution_bars(
    fig,
    df_exec,
    y_map_func,
    task_colors,
    use_time_axis,
    row=None,
    col=None,
    shade_by_core=False,
    visible=True,
):
    if df_exec.empty:
        return

    exec_rows = list(df_exec.iterrows())
    background_rows = [item for item in exec_rows if is_background_task_name(item[1]["TASK_LABEL"])]
    foreground_rows = [item for item in exec_rows if not is_background_task_name(item[1]["TASK_LABEL"])]

    for _, row_data in background_rows + foreground_rows:
        start_value, _, duration = get_execution_display_values(row_data, use_time_axis)
        if use_time_axis:
            hover_lines = [
                f"<b>{row_data['TASK_LABEL']}</b>",
                f"Core: {row_data['CORE']}",
                f"Start Tick: {row_data['START_TICK']}",
                f"Start Time: {start_value} us",
                f"Duration: {duration} us",
            ]
        else:
            hover_lines = [
                f"<b>{row_data['TASK_LABEL']}</b>",
                f"Core: {row_data['CORE']}",
                f"Start Tick: {start_value}",
                f"Duration: {duration} ticks ({row_data['ABS_END'] - row_data['ABS_START']} us)",
                f"Start: {int(row_data.get('ELAPSED_START_US', row_data['ABS_START']))} us",
            ]
        if int(row_data["TASK_UID"]) != UINT32_MAX:
            hover_lines.append(f"Task UID: {int(row_data['TASK_UID'])}")
        if row_data["DEADLINE"] != UINT32_MAX:
            hover_lines.append(f"Deadline: {row_data['DEADLINE']}")
        if row_data["PRIORITY"] != UINT32_MAX:
            hover_lines.append(f"Priority: {row_data['PRIORITY']}")

        bar_color = task_colors[row_data["TASK_LABEL"]]
        marker_pattern = None
        if shade_by_core:
            marker_pattern = _core_pattern_for_task_view(int(row_data["CORE"]))

        trace = go.Bar(
            base=[start_value],
            x=[float(duration)],
            y=[y_map_func(row_data)],
            orientation="h",
            marker=dict(color=bar_color, line=dict(color="black", width=1), pattern=marker_pattern),
            hovertext=["<br>".join(hover_lines)],
            hoverinfo="text",
            showlegend=False,
            visible=visible,
        )

        if row is None or col is None:
            fig.add_trace(trace)
        else:
            fig.add_trace(trace, row=row, col=col)


def _apply_synthetic_tick_boundaries(fig, use_time_axis, enable_synthetic_boundaries):
    if not (use_time_axis and enable_synthetic_boundaries):
        return

    fig.update_xaxes(
        tickmode="linear",
        tick0=0,
        dtick=SYNTHETIC_TICK_BOUNDARY_US,
        showgrid=True,
        gridcolor="rgba(120, 120, 120, 0.22)",
        zeroline=False,
    )


def _axis_updates_for_mode(view, use_time_axis, synthetic_tick_boundaries):
    x_axis_title = "Elapsed Time (us)" if use_time_axis else "System Ticks"
    updates = {}

    if view == "combined":
        axis_names = ["xaxis", "xaxis2"]
        updates["xaxis2.title.text"] = x_axis_title
    else:
        axis_names = ["xaxis"]
        updates["xaxis.title.text"] = x_axis_title

    for axis_name in axis_names:
        if use_time_axis and synthetic_tick_boundaries:
            updates[f"{axis_name}.tickmode"] = "linear"
            updates[f"{axis_name}.tick0"] = 0
            updates[f"{axis_name}.dtick"] = SYNTHETIC_TICK_BOUNDARY_US
            updates[f"{axis_name}.showgrid"] = True
            updates[f"{axis_name}.gridcolor"] = "rgba(120, 120, 120, 0.22)"
            updates[f"{axis_name}.zeroline"] = False
        else:
            updates[f"{axis_name}.tickmode"] = "auto"
            updates[f"{axis_name}.tick0"] = None
            updates[f"{axis_name}.dtick"] = None
            updates[f"{axis_name}.showgrid"] = True
            updates[f"{axis_name}.gridcolor"] = None
            updates[f"{axis_name}.zeroline"] = None

    return updates


def _visibility_mask(total_traces, visible_indices):
    visible_set = set(visible_indices)
    return [index in visible_set for index in range(total_traces)]


def _build_axis_toggle_menu(view, use_time_axis, synthetic_tick_boundaries, tick_visibility, time_visibility, y_pos):
    return dict(
        type="buttons",
        direction="right",
        x=0.5,
        y=y_pos,
        xanchor="center",
        yanchor="top",
        showactive=True,
        active=1 if use_time_axis else 0,
        buttons=[
            dict(
                label="Tick Axis",
                method="update",
                args=[
                    {"visible": tick_visibility},
                    _axis_updates_for_mode(view, False, synthetic_tick_boundaries),
                ],
            ),
            dict(
                label="Time Axis",
                method="update",
                args=[
                    {"visible": time_visibility},
                    _axis_updates_for_mode(view, True, synthetic_tick_boundaries),
                ],
            ),
        ],
    )


def build_trace_figure(df, df_exec, title, view, use_time_axis, synthetic_tick_boundaries):
    # Tick view is the default unless explicitly overridden by CLI flags.
    use_time_axis = bool(use_time_axis)

    core_labels = sorted(df["CORE"].unique())
    lane_names = {core: f"Core {int(core)}" for core in core_labels}
    core_to_y = {core: idx for idx, core in enumerate(core_labels)}
    marker_offsets = _build_marker_offset_map(df)
    df_plot = df.copy()
    df_plot["DISPLAY_X_TICK"] = build_marker_display_x(df_plot, use_time_axis=False)
    df_plot["DISPLAY_X_TIME"] = build_marker_display_x(df_plot, use_time_axis=True)
    df_exec_core = df_exec[~df_exec["TASK_LABEL"].apply(is_background_task_name)].copy()
    df_plot_core = df_plot[~df_plot["TASK_LABEL"].apply(is_background_task_name)].copy()
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

        def add_traces_for_mode(mode_time_axis, visible):
            x_column = "DISPLAY_X_TIME" if mode_time_axis else "DISPLAY_X_TICK"
            start_index = len(fig.data)

            _add_execution_bars(
                fig,
                df_exec_core,
                y_map_func=lambda r: core_to_y[r["CORE"]],
                task_colors=task_colors,
                use_time_axis=mode_time_axis,
                row=1,
                col=1,
                shade_by_core=True,
                visible=visible,
            )
            _add_event_markers(
                fig,
                df_plot_core,
                y_map_func=lambda r: core_to_y[int(r["CORE"])],
                marker_offsets=marker_offsets,
                row=1,
                col=1,
                showlegend=False,
                x_column=x_column,
                visible=visible,
            )

            _add_execution_bars(
                fig,
                df_exec,
                y_map_func=lambda r: task_to_y[r["TASK_LABEL"]],
                task_colors=task_colors,
                use_time_axis=mode_time_axis,
                row=2,
                col=1,
                shade_by_core=True,
                visible=visible,
            )
            _add_event_markers(
                fig,
                df_plot,
                y_map_func=lambda r: task_to_y[r["TASK_LABEL"]],
                marker_offsets=marker_offsets,
                row=2,
                col=1,
                showlegend=True,
                x_column=x_column,
                visible=visible,
            )
            return list(range(start_index, len(fig.data)))

        tick_indices = add_traces_for_mode(mode_time_axis=False, visible=not use_time_axis)
        time_indices = add_traces_for_mode(mode_time_axis=True, visible=use_time_axis)

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

        fig.update_layout(
            title=f"{title} (Combined)",
            barmode="overlay",
            hovermode="closest",
            height=900,
            margin=dict(b=130),
        )
        fig.update_layout(_axis_updates_for_mode(view, use_time_axis, synthetic_tick_boundaries))

        total_traces = len(fig.data)
        tick_visibility = _visibility_mask(total_traces, tick_indices)
        time_visibility = _visibility_mask(total_traces, time_indices)

        fig.update_layout(
            updatemenus=[
                _build_axis_toggle_menu(
                    view,
                    use_time_axis,
                    synthetic_tick_boundaries,
                    tick_visibility,
                    time_visibility,
                    y_pos=-0.16,
                )
            ]
        )
        return fig

    fig = go.Figure()

    def add_traces_for_mode(mode_time_axis, visible):
        x_column = "DISPLAY_X_TIME" if mode_time_axis else "DISPLAY_X_TICK"
        start_index = len(fig.data)
        _add_execution_bars(
            fig,
            df_exec_core,
            y_map_func=lambda r: core_to_y[r["CORE"]],
            task_colors=task_colors,
            use_time_axis=mode_time_axis,
            shade_by_core=True,
            visible=visible,
        )
        _add_event_markers(
            fig,
            df_plot_core,
            y_map_func=lambda r: core_to_y[int(r["CORE"])],
            marker_offsets=marker_offsets,
            x_column=x_column,
            visible=visible,
        )
        return list(range(start_index, len(fig.data)))

    tick_indices = add_traces_for_mode(mode_time_axis=False, visible=not use_time_axis)
    time_indices = add_traces_for_mode(mode_time_axis=True, visible=use_time_axis)

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
        margin=dict(b=90),
    )
    fig.update_layout(_axis_updates_for_mode(view, use_time_axis, synthetic_tick_boundaries))

    total_traces = len(fig.data)
    tick_visibility = _visibility_mask(total_traces, tick_indices)
    time_visibility = _visibility_mask(total_traces, time_indices)

    fig.update_layout(
        updatemenus=[
            _build_axis_toggle_menu(
                view,
                use_time_axis,
                synthetic_tick_boundaries,
                tick_visibility,
                time_visibility,
                y_pos=-0.14,
            )
        ]
    )
    return fig


def render_trace_plot(csv_data, title, view, use_time_axis, synthetic_tick_boundaries):
    df, df_exec = parse_trace_dataframe(csv_data)
    fig = build_trace_figure(df, df_exec, title, view, use_time_axis, synthetic_tick_boundaries)
    fig.show()


def parse_args():
    parser = argparse.ArgumentParser(description="SMP trace monitor and plotter")
    parser.add_argument(
        "--view",
        choices=["lanes", "combined"],
        default="combined",
        help="Visualization mode: combined (core lanes + task timeline) or lanes (core-only).",
    )
    parser.add_argument(
        "--time-axis",
        action="store_true",
        default=False,
        help="Start in time-axis mode (default starts in tick-axis mode).",
    )
    parser.add_argument(
        "--synthetic-tick-boundaries",
        action="store_true",
        help="When used with --time-axis, overlay a 1000 us tick grid starting at the first event.",
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
                        render_trace_plot(
                            csv_string,
                            current_title,
                            args.view,
                            args.time_axis,
                            args.synthetic_tick_boundaries,
                        )
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
