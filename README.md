# Memory System

## Overview

The memory component is responsible for storing, loading, updating, retrieving, and processing information that the companion agent has acquired during interactions and simulation.

The system is designed to separate **raw memories** from the smaller set of information that is relevant to the agent's current decision.

The general flow is:

```text
User Input
    ↓
NLP Processing
    ↓
Input Parser
    ↓
Parsed Input
    ↓
Memory Component
    ├── Store / Update Memory
    ├── Retrieve Relevant Records
    └── Extract Relevant Features
              ↓
        Agent / RL State
```

The memory system is therefore not intended to directly control the agent. Instead, it provides information that can be used by the agent's decision-making system.

---

# Components

## `fact`

```cpp
typedef struct {
    std::string subject;
    std::string relation;
    std::string object;
} fact;
```

Represents the basic semantic relationship contained within a piece of information.

A fact follows the general structure:

```text
subject → relation → object
```

For example:

```text
user → likes → fetch
```

or:

```text
agent → observed → user_playing_fetch
```

The `fact` structure represents the actual information, while a `record` contains additional metadata about that information.

---

# `record`

```cpp
class record
{
public:
    fact raw_fact;
    float confidence;
    float generalization_factor;
    std::vector<std::string> relevant_users;

    enum user_context
    {
        GENERAL,
        USER_SPECIFIC
    };

    user_context current_context;

    record();
    void update_record();
};
```

A `record` represents a piece of information stored in the agent's long-term memory.

Unlike a `fact`, a record contains additional information describing how reliable, general, and context-dependent the memory is.

### Fields

### `raw_fact`

Contains the actual information being remembered.

Example:

```text
user → likes → fetch
```

### `confidence`

Represents how confident the system is that the stored information is accurate.

Confidence can potentially be updated when the same information is observed or stated again.

For example:

```text
Initial observation:
    "I like fetch."

confidence = 0.6

Repeated confirmation:
    "I really like playing fetch."

confidence = 0.85
```

The exact confidence update mechanism is determined by `update_record()`.

### `generalization_factor`

Represents how broadly the memory can be applied.

A highly specific memory might describe one particular user or situation:

```text
User likes playing fetch after work.
```

A generalized memory could become:

```text
User enjoys play after periods of inactivity.
```

The generalization factor allows the memory system to distinguish between highly specific memories and information that may be applicable in a broader context.

### `relevant_users`

Contains users to whom the record applies.

This allows the memory system to distinguish between:

```text
General knowledge
```

and:

```text
Information specific to a particular user
```

### `user_context`

Determines whether a record is general or user-specific.

```cpp
GENERAL
USER_SPECIFIC
```

For example:

```text
GENERAL:
    Dogs generally respond positively to play.

USER_SPECIFIC:
    User A enjoys playing fetch.
```

---

# `action_log`

```cpp
class action_log
{
    std::string action_taken;
    std::string time;
    float count;

    action_log();
};
```

`action_log` stores information about actions performed by the agent.

The log can be used as another source of information for the memory system.

For example:

```text
action_taken = "play_fetch"
time = "18:30"
count = 5
```

Repeated actions can potentially be used to identify behavioral patterns.

This is different from a `record` because an action log represents **what happened**, whereas a memory record represents **information derived from observations or interactions**.

---

# `ex_record`

```cpp
typedef struct {
    std::string subject;
    float confidence;
} ex_record;
```

`ex_record` is intended to represent a simplified piece of information extracted from memory.

Its purpose is to provide information that can be passed from the memory system toward the agent's decision-making system.

For example, a collection of detailed records could be processed into a smaller representation such as:

```text
subject = "user_fetch_affinity"
confidence = 0.87
```

This allows the decision-making system to work with relevant information without needing to process every stored memory record.

This structure may be expanded as the memory-to-RL interface is developed.

---

# `cache`

```cpp
class cache
{
    std::vector<ex_record> data_store;
};
```

The cache stores recently or frequently processed memory information.

Its purpose is to reduce unnecessary processing.

Instead of repeatedly extracting the same information from long-term memory, already processed information can be kept in the cache and reused when appropriate.

Conceptually:

```text
Long-Term Memory
       ↓
Memory Processing
       ↓
Relevant Information
       ↓
Cache
       ↓
Agent
```

The cache is therefore an optimization layer rather than the primary source of persistent memory.

---

# `memory_loader`

```cpp
class memory_loader