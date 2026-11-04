# Self-Contained Client Packaging Guide
## Packaging the Animal Agent so participants do not install project dependencies

**Targets:** Separate Windows and Linux participant ZIPs  
**Purpose:** A practical release plan for bundling the C++ application, Python NLP runtime, spaCy model, native libraries, data, assets, and launchers.

## 1. Goal and limits

A participant should be able to download the ZIP for their operating system, extract it, and launch the application without installing C++, CMake, Python, spaCy, SFML, or project libraries.

This does not mean bundling an entire operating system. The application will still rely on operating-system facilities such as the kernel, graphics drivers, system services, and a compatible system runtime. The goal is to eliminate **participant-side installation of project dependencies**.

Build a separate archive for each OS and architecture. A Windows package cannot be used on Linux and vice versa.

A release is not complete until it has been tested on a clean machine or VM without the development environment installed.

## 2. Current project architecture

The current design has a C++ application that starts a persistent Python NLP subprocess:

```text
Participant
   |
   v
C++ application
   +-- agent, memory, affect, Q-learning, simulation
   +-- JSON persistence and study configuration
   +-- NLPClient
          |
          v
       Python process
          +-- spaCy
          +-- en_core_web_sm
```

The project uses or intends to use:

- C++ for the main application and agent logic.
- Python + spaCy for linguistic preprocessing.
- `en_core_web_sm` as the spaCy language model.
- SFML for the graphical/simulation layer where linked by the participant target.
- `nlohmann/json` for JSON handling in C++.
- Tabular Q-learning and JSON policy/data files.
- A Python uploader script for queued questionnaire submissions to the Vercel endpoint.

The NLP process is persistent, so packaging must preserve the interpreter path, script path, model location, working directory assumptions, and stdin/stdout protocol.

**Status caveat:** Existing packaging scripts were designed to stage a relocatable Python runtime, spaCy/model files, SFML runtime dependencies, assets, and policy files. That is not proof that the resulting archives work on clean machines. Treat all items below as unverified until tested.

## 3. Dependency inventory

### 3.1 C++ executable and libraries

| Dependency | Purpose | Release requirement |
|---|---|---|
| Participant executable | Main application | Build a Release executable for each target OS |
| C++ runtime | Supports compiled C++ code | Bundle required Windows runtime DLLs where appropriate; Linux needs a compatible OS baseline |
| SFML | Graphics/window/system functions if used | Bundle only the SFML runtime modules actually linked |
| Other shared libraries | Third-party native dependencies | Bundle non-system DLLs/`.so` files required at runtime |
| `nlohmann/json` | JSON parsing/serialisation | Header-only in normal use; usually compiled into the executable |
| Project data | Policies/configuration/assets | Stage every file actually read at runtime |

Do not assume a dependency is included because it existed on the developer's computer. Inspect the executable's runtime dependency graph.

### 3.2 SFML

If the participant executable dynamically links SFML, include the SFML runtime libraries and their non-system dependencies.

Only include modules actually used by the application, such as graphics, window, system, audio, or network. Do not copy all modules by default.

- **Windows:** Place required SFML DLLs beside the executable or in a directory that the launcher adds to the DLL search path. Inspect the SFML DLLs themselves for further dependencies.
- **Linux:** Place required SFML `.so` files and their non-system dependencies in a controlled runtime directory. The launcher can set `LD_LIBRARY_PATH` to that directory.

Do not bundle core OS libraries indiscriminately.

### 3.3 Python runtime: recommended first-release strategy

The participant must not need to install Python. Since C++ currently starts Python as a subprocess, the most direct approach is to bundle a platform-specific Python runtime.

Include:

- a redistributable/relocatable Python runtime;
- the NLP service script;
- spaCy and its runtime dependencies;
- `en_core_web_sm`;
- the uploader script;
- any third-party packages imported by the uploader.

**Do not simply ZIP the developer's `.venv` directory.** Virtual environments commonly contain absolute paths and references to the machine on which they were created. Build a runtime specifically for each target OS, then test it after moving the package.

#### Alternative: freeze the NLP service

PyInstaller or a similar tool can package the NLP service and Python dependencies into a standalone executable.

This may simplify deployment, but spaCy's native extensions and model resources need careful collection and testing. The uploader is a separate Python task unless it is frozen too or replaced. This is a possible later migration, not something to combine with the first release without a deliberate decision.

