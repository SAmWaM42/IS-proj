# Project Hand-Off: Memory- and Affect-Augmented Companion Agent

**Document type:** Development hand-off and specification baseline  
**Project stage:** Architecture and initial implementation  
**Development approach:** Specification-driven development (SDD)  
**Primary language:** C++  
**NLP component:** Python + spaCy  
**Simulation:** SFML (planned/intended)  
**Persistence:** JSON using `nlohmann/json`  
**Learning approach:** Tabular Q-learning  
**Status:** Initial architecture defined; implementation details and interfaces require validation against the existing codebase.

---

## 1. Project Overview

### 1.1 Research context

The project investigates how persistent semantic memory and affective-state modelling influence the perceived believability of a simulated companion agent.

The intended contribution is an experimental comparison between:

1. **Baseline agent:** A relatively simple learning agent that uses environmental observations to select actions, without the proposed semantic-memory and affective-state augmentation.
2. **Proposed agent:** An agent that incorporates persistent semantic memory, affective states, and memory-derived information into its decision-making process.

Both agents should operate within the same simulation environment and be evaluated under comparable interaction conditions.

The objective is not to develop a general-purpose conversational AI or compete with large language model-based companion systems. The project investigates whether a bounded, explicitly modelled internal state can produce meaningful differences in agent behaviour and perceived believability.

### 1.2 Core research question

How does integrating persistent semantic memory and affective-state modelling into a learning-based companion agent influence its perceived believability, behavioural consistency, and adaptability over repeated interactions?

### 1.3 Development principle

The project should prioritise a working, testable research prototype over architectural complexity.

Every major component must have a defined responsibility, explicit inputs and outputs, and acceptance criteria. Features should only be implemented when they support the experimental objectives or are necessary for the system to function.

---

## 2. Development Rules

The development process must follow specification-driven development rather than unstructured implementation.

### 2.1 Required workflow

For each feature:

1. Define the problem the feature solves.
2. Write a specification describing expected behaviour.
3. Define interfaces, data structures, constraints, and acceptance criteria.
4. Identify dependencies and integration requirements.
5. Review the specification before implementation.
6. Implement the smallest complete increment.
7. Test against the acceptance criteria.
8. Update documentation and record any architectural changes.

The implementation must not silently introduce features or architectural decisions that have not been justified by the specification.

### 2.2 AI-assisted development rules

Any AI coding assistant used on this project must:

- Inspect the existing codebase before proposing modifications.
- Read the relevant specifications and existing interfaces before writing code.
- Avoid rewriting unrelated components.
- Prefer minimal, incremental changes.
- Explain significant design decisions and trade-offs.
- Identify assumptions when requirements are ambiguous.
- Provide tests or reproducible verification steps for each implementation increment.
- Report which files were modified and whether tests passed.
- Never claim that a component works without evidence from a test or execution.
- Avoid introducing additional libraries, frameworks, or architectural patterns without justification.

When an essential requirement is ambiguous, ask for clarification rather than inventing a requirement.

### 2.3 User's preferred development approach

The project owner prefers conceptual explanations, design feedback, and guided debugging rather than receiving large amounts of unexplained copy-paste code.

AI assistance should support understanding and implementation rather than obscure the architecture behind generated code.

---

## 3. Scope and Constraints

### 3.1 In scope

- A simulated companion with a bounded action space.
- A common simulation environment for both agents.
- Environmental state representation.
- Affective-state modelling.
- Persistent semantic memory.
- Natural-language preprocessing.
- Memory retrieval and semantic fact representation.
- State construction and memory-derived features.
- Tabular Q-learning and exploration.
- Initial Q-table loading or experience-based initialisation.
- JSON persistence.
- A controlled comparison between the baseline and proposed agents.
- Collection of behavioural and user-evaluation data.

### 3.2 Out of scope unless explicitly approved

- Training a large language model from scratch.
- Developing a general-purpose semantic understanding system.
- Building a physical robot.
- Requiring cloud-based NLP processing.
- Implementing an unnecessarily complex distributed architecture.
- Using neural reinforcement learning before the suitability of tabular Q-learning has been evaluated.
- Adding advanced features that do not contribute to the research objectives.

### 3.3 Time constraint

