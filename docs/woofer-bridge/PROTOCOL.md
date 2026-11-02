# Woofer-brug: protocol v1

Dit bestand staat identiek in WooferCitra en in elke Woofer-game
(eerst PokemonWoofer), op `docs/woofer-bridge/PROTOCOL.md`. Wijzig het altijd
in alle repo's tegelijk.

Revisie: 2026-11-02. Protocolversie: 1.

## 1. Principe

- **De game levert alles.** Tekst, vertaling, woorden, pinyin en betekenis
  komen uit de game zelf. De emulator kent geen spelbestanden, geen hashes en
  geen tekentabellen. Daardoor werkt elke volgende Woofer-game in WooferCitra
  zonder wijziging aan de emulator.
- **De game werkt ook zonder emulator.** Het woordenboek in de game zelf
  (onderscherm) is de basis en werkt op een echte 3DS en in elke emulator.
  WooferCitra voegt alleen extra's toe: een groter paneel, uitspraak, de
  woordenlijst buiten het spel en export.
- **Eén woordenlijst voor alle games:** `sdmc:/woofer/vocab.jsonl`.

## 2. Handdruk: emulator naar game

De game vraagt bij het opstarten:

```c
s64 v = 0;
if (R_SUCCEEDED(svcGetSystemInfo(&v, 0x574F4F46 /* 'WOOF' */, 0)) && v >= 1)
    /* brug actief, v = hoogste protocolversie van de emulator */;
```

| type | param | antwoord |
|---|---|---|
| `0x574F4F46` | 0 | hoogste protocolversie die de emulator kent (WooferCitra: 1) |
| `0x574F4F46` | 1 | vlaggen van de emulator, zie hieronder |

Vlaggen (param 1):

- bit 0 `HOST_PANEL_VISIBLE`: het woordenboekpaneel van de emulator is nu in
  beeld. De game mag dan bijvoorbeeld zijn eigen paneel compacter maken.

Een echte 3DS geeft een foutcode, andere emulators geven 0. In beide gevallen
stuurt de game niets. Dat spaart rekentijd op een Old 3DS.

## 3. Berichten: game naar emulator

Eén bericht per aanroep van `svcOutputDebugString`, UTF-8:

```
WOOF1 {"t":"...", ...}
```

- Het voorvoegsel is `WOOF`, het versienummer en één spatie. Daarna één
  JSON-object op één regel.
- Een emulator negeert berichten met een versie die hij niet kent.
- Onbekende waarden van `t` en onbekende sleutels worden genegeerd. Nieuwe,
  optionele sleutels mogen binnen v1 worden toegevoegd. Wat oude lezers zou
  breken wordt v2 (`WOOF2 `).
- Maximaal 8 KiB per bericht. Past een `line` niet, dan laat de game eerst
  `en` weg en daarna de `g` van de woorden.
- Andere regels van de game (gewone logregels) gaan zoals altijd naar de log.

### `hello`: de game stelt zich voor

Na een geslaagde handdruk, één keer bij het opstarten.

```json
{"t":"hello","game":"pokeemerald-zh","title":"PokemonWoofer","lang":"zh-Hans","src":"en","build":"0.1.0"}
```

- `game`: vaste id, kleine letters, cijfers en `-`. Vorm: `<spel>-<taal>`.
- `lang`: de taal die de speler leert (BCP 47, bijv. `zh-Hans`).
- `src`: de oorspronkelijke taal van de game.
- `build`: versie van de game, alleen ter informatie.

### `line`: er staat een nieuwe pagina tekst in beeld

```json
{"t":"line","h":"6b8a1f0c9e2d4a17","zh":"...","en":"...","loc":"...",
 "w":[{"s":"银行","p":"yínháng","g":"bank","hsk":3},{"s":"。"}]}
```

- `h`: id van de tekst (16 hextekens), stabiel tussen builds zolang de
  oorspronkelijke tekst gelijk blijft. Samen met `game` een vaste sleutel.
- `zh`: de pagina zoals de speler hem ziet, met ingevulde namen. (De sleutel
  heet `zh` voor elke doeltaal; `lang` uit `hello` zegt welke taal het is.)
- `en`: de oorspronkelijke tekst, optioneel.
- `loc`: plek in het spel (kaart, scène), optioneel.
- `w`: de stukken van de pagina op volgorde. Samen vormen ze `zh`. Een stuk
  met `p` is een woord en aantikbaar; zonder `p` is het een leesteken of een
  ingevulde naam. `hsk` (1-6, 7 voor 7-9) is optioneel.

### `clear`: het berichtvak is dicht

```json
{"t":"clear"}
```

### `saved`: de speler heeft in de game een woord opgeslagen

```json
{"t":"saved","s":"银行","p":"yínháng"}
```

De game heeft de regel dan al aan `vocab.jsonl` toegevoegd. De emulator leest
het bestand opnieuw.

## 4. Woordenlijst `sdmc:/woofer/vocab.jsonl`

Eén JSON-object per regel. Game en emulator voegen alleen regels toe, elk als
één schrijfactie, en herschrijven het bestand nooit.

```json
{"v":1,"game":"pokeemerald-zh","s":"银行","p":"yínháng","g":"bank","zh":"...","en":"...","loc":"...","at":"2026-11-02T14:03:00Z","status":"new"}
```

- Sleutel van een woord: `s` + `p` (dus 行 als `xíng` en als `háng` zijn twee
  woorden). Wie leest, neemt per sleutel de laatste regel.
- `status`: `new`, `learning`, `known` of `deleted`.
- `game` zegt waar het woord vandaan komt; de lijst geldt voor alle games.
- `at` is optioneel. Een regel die geen geldige JSON is (half geschreven bij
  stroomuitval) wordt overgeslagen.

## 5. Een nieuwe game Woofer-klaar maken

PokemonWoofer is het voorbeeld. Per game:

1. **Tekst vervangen per hash** van de oorspronkelijke tekst (zoals
   `zh_dialog.bin` en `Cjk_TextOverride`).
2. **Woorddata bij het bouwen.** De modules `zh_pinyin.py`, `zh_gloss.py` en
   `zh_gloss_bin.py` uit `kit/` van PokemonWoofer zijn spelonafhankelijk en
   kunnen zo mee. Alleen de tekstcodering (`zh_text.py`) en het
   uitlezen van de dialogen zijn per game.
3. **De brug:** `port/woofer_bridge.c` en `.h` uit PokemonWoofer kopiëren en bij
   het opstarten `Woofer_Init` aanroepen met een eigen `game`-id.
4. **Berichten:** `line` bij elke nieuwe pagina, `clear` bij sluiten, `saved`
   bij opslaan (na `Woofer_AppendVocab`).
5. **Testen:** in WooferCitra staan de ontvangen berichten in de log
   (`Debug_Emulated`, niveau Debug); zonder WooferCitra moet de game
   ongewijzigd werken.
