#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-participant-linux"
DIST_DIR="${ROOT}/dist/linux"
STAGE="${DIST_DIR}/AnimalAgent"

# Prefer an explicitly supplied environment; otherwise detect common project venv names.
if [[ -z "${ANIMAL_AGENT_PYTHON_RUNTIME:-}" ]]; then
  for candidate in "$ROOT/wordTokenizer" "$ROOT/.venv" "$ROOT/venv"; do
    if [[ -x "$candidate/bin/python" || -x "$candidate/bin/python3" ]]; then
      ANIMAL_AGENT_PYTHON_RUNTIME="$candidate"
      break
    fi
  done
fi
: "${ANIMAL_AGENT_PYTHON_RUNTIME:?Set ANIMAL_AGENT_PYTHON_RUNTIME to the project venv directory}"
: "${ANIMAL_AGENT_SFML_RUNTIME:?Set this to a directory containing SFML 3 shared libraries and required non-system dependencies}"
PYTHON="$ANIMAL_AGENT_PYTHON_RUNTIME/bin/python"
[[ -x "$PYTHON" ]] || PYTHON="$ANIMAL_AGENT_PYTHON_RUNTIME/bin/python3"

if [[ ! -x "$PYTHON" ]]; then
  echo "Missing executable: $PYTHON" >&2; exit 1
fi
"$PYTHON" -c 'import spacy; spacy.load("en_core_web_sm")'

if grep -q "YOUR-VERCEL-PROJECT" "$ROOT/Data/Study/upload-config.json"; then
  echo "Configure the Vercel endpoint in Data/Study/upload-config.json before packaging." >&2; exit 1
fi
for policy in baseline.qtable proposed.qtable; do
  [[ -s "$ROOT/artifacts/training/$policy" ]] || { echo "Missing trained policy: $policy" >&2; exit 1; }
done

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DANIMAL_AGENT_BUILD_RESEARCH_TESTS=OFF
cmake --build "$BUILD_DIR" --config Release --parallel

rm -rf "$STAGE"
mkdir -p "$STAGE/Data/Assets/free version" "$STAGE/Data/Assets/white_dog_by_Brysia" "$STAGE/Data/Fonts" "$STAGE/Data/Study" "$STAGE/Components/NLP" "$STAGE/Policies" "$STAGE/runtime" "$STAGE/lib"
cp "$BUILD_DIR/AnimalAgent" "$STAGE/AnimalAgent"
cp "$ROOT/Data/Assets/free version/free.png" "$STAGE/Data/Assets/free version/free.png"
FONT_FILE="${ANIMAL_AGENT_FONT_FILE:-/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf}"
[[ -s "$FONT_FILE" ]] || { echo "Set ANIMAL_AGENT_FONT_FILE to a redistributable TTF font." >&2; exit 1; }
cp "$FONT_FILE" "$STAGE/Data/Fonts/UIFont.ttf"
mkdir -p "$STAGE/Data/Assets/white_dog_by_Brysia"
find "$ROOT/Data/Assets/white_dog_by_Brysia" -maxdepth 1 -type f -name '*.png' -exec cp {} "$STAGE/Data/Assets/white_dog_by_Brysia/" \;
cp "$ROOT"/Data/Study/*.json "$STAGE/Data/Study/"
cp "$ROOT/Components/NLP/nlp_intent_service.py" "$STAGE/Components/NLP/"
cp "$ROOT/Components/NLP/upload_submission.py" "$STAGE/Components/NLP/"
cp "$ROOT/artifacts/training/baseline.qtable" "$ROOT/artifacts/training/proposed.qtable" "$STAGE/Policies/"
# Copy the project venv, including its installed spaCy packages and model.
cp -a "$ANIMAL_AGENT_PYTHON_RUNTIME" "$STAGE/runtime/python"
STAGED_PYTHON="$STAGE/runtime/python/bin/python"
[[ -x "$STAGED_PYTHON" ]] || STAGED_PYTHON="$STAGE/runtime/python/bin/python3"
LD_LIBRARY_PATH="$STAGE/lib:$STAGE/runtime/python/lib:${LD_LIBRARY_PATH:-}" "$STAGED_PYTHON" -c 'import spacy; spacy.load("en_core_web_sm")'
find "$ANIMAL_AGENT_SFML_RUNTIME" -maxdepth 1 \( -type f -o -type l \) -name '*.so*' -exec cp -L {} "$STAGE/lib/" \;

cat > "$STAGE/run-animal-agent.sh" <<'LAUNCH'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib:$HERE/runtime/python/lib:${LD_LIBRARY_PATH:-}"
cd "$HERE"
exec "$HERE/AnimalAgent"
LAUNCH
chmod +x "$STAGE/run-animal-agent.sh"

if command -v patchelf >/dev/null 2>&1; then
  patchelf --set-rpath '$ORIGIN/lib' "$STAGE/AnimalAgent"
fi
# Check both the C++ executable and staged Python for missing shared libraries.
for binary in "$STAGE/AnimalAgent" "$STAGED_PYTHON"; do
  if ! LD_LIBRARY_PATH="$STAGE/lib:$STAGE/runtime/python/lib:${LD_LIBRARY_PATH:-}" ldd "$binary" > "$STAGE/.ldd-check.txt"; then
    echo "ldd could not inspect $binary" >&2; exit 1
  fi
  if grep -q "not found" "$STAGE/.ldd-check.txt"; then
    echo "Unresolved shared libraries for $binary:" >&2
    grep "not found" "$STAGE/.ldd-check.txt" >&2
    exit 1
  fi
done
rm -f "$STAGE/.ldd-check.txt"

cat > "$STAGE/README.txt" <<'README'
Animal Agent participant application
1. Extract the complete ZIP to a writable folder.
2. Run run-animal-agent.sh.
3. Choose General Interaction or Believability Study from the menu.
4. In the study, complete both companion conditions and submit each questionnaire form.
5. Do not move or delete files while the application is running.

The application stores General Interaction memory locally in your user data directory.
Study questionnaire responses are submitted by the application. Failed uploads are retained locally and retried on the next launch.
README

mkdir -p "$DIST_DIR"
(cd "$DIST_DIR" && zip -qr AnimalAgent-Linux.zip AnimalAgent)
echo "Created $DIST_DIR/AnimalAgent-Linux.zip"
