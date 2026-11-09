# Recept: filmes jelenet (cutscene) a TMST add-onban

Az első példa a Tournament Mode T2 Grand Finalja, 2026-11-07: Kazuya legyőzése → loading → intró (Kazuya áll,
a TTT1 tag-átváltozása villámmal, fehér villanás, Devil) → harc Devil ellen. Az első változat (karba tett kéz +
égbe lövő lézer) a felhasználó kérésére kikerült; a lézer megoldása lent, a 9. pontban, ha újra kellene. Ez a leírás azt rögzíti, hogyan készül egy ilyen jelenet, mire kell figyelni, és hogyan adható hozzá a
következő (más boss, más alkalom).

## 1. Az alapelv
- **Nem saját motor:** az Expanded filmes motorja (`src/tekken3_cine.c`, upstream, csak olvassuk) játszik le egy
  `.cine` szövegfájlt **egy futó harcban**, saját képkocka-ciklussal. Mi csak megírjuk a fájlt, és meghívjuk:
  `tekken3_cine_play(path, 0, NULL)`. `NULL` utófeladat → a végén a harc ott folytatódik, ahol volt (a motor
  visszaírja a fázist, a szereplők helyét, a kamerát).
- **A mozdulatok a játékos saját telepítéséből jönnek:** a TTT1 harci csomagok (`mods/ttt1/<Név>-TTT1-combat.jmv`)
  klipjei már kibontott pózok (képkockánként 57 csatorna). Semmit nem másolunk videóból vagy máshonnan (a
  felhasználó kifejezett kérése). Egy klip bármelyik szereplőn lejátszható, akinek ugyanaz a teste (Kazuya →
  Devil).
- **A `.cine` futásidőben készül** (`src/tournament/tmst_tournament_cine.c`, `tmst_boss_intro_write`) a
  `<Expanded>/addons/tmst/workspace/boss-intro/<kulcs>.cine` fájlba, minden lejátszás előtt.

## 2. A részek (ki mit csinál)
| Hely | Szerep |
|---|---|
| `src/tournament/tmst_tournament_cine.[ch]` | **A tábla** (`TmstBossIntro`) és a `.cine` írása. Önállóan fordul, tesztelhető. |
| `src/tournament/tmst_tournament.c`, „the boss intro” szakasz | Mikor indul (`boss_intro_tick`), a csomagok betöltése (`intro_pack`), START-átugrás (`__wrap_func_80050450`), a részek hangjai (`intro_sounds_load`, `intro_mix`). |
| `src/jukebox/tmst_jukebox.c`, `tmst_jukebox_lead_in()` | A boss zenéje már a jelenettel indul, a harc elején a játék megszakítás nélkül átveszi. |
| `tests/test_tournament.py` (`test_intro_*`), `tests/c/tournament_main.c` (`cine`, `sounds`) | Szintetikus csomaggal: hossz, sorok, csere, villanás, aura, áttűnés, kamera-simaság, hangok. |
| `docs/COMPAT.md` | Minden upstream felület, amire támaszkodunk (cine API, 0x80050450, `.jmv`, `.jus`, a rekordok hanglistája). |

