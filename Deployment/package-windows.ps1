$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build-participant-windows"
$DistDir = Join-Path $Root "dist/windows"
$Stage = Join-Path $DistDir "AnimalAgent"

if (-not $env:ANIMAL_AGENT_PYTHON_RUNTIME) {
    throw "Set ANIMAL_AGENT_PYTHON_RUNTIME to a relocatable Windows Python runtime containing python.exe, spaCy, and en_core_web_sm."
}
if (-not $env:ANIMAL_AGENT_SFML_DLL_DIR) {
    throw "Set ANIMAL_AGENT_SFML_DLL_DIR to the directory containing SFML 3 runtime DLLs."
}
$Python = Join-Path $env:ANIMAL_AGENT_PYTHON_RUNTIME "python.exe"
if (-not (Test-Path $Python)) { throw "Missing $Python" }
& $Python -c "import spacy; spacy.load('en_core_web_sm')"
if ($LASTEXITCODE -ne 0) { throw "Bundled Python runtime cannot load spaCy en_core_web_sm." }

$formConfig = Get-Content (Join-Path $Root "Data/Study/form-config.json") -Raw
if ($formConfig.Contains("REPLACE_WITH_")) { throw "Complete Data/Study/form-config.json before packaging." }
foreach ($policy in @("baseline.qtable", "proposed.qtable")) {
    if (-not (Test-Path (Join-Path $Root "artifacts/training/$policy"))) { throw "Missing trained policy $policy" }
}

cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release -DANIMAL_AGENT_BUILD_RESEARCH_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
$Executable = Join-Path $BuildDir "AnimalAgent.exe"
if (-not (Test-Path $Executable)) { $Executable = Join-Path $BuildDir "Release/AnimalAgent.exe" }
if (-not (Test-Path $Executable)) { throw "Could not locate the built AnimalAgent.exe." }
Copy-Item $Executable $Stage
Copy-Item $env:ANIMAL_AGENT_SFML_DLL_DIR/*.dll $Stage
New-Item -ItemType Directory -Force -Path (Join-Path $Stage "Data/Assets/free version"), (Join-Path $Stage "Data/Assets/white_dog_by_Brysia"), (Join-Path $Stage "Data/Study"), (Join-Path $Stage "Components/NLP"), (Join-Path $Stage "Policies"), (Join-Path $Stage "runtime/python") | Out-Null
Copy-Item (Join-Path $Root "Data/Assets/free version/free.png") (Join-Path $Stage "Data/Assets/free version/free.png")
Copy-Item (Join-Path $Root "Data/Assets/white_dog_by_Brysia/*.png") (Join-Path $Stage "Data/Assets/white_dog_by_Brysia/")
Copy-Item (Join-Path $Root "Data/Study/*.json") (Join-Path $Stage "Data/Study/")
Copy-Item (Join-Path $Root "Components/NLP/nlp_intent_service.py") (Join-Path $Stage "Components/NLP/")
Copy-Item (Join-Path $Root "Components/NLP/upload_submission.py") (Join-Path $Stage "Components/NLP/")
Copy-Item (Join-Path $Root "artifacts/training/baseline.qtable") (Join-Path $Root "artifacts/training/proposed.qtable") (Join-Path $Stage "Policies/")
Copy-Item (Join-Path $env:ANIMAL_AGENT_PYTHON_RUNTIME "*") (Join-Path $Stage "runtime/python/") -Recurse
$StagedPython = Join-Path $Stage "runtime/python/python.exe"
& $StagedPython -c "import spacy; spacy.load('en_core_web_sm')"
if ($LASTEXITCODE -ne 0) { throw "The staged Python runtime cannot load spaCy en_core_web_sm." }

@'
@echo off
cd /d "%~dp0"
start "" "%~dp0AnimalAgent.exe"
'@ | Set-Content (Join-Path $Stage "run-animal-agent.bat") -Encoding ASCII

@'
Animal Agent participant application
1. Extract the complete ZIP to a writable folder.
2. Run run-animal-agent.bat.
3. Choose General Interaction or Believability Study from the menu.
4. In the study, complete both companion conditions and submit each questionnaire form.
5. Do not move or delete files while the application is running.

The application stores General Interaction memory locally in your user data directory.
Study questionnaire responses are submitted by the application. Failed uploads are retained locally and retried on the next launch.
'@ | Set-Content (Join-Path $Stage "README.txt") -Encoding UTF8

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$Zip = Join-Path $DistDir "AnimalAgent-Windows.zip"
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Compress-Archive -Path $Stage -DestinationPath $Zip
Write-Host "Created $Zip"
