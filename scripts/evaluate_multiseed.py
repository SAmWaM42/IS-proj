#!/usr/bin/env python3
"""Reproducible multi-seed train/held-out evaluation for IS-2.

The reported oracle agreement is explicitly a one-step contextual diagnostic,
not a claim of optimality for the full multi-step environment.
"""
import argparse
import json
import shutil
import statistics
import subprocess
from pathlib import Path


def mean_sd(values):
    values = [float(v) for v in values]
    if not values:
        return {"mean": None, "sample_sd": None, "n": 0}
    return {
        "mean": statistics.mean(values),
        "sample_sd": statistics.stdev(values) if len(values) > 1 else 0.0,
        "n": len(values),
    }


def read_json(path):
    with path.open(encoding="utf-8") as stream:
        return json.load(stream)


def run(binary, mode, episodes, steps, seed, out_dir):
    command = [
        str(binary), mode, "--episodes", str(episodes), "--steps", str(steps),
        "--seed", str(seed), "--out", str(out_dir),
    ]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True)
    return read_json(out_dir / f"{mode[2:]}_summary.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="build/IS-2")
    parser.add_argument("--out", default="artifacts/multiseed-evaluation")
    parser.add_argument("--first-seed", type=int, default=42)
    parser.add_argument("--seeds", type=int, default=10)
    parser.add_argument("--episodes", type=int, default=200)
    parser.add_argument("--eval-episodes", type=int, default=40)
    parser.add_argument("--steps", type=int, default=40)
    args = parser.parse_args()

    if args.seeds < 2:
        parser.error("--seeds must be at least 2 to report sample standard deviation")
    if min(args.episodes, args.eval_episodes, args.steps) < 1:
        parser.error("episode and step counts must be positive")

    binary = Path(args.binary).resolve()
    if not binary.is_file():
        parser.error(f"binary does not exist: {binary}")
    root = Path(args.out).resolve()
    root.mkdir(parents=True, exist_ok=True)

    train_summaries = []
    held_out_summaries = []
    distribution_summaries = []
    seeds = list(range(args.first_seed, args.first_seed + args.seeds))

    for seed in seeds:
        seed_dir = root / f"seed-{seed}"
        if seed_dir.exists():
            shutil.rmtree(seed_dir)
        seed_dir.mkdir(parents=True)
        train = run(binary, "--train", args.episodes, args.steps, seed, seed_dir)
        # Evaluation loads the saved policies, disables Q-table updates and exploration while allowing proposed-agent memory/affect to adapt online, and
        # uses the held-out state-combination generator and a separate RNG seed.
        evaluation_seed = seed + 100000
        held_out = run(binary, "--validate", args.eval_episodes, args.steps,
                       evaluation_seed, seed_dir)
        # Evaluate the same saved policies on fresh scenarios sampled from the
        # training distribution. This is separate from held-out stress testing.
        distribution = run(binary, "--validate-distribution", args.eval_episodes,
                           args.steps, evaluation_seed + 100000, seed_dir)
        train_summaries.append(train)
        held_out_summaries.append(held_out)
        distribution_summaries.append(distribution)

    agents = ("baseline", "proposed")
    metrics = (
        "mean_reward_per_step",
        "completion_rate",
        "diagnostic_one_step_oracle_agreement_rate",
    )
    aggregate = {
        "schema_version": 1,
        "seeds": seeds,
        "training_episodes_per_seed": args.episodes,
        "held_out_stress_evaluation_episodes_per_seed": args.eval_episodes,
        "in_distribution_evaluation_episodes_per_seed": args.eval_episodes,
        "steps_per_episode": args.steps,
        "evaluation_description": (
            "Each seed trains policies independently. Both evaluation strata freeze Q-table "
            "updates and exploration while proposed-agent memory/affect continue adapting online. "
            "held_out_stress uses unseen joint observation combinations; in_distribution samples "
            "the shuffled training scenario catalogue with independent evaluation seeds."
        ),
        "results": {},
    }

    for agent in agents:
        aggregate["results"][agent] = {}
        for metric in metrics:
            if metric == "diagnostic_one_step_oracle_agreement_rate":
                def get_metric(summary):
                    return summary[agent].get(
                        metric, summary[agent].get("one_step_oracle_agreement_rate", 0.0))
            else:
                def get_metric(summary):
                    return summary[agent].get(metric, 0.0)
            aggregate["results"][agent][metric] = {
                "training": mean_sd(get_metric(summary) for summary in train_summaries),
                "held_out_stress": mean_sd(get_metric(summary) for summary in held_out_summaries),
                "in_distribution": mean_sd(get_metric(summary) for summary in distribution_summaries),
            }

    # Per-episode reward curves aligned by episode index across independent seeds.
    learning_curves = {"schema_version": 1, "seeds": seeds, "agents": {}}
    for agent in agents:
        curve = []
        for episode_index in range(args.episodes):
            values = []
            for summary in train_summaries:
                episodes = summary[agent].get("episodes", [])
                if episode_index < len(episodes):
                    row = episodes[episode_index]
                    values.append(row.get("mean_reward_per_step", 0.0))
            curve.append({"episode": episode_index, **mean_sd(values)})
        learning_curves["agents"][agent] = curve

    validation_paragraph = (
        "Evaluation reports two distinct strata across independent seeds. Held-out stress "
        "evaluation uses joint observation combinations absent from the training catalogue; "
        "in-distribution evaluation samples the training scenario catalogue using independent "
        "evaluation seeds. Both freeze Q-table updates and exploration, while allowing proposed-agent "
        "memory and affect to adapt online. Mean reward per step summarises the synthetic interaction "
        "objective; completion rate checks task outcomes; learning curves show changes during training. "
        "The diagnostic_one_step_oracle_agreement_rate is agreement with the exact Q* oracle for a "
        "separate one-step diagnostic MDP whose next state is terminal. It is not optimal-policy "
        "agreement for the full multi-step environment. These synthetic metrics validate implementation "
        "and policy learning only, not perceived believability, which requires separate participant "
        "ratings and behavioural analysis."
    )
    aggregate["justified_validation_approach"] = validation_paragraph

    summary_path = root / "multiseed_summary.json"
    curve_path = root / "learning_curves.json"
    summary_path.write_text(json.dumps(aggregate, indent=2) + "\n", encoding="utf-8")
    curve_path.write_text(json.dumps(learning_curves, indent=2) + "\n", encoding="utf-8")
    (root / "validation-approach.txt").write_text(validation_paragraph + "\n", encoding="utf-8")

    print("\nHeld-out results (mean ± sample SD across seeds):")
    for agent in agents:
        print(agent)
        for metric in metrics:
            for stratum in ("held_out_stress", "in_distribution"):
                row = aggregate["results"][agent][metric][stratum]
                print(f"  {stratum} {metric}: {row['mean']:.4f} ± {row['sample_sd']:.4f} (n={row['n']})")
    print(f"\nWrote {summary_path}")
    print(f"Wrote {curve_path}")
    print(f"Wrote {root / 'validation-approach.txt'}")


if __name__ == "__main__":
    main()