Development must proceed in small, independently testable increments. A complete, reproducible experimental prototype takes priority over optional functionality.

---

## 4. Proposed System Architecture

The system should separate interaction processing, memory, affect, state construction, learning, and simulation.

```text
                     User
                       |
                       v
              Interaction Interface
                       |
                       v
                Interaction Manager
                       |
                       v
                   NLPClient
                       |
                  IPC / stdin-out
                       |
                       v
                Python NLP Process
                 Python + spaCy
                       |
                 Structured Result
                       |
                       v
                 Memory Parser
                       |
             +---------+----------+
             |                    |
             v                    v
        Memory Manager      Affective State
             |                    |
             +---------+----------+
                       |
                       v
                 State Builder
                       |
                       v
                  QLearning
                       |
                       v
                    Action
                       |
                       v
              Simulation Environment
                       |
                       v
                Outcome / Reward
                       |
             +---------+----------+
             |                    |
             v                    v
        Q-table Update      Memory Update
```

This diagram represents the proposed logical architecture, not a final implementation contract. The precise ordering of affect updates, memory writes, and action execution must be established in the relevant specifications.

### 4.1 Architectural boundaries

**C++ application**
- Manages the companion agent, simulation, memory, affective state, and Q-learning.
- Communicates with the NLP process.
- Owns the learning and decision-making logic.

**Python NLP process**
- Loads spaCy and its language model once at startup.
- Accepts text-processing requests.
- Returns structured linguistic information.
- Does not select companion behaviours or modify the Q-table.

**Memory subsystem**
- Stores episodic experiences and semantic information.
- Retrieves records relevant to the current context.
- Updates confidence, applicability, and generalisation as evidence accumulates.

**Affective subsystem**
- Maintains the agent's internal affective variables.
- Updates them in response to interactions and relevant experiences.
- Exposes a structured representation for state construction.

**Q-learning subsystem**
- Selects actions based on a supplied state.
- Updates action values from experience.
- Does not directly parse natural language, query spaCy, or execute simulation actions.

**Simulation environment**
- Provides environmental observations.
- Executes selected actions.
- Reports outcomes and relevant reward information.
- Uses the same external environment for both experimental agents.

---

## 5. Component Specifications

### 5.1 NLP service and NLPClient

**Purpose:** Convert natural-language input into structured linguistic information that the C++ application can process.

The current design uses a persistent Python subprocess instead of starting Python for every request.

#### Responsibilities

- Start the Python process using the project's virtual-environment interpreter.
- Ensure the NLP model is loaded once.
- Send input text to the process.
- Receive and validate structured results.
- Handle malformed responses, process termination, and communication failures.
- Shut down the subprocess cleanly.

#### Proposed interface

```cpp
class NLPClient
{
public:
    bool start();

    NLPResult query(const std::string& text);

    void stop();
};
```

This is a proposed interface, not implemented or verified code.

#### NLP output

The Python process should initially return:

- Token text.
- Part-of-speech tag.
- Lemma.
- Dependency relation.
- Head token.

Example conceptual response:

```json
{
  "tokens": [
    {
      "text": "like",
      "pos": "VERB",
      "lemma": "like",
      "dependency": "ROOT",
      "head": "like"
    }
  ]
}
```

The example is illustrative; the final response schema must support multiple tokens and include the information required by the parser.

#### Communication constraints

- Define a request/response protocol before implementation.
- Ensure each request can be matched to its corresponding response.
- Prefer a newline-delimited JSON protocol if using stdin/stdout.
- Reserve stdout for machine-readable responses; send diagnostics to stderr.
- Account for missing or malformed responses.
- Use explicit executable and script paths instead of relying on the caller's working directory.
- Do not require virtual-environment activation when invoking its Python executable directly.

#### Acceptance criteria

- The process starts using the intended virtual environment.
- The NLP model loads successfully.
- Multiple requests can be processed without restarting the process.
- Results conform to the defined schema.
- An invalid request does not silently corrupt subsequent responses.
- Shutdown does not leave an unintended child process running.

---

### 5.2 Semantic memory

**Purpose:** Represent persistent facts, preferences, and behavioural patterns derived from interactions.

The initial memory model uses structured facts rather than relying exclusively on unstructured text.

#### Proposed fact representation

