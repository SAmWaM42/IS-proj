#!/usr/bin/env python3
"""Create training/evaluation plots from the IS-2 JSON/JSONL artifacts.

Default input:  artifacts/training
Default output: artifacts/visualizations

Requires matplotlib (install into the project's active virtual environment).
The script only reads experiment outputs; it does not train or modify policies.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError as exc:
    raise SystemExit(
        "matplotlib is required. Activate the project virtual environment and run:\n"
        "  python -m pip install matplotlib"
    ) from exc


AGENTS = ("baseline", "proposed")
MODES = ("train", "validate", "compare")
GOAL_LABELS = {0: "social", 1: "request", 2: "disturbance/rest"}
PROXIMITY_LABELS = {0: "near", 1: "mid", 2: "far", 3: "unavailable"}
USER_LABELS = {0: "neutral", 1: "positive", 2: "rejection"}


def load_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise FileNotFoundError(f"Required artifact not found: {path}")
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return data


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        raise FileNotFoundError(f"Required step log not found: {path}")
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, start=1):
            if not line.strip():
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"Invalid JSON in {path}:{line_number}: {exc}") from exc
            if isinstance(value, dict):
                rows.append(value)
    if not rows:
        raise ValueError(f"No step records found in {path}")
    return rows


def episode_rows(summary: dict[str, Any], agent: str) -> list[dict[str, Any]]:
    rows = summary.get(agent, {}).get("episodes", [])
    return [row for row in rows if isinstance(row, dict)]


def episode_reward(row: dict[str, Any]) -> float:
    return float(row.get("total_reward", 0.0))


def safe_state(value: Any) -> str:
    return str(value) if value is not None else "unknown"


def context_label(row: dict[str, Any]) -> str:
    goal = GOAL_LABELS.get(row.get("goal_state"), f"goal={row.get('goal_state', '?')}")
    proximity = PROXIMITY_LABELS.get(
        row.get("target_proximity"), str(row.get("target_proximity", "?"))
    )
    user = USER_LABELS.get(row.get("user_status"), str(row.get("user_status", "?")))
    flags = []
    if row.get("food_available"):
        flags.append("food")
    if row.get("play_available"):
        flags.append("play")
    if row.get("task_status"):
        flags.append("task")
    suffix = f", {','.join(flags)}" if flags else ""
    return f"{goal}; {proximity}; {user}{suffix}"


def plot_episode_rewards(summary: dict[str, Any], out_path: Path, window: int) -> None:
    fig, ax = plt.subplots(figsize=(12, 6.5))
    plotted = 0
    for agent in AGENTS:
        rows = sorted(episode_rows(summary, agent), key=lambda row: int(row.get("episode", 0)))
        if not rows:
            continue
        episodes = [int(row.get("episode", i)) + 1 for i, row in enumerate(rows)]
        rewards = [episode_reward(row) for row in rows]
        ax.plot(episodes, rewards, alpha=0.28, linewidth=0.9, label=f"{agent.title()} episode reward")
        if len(rewards) >= window and window > 1:
            rolling = [
                sum(rewards[max(0, i - window + 1):i + 1]) /
                len(rewards[max(0, i - window + 1):i + 1])
                for i in range(len(rewards))
            ]
            ax.plot(episodes, rolling, linewidth=2.0, label=f"{agent.title()} rolling mean ({window})")
        plotted += 1
    if not plotted:
        raise ValueError("Training summary does not contain baseline/proposed episode rewards.")
    ax.set_title("Training reward per episode")
    ax.set_xlabel("Episode")
    ax.set_ylabel("Total reward in episode")
    ax.grid(True, alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def plot_state_visitation(rows_by_agent: dict[str, list[dict[str, Any]]],
                          out_path: Path, top_n: int) -> dict[str, Counter[str]]:
    counts: dict[str, Counter[str]] = {}
    for agent, rows in rows_by_agent.items():
        counts[agent] = Counter(safe_state(row.get("encoded_state")) for row in rows)
    combined = Counter()
    for counter in counts.values():
        combined.update(counter)
    selected = [state for state, _ in combined.most_common(top_n)]
    if not selected:
        raise ValueError("No encoded_state values were found in the training step logs.")

    positions = list(range(len(selected)))
    width = 0.40
    fig, ax = plt.subplots(figsize=(max(12, len(selected) * 0.62), 6.5))
    ax.bar([p - width / 2 for p in positions],
           [counts.get("baseline", Counter()).get(state, 0) for state in selected],
           width, label="Baseline")
    ax.bar([p + width / 2 for p in positions],
           [counts.get("proposed", Counter()).get(state, 0) for state in selected],
           width, label="Proposed")
    ax.set_title(f"Most-visited encoded states in retained final episodes (top {len(selected)})")
    ax.set_xlabel("Encoded state ID")
    ax.set_ylabel("Number of visits")
    ax.set_xticks(positions)
    ax.set_xticklabels(selected, rotation=55, ha="right", fontsize=8)
    ax.grid(axis="y", alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)
    return counts


def plot_mean_rewards(input_dir: Path, out_path: Path) -> list[dict[str, Any]]:
    values: list[dict[str, Any]] = []
    for mode in MODES:
        path = input_dir / f"{mode}_summary.json"
        if not path.is_file():
            continue
        summary = load_json(path)
        for agent in AGENTS:
            block = summary.get(agent, {})
            mean_episode = block.get("mean_total_reward_per_episode", block.get("mean_reward"))
            mean_step = block.get("mean_reward_per_step")
            if mean_episode is None:
                episode_values = [episode_reward(row) for row in episode_rows(summary, agent)]
                mean_episode = sum(episode_values) / len(episode_values) if episode_values else math.nan
            values.append({
                "mode": mode,
                "agent": agent,
                "mean_total_reward_per_episode": float(mean_episode),
                "mean_reward_per_step": float(mean_step) if mean_step is not None else math.nan,
                "episodes": summary.get("episodes", ""),
                "seed": summary.get("seed", ""),
            })
    if not values:
        raise FileNotFoundError(
            f"No train/validate/compare summaries found in {input_dir}. Run the C++ experiment modes first."
        )

    modes_present = [mode for mode in MODES if any(row["mode"] == mode for row in values)]
    fig, ax = plt.subplots(figsize=(10, 6))
    positions = list(range(len(modes_present)))
    width = 0.36
    for offset, agent in ((-width / 2, "baseline"), (width / 2, "proposed")):
        lookup = {(row["mode"], row["agent"]): row["mean_total_reward_per_episode"] for row in values}
        plotted = [
            lookup.get((mode, agent), math.nan) for mode in modes_present
        ]
        ax.bar([p + offset for p in positions], plotted, width, label=agent.title())
    ax.set_title("Mean total reward per episode by experiment mode")
    ax.set_xlabel("Experiment mode")
    ax.set_ylabel("Mean total reward per episode")
    ax.set_xticks(positions)
    ax.set_xticklabels([mode.title() for mode in modes_present])
    ax.grid(axis="y", alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)
    return values


def plot_policy_coverage(summary: dict[str, Any],
                         rows_by_agent: dict[str, list[dict[str, Any]]],
                         out_path: Path,
                         pair_out_path: Path,
                         top_n: int = 20) -> dict[str, dict[str, Any]]:
    """Plot how often decisions had stored Q-values, and most-visited state/action pairs."""
    coverage: dict[str, dict[str, Any]] = {}
    pair_counts: dict[str, Counter[str]] = {}
    for agent in AGENTS:
        episodes = episode_rows(summary, agent)
        has_policy_coverage_fields = bool(episodes) and all(
            "q_table_supported_decisions" in row and "fallback_decisions" in row
            for row in episodes
        )
        decisions = sum(int(row.get("steps", 0)) for row in episodes)
        supported = sum(int(row.get("q_table_supported_decisions", 0)) for row in episodes)
        fallback = sum(int(row.get("fallback_decisions", 0)) for row in episodes)
        # Episode summaries aggregate coverage across the whole run. Exact global
        # unique-state unions require full step logs, so do not infer them from
        # the intentionally truncated diagnostic trace.
        coverage[agent] = {
            "coverage_fields_available": has_policy_coverage_fields,
            "scope": "all_training_episodes_from_episode_summary",
            "decisions": decisions,
            "q_table_supported_decisions": supported if has_policy_coverage_fields else None,
            "fallback_decisions": fallback if has_policy_coverage_fields else None,
            "q_table_supported_decision_rate": supported / decisions if has_policy_coverage_fields and decisions else None,
            "fallback_decision_rate": fallback / decisions if has_policy_coverage_fields and decisions else None,
            "sum_episode_unique_states_visited": sum(int(row.get("unique_states_visited", 0)) for row in episodes),
            "sum_episode_unique_state_action_pairs": sum(
                len(row.get("state_action_visits", {}))
                if isinstance(row.get("state_action_visits", {}), dict) else 0
                for row in episodes
            ),
            "retained_step_log_rows": len(rows_by_agent.get(agent, [])),
        }
        rows = rows_by_agent.get(agent, [])
        pair_counts[agent] = Counter(
            f"{safe_state(row.get('encoded_state'))} | {row.get('action', 'UNKNOWN')}"
            for row in rows
        )

    fig, ax = plt.subplots(figsize=(8, 5.5))
    positions = list(range(len(AGENTS)))
    coverage_available = all(coverage.get(agent, {}).get("coverage_fields_available", False) for agent in AGENTS)
    supported_values = [coverage.get(agent, {}).get("q_table_supported_decisions") or 0 for agent in AGENTS]
    fallback_values = [coverage.get(agent, {}).get("fallback_decisions") or 0 for agent in AGENTS]
    if coverage_available:
        ax.bar(positions, supported_values, label="Stored valid Q-values available")
        ax.bar(positions, fallback_values, bottom=supported_values, label="No stored valid Q-values")
        ax.legend()
        ax.set_title("Training decision coverage")
    else:
        ax.text(0.5, 0.5, "Coverage fields missing. Rebuild and rerun --train.", 
                ha="center", va="center", transform=ax.transAxes)
        ax.set_title("Training decision coverage — unavailable for older logs")
    ax.set_xlabel("Agent")
    ax.set_ylabel("Decision count")
    ax.set_xticks(positions)
    ax.set_xticklabels([agent.title() for agent in AGENTS])
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)

    combined = Counter()
    for counter in pair_counts.values():
        combined.update(counter)
    selected = [pair for pair, _ in combined.most_common(top_n)]
    if selected:
        x = list(range(len(selected)))
        width = 0.40
        fig, ax = plt.subplots(figsize=(max(12, len(selected) * 0.7), 6.5))
        ax.bar([i - width / 2 for i in x],
               [pair_counts.get("baseline", Counter()).get(pair, 0) for pair in selected],
               width, label="Baseline")
        ax.bar([i + width / 2 for i in x],
               [pair_counts.get("proposed", Counter()).get(pair, 0) for pair in selected],
               width, label="Proposed")
        ax.set_title(f"Most-visited state/action pairs in retained final episodes (top {len(selected)})")
        ax.set_xlabel("Encoded state | action")
        ax.set_ylabel("Visit count")
        ax.set_xticks(x)
        ax.set_xticklabels(selected, rotation=60, ha="right", fontsize=7)
        ax.grid(axis="y", alpha=0.25)
        ax.legend()
        fig.tight_layout()
        fig.savefig(pair_out_path, dpi=160)
        plt.close(fig)
    return coverage


def write_coverage_json(coverage: dict[str, dict[str, int]], path: Path) -> None:
    with path.open("w", encoding="utf-8") as handle:
        json.dump(coverage, handle, indent=2)
        handle.write("\n")


def write_mean_reward_csv(rows: list[dict[str, Any]], path: Path) -> None:
    fields = [
        "mode", "agent", "episodes", "seed",
        "mean_total_reward_per_episode", "mean_reward_per_step",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def plot_final_episode_trace(rows_by_agent: dict[str, list[dict[str, Any]]],
                             out_path: Path,
                             dot_path: Path) -> int:
    final_episodes: dict[str, int] = {}
    traces: dict[str, list[dict[str, Any]]] = {}
    for agent, rows in rows_by_agent.items():
        final_episode = max(int(row.get("episode", 0)) for row in rows)
        final_episodes[agent] = final_episode
        traces[agent] = sorted(
            [row for row in rows if int(row.get("episode", 0)) == final_episode],
            key=lambda row: int(row.get("step", 0)),
        )
    if not any(traces.values()):
        raise ValueError("No final-episode trace records were found.")

    # A pair of ordered paths is a compact behaviour trace: each action node
    # records the observed context, action, reward, and encoded state.
    fig_height = max(8.0, max(len(trace) for trace in traces.values()) * 0.58)
    fig, axes = plt.subplots(1, 2, figsize=(18, fig_height), squeeze=False)
    dot_lines = [
        "digraph FinalEpisodeBehaviourTrace {",
        '  rankdir="TB";',
        '  graph [label="Final training episode behaviour traces", labelloc="t"];',
        '  node [shape=box, style="rounded"];',
    ]
    for axis, agent in zip(axes[0], AGENTS):
        trace = traces.get(agent, [])
        axis.set_title(f"{agent.title()} — episode {final_episodes.get(agent, '?')}")
        axis.set_xlim(0, 1)
        axis.set_ylim(-0.5, max(0.5, len(trace) - 0.5))
        axis.invert_yaxis()
        axis.axis("off")
        previous_y = None
        previous_dot = None
        for index, row in enumerate(trace):
            step = int(row.get("step", index))
            action = str(row.get("action", "UNKNOWN"))
            reward = float(row.get("reward", 0.0))
            state = safe_state(row.get("encoded_state"))
            context = context_label(row)
            energy = row.get("energy_before", "?")
            label = (
                f"Step {step + 1} | {context}\n"
                f"Action: {action} | reward: {reward:+.3f}\n"
                f"State: {state} | energy: {energy}"
            )
            y = float(index)
            axis.text(
                0.5, y, label, ha="center", va="center", fontsize=7.5,
                bbox={"boxstyle": "round,pad=0.35", "alpha": 0.9},
                transform=axis.get_yaxis_transform(),
            )
            node_id = f"{agent}_step_{step}"
            dot_lines.append(
                f'  "{node_id}" [label="{agent.title()} step {step + 1}\\n'
                f'{context.replace(chr(34), chr(39))}\\n'
                f'{action} | r={reward:+.3f}\\nstate={state}"];'
            )
            if previous_y is not None:
                axis.annotate(
                    "", xy=(0.5, y - 0.26), xytext=(0.5, previous_y + 0.26),
                    xycoords=axis.get_yaxis_transform(),
                    arrowprops={"arrowstyle": "->", "linewidth": 0.8},
                )
            if previous_dot is not None:
                dot_lines.append(f'  "{previous_dot}" -> "{node_id}";')
            previous_y = y
            previous_dot = node_id
    dot_lines.append("}")
    fig.suptitle("Final training episode: observed context → selected action → reward", y=0.998)
    fig.tight_layout(rect=(0, 0, 1, 0.995))
    fig.savefig(out_path, dpi=160, bbox_inches="tight")
    plt.close(fig)
    dot_path.write_text("\n".join(dot_lines) + "\n", encoding="utf-8")
    return max(final_episodes.values())




def population_std(values: list[float]) -> float:
    if not values:
        return math.nan
    mean = sum(values) / len(values)
    return math.sqrt(sum((value - mean) ** 2 for value in values) / len(values))


def write_training_window_report(summary: dict[str, Any],
                                 csv_path: Path, plot_path: Path,
                                 window_size: int) -> list[dict[str, Any]]:
    """Compare early/late reward and summary-derived coverage over all training episodes."""
    report: list[dict[str, Any]] = []
    actual_window = window_size
    for agent in AGENTS:
        episodes = sorted(
            episode_rows(summary, agent), key=lambda row: int(row.get("episode", 0))
        )
        if not episodes:
            continue
        actual_window = min(window_size, len(episodes))
        windows = [
            ("early", episodes[:actual_window]),
            ("late", episodes[-actual_window:]),
        ]
        for label, selected in windows:
            rewards = [episode_reward(row) for row in selected]
            decisions = sum(int(row.get("steps", 0)) for row in selected)
            supported = sum(int(row.get("q_table_supported_decisions", 0)) for row in selected)
            fallback = sum(int(row.get("fallback_decisions", 0)) for row in selected)
            state_counts = [int(row.get("unique_states_visited", 0)) for row in selected]
            pair_counts = [
                len(row.get("state_action_visits", {}))
                if isinstance(row.get("state_action_visits", {}), dict) else 0
                for row in selected
            ]
            report.append({
                "agent": agent,
                "window": label,
                "episodes_in_window": len(rewards),
                "start_episode": int(selected[0].get("episode", 0)) + 1,
                "end_episode": int(selected[-1].get("episode", 0)) + 1,
                "mean_total_reward": sum(rewards) / len(rewards),
                "std_total_reward": population_std(rewards),
                "min_total_reward": min(rewards),
                "max_total_reward": max(rewards),
                "decisions": decisions,
                "sum_episode_unique_states_visited": sum(state_counts),
                "sum_episode_unique_state_action_pairs": sum(pair_counts),
                "q_table_coverage_fields_available": bool(selected) and all(
                    "q_table_supported_decisions" in row and "fallback_decisions" in row
                    for row in selected
                ),
                "q_table_supported_decisions": supported,
                "fallback_decisions": fallback,
                "q_table_supported_decision_rate": supported / decisions if decisions else "",
                "fallback_decision_rate": fallback / decisions if decisions else "",
            })

    fields = [
        "agent", "window", "episodes_in_window", "start_episode", "end_episode",
        "mean_total_reward", "std_total_reward", "min_total_reward", "max_total_reward",
        "decisions", "sum_episode_unique_states_visited",
        "sum_episode_unique_state_action_pairs", "q_table_coverage_fields_available",
        "q_table_supported_decisions", "fallback_decisions",
        "q_table_supported_decision_rate", "fallback_decision_rate",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(report)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    positions = [0, 1]
    width = 0.36
    for offset, agent in ((-width / 2, "baseline"), (width / 2, "proposed")):
        selected = [row for row in report if row["agent"] == agent]
        lookup = {row["window"]: row for row in selected}
        means = [lookup.get(window, {}).get("mean_total_reward", math.nan)
                 for window in ("early", "late")]
        errors = [lookup.get(window, {}).get("std_total_reward", 0.0)
                  for window in ("early", "late")]
        ax.bar([position + offset for position in positions], means, width,
               yerr=errors, capsize=4, label=agent.title())
    ax.set_title(f"Early vs late training reward (window = {actual_window} episodes)")
    ax.set_xlabel("Training window")
    ax.set_ylabel("Mean total reward per episode (error bars: SD)")
    ax.set_xticks(positions)
    ax.set_xticklabels(["Early", "Late"])
    ax.grid(axis="y", alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(plot_path, dpi=160)
    plt.close(fig)
    return report

def write_sleep_lay_diagnostics(rows_by_agent: dict[str, list[dict[str, Any]]],
                                csv_path: Path, json_path: Path) -> list[dict[str, Any]]:
    """Extract sleep decisions during an active request for inspection, not automatic classification."""
    candidates: list[dict[str, Any]] = []
    for agent, rows in rows_by_agent.items():
        for row in rows:
            if row.get("action") != "SLEEP_LAY":
                continue
            # goal_state 1 is a request; task_status 1 denotes an active task in current logs.
            if row.get("goal_state") != 1 or row.get("task_status") != 1:
                continue
            energy = row.get("energy_before")
            energy_value = float(energy) if isinstance(energy, (int, float)) else math.nan
            candidates.append({
                "agent": agent,
                "episode": row.get("episode"),
                "step": row.get("step"),
                "encoded_state": row.get("encoded_state"),
                "energy_before": energy,
                "energy_after": row.get("energy_after"),
                "energy_below_35pct_recovery_threshold": energy_value < 0.35 if not math.isnan(energy_value) else None,
                "energy_at_or_below_20pct_restriction_threshold": energy_value <= 0.20 if not math.isnan(energy_value) else None,
                "target_proximity": row.get("target_proximity"),
                "target_proximity_label": PROXIMITY_LABELS.get(row.get("target_proximity"), "unknown"),
                "food_available": row.get("food_available"),
                "play_available": row.get("play_available"),
                "contextual_fallback": row.get("contextual_fallback", ""),
                "q_table_state_present": row.get("q_table_state_present"),
                "q_table_has_valid_action_values": row.get("q_table_has_valid_action_values"),
                "decision_used_fallback": row.get("decision_used_fallback"),
                "policy_action_before_overrides": row.get("policy_action_before_overrides", ""),
                "final_action_after_overrides": row.get("final_action_after_overrides", row.get("action", "")),
                "general_preference_override": row.get("general_preference_override", ""),
                "rejection_override_applied": row.get("rejection_override_applied"),
                "valid_action_q_values": json.dumps(row.get("valid_action_q_values", []), ensure_ascii=False),
                "reward": row.get("reward"),
                "terminal": row.get("terminal"),
                "terminal_reason": row.get("terminal_reason"),
                "interpretation": (
                    "Inspect energy and action constraints before judging this decision; "
                    "low-energy thresholds are contextual clues, not proof that sleep was required."
                ),
            })

    fields = list(candidates[0].keys()) if candidates else [
        "agent", "episode", "step", "encoded_state", "energy_before", "energy_after",
        "energy_below_35pct_recovery_threshold", "energy_at_or_below_20pct_restriction_threshold",
        "target_proximity", "target_proximity_label", "food_available", "play_available",
        "contextual_fallback", "q_table_state_present", "q_table_has_valid_action_values",
        "decision_used_fallback", "policy_action_before_overrides", "final_action_after_overrides",
        "general_preference_override", "rejection_override_applied", "valid_action_q_values",
        "reward", "terminal", "terminal_reason", "interpretation"
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(candidates)
    with json_path.open("w", encoding="utf-8") as handle:
        json.dump({
            "candidate_definition": "SLEEP_LAY while goal_state=request and task_status=active",
            "candidate_count": len(candidates),
            "notes": [
                "These records are diagnostic candidates, not confirmed errors.",
                "Energy thresholds mirror the current README description: recovery shaping below 35%; recovery-action restriction at or below 20%.",
                "target_proximity is logged context; it does not by itself prove a distance guard was triggered.",
                "Q-values and override details are available for proposed-agent decisions when present in current C++ step logs."
            ],
            "candidates": candidates,
        }, handle, indent=2)
        handle.write("\n")
    return candidates


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=Path("artifacts/training"))
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/visualizations"))
    parser.add_argument("--top-states", type=int, default=20,
                        help="number of most-visited encoded states to show (default: 20)")
    parser.add_argument("--rolling-window", type=int, default=25,
                        help="episode window for the reward trend line (default: 25)")
    parser.add_argument("--training-window", type=int, default=100,
                        help="episodes in each early/late training comparison window (default: 100)")
    args = parser.parse_args()
    if args.top_states < 1 or args.rolling_window < 1 or args.training_window < 1:
        parser.error("--top-states, --rolling-window, and --training-window must be positive integers")

    input_dir = args.input_dir
    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    training_summary = load_json(input_dir / "train_summary.json")
    step_logs = {
        agent: load_jsonl(input_dir / f"train_{agent}_steps.jsonl")
        for agent in AGENTS
    }
    plot_episode_rewards(training_summary, output_dir / "reward_per_episode.png", args.rolling_window)
    state_counts = plot_state_visitation(step_logs, output_dir / "state_visitation.png", args.top_states)
    mean_rows = plot_mean_rewards(input_dir, output_dir / "mean_reward_by_mode.png")
    write_mean_reward_csv(mean_rows, output_dir / "mean_reward_by_mode.csv")
    coverage = plot_policy_coverage(
        training_summary,
        step_logs,
        output_dir / "policy_coverage.png",
        output_dir / "state_action_visitation.png",
        args.top_states,
    )
    write_coverage_json(coverage, output_dir / "coverage_summary.json")
    window_report = write_training_window_report(
        training_summary,
        output_dir / "training_window_summary.csv",
        output_dir / "early_vs_late_training_reward.png",
        args.training_window,
    )
    sleep_candidates = write_sleep_lay_diagnostics(
        step_logs,
        output_dir / "sleep_lay_active_request_diagnostics.csv",
        output_dir / "sleep_lay_active_request_diagnostics.json",
    )
    final_episode = plot_final_episode_trace(
        step_logs,
        output_dir / "final_episode_behaviour_trace.png",
        output_dir / "final_episode_behaviour_trace.dot",
    )

    print(f"Visualizations written to: {output_dir}")
    print("  reward_per_episode.png")
    print("  state_visitation.png")
    print("  mean_reward_by_mode.png")
    print("  mean_reward_by_mode.csv")
    print("  policy_coverage.png")
    print("  state_action_visitation.png")
    print("  coverage_summary.json")
    print("  training_window_summary.csv")
    print("  early_vs_late_training_reward.png")
    print("  sleep_lay_active_request_diagnostics.csv")
    print("  sleep_lay_active_request_diagnostics.json")
    print(f"Training window size: {args.training_window} episodes per window")
    print(f"Active-request SLEEP_LAY candidates: {len(sleep_candidates)}")
    for row in window_report:
        coverage_text = (
            f", sum of per-episode unique states={row['sum_episode_unique_states_visited']}, "
            f"sum of per-episode state-action pairs={row['sum_episode_unique_state_action_pairs']}, "
            f"fallback rate={row['fallback_decision_rate']:.2%}"
            if row["q_table_coverage_fields_available"] else
            f", sum of per-episode unique states={row['sum_episode_unique_states_visited']}, "
            f"sum of per-episode state-action pairs={row['sum_episode_unique_state_action_pairs']}, "
            "Q coverage unavailable"
        )
        print(
            f"{row['agent'].title()} {row['window']} window "
            f"(episodes {row['start_episode']}-{row['end_episode']}): "
            f"mean={row['mean_total_reward']:.3f}, SD={row['std_total_reward']:.3f}"
            f"{coverage_text}"
        )
    print("  final_episode_behaviour_trace.png")
    print("  final_episode_behaviour_trace.dot")
    print(f"Final training episode traced: {final_episode}")
    for agent in AGENTS:
        stats = coverage.get(agent, {})
        print(
            f"{agent.title()}: {stats.get('q_table_supported_decisions', 0)}/"
            f"{stats.get('decisions', 0)} training decisions had stored valid Q-values; "
            f"{len(state_counts.get(agent, {}))} unique states appear in retained final-episode logs"
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"visualization error: {exc}", file=sys.stderr)
        raise SystemExit(1)
