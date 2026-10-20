#!/usr/bin/env bash

set -e

goal="${1:-}"

# Detect platform to pick the right CMake generator
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    GENERATOR="MinGW Makefiles"
else
    GENERATOR="Unix Makefiles"
fi

if [ "$goal" = "build" ]; then
    cmake -B build -S . -G "$GENERATOR"
    cmake --build build

elif [ "$goal" = "train-test" ]; then
    ./build/IS-2 --self-test
    ./build/IS-2 --train --episodes 1000 --steps 40 --seed 42 --out artifacts/training
    ./build/IS-2 --validate --episodes 200 --steps 40 --seed 43 --out artifacts/training 
    ./build/IS-2 --compare --episodes 200 --steps 40 --seed 44 --out artifacts/training
elif [ "$goal" = "run" ]; then
    ./build/IS-2 sim

elif [ "$goal" = "clean" ]; then
    rm -rf build
elif [ "$goal" = "study" ]; then
    ./build/IS-2 --study --policy-dir artifacts/training --study-out artifacts/study

else
    echo "Usage: $0 {build|test|run|clean}"
    exit 1
fi