```cpp
struct Fact
{
    std::string subject;
    std::string relation;
    std::string object;
};
```

#### Proposed memory record

```cpp
class MemoryRecord
{
public:
    Fact raw_fact;

    float confidence;
    float generalization_factor;

    std::vector<std::string> relevant_users;

    enum UserContext
    {
        GENERAL,
        USER_SPECIFIC
    };

    UserContext current_context;
};
```

The actual class design may change after reviewing existing code.

#### Important modelling distinction

- **Confidence:** How strongly the available evidence supports a fact.
- **Generalisation factor:** How broadly the fact is believed to apply.
- **User context:** Whether the fact is general or tied to particular users.

These concepts must not be treated as interchangeable.

A future version may require evidence counts, timestamps, exclusions, or salience. Those fields should only be added when their update and retrieval semantics have been specified.

#### Illustrative example

```text
Fact: user_A likes playing
Confidence: 0.72
Generalisation: 0.15
Context: USER_SPECIFIC
```

After accumulating evidence across interactions and users, the system may derive a more general pattern. Contradictory evidence must remain representable rather than being discarded simply because a general pattern has emerged.

#### Acceptance criteria

- Facts can be represented independently of their original sentence.
- Records can be serialised and restored.
- Confidence and generalisation can be updated according to explicit rules.
- Retrieval can distinguish general facts from user-specific facts.
- Contradictory evidence does not automatically overwrite existing information without a defined policy.

---

### 5.3 Memory parser and retrieval

**Purpose:** Convert linguistic output into candidate semantic facts and retrieve records relevant to the current interaction.

#### Responsibilities

- Receive structured NLP results.
- Identify candidate subjects, relations, and objects.
- Associate candidate facts with user context.
- Retrieve potentially relevant memory records.
- Filter records using context, confidence, and applicability.
- Convert selected memory information into structured features usable by the state builder.

POS tagging and dependency parsing provide linguistic information; they do not guarantee correct semantic interpretation. Extraction rules must therefore be tested against representative inputs.

#### Acceptance criteria

- The parser accepts valid NLP output.
- Candidate facts use the agreed fact schema.
- Irrelevant records can be excluded from retrieval.
- The state builder receives structured features rather than raw memory text.
- Unsupported or ambiguous input can be reported as unresolved instead of generating a fabricated fact.

---

### 5.4 Affective state

**Purpose:** Maintain the agent's internal affective variables and expose them to the decision-making system.

The currently proposed variables are:

- Attachment.
- Stress.
- Energy.
- Curiosity.

#### Requirements

- Each variable must have a defined representation and valid range.
- Updates must be associated with explicit interactions, experiences, or environmental conditions.
- Update rules must be deterministic where appropriate and testable.
- Affective values must be accessible to the state builder.
- Baseline behaviour must not accidentally receive the proposed agent's affective inputs.

The update equations, initial values, decay rules, and action-specific effects remain to be specified.

---

### 5.5 State representation and StateBuilder

**Purpose:** Construct the state used by Q-learning.

The proposed agent's state should incorporate:

1. Relevant environmental observations.
2. Affective-state features.
3. Memory-derived features.

Conceptually:

```text
AgentState =
    EnvironmentFeatures
    + AffectiveFeatures
    + MemoryFeatures
```

The baseline state should use the common environmental features without the proposed memory and affective augmentation.

#### Critical design decision

Tabular Q-learning requires a discrete state representation. Continuous variables must therefore be discretised or mapped into a finite set of state identifiers.

Before implementation, specify:

- The environmental variables and their possible values.
- The affective variables and their discretisation.
- The memory features included in the state.
- How retrieved records become numerical or categorical features.
- How equivalent states receive the same identifier.
- How unseen states are represented.
- How state-space growth will be controlled.

The state space should be deliberately bounded. Do not encode every memory record or arbitrary sentence directly into the Q-table key.

---

### 5.6 Q-learning

**Purpose:** Select actions from the supplied state and update learned action values using observed experience.

#### Proposed class separation

```text
QLearning
    |
    +-- action selection
    +-- exploration policy
    +-- Q-learning update
    +-- learning parameters
    |
    +-- QTable
           |
           +-- state/action values
           +-- value lookup
           +-- value modification
           +-- persistence interface
```