For the first participant release, a bundled Python runtime is the lower-risk fit for the existing C++-starts-Python architecture.

### 3.4 spaCy and the language model

Installing spaCy alone is insufficient. `en_core_web_sm` is a separate model package/resource.

Use one consistent approach:

1. Install the model into the bundled Python runtime and load it by package name; or
2. Include the model directory and load it through an explicit path.

Do not rely on a model installed globally on the development computer.

At startup, the NLP service should report a clear error if the model is missing or incompatible. Keep machine-readable protocol output on stdout and diagnostics on stderr.

Pin and record the Python, spaCy, model, and other runtime package versions. Create a runtime requirements file containing only packages needed by the participant application. Do not include development-only packages such as test frameworks, notebooks, or linters.

Some Python packages contain compiled native extensions. Build/install them separately for Windows and Linux, matching the Python version and CPU architecture. Do not copy native package files from one OS into the other package.

### 3.5 Uploader dependencies and configuration

Inspect `Components/NLP/upload_submission.py` and list its actual imports. If it uses only the Python standard library, it needs no extra third-party package. If it imports external packages, include them in the bundled runtime.

Include:

- `Components/NLP/upload_submission.py`;
- the packaged Python runtime;
- its imported third-party dependencies, if any;
- `Data/Study/upload-config.json` containing the deployed API endpoint.

Do not include Google OAuth client secrets, refresh tokens, Drive credentials, Vercel secrets, or the server-side participant-token map. Those belong on the server.

The participant's individual upload token must be issued separately from the common ZIP. Do not bake a shared participant token into the archive. Verify that only approved questionnaire response data is uploaded—not raw interaction text, memory traces, affect traces, Q-table data, or diagnostics.

### 3.6 Policies, configuration, fonts, and assets

Audit every file opened by the participant client. The release may need:

- fixed/shared Q-tables or policy files;
- scenario and simulation configuration;
- questionnaire configuration once the in-app questionnaire is complete;
- `upload-config.json`;
- fonts;
- images, textures, icons, audio, and other UI assets;
- other JSON/static files explicitly read at runtime.

Do not copy all repository data blindly. Use an explicit allowlist of runtime files. In particular, exclude obsolete Google Forms configuration if the client no longer uses it.

Treat bundled policies and assets as read-only inputs where appropriate. Participant-specific writable data belongs outside the extracted package.

The intended user-data locations are:

| OS | User-data location |
|---|---|
| Linux | `~/.local/share/animal-agent/` |
| Windows | `%LOCALAPPDATA%\AnimalAgent` |

Memory/affect persistence, local session state, and pending submission queues should use these locations. This avoids writing into protected installation folders and prevents updates from overwriting participant data.

## 4. Suggested package layout

This is a conceptual layout; retain the paths expected by the current code and packaging scripts.

```text
AnimalAgent-Participant/
├── AnimalAgent.exe              # Windows; Linux executable on Linux package
├── launch.bat                   # Windows
├── launch.sh                    # Linux
├── README.txt
├── bin/                         # Optional: native DLLs / shared libraries
├── runtime/
│   └── python/                  # Platform-specific Python runtime
├── Components/
│   └── NLP/
│       ├── nlp_service.py
│       └── upload_submission.py
├── Models/
│   └── en_core_web_sm/          # If model is stored as a separate directory
├── Data/
│   ├── Policies/
│   └── Study/
│       ├── questionnaire.json   # Once implemented
│       ├── scenarios.json       # If used
│       └── upload-config.json
└── Assets/
    ├── Fonts/
    └── ...
```

Do not place participant-writable files here.

## 5. Windows packaging

Build the Windows release on Windows (or a Windows build environment), for the intended architecture—normally x64.

### Step 1: Build the participant executable

- Configure CMake for Release.
- Build only the participant target.
- Exclude research tools, training utilities, tests, and development executables.
- Use explicit runtime output directories.
- Confirm the executable starts in the build environment.

Example command pattern; verify the actual target name in `CMakeLists.txt`:

```powershell
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release --target AnimalAgent
```

### Step 2: Inspect and collect DLL dependencies

Use `dumpbin /DEPENDENTS`, the Dependencies utility, or another PE dependency scanner.

Inspect both the application and every third-party DLL. Collect the SFML DLLs and other non-system DLLs that are required.

Do not copy arbitrary Windows system DLLs. Windows supplies core system libraries.

