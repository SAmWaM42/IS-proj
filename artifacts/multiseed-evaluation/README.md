# Multi-seed evaluation artifacts

The JSON summaries and logs currently checked into this directory are a **historical snapshot** from the earlier evaluation design: ten seeds (42–51), 200 training episodes per seed, and 40 held-out episodes per seed using the original four-combination stress suite.

The evaluation code has since been updated to:
- expand the held-out stress suite to 12 joint observation combinations;
- evaluate the same trained policies separately on the training scenario distribution using independent evaluation seeds;
- report the two evaluation strata separately; and
- name the oracle metric `diagnostic_one_step_oracle_agreement_rate` to make its limited scope explicit.

Do not treat the currently checked-in numbers as results from the updated design. After building and testing the branch, regenerate the results with:

```sh
python3 scripts/evaluate_multiseed.py --binary ./build/IS-2 --seeds 10 --episodes 200 --eval-episodes 40 --steps 40 --out artifacts/multiseed-evaluation
```

The script removes and recreates each `seed-N` directory under the chosen output directory. Keep a copy of the existing historical snapshot elsewhere if it must be retained before rerunning the script.
