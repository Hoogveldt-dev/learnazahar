#!/usr/bin/env bash
# Controleert of de kopie van de game-kant van de brug in de tests
# (src/tests/core/woofer/game) gelijk is aan port/woofer_bridge.{c,h} van PokemonWoofer.
#   tools/woofer/check_game_bridge.sh /pad/naar/PokemonWoofer [--update]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAME="${1:?Geef het pad naar een PokemonWoofer-checkout}"
COPY="$ROOT/src/tests/core/woofer/game"
status=0
for f in woofer_bridge.c woofer_bridge.h; do
    if ! cmp -s "$GAME/port/$f" "$COPY/$f"; then
        if [ "${2:-}" = "--update" ]; then
            cp "$GAME/port/$f" "$COPY/$f"
            echo "bijgewerkt: $f"
        else
            echo "verschilt: $f" >&2
            status=1
        fi
    fi
done
[ $status -eq 0 ] && echo "game-kant van de brug: gelijk aan $GAME"
exit $status