For the Microsoft Visual C++ runtime, choose a deliberate strategy:
- use a supported app-local runtime deployment where the toolchain permits it; or
- use an installer that installs the Microsoft redistributable.

Because this project aims for a ZIP that does not ask participants to install dependencies, investigate app-local deployment first. Verify redistribution licensing and technical requirements. Do not copy random DLLs from a developer PC.

### Step 3: Assemble Python

- Obtain a Windows x64 Python runtime suitable for redistribution and relocation.
- Install the pinned runtime requirements into it.
- Install the matching `en_core_web_sm` model.
- Test model loading using the packaged interpreter.
- Test both the NLP service and uploader.
- Move the runtime folder and confirm it still works.

### Step 4: Stage data and assets

Copy only participant runtime files: policies, study configuration, questionnaire configuration when ready, upload endpoint configuration, fonts, other assets, and NLP scripts/model resources.

Exclude source-control metadata, development logs, tests, training tools, and participant-specific data.

### Step 5: Create the Windows launcher

The launcher should:

1. Determine its own directory.
2. Set the working directory to the package root.
3. Configure the DLL search path if libraries are in a separate directory.
4. Start the participant executable.
5. Report a useful error if the executable is missing.

Do not rely on the current working directory or a path on the developer's machine.

### Step 6: Clean Windows test

Test the ZIP in a fresh VM matching the minimum supported Windows version, with no Python, spaCy, SFML, CMake, Visual Studio, or repository checkout.

## 6. Linux packaging

Linux portability requires a deliberate minimum supported distribution/version. A binary built on a recent distribution may require newer system libraries than are present on an older participant machine.

### Step 1: Choose the Linux baseline

Document the minimum supported distribution/version and architecture. Build on that baseline or an equivalent environment. Do not assume a binary built on a recent distribution runs on older Linux versions.

### Step 2: Build the participant executable

Example command pattern; verify the actual target name:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target AnimalAgent -j
```

Build the participant target only, in Release mode.

### Step 3: Inspect shared-library dependencies

Use:

```bash
ldd ./AnimalAgent
readelf -d ./AnimalAgent
```

Inspect the executable and each third-party shared library. Bundle required non-system libraries, including SFML libraries where dynamically linked.

**Do not bundle glibc, the Linux dynamic loader, or arbitrary core system libraries.** These are tightly coupled to the host OS. Build against the supported Linux baseline instead.

### Step 4: Assemble Python

Build/assemble a Linux Python runtime for the target architecture and baseline. Install pinned spaCy dependencies and the matching model. Include the NLP and uploader scripts. Test native Python extensions on the baseline.

A normal `.venv` is not automatically relocatable. Test the runtime after moving it to another directory.

### Step 5: Create the Linux launcher

The launcher should resolve its own package root, change to that directory, set the bundled library search path, set any required model/runtime variables, start the application, and return its exit code.

Do not hard-code `/home/developer/...` or a fixed extraction directory.

If executable permissions are not preserved by the archive/extraction method, tell participants to run:

```bash
chmod +x launch.sh
./launch.sh
```

### Step 6: Clean Linux test

Test in a fresh VM matching the supported baseline, without Python, spaCy, SFML development packages, compiler tools, or repository checkout. Also extract into a path containing spaces to catch quoting/path errors.

## 7. CMake and build-system requirements

Separate development builds from participant release builds.

The participant build should:

- build only the participant application;
- use Release configuration;
- exclude tests, research tools, and training targets;
- use predictable runtime output directories;
- stage runtime files in a known directory;
- avoid developer-specific absolute paths;
- provide a repeatable build/package entry point.

Recommended pipeline:

```text
CMake configure
    -> build participant executable
    -> inspect/collect native dependencies
    -> assemble Python + spaCy + model
    -> copy policies/configuration/assets
    -> create launchers and README
    -> validate staging directory
    -> create ZIP
    -> clean-machine test
