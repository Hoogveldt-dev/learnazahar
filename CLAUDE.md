# WooferAzahar: notities voor Claude

* Taal met de gebruiker: Nederlands. Geen em-dash gebruiken.
* Fork van azahar-emu/azahar (basis: release 2126.2), met de Woofer-brug en
  de taalfuncties. Overzicht: `docs/woofer/README.md`.
* Het protocol staat in `docs/woofer-bridge/PROTOCOL.md` en is byte voor byte
  gelijk aan dat in PokemonWoofer en WooferCitra. Alleen in alle repo's
  tegelijk wijzigen.
* Wijzigingen aan Azahar's eigen bestanden klein houden; nieuwe code in eigen
  bestanden (`src/core/woofer/`, `jni/woofer.cpp`, pakket
  `org.citra.citra_emu.woofer`). Dan blijft samenvoegen met Azahar makkelijk.
* De game-kant van de brug in `src/tests/core/woofer/game/` is een ongewijzigde
  kopie van PokemonWoofer `port/woofer_bridge.{c,h}`; bijwerken met
  `tools/woofer/check_game_bridge.sh <PokemonWoofer> --update`.
* Testen: `tools/woofer/test.sh` (snel) en `tools/woofer/test.sh --full`.
  Beide moeten slagen voor een commit aan de brug. Voor de app:
  `./gradlew testVanillaDebugUnitTest ktlintCheck` in `src/android`.
* APK: `tools/woofer/build_apk.sh` (arm64-v8a, debug-sleutel, native code
  geoptimaliseerd), uitvoer in `build/apk/`. Nooit APK's, ROM's of
  spelbestanden committen. Let op: `dist/` is een bronmap van Azahar zelf.
* Azahar's eigen GitHub-workflows staan op handmatig starten (kosten in een
  privérepo); `woofer.yml` draait de snelle tests.
