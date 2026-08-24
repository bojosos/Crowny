#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$repository_root/3rdparty/premake/bin/premake5" gmake2 --with-nodes "$@"