```

### Static versus dynamic linking

Static linking can reduce the number of libraries to ship, but it is not a universal solution. Some dependencies may not support it; it can increase executable size; licensing obligations still apply; OS runtime components may remain dynamic; and Python/spaCy remain separate in the current architecture.

Use static linking selectively after checking compatibility, licensing, and actual dependencies. Do not make the release depend on “static everything.”

Record the OS/build version, compiler, CMake, C++ standard, architecture, SFML, Python, spaCy, model, and runtime package versions. Pin dependencies so future builds do not silently change.

## 8. Packaging script responsibilities

The existing packaging entry points are:

- `Deployment/package-windows.ps1`
- `Deployment/package-linux.sh`

Each should:

1. Fail if the required executable is missing.
2. Create a clean staging directory.
3. Copy the participant executable.
4. Copy required native runtime libraries.
5. Copy the bundled Python runtime.
6. Copy spaCy and `en_core_web_sm` (or the frozen NLP executable if that strategy is deliberately adopted).
7. Copy NLP and uploader scripts.
8. Copy required policies, configuration, fonts, and assets.
9. Validate the upload endpoint is not a placeholder.
10. Generate/copy the OS launcher and participant README.
11. Check that no development-only files or secrets are included.
12. Create the ZIP and report its exact path.

Use an explicit allowlist for `Data/Study/` rather than copying every JSON file. For example, include the current questionnaire, scenario configuration if used, and `upload-config.json`. Do not include legacy `form-config.json` if the client no longer reads it.

Packaging scripts should fail rather than produce incomplete ZIPs when the executable, Python runtime, model, required SFML library, policy file, upload configuration, launcher, or NLP script is missing.

A successful script run means only that the archive was assembled. It does not prove that the package works on a clean machine.

## 9. Runtime paths and subprocess handling

Resolve bundled read-only resources relative to the package root:

- NLP scripts;
- spaCy model;
- policy files;
- fonts/images;
- questionnaire/scenario configuration;
- upload endpoint configuration.

Resolve participant-writable data under the OS user-data directory.

When C++ starts Python:

- invoke the packaged interpreter, not `python` from `PATH`;
- pass an explicit script path;
- derive paths from the package root;
- set the working directory deliberately;
- do not require virtual-environment activation;
- keep stdout reserved for machine-readable JSON;
- send diagnostics to stderr;
- handle missing interpreter/model and startup failures clearly.

Test paths containing spaces and extraction outside the user's home directory. After a successful run, move the entire extracted package elsewhere and launch it again.

## 10. Security and participant-data handling

Anything inside the common ZIP can be inspected. Never include OAuth secrets, refresh tokens, Drive credentials, Vercel secrets, private keys, developer credentials, or the server-side participant-token map.

The client contains the public endpoint URL and receives an individual participant token separately. Avoid printing that token to logs.

The local pending queue supports offline retries, but it should contain only the approved submission payload and the token needed for authorization—not raw interaction logs or unrelated agent internals.

## 11. Clean-machine acceptance test

Use a fresh VM or physical machine without the development environment.

### Windows

- Supported Windows version.
- No Python or spaCy.
- No SFML installation.
- No Visual Studio/CMake requirement.
- No repository checkout.

### Linux

- Supported distribution/version.
- No Python or spaCy.
- No SFML development package.
- No compiler requirement.
- No repository checkout.

### Checklist

- [ ] Download ZIP as a participant would.
- [ ] Extract to a new directory.
- [ ] Test a path containing spaces.
- [ ] Launch using only participant instructions.
- [ ] Application starts without installing project dependencies.
- [ ] Fonts and assets load.
- [ ] NLP subprocess starts using bundled Python.
- [ ] spaCy loads `en_core_web_sm`.
- [ ] Several NLP requests work in one run.
- [ ] Invalid/empty input does not permanently break the NLP process.
- [ ] Application shuts down cleanly.
- [ ] Restart preserves intended participant state.
- [ ] Writable data is stored under the OS user-data directory.
- [ ] Shared policy files remain unchanged.
- [ ] In-app questionnaire can be completed and validated.
- [ ] Offline submission is queued.
- [ ] Pending submission uploads when connectivity returns.
- [ ] Server acknowledgement is handled.
- [ ] Retry does not create duplicate Drive submissions.
- [ ] No raw interaction, memory, affect, Q-table, or diagnostic data is uploaded.
- [ ] No server secrets are present in the ZIP.
- [ ] Package contains no unnecessary development tools or test data.
- [ ] Package still works after moving the extracted folder.

Record the ZIP filename/hash, OS/version, architecture, release commit, build date, dependency versions, clean-machine result, and known limitations.

## 12. Common failures

| Symptom | Likely cause | Investigation |
|---|---|---|
| Windows app fails on another PC | Missing DLL or incompatible runtime | Inspect executable and DLL dependencies |
| Graphics fail | Missing SFML DLL or graphics dependency | Inspect SFML dependencies and graphics requirements |
| Linux cannot find shared object | Missing `.so` or incorrect search path | Inspect `ldd`; verify launcher |
| Linux GLIBC version error | Built against newer userspace | Build on oldest supported Linux baseline |
| NLP process fails to start | Wrong Python/script path, missing file, permissions, working directory | Test packaged interpreter/script directly |
| spaCy model not found | Model omitted or wrong loading method | Test with bundled interpreter |
| Native spaCy import fails | Incompatible package or missing native library | Rebuild for target OS/architecture |
| Works only in repository | Incorrect relative paths | Test extracted ZIP after relocation |
| Uploader fails but NLP works | Missing uploader dependency/configuration | Inspect imports, endpoint, TLS/network errors |
| State cannot be saved | Writing inside protected package directory | Use OS user-data path |
| Duplicate uploads | Idempotency or request identity issue | Verify submission ID and server handling |

## 13. Recommended implementation order

### Phase A — Dependency audit

1. Inspect CMake targets and linked libraries.
2. Inspect imports in the NLP service and uploader.
3. Record Python, spaCy, and model versions.
4. List every runtime file opened by the participant client.
5. Inspect Windows DLL and Linux shared-library dependencies.
6. Choose supported OS versions and architectures.

**Exit condition:** A verified dependency manifest exists for both OSes.

### Phase B — Portable Python runtime

1. Use a bundled-runtime approach for the first release.
2. Assemble a Windows runtime.
3. Install pinned packages and the model.
4. Repeat independently for Linux.
5. Test each runtime after moving it.
6. Verify both NLP and uploader scripts.

**Exit condition:** Both scripts run without system Python.

### Phase C — Native runtime collection

1. Build the C++ participant executable in Release mode.
2. Inspect its native dependencies.
3. Stage required SFML and other non-system libraries.
4. Verify the Windows compiler-runtime strategy.
5. Stage Linux non-system libraries without copying glibc/loader.
6. Test the staged application.

**Exit condition:** The application starts on a clean machine without project libraries installed.

### Phase D — Data and path handling

1. Stage exact policies, study configuration, fonts, and assets.
2. Exclude legacy/research-only files.
3. Resolve read-only resources relative to package root.
4. Store writable state under OS user-data directories.
5. Verify Python subprocess/model paths.
6. Configure the real Vercel endpoint.

**Exit condition:** The app works from the staged directory after relocation.

### Phase E — Automated release

1. Update the Windows packaging script.
2. Update the Linux packaging script.
3. Add fail-fast checks.
4. Generate launchers and participant README files.
5. Create both ZIPs.
6. Test each on a clean VM.
7. Record hashes, versions, and evidence.

**Exit condition:** Both ZIPs pass the clean-machine acceptance test.

## 14. Decisions to confirm before release

These cannot be safely inferred without inspecting the current repository and target machines:

1. Minimum Windows version and architecture.
2. Minimum Linux distribution/version and architecture.
3. Actual Windows compiler/toolchain and Linux compiler.
4. SFML version and modules linked by the participant executable.
5. Python version compatible with the pinned spaCy version.
6. Bundled Python runtime versus frozen NLP executable.
7. Exact runtime Python dependencies from actual imports.
8. Model loading by package name versus explicit directory.
9. Exact allowlist of policies, questionnaire/scenario files, endpoint config, fonts, and assets.
10. Completion of the real participant menu and in-app questionnaire.
11. Integration of participant-token collection and upload.
12. Clean-machine verification on both OSes.

## 15. Practical recommendation

For the first release:

- Build one Windows package and one Linux package.
- Keep C++ as the main application.
- Bundle a platform-specific relocatable Python runtime.
- Bundle spaCy and `en_core_web_sm`.
- Bundle the exact SFML and other non-system native libraries required.
- Include the fixed policies and runtime assets.
- Store participant-writable data in OS user-data directories.
- Use one launcher per OS to establish paths and start the application.
- Include only the public Vercel endpoint in the client; keep server credentials on Vercel.
- Test both archives on clean virtual machines before distribution.

Avoid changing to PyInstaller, static-linking everything, and introducing a new installer framework simultaneously with questionnaire integration. The immediate goal is a reproducible ZIP that runs without participants installing project dependencies.

**The ZIP is ready only when the clean-machine test passes on both supported operating systems.**