#### Proposed conceptual interfaces

```cpp
class QTable
{
    // State/action value storage and access
};

class QLearning
{
public:
    Action chooseAction(const State& state);

    void update(
        const State& current_state,
        const Action& action,
        float reward,
        const State& next_state
    );
};
```

These are interface sketches, not final declarations. Exact types depend on the state and action specifications.

#### Responsibilities

`QLearning` should:
- Select an action from the current state.
- Implement the chosen exploration policy.
- Apply the Q-learning update rule.
- Use the supplied reward and next state.
- Expose appropriate configuration for learning rate, discount factor, and exploration.

`QTable` should:
- Store action values for discrete states.
- Provide lookup and modification operations.
- Handle unseen states according to an explicit policy.
- Support saving and loading values.

The Q-learning component must not know what a memory record or affective variable means. Those features belong to state construction.

#### Acceptance criteria

- The action selector returns a valid action.
- Exploration and exploitation follow the configured policy.
- Updates use the current state, selected action, reward, and next state.
- Terminal transitions are handled correctly.
- Unseen states do not cause undefined behaviour.
- Q-table persistence preserves values across reloads.

---

### 5.7 Simulation environment

**Purpose:** Provide a controlled environment for agent interaction and evaluation.

SFML is intended to provide graphics, input, timing, and the simulation interface. The learning logic remains independent of SFML rendering code.

#### Requirements

- Represent environmental state independently of visual rendering.
- Define the legal actions available to the agent.
- Execute actions and report their outcomes.
- Provide consistent interaction conditions for both agents.
- Support reproducible experiments where practical.
- Avoid placing memory retrieval or Q-learning logic inside rendering code.

The action space, environment variables, transition rules, and reward design must be documented before training.

---

## 6. Training and Pretraining Strategy

The state space may be too large for practical learning through random exploration alone.

The proposed strategy is to use prior interaction experience to initialise behavioural knowledge, followed by further learning in the simulation.

### 6.1 Initial experience dataset

An existing or modified agent may be used to generate interaction records.

Each record should contain sufficient information to reconstruct an experience, such as:

- State or observation.
- Selected action.
- Reward or feedback.
- Next state.
- Terminal indicator, where applicable.

The dataset must be converted into the state/action representation used by the experimental agent.

### 6.2 Initialisation options

Investigate the following in order of simplicity:

1. **Experience-based Q-learning:** Replay recorded transitions through the Q-learning update rule.
2. **Precomputed Q-table:** Load values generated by a compatible training process.
3. **Policy-based initialisation:** Only if the first two approaches prove unsuitable and the additional complexity is justified.

A dataset of actions alone is not sufficient to perform ordinary Q-learning updates; transition and reward information are needed.

### 6.3 Experimental fairness

Both agents should start with comparable baseline behavioural knowledge wherever possible.

A preferred experiment is:

```text
             Shared initial experience
                       |
                 Initial learning
                       |
                +------+------+
                |             |
                v             v
            Baseline       Proposed
                |             |
                +------+------+
                       |
              Same simulation setup
                       |
              Continued interaction
                       |
              Behavioural comparison
```

The baseline must not receive memory or affective features through the training pipeline if those are intended to be the independent variables.

Document the source model, dataset generation method, reward assumptions, preprocessing, and any mismatch between the source model and experimental agents.

---

## 7. Persistence and Data Management

Use JSON persistence initially, subject to performance and data-volume testing.

Proposed persistent data categories:

- Semantic memory records.
- Episodic interaction records.
- Action history, where required.
- Q-table data.
- Experiment and evaluation records.

### Requirements

- Define a schema for each persisted data type.
- Validate data when loading.
- Handle missing, malformed, or incompatible files.
- Avoid silently replacing corrupted data with empty state.
- Separate experimental results from mutable learning data.
- Provide reproducible initial conditions for evaluation.
- Document schema changes and migration assumptions.

The memory system and Q-table should have separate responsibilities and persistence interfaces.

---

## 8. Experimental Evaluation

The prototype must support a controlled comparison between the baseline and proposed agents.

### 8.1 Independent variable

The presence of the proposed memory and affective-state augmentation.

### 8.2 Outcome measures

- Perceived believability.
- Behavioural consistency.
- Adaptability over repeated interactions.
- User engagement, if it can be measured within the approved study design.

