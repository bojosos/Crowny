#!/usr/bin/env bash
set -euo pipefail

configuration="${1:-Release}"
if [[ "$configuration" != "Debug" && "$configuration" != "Release" ]]; then
    echo "usage: $0 [Debug|Release] [avx2|sse4.1]" >&2
    exit 2
fi

simd="${2:-avx2}"
if [[ "$simd" != "avx2" && "$simd" != "sse4.1" ]]; then
    echo "usage: $0 [Debug|Release] [avx2|sse4.1]" >&2
    exit 2
fi

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$repository_root/Scripts/crowny" deps physics --configuration "$configuration" --simd "$simd"