## 3. A `.cine` formátum (ami nekünk kell)
```
cine 1
length <kockák>          # 60 kocka = 1 s
arena 0                  # a motor csak naplóz, a harc arénája marad
dark 0                   # 1: True Ogre-féle sötét aréna (a T2 pályán éjszakai padlóra váltana: kerüld)
actor 0 <ID> <jelmez>    # 0 = a játékos (P1), 1 = a boss (P2); numerikus ID is jó
actor 1 <ID> <jelmez>    # vagy kulcs: "actor 1 kazuya 0" egy másik karakterrel kezdve...
swap 1 <ID> <jelmez> <f> # ...és az f. kockától a boss (a motor betölti, a végén a boss marad: a harc vele megy)
aura 1 <tól> <ig> <lépés> <csontok...>   # villám (a szereplő saját hit-effektje) a csontokon felváltva
hide 0 0                 # a játékos a 0. kockától rejtve
place 1 x y z facing     # a boss helye; facing 0x10000 = egy fordulat, 0x8000 → −z felé (a kamera felé) néz
fade 0 30 -256 0         # feketéből be (−256 fekete, 0 semmi, 256 fehér)
fade <s-6> <s-1> 0 256   # villanás: fehérbe a csere elé...
fade <s> <s+12> 256 0    # ...és vissza (egy kockára mindig az első illeszkedő fade számít: ne fedjék egymást)
fade <n-24> <n-1> 0 -256 # a végén feketébe
camera 0 <n>             # utána n sor: ex ey ez tx ty tz (int16; y LEFELÉ nő, a padló y = 0)
poses 0 0 1              # a rejtett játékos egy póza (távolra tolva: distance 12000)
poses 1 0 <n>            # utána n sor, 57 uint16: 0 heading, 1 magasság, 2 távolság (poláris gyökér a place-hez
                         # és a facinghez képest; egy séta klip távolsága előre nő), 3.. 18 ízület XYZ (0x10000/fordulat)
```
Hasznos még (nem használtuk): `light rrggbb`, `music <pcm> f`, `spark`, `eyes`, `hand`, `model`, `effect`
(lásd az upstream `cine_read`-jét).

## 4. A kamera és a tér (mérve)
- A harc arénájában (pl. a mi T2 Mirror Darkness-ünkön) játszik; a padló a szereplő körül világos, a T2 tükör
  megsokszorozza az alakot (látványos, maradt).
- Egy álló szereplő: a feje kb. y −1500 … −1700, a csípő ~ −1040. Teljes alak 2 m-ről: szem
  `(300, -900, -1300)` → cél `(0, -1300, 1500)`, ha a boss `place 0 0 1500 0x8000`.
- Kulcskockák között smoothstep, a teszt 40 egység/kocka fölötti ugrást nem enged (nincs „vágás”).
- Ha a kamera nagyon felfelé fordul (pl. egy égbe lövő lézer után), a szereplő kiesik a képből: inkább hátrébb és lejjebb.
- Ha a mozdulat előre lép (a tag-átváltozás ~500-at), a kamera húzódjon hátrébb vele (különben levágja a fejét).

## 5. Új jelenet lépésről lépésre
1. **Hol induljon?** Most: a 2 fős CPU-csapat 2. tagjának menete, FIGHT képernyő, **4-es fázis** (Team
   Battle-ben a 2. tagnak nincs saját bevezetője; a 4-esből 0,3 s alatt indul a menet). Más alkalomhoz (pl. egy
   torna első meccse előtt) előbb headless mérd meg a fázisokat (`test-scripts/cine/c1.py` mintájára).
2. **Klipek keresése** a csomagban (`test-scripts/cine/clips.py <pack.jmv>`: rekordonként kockaszám, gyökér
   eleje-vége, cancel-szám). Tippek:
   - álló pózok: kevés cancel (1–2), a távolság alig változik, a magasság ~1000–1130;
   - a csomag **vége** felé vannak az intró/győzelmi pózok (Kazuya 698–707, Heihachi 724–826, Devil 689–699);
   - **az 1-es rekord** (TTT1 0x80036EEC) Kazuyánál és Devilnél a **tag-átváltozás** (karok fel, előregörnyed, a
     34. kockától villámmal felemelkedik; 0x705C hang a 34. kockán); más pároknál a tag-be/kilépés más rekord lehet:
     a villámos/fényes időzített tulajdonságok (+32 lista, 6-os/7-es fajta) jó nyom;
   - lézer-rekord: a rekord +40 bájtja 0x3F, a típus +41, az aktív ablak +45/+46 (Devil: 277/278 Inferno,
     279/283 Blaster, 281, 282 Air Inferno, **695 Sky sweep** = égbe lő).
3. **Katalógus képekkel** (a felhasználó választ): `gen_cine.py` több klipet fűz egymás után
   (`seq=klip:kockák:sebesség,...`, más csomagból `pseq=<fájl>|klip:kockák:sebesség;...`), fix kamerával. Futtatás:
   `TMST_BOSS_INTRO_CINE=<cat.cine> TEKKEN3_CINE_HOLD=<kockák,...> TEKKEN3_CINE_HOLD_TICKS=40 C4_TAG=cat
   python -u tournament/h.py cine/c4.py tournament,jukebox,tekken2,stages,music`. A `HOLD` kimereví a megadott
   kockákat, a `c4.py` minden kimerevített kockáról képet készít (`cat_fNNNN.png`), utána címkézett lapba
   rakhatók (`catalogue*.png` minták).