### 8.3 Experimental controls

- Same simulation environment.
- Same available action space, where applicable.
- Equivalent initial behavioural knowledge.
- Comparable interaction opportunities.
- Documented training conditions.
- Repeated runs or equivalent trials where practical.
- Clear separation of training and evaluation data.

The evaluation must distinguish observable behavioural differences from subjective user ratings.

The final experimental protocol, participant procedure, and measurement instruments require separate specifications and any applicable institutional approval.

---

## 9. Implementation Roadmap

Implement one vertical slice at a time. The order below reflects dependencies and may be adjusted after auditing the current repository.

### Phase 0 — Repository audit

- [ ] Inspect existing source files, build files, and directory structure.
- [ ] Identify implemented components and incomplete declarations.
- [ ] Record current compiler, C++ standard, and dependency versions.
- [ ] Confirm SFML setup and Python virtual-environment location.
- [ ] Identify current tests and known failures.
- [ ] Produce an implementation-status report without rewriting code.

**Exit criterion:** The current implementation is understood, and existing functionality is distinguished from planned functionality.

### Phase 1 — Define contracts and shared types

- [ ] Define the action type and legal action set.
- [ ] Define environmental observations.
- [ ] Define the state representation.
- [ ] Define state discretisation and state identifiers.
- [ ] Define reward and terminal-transition conventions.
- [ ] Define the NLP request/response schema.
- [ ] Define memory-record persistence schemas.

**Exit criterion:** Components can be implemented against stable interfaces.

### Phase 2 — Establish the NLP service

- [ ] Implement the Python tokenizer and linguistic preprocessing.
- [ ] Implement the persistent Python request loop.
- [ ] Define and test the communication protocol.
- [ ] Implement the C++ NLPClient lifecycle.
- [ ] Test repeated requests, malformed input, and shutdown.
- [ ] Confirm that the process uses the intended virtual environment.

**Exit criterion:** C++ can reliably submit text and receive structured NLP results.

### Phase 3 — Implement memory

- [ ] Implement the fact and memory-record structures.
- [ ] Implement JSON serialisation and deserialisation.
- [ ] Define confidence-update rules.
- [ ] Define generalisation rules.
- [ ] Implement contextual retrieval.
- [ ] Implement the initial semantic parser.
- [ ] Convert retrieved information into bounded memory features.

**Exit criterion:** Representative inputs can produce validated facts, persist them, and retrieve relevant records.

### Phase 4 — Implement state and affect

- [ ] Implement the affective-state representation.
- [ ] Define and implement affect update rules.
- [ ] Implement environmental-state extraction.
- [ ] Implement state construction.
- [ ] Implement discretisation and stable state identifiers.
- [ ] Verify that memory and affect features are absent from the baseline state.

**Exit criterion:** Given the same environment observation, each agent produces a valid state representation according to its specification.

### Phase 5 — Implement Q-learning

- [ ] Implement QTable.
- [ ] Implement action selection.
- [ ] Implement the exploration policy.
- [ ] Implement the Q-learning update rule.
- [ ] Define terminal-state handling.
- [ ] Implement Q-table persistence.
- [ ] Test the update rule against hand-calculated examples.

**Exit criterion:** Q-learning passes deterministic unit tests and can learn in a small test environment.

### Phase 6 — Integrate the simulation

- [ ] Implement or complete the SFML simulation.
- [ ] Connect environmental observations to state construction.
- [ ] Connect selected actions to environment execution.
- [ ] Return rewards and next states to Q-learning.
- [ ] Add the baseline agent.
- [ ] Add the proposed agent.
- [ ] Verify equivalent environment and interaction conditions.

**Exit criterion:** Both agents complete an end-to-end interaction cycle.

### Phase 7 — Training and evaluation

- [ ] Define the initial experience dataset.
- [ ] Implement experience-based initialisation or the chosen alternative.
- [ ] Validate training data compatibility.
- [ ] Run baseline and proposed-agent trials.
- [ ] Record interaction and behavioural metrics.
- [ ] Collect believability evaluation data under the approved protocol.
- [ ] Analyse results and document limitations.

**Exit criterion:** The comparison can be reproduced and the collected data supports the planned analysis.

