# Fetch-object preference bias

**Status:** Implemented on `Dev`; local build and runtime verification pending  
**Guided script version:** 2  
**Scope:** Proposed-agent memory and guided simulation

## Purpose

A single successful FETCH does not demonstrate a preference. The companion must choose among multiple available objects so a remembered user preference can bias an observable choice.

## Guided interaction

The yard presents three fetch objects: a ball, a stick, and a rope toy. The guided sequence retains the earlier ball-throw interaction, then replaces the repeated ball-throw probe with:
1. The participant states a toy preference (fixed prompt: “I prefer the rope toy.”).
2. All three objects are placed in the yard.
3. The participant asks the companion to fetch a toy.
4. The visible companion chooses, retrieves, carries, and returns one object.

## Behaviour

- The proposed agent records explicit toy preferences as user-scoped semantic memory, e.g. `user likes rope`.
- On FETCH, the proposed agent selects the preferred object among `ball`, `stick`, and `rope`.
- With no relevant preference, the neutral default is ball.
- If the participant explicitly dislikes ball and has no positive toy preference, stick is selected.
- The baseline does not receive or consult memory and continues to fetch ball.
- This is an object-selection bias only. It does not add actions to the Q-learning action vocabulary or change FETCH reward semantics.
- The selected object is logged in the interaction event.

## Tests

The training self-test checks:
- no preference → ball;
- `likes rope` → rope;
- `likes stick` → stick;
- `dislikes ball` → stick.

The guided protocol matcher accepts the stated rope preference and the fetch-toy interaction. Full scene pickup/carry/drop behaviour still requires a local interactive simulation check.

## Non-goals

- Inferring preference from one unlabelled fetch.
- Changing Q-learning, action vocabulary, reward weights, or synthetic-user response logic.
- Changing participant deployment, production, or questionnaire scoring.
