# Proposed implementation changes (not implemented in this review)

This list is intentionally limited to findings that would require explicit approval before any source or instrumentation change is made.

1. Add runtime export fields for the delayed-INSIST and repetition paths:
   - `no_response_count`
   - `repetition_category`
   - `repetition_consecutive_count`
   - `boredom_penalty`
   - `valid_action_q_values`
   - `final_action_after_overrides`
   - `q_table_has_valid_action_values`
2. Add revision metadata to each exported artifact record:
   - git revision
   - build mode (`research` vs `participant`)
   - executable path or binary hash
   - command line arguments used for the run
3. Add a formal request lifecycle field for persistent escalation and abandonment tracking:
   - `request_state`
   - `escalation_level`
   - `request_started_at`
   - `last_response_at`
   - `abandoned_at`
4. Keep reward/state logic unchanged; only add serialization and provenance metadata.
5. Produce a fresh deterministic run bundle for the current revision before any behavioural change is approved.

These are proposed changes only. No source or deployment files were changed in this review.
