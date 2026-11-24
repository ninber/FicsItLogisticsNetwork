#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
plugin="$root/workspace/mods/FicsItLogisticsNetwork"
build_dir=$(mktemp -d /tmp/filn-tests.XXXXXX)
trap 'rm -rf -- "$build_dir"' EXIT
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/core_tests.cpp" -o "$build_dir/core_tests"
"$build_dir/core_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/dispatch_policy_tests.cpp" -o "$build_dir/dispatch_policy_tests"
"$build_dir/dispatch_policy_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/routing_tests.cpp" -o "$build_dir/routing_tests"
"$build_dir/routing_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/recovery_tests.cpp" -o "$build_dir/recovery_tests"
"$build_dir/recovery_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/enrollment_tests.cpp" -o "$build_dir/enrollment_tests"
"$build_dir/enrollment_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/lifecycle_tests.cpp" -o "$build_dir/lifecycle_tests"
"$build_dir/lifecycle_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/restoration_tests.cpp" -o "$build_dir/restoration_tests"
"$build_dir/restoration_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/fuel_tests.cpp" -o "$build_dir/fuel_tests"
"$build_dir/fuel_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/idle_tests.cpp" -o "$build_dir/idle_tests"
"$build_dir/idle_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/provider_discovery_tests.cpp" -o "$build_dir/provider_discovery_tests"
"$build_dir/provider_discovery_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/depot_role_tests.cpp" -o "$build_dir/depot_role_tests"
"$build_dir/depot_role_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/rehome_tests.cpp" -o "$build_dir/rehome_tests"
"$build_dir/rehome_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/transfer_ledger_tests.cpp" -o "$build_dir/transfer_ledger_tests"
"$build_dir/transfer_ledger_tests"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
 -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$plugin/tests/registry_tests.cpp" -o "$build_dir/registry_tests"
"$build_dir/registry_tests"