4. **Bejegyzés a táblába** (`tmst_tournament_cine.c`, `intros[]`):
   ```c
   static const TmstCinePart x_parts[] = {
       {"Kazuya-TTT1", -1, 0, 100, 80, 0},    /* más csomag: fájltő; klip (-1: annak neutralja); kezdő kocka; sebesség %; kockák; áttűnés */
       {"Kazuya-TTT1", 1, 0, 70, 70, 12},
       {NULL, -1, 0, 100, 80, 18},            /* NULL: a boss saját csomagja */
   };
   static const TmstCineKey x_cam[] = { {0, {ex, ey, ez}, {tx, ty, tz}}, ... };
   {"kulcs", x_parts, N(x_parts), hossz, {x, y, z, facing}, fade_be, fade_ki, x_cam, N(x_cam),
    "kezdő karakter kulcsa" /* NULL: végig a boss */, csere_kockája, villanás, aura_tól, aura_ig, aura_lépés,
    {csontok...}, csontok_száma},
   ```
   - **Ne fusson túl egy klip végén** (különben visszaugrik az elejére): `from + (frames-1)·speed/100 ≤ klip-1`.
     Egy séta hurkolhat (a távolság körről körre nő), egy egyszeri mozdulat ne.
   - Áttűnés (`blend`) két rész között: 15–20 kocka elég.
   - Egy klip-kocka a jelenetben: `rész_kezdete + (klip_kocka − from) / (speed/100)` (így jön a csere, az aura és
     a hangok kockája is; Devil: 80 + 38 / 0,7 = 134).
   - Egy álló rész (a távolsága nem változik) egy mozgó után megtartja annak irányát: nem ugrik vissza.
5. **Kép ellenőrzése headless:** `TEKKEN3_CINE_HOLD`-dal a kulcspillanatokra, és egy képsorba rendezve
   (`p3_sheet.png` minta). A kamerát addig állítsd, amíg minden kockán jó a keret.
6. **Teszt** a `tests/test_tournament.py`-ban (új bejegyzéshez legalább: hossz, szereplők, a részek határán
   nincs ugrás, kamera-simaság), aztán `dev\test.cmd`, debug és release build.
7. **Felhasználói próba játékban** (GL, széles képernyő: a headless kép 4:3!).

## 6. Ami magától jön (nem kell újra megírni)
- **START-átugrás:** a bemenet-szűrő az intró alatt mindent elnyel (a játék pause-a megállítaná a motort), a
  START élére a `__wrap_func_80050450` a motor egy képkockájának *elején* (a harc-képkocka saját sp-je) hívja
  a `tekken3_cine_abandon()`-t, és visszaírja, amit a motor a végén írna (a play előtt mentve).
- **Zene:** `tmst_jukebox_lead_in()` a boss (Jukebox / torna) darabját azonnal indítja; Jukebox nélkül csend.
- **Csere előtti START:** megjegyzi, és csak a csere után ugrik (különben a boss a másik karakter modelljével
  maradna a harcban).
- **Hangok:** a részek rekordjainak saját hanglistája (`tmst_boss_intro_sounds`) a rész csomagjának `-sfx.jus`-ából,
  a jelenet megfelelő kockáján, a játékos SFX-hangerején.
- **Fallback:** tábla nélküli guest boss → álló póz, a kamera közelít.

## 6b. Natív boss, egyfős boss (Heihachi, 2026-11-09)
- **Natív** (Heihachi, Ogre, True Ogre): a motor a TTT1-pózokat a natív csontvázára is lejátssza, T3-mozdulatokon is
  („donor pose reached native skeleton bridge”). A bejegyzés `own` mezője a saját csomag töve (`"Heihachi-TTT1"`):
  a pózok és a hangok onnan; `NULL` = a P2 guest-prefixe (Devil).
