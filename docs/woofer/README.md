# WooferAzahar

WooferAzahar is een fork van [Azahar](https://github.com/azahar-emu/azahar)
(gebaseerd op release 2126.2) met de taalfuncties van WooferCitra: de
Woofer-brug, het woordenboekpaneel op Android, uitspraak, de gedeelde
woordenlijst en export naar Anki. Het protocol is ongewijzigd
(`docs/woofer-bridge/PROTOCOL.md`, gelijk aan PokemonWoofer en WooferCitra),
dus PokemonWoofer werkt zonder aanpassingen.

Revisie: 2026-11-02.

## Wat zit waar

| Onderdeel | Bestand |
|---|---|
| Core-brug (spelonafhankelijk) | `src/core/woofer/bridge.{h,cpp}` |
| Woordenlijst `sdmc:/woofer/vocab.jsonl` | `src/core/woofer/vocab.{h,cpp}` |
| Haakjes in de kernel | `src/core/hle/kernel/svc.cpp`: `Woofer::HandleDebugString` in `OutputDebugString`, type `WOOFER_BRIDGE = 0x574F4F46` in `GetSystemInfo` |
| Opstarten, stoppen, savestate | `src/core/core.cpp`, `System::Shutdown`: `Woofer::Reset()`, bij het laden van een savestate `Woofer::OnStateLoaded()` |
| Tests | `src/tests/core/woofer/bridge.cpp` (eenheid), `game_bridge.cpp` (end-to-end), `game/` (kopie van `port/woofer_bridge.{c,h}` uit PokemonWoofer) |
| JNI | `src/android/app/src/main/jni/woofer.{h,cpp}`; aangeroepen vanuit `id_cache.cpp` (`JNI_OnLoad`) en `native.cpp` (start en stop van een game) |
| Android | `org.citra.citra_emu.woofer`: `WooferBridge`, `WooferPanel`, `WooferSpeech`, `WrapLayout`, `WooferVocabActivity`, `WooferEmulationUi` |
| Koppeling in Azahar's app | `EmulationFragment.kt` (een paar regels), `HomeSettingsFragment.kt` (tegel "Woofer-woorden"), `menu_in_game.xml`, `AndroidManifest.xml` |
| Teksten | `res/values/strings_woofer.xml` (Engels), `res/values-nl/strings_woofer.xml` (Nederlands) |

De wijzigingen aan Azahar's eigen bestanden zijn bewust klein gehouden; de
rest staat in eigen bestanden. Daardoor gaat een nieuwe Azahar-release
samenvoegen meestal zonder conflicten.

## Verschillen met WooferCitra

- **Kotlin** in plaats van Java, in Azahar's pakket `org.citra.citra_emu`.
- **Zwevend paneel.** Het paneel ligt over het beeld en is te verslepen aan
  de titelbalk; de plek blijft per stand (liggend, staand) bewaard. Tik op de
  titel om in te klappen. Zonder tekst in beeld krimpt het tot de titelbalk.
  Via het menu in het spel (lade links) is het helemaal te verbergen.
- **Pinyin boven de tekens** (knop "Pinyin"), lang drukken op een woord
  bewaart het direct.
- **Savestates:** na het laden van een savestate verdwijnt de oude zin uit het
  paneel (de game blijft bekend, de zin niet).
- **Woordenlijst** in Material-stijl: zoeken (ook zonder toontekens, `yinhang`
  vindt `yínháng`), filter op status, tikken laat het woord horen, lang drukken
  zet de status of verwijdert (met "Ongedaan maken").
- **Anki-export** met kopregels voor Anki 2.1.55+: tab als scheiding, namen
  van de velden, een vaste id per woord (een latere export werkt de kaarten
  bij in plaats van ze dubbel toe te voegen) en tags `woofer <game> <status>`.
- **Uitspraak:** als Android geen Chinese stem heeft, zegt de app dat één keer.
- **Eigen app-id** `dev.woofer.azahar` (debug: `dev.woofer.azahar.debug`) en
  naam "WooferAzahar": installeert naast Azahar.

Vervallen (goedgekeurd): geheugen bewerken, combo-toetsen en knoppenthema's
van de oude WooferCitra-app.

## Testen

```sh
git submodule update --init --recursive
tools/woofer/test.sh          # snel: woofer_tests (enkele minuten)
tools/woofer/test.sh --full   # het hele Azahar-testdoel, eerst [woofer]
```

De end-to-end-tests koppelen de game-kant van PokemonWoofer
(`port/woofer_bridge.c`, ongewijzigd gekopieerd naar `src/tests/core/woofer/game`)
aan de emulatorkant: `gWooferSystemInfo` doet wat `SVC::GetSystemInfo` doet en
`gWooferOutput` wat `SVC::OutputDebugString` doet. Getest wordt: handdruk en
`hello`, `line` (ook dat het geldige JSON is en dat de stukken van `w` samen
`zh` vormen), `clear`, `saved`, onvolledige berichten, een echte 3DS en een
andere emulator (dan stuurt de game niets), savestates, en dat game en
emulator samen `vocab.jsonl` delen.

De logica van de app zonder telefoon (inlezen van `vocab.jsonl`, de
Anki-export, zoeken zonder toontekens, de taal voor uitspraak) staat in
JVM-tests, samen met de stijlcontrole van Azahar:

```sh
cd src/android
./gradlew testVanillaDebugUnitTest ktlintCheck
```

Na een wijziging aan de brug in PokemonWoofer:

```sh
tools/woofer/check_game_bridge.sh /pad/naar/PokemonWoofer            # vergelijken
tools/woofer/check_game_bridge.sh /pad/naar/PokemonWoofer --update   # overnemen
```

Op GitHub draait `.github/workflows/woofer.yml` de snelle tests bij elke push.
Azahar's eigen workflows (alle platforms) draaien alleen nog als je ze met de
hand start; anders kost elke push in een privérepo veel Actions-minuten. De
debug-APK kan ook via "Run workflow" met `apk` aangevinkt (ongeveer een uur).

## APK bouwen

Nodig: JDK 17 of nieuwer, Android SDK met `platforms;android-35`,
`build-tools;35.0.0`, `ndk;27.3.13750724` en `cmake;3.31.6`.

```sh
export ANDROID_HOME=/pad/naar/android-sdk
tools/woofer/build_apk.sh woofer-azahar-debug.apk   # komt in build/apk/
```

- Standaard alleen `arm64-v8a` (elke recente telefoon). Andere ABI's:
  `ABIS=arm64-v8a,x86_64 tools/woofer/build_apk.sh`, of in Gradle
  `-PabiFilters=...`.
- De debug-APK heeft **geoptimaliseerde native code** (`Release`, zonder LTO):
  een Debug-build van de emulator is te traag om te spelen. Kotlin blijft
  debugbaar. `-PnativeDebug` geeft de trage Debug-build terug.
- Ondertekend met de eigen debug-sleutel van de bouwmachine. Een nieuwere APK
  van een andere machine moet je daarom eerst de oude laten verwijderen.

Lessen uit de cloudomgeving: Maven Central en de Gradle-pluginportal geven
daar vaak 429. Een Gradle-init-script in `~/.gradle/init.d/` dat Google's Maven
en de mirror `https://maven-central.storage-download.googleapis.com/maven2/`
eerst laat vragen, lost dat op (niet in de repo, het is alleen voor die
omgeving).

## Proberen met PokemonWoofer

1. Installeer de APK en kies bij de eerste start een gebruikersmap.
2. Zet `PokemonWoofer.3dsx` met `zh_dialog.bin` en `zh_gloss.bin` klaar zoals
   in PokemonWoofer `kit/` beschreven (`translate_dialogue.py build`).
3. Start de game. Na `hello` verschijnt het paneel; in het menu in het spel
   staat dan "Woofer-woordenboek verbergen/tonen".
4. Bij een dialoog: tik een woord aan (pinyin, betekenis, HSK, uitspraak),
   "Bewaren" of lang drukken bewaart het in `sdmc/woofer/vocab.jsonl`.
5. Op het startscherm: tegel "Woofer-woorden" voor de lijst en de Anki-export.

## Azahar bijwerken

```sh
git remote add upstream https://github.com/azahar-emu/azahar.git   # eenmalig
git fetch upstream --tags
git merge <nieuwe release-tag>
git submodule update --init --recursive
tools/woofer/test.sh --full
```
