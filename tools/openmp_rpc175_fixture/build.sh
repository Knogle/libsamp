#!/usr/bin/env bash
set -euo pipefail

fixture_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${fixture_dir}/build"
sdk_dir="${OMP_SDK_DIR:-/home/chairman/Projects/omp-ipv6/open.mp/SDK}"

if [[ ! -f "${sdk_dir}/include/sdk.hpp" ]]; then
	echo "open.mp SDK not found at: ${sdk_dir}" >&2
	echo "Set OMP_SDK_DIR to a local open.mp SDK checkout." >&2
	exit 1
fi

CCACHE_DISABLE=1 cmake --fresh \
	-S "${fixture_dir}" \
	-B "${build_dir}" \
	-DOMP_SDK_DIR="${sdk_dir}" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_CXX_FLAGS=-m32 \
	-DCMAKE_SHARED_LINKER_FLAGS=-m32

CCACHE_DISABLE=1 cmake --build "${build_dir}" --parallel

artifact="${build_dir}/rpc175_fixture.so"
if [[ ! -f "${artifact}" ]]; then
	echo "Expected artifact was not produced: ${artifact}" >&2
	exit 1
fi

if ! file "${artifact}" | rg -q "ELF 32-bit.*Intel (80386|i386)"; then
	file "${artifact}" >&2
	echo "Refusing non-i386 fixture artifact." >&2
	exit 1
fi

file "${artifact}"
sha256sum "${artifact}"