- **Egyfős boss** (T1, TTT GF): a VS-képernyő (8/3) után a **8-as képernyő 7-es fázisa** (`ALONE_PHASE`); a 8-asban
  rögtön a menet címe jön. Csak saját bejegyzéssel (a Free Tournament húzott bossa nem kap). Kétfősnél marad a 4-es.
- **Kamera:** teljes alakhoz a cél y ≈ −950 (derék), nem −1300 (fej: a láb kilóg); headless 4:3-ban 3,5–3,8 m.
  A szereplő tényleges útja: `TEKKEN3_CINE_RECORD` + `rec.py` (Heihachi felállva *távolodik*).
- **Hangolás torna nélkül:** `test-scripts/cine/entry_cine.sh <kulcs> <tő> <swap-kulcs>` (a tábla `.cine`-ja a
  teszt-exével a telepítés csomagjaiból, `actor 1 xiaoyu` + `swap`), Practice-ben `c5.py` + `TEKKEN3_CINE_TEST`.
  A Practice-ben a P2 ID írása nem cserél modellt: ezért a `swap`.
- Egy klip eleje „tartásként”: ugyanaz a klip kis sebességgel (Heihachi 815 0..5 kocka 5 %-on), majd onnan tovább.

## 7. Buktatók (amibe belefutottunk)
- **A debug buildben nincs TTT1 adat** (nincs Kazuya/Devil → „FINAL BOSS NOT IN THIS INSTALL”): minden
  `dev\build.cmd --debug` után `sh docs/handoffs/test-scripts/cine/copy_mods.sh` (**másolat, nem junction**: a
  build kiüríti a debug `mods/`-ját, és egy junctionön át a release-t is érhetné).
- A headless rácsválasztás néha elakad (a mérés „ids 22 22”-t ír): újraindítás.
- **Natív bossok:** mennek, de csak `own` csomag-tővel (6b). Egy natívra `swap` után a motor a TTT1-mozdulatokat
  kényszeríti (`tekken3_native_moves_force`): a harcban azokon maradhat (Ogre → True Ogre-nál mérni).
- A gyökér-távolság a klipben **előre nő**: ha csökkented, a szereplő hátrafelé „moonwalkol”. Az irányt a
  `place` facing-je adja.
- A Practice-ben a karakter-ID felülírása nem tölt guestet: a méréshez a torna GF-je kell (kb. 6 perc).
- Sok backslash-es kódot ne heredoc-kal írj (a `\n` elromlik): Write eszközzel.
- Az intró alatt a játék saját zenéje a loadingnál elhallgat: ezért kell a lead-in.

## 8. Szerszámok (helyi, `docs/handoffs/test-scripts/cine/`, a git nem követi)
`clips.py` (rekordok listája), `gen_cine.py` (katalógus és próba-`.cine`), `c4.py` (GF + képek, `C4_TAG`,
`C4_EVERY`, `C4_SKIP`, a `TEKKEN3_CINE_HOLD` kockái), `c1.py` (fázis-idővonal), `rec.py`
(`TEKKEN3_CINE_RECORD` olvasása: kamera és szereplő-pozíció kockánként), `sheet.py`, `copy_mods.sh`.
Bench env-ek a játékban: `TMST_BOSS_INTRO_CINE` (más `.cine`), `TMST_BOSS_INTRO_PHASE` (más fázis),
`TEKKEN3_CINE_HOLD` / `_TICKS`, `TEKKEN3_CINE_RECORD` (upstream).

## 9. Ha egyszer újra lézer kellene (az első változatból, kivéve)
- Lézer-rekord: +40 bájt 0x3F, típus +41, ablak +45/+46 (Devil: 277/278 Inferno, 279/283 Blaster, 281, 282 Air
  Inferno, 695 Sky sweep = égbe lő, 118-tól). A sugarat az Expanded renderere rajzolja a `tekken3_laser_beam` /
  `tekken3_laser_age` szerint, amit a harci rekordhoz köt: az intró motor-rekordjánál ezeket `--wrap`-pel kell
  felülírni (a fej: actor + 0x908 + 2·68; a típus 5 eltolása: 140 hátra, 160 fel). Hangja a `-sfx.jus` 1024-es
  hangja. A kódja nem került be a git történetbe (az a változat nem lett commitolva): ez alapján újraírható.
