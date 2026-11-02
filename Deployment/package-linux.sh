#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-participant-linux"
DIST_DIR="${ROOT}/dist/linux"
STAGE="${DIST_DIR}/AnimalAgent"

: "${ANIMAL_AGENT_PYTHON_RUNTIME:?Set this to a relocatable Linux Python runtime containing bin/python3 and spaCy en_core_web_sm}"
: "${ANIMAL_AGENT_SFML_RUNTIME:?Set this to a directory containing the SFML 3 shared libraries and required non-system dependencies}"
PYTHON="${ANIMAL_AGENT_PYTHON_RUNTIME}/bin/python3"

if [[ ! -x "$PYTHON" ]]; then
  echo "Missing executable: $PYTHON" >&2; exit 1
fi
"$PYTHON" -c 'import spacy; spacy.load("en_core_web_sm")'

if grep -q "REPLACE_WITH_" "$ROOT/Data/Study/form-config.json"; then
  echo "Complete Data/Study/form-config.json before packaging." >&2; exit 1
fi
for policy in baseline.qtable proposed.qtable; do
  [[ -s "$ROOT/artifacts/training/$policy" ]] || { echo "Missing trained policy: $policy" >&2; exit 1; }
done

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DANIMAL_AGENT_BUILD_RESEARCH_TESTS=OFF
cmake --build "$BUILD_DIR" --config Release --parallel

rm -rf "$STAGE"
mkdir -p "$STAGE"/{Data/Assets,Data/Study,Components/NLP,Policies,runtime,lib}
cp "$BUILD_DIR/AnimalAgent" "$STAGE/AnimalAgent"
cp "$ROOT/Data/Assets/free version/free.png" "$STAGE/Data/Assets/free.png"
mkdir -p "$STAGE/Data/Assets/white_dog_by_Brysia"
find "$ROOT/Data/Assets/white_dog_by_Brysia" -maxdepth 1 -type f -name '*.png' -exec cp {} "$STAGE/Data/Assets/white_dog_by_Brysia/" \;
cp "$ROOT"/Data/Study/*.json "$STAGE/Data/Study/"
cp "$ROOT/Components/NLP/nlp_intent_service.py" "$STAGE/Components/NLP/"
cp "$ROOT/artifacts/training/baseline.qtable" "$ROOT/artifacts/training/proposed.qtable" "$STAGE/Policies/"
cp -a "$ANIMAL_AGENT_PYTHON_RUNTIME" "$STAGE/runtime/python"
find "$ANIMAL_AGENT_SFML_RUNTIME" -maxdepth 1 -type f \( -name 'libsfml-*.so*' -o -name 'libfreetype.so*' -o -name 'libFLAC.so*' -o -name 'libvorbis*.so*' -o -name 'libogg.so*' \) -exec cp -L {} "$STAGE/lib/" \;

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
if ldd "$STAGE/AnimalAgent" | grep -q "not found"; then
  echo "Unresolved shared libraries in staged executable:" >&2
  ldd "$STAGE/AnimalAgent" | grep "not found" >&2
  exit 1
fi

cat > "$STAGE/README.txt" <<'README'
Animal Agent participant application
1. Extract the complete ZIP to a writable folder.
2. Run run-animal-agent.sh.
3. Choose General Interaction or Believability Study from the menu.
4. In the study, complete both companion conditions and submit each questionnaire form.
5. Do not move or delete files while the application is running.

The application stores General Interaction memory locally in your user data directory.
Study questionnaire responses are submitted through the forms. No automatic data upload occurs.
README

mkdir -p "$DIST_DIR"
(cd "$DIST_DIR" && zip -qr AnimalAgent-Linux.zip AnimalAgent)
echo "Created $DIST_DIR/AnimalAgent-Linux.zip"