### Phase 8 — Finalisation

- [ ] Run regression tests.
- [ ] Verify clean startup and shutdown.
- [ ] Document installation and execution.
- [ ] Document configuration and data formats.
- [ ] Complete architecture and implementation documentation.
- [ ] Record limitations and future extensions.

**Exit criterion:** The system can be built, run, evaluated, and explained independently of the development session.

---

## 10. Required Specifications

Maintain a separate specification for each major subsystem.

Recommended structure:

```text
specs/
├── 00-project-scope.md
├── 01-system-architecture.md
├── 02-shared-types-and-state.md
├── 03-nlp-service.md
├── 04-semantic-memory.md
├── 05-affective-state.md
├── 06-state-builder.md
├── 07-q-learning.md
├── 08-simulation-environment.md
├── 09-training-and-pretraining.md
├── 10-experimental-evaluation.md
└── 11-build-and-deployment.md
```

Every feature specification should contain:

1. Purpose.
2. Scope and non-goals.
3. Dependencies.
4. Data structures and interfaces.
5. Required behaviour.
6. Error handling.
7. Acceptance criteria.
8. Tests.
9. Known limitations.
10. Implementation status.

Specifications are the source of truth for intended behaviour. If the code and specification disagree, record the discrepancy and resolve it deliberately.

---

## 11. Immediate Priorities

The next development session should not begin by implementing every class.

First:

1. Audit the existing repository.
2. Identify which proposed components already exist.
3. Freeze the minimum viable action space and state representation.
4. Specify the Q-learning interface and state identifier strategy.
5. Specify the persistent NLP process protocol.
6. Choose one small end-to-end feature and implement it completely.

A useful first vertical slice is:

```text
Input text
    ↓
C++ NLPClient
    ↓
Persistent Python process
    ↓
Structured NLP result
    ↓
C++ receives and validates result
    ↓
Test output
```

This validates the cross-language boundary before memory, affect, and learning depend on it.

The next vertical slice can then be:

```text
Known state
    ↓
Q-learning action selection
    ↓
Environment action
    ↓
Reward + next state
    ↓
Q-table update
```

These two slices can be tested independently before being integrated.

---

## 12. Open Decisions and Risks

The following decisions must remain explicit until resolved.

| Decision | Why it matters |
|---|---|
| Exact discrete state representation | Determines Q-table size and feasibility |
| Memory feature encoding | Prevents raw text or unbounded records from exploding the state space |
| Action space and reward function | Determines what the agent can learn |
| Initialisation from external experience | Depends on dataset compatibility and reward availability |
| Memory generalisation rules | Determines when user-specific facts become broader patterns |
| Affect update rules | Determines how internal state changes behaviour |
| NLP communication protocol | Affects reliability and integration complexity |
| Synchronous versus asynchronous queries | Depends on measured processing time and simulation responsiveness |
| Experimental controls | Prevents differences from being attributed to the wrong feature |
| User-evaluation protocol | Determines how perceived believability can be assessed credibly |

Do not attempt to solve all of these at once. Resolve each decision when its dependent component is next in the implementation sequence.

---

## 13. Definition of Done

A component is complete only when:

- Its specification is approved.
- Its public interface is documented.
- Its required behaviour is implemented.
- Acceptance tests pass.
- Error cases are handled according to specification.
- Integration assumptions are documented.
- No unrelated functionality has been introduced.
- The implementation status and any remaining limitations are recorded.

The overall project is complete when the baseline and proposed agents can run under comparable conditions, the proposed architecture demonstrably incorporates memory and affective state, and the evaluation produces reproducible data relevant to the research question.

---

## 14. Instructions for the Next Development Session

Start by auditing the existing repository rather than generating a new implementation from this document.

Produce:

1. A directory and component inventory.
2. A table mapping each proposed component to its current implementation status.
3. A list of interface mismatches and build issues.
4. A list of decisions that genuinely block implementation.
5. A prioritised list of small implementation tasks with acceptance criteria.

Do not rewrite the system or generate every class at once. Use the specifications to guide incremental development and validate each increment before moving on.

**Primary objective:** Deliver the smallest complete, scientifically defensible companion-agent experiment within the available time, without sacrificing architectural clarity or the ability to explain the implementation.
