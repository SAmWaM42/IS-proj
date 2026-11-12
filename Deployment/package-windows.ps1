$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build-participant-windows"
$DistDir = Join-Path $Root "dist/windows"
$Stage = Join-Path $DistDir "AnimalAgent"

if (-not $env:ANIMAL_AGENT_PYTHON_RUNTIME) {
    throw "Set ANIMAL_AGENT_PYTHON_RUNTIME to a relocatable Windows Python runtime containing python.exe, spaCy, and en_core_web_sm."
}
$Python = Join-Path $env:ANIMAL_AGENT_PYTHON_RUNTIME "python.exe"
if (-not (Test-Path $Python)) { throw "Missing $Python" }
& $Python -c "import spacy; spacy.load('en_core_web_sm')"
if ($LASTEXITCODE -ne 0) { throw "Bundled Python runtime cannot load spaCy en_core_web_sm." }

$uploadConfig = Get-Content (Join-Path $Root "Data/Study/upload-config.json") -Raw
if ($uploadConfig.Contains("YOUR-VERCEL-PROJECT")) { throw "Configure the Vercel endpoint in Data/Study/upload-config.json before packaging." }
foreach ($policy in @("baseline.qtable", "proposed.qtable")) {
    if (-not (Test-Path (Join-Path $Root "artifacts/training/$policy"))) { throw "Missing trained policy $policy" }
}

$cmakeArgs = @("-S", $Root, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Release", "-DANIMAL_AGENT_BUILD_TESTS=OFF")
if ($env:CMAKE_TOOLCHAIN_FILE) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$env:CMAKE_TOOLCHAIN_FILE"
}
if ($env:VCPKG_TARGET_TRIPLET) {
    $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=$env:VCPKG_TARGET_TRIPLET"
}
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
$Executable = Join-Path $BuildDir "AnimalAgent.exe"
if (-not (Test-Path $Executable)) { $Executable = Join-Path $BuildDir "Release/AnimalAgent.exe" }
if (-not (Test-Path $Executable)) { throw "Could not locate the built AnimalAgent.exe." }
Copy-Item $Executable $Stage

# Static SFML builds need no SFML DLLs. Dynamic builds can provide them explicitly.
if ($env:ANIMAL_AGENT_SFML_DLL_DIR) {
    if (-not (Test-Path $env:ANIMAL_AGENT_SFML_DLL_DIR)) {
        throw "ANIMAL_AGENT_SFML_DLL_DIR does not exist."
    }
    $sfmlDlls = Get-ChildItem (Join-Path $env:ANIMAL_AGENT_SFML_DLL_DIR "*.dll")
    if (-not $sfmlDlls) { throw "No SFML DLLs found in ANIMAL_AGENT_SFML_DLL_DIR." }
    Copy-Item $sfmlDlls.FullName $Stage
}

New-Item -ItemType Directory -Force -Path (Join-Path $Stage "Data/Assets/free version"), (Join-Path $Stage "Data/Assets/white_dog_by_Brysia"), (Join-Path $Stage "Data/Fonts"), (Join-Path $Stage "Data/Study"), (Join-Path $Stage "Components/NLP"), (Join-Path $Stage "Policies"), (Join-Path $Stage "runtime/python") | Out-Null
Copy-Item (Join-Path $Root "Data/Assets/free version/free.png") (Join-Path $Stage "Data/Assets/free version/free.png")
if (-not $env:ANIMAL_AGENT_FONT_FILE -or -not (Test-Path $env:ANIMAL_AGENT_FONT_FILE)) {
    throw "Set ANIMAL_AGENT_FONT_FILE to a redistributable TTF font file."
}
Copy-Item $env:ANIMAL_AGENT_FONT_FILE (Join-Path $Stage "Data/Fonts/UIFont.ttf")
if ($env:ANIMAL_AGENT_FONT_LICENSE_FILE -and (Test-Path $env:ANIMAL_AGENT_FONT_LICENSE_FILE)) {
    Copy-Item $env:ANIMAL_AGENT_FONT_LICENSE_FILE (Join-Path $Stage "Data/Fonts/UIFont-LICENSE.txt")
}
Copy-Item (Join-Path $Root "Data/Assets/white_dog_by_Brysia/*.png") (Join-Path $Stage "Data/Assets/white_dog_by_Brysia/")
Copy-Item (Join-Path $Root "Data/Study/*.json") (Join-Path $Stage "Data/Study/")
Copy-Item (Join-Path $Root "Components/NLP/nlp_intent_service.py") (Join-Path $Stage "Components/NLP/")
Copy-Item (Join-Path $Root "Components/NLP/upload_submission.py") (Join-Path $Stage "Components/NLP/")
Copy-Item -Path @(
    (Join-Path $Root "artifacts/training/baseline.qtable"),
    (Join-Path $Root "artifacts/training/proposed.qtable")
) -Destination (Join-Path $Stage "Policies/")
Copy-Item (Join-Path $env:ANIMAL_AGENT_PYTHON_RUNTIME "*") (Join-Path $Stage "runtime/python/") -Recurse
$StagedPython = Join-Path $Stage "runtime/python/python.exe"
if (-not (Test-Path $StagedPython)) { throw "The staged Python runtime is missing python.exe." }
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
For Believability Study, obtain your participant credentials separately from the researcher.
Place the supplied credentials.json beside AnimalAgent.exe before starting the study.
It must contain a participant_number integer and the private token supplied to you.
Do not share this file. It is not included in the common ZIP.
Study questionnaire responses are submitted by the application. Failed uploads are retained locally and retried on the next launch.
'@ | Set-Content (Join-Path $Stage "README.txt") -Encoding UTF8

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$Zip = Join-Path $DistDir "AnimalAgent-Windows.zip"
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Compress-Archive -Path $Stage -DestinationPath $Zip
Write-Host "Created $Zip"
