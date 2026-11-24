#!/usr/bin/env bash
# Source from Bash inside the Distrobox: source scripts/env.sh
SATISFACTORY_MODDING_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
export SATISFACTORY_MODDING_ROOT
export UE_ROOT="${UE_ROOT:-$SATISFACTORY_MODDING_ROOT/toolchains/UnrealEngineCSS}"
export UE_WINE_MSVC="${UE_WINE_MSVC:-$SATISFACTORY_MODDING_ROOT/toolchains/msvc}"
export SATISFACTORY_PROJECT="$SATISFACTORY_MODDING_ROOT/workspace/StarterProject/FactoryGame.uproject"
