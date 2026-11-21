# Kopplung per Code (Station-Link 3.6): Entscheidung, nicht Umsetzung

Stand 2026-11-21. **Nicht gebaut — mit Absicht.** Der Auftrag lautete: erst
nachsehen, welche Bibliotheken es gibt, und wenn SPAKE2+EE ohne neue
Abhaengigkeit nicht geht, die Wahl begruenden statt Kryptografie selbst zu
schreiben. Es geht nicht ohne.

## Was NereusSDR verwendet

SPAKE2+EE ist hier **nicht** der Standard (RFC 9383), sondern Frank Denis'
`jedisct1/spake2-ee`, Commit `fd3ea61f…` (BSD-2-Clause): eine einzige
C-Datei, `crypto_spake.c`, 421 Zeilen, ueber **libsodium**. Sie benutzt:
`crypto_core_ed25519_from_uniform` (Elligator), `crypto_core_ed25519_add/sub`,
`crypto_scalarmult_ed25519(_noclamp)`, `crypto_scalarmult_ed25519_base(_noclamp)`,
`crypto_generichash_*` (BLAKE2b), `crypto_kdf_derive_from_key` und
`crypto_pwhash` (Argon2id, `OPSLIMIT/MEMLIMIT_INTERACTIVE`). Die Stationsseite
(`SpakeExchange.cpp`) und das iPhone (`CSpake2EE`) rufen genau diese Datei
unveraendert auf. Der Draht (Schritte 0 bis 3, 36/32/64/32 Byte, die zwei festen
Namen `nereussdr-device-v1` / `nereussdr-station-v1`, `pair.confirm` als
XChaCha20-Poly1305-Box) steht in der Station-Link-Spezifikation, 3.6.

## Was hier vorhanden ist

| Ort | Vorhanden | Fehlt |
| --- | --- | --- |
| Python-Station (`cryptography` 50.0.2, schon im venv) | Argon2id, BLAKE2b (`hashlib`), HKDF, ChaCha20-Poly1305, P-256 | **Ed25519-Gruppenoperationen** (nur Signieren/Pruefen), **Elligator** (`from_uniform`), XChaCha20, `crypto_kdf` |
| Seite (Browser, WebCrypto) | P-256, HMAC, HKDF, AES-GCM, ChaCha nicht | alles Ed25519-Gruppen-, Elligator- und Argon2id-Zeug |
| `PyNaCl` / `libsodium-wrappers` (npm 0.8.4) | libsodium selbst | `crypto_spake_*` ist **nicht** Teil von libsodium; PyNaCl exportiert zudem `from_uniform` und `crypto_kdf` nicht |
| Longpath (C++) | Mbed TLS 3.6.7 in Auszuegen (P-256, ECDSA) | libsodium: nicht im Baum |
| Dieser Mac | `clang`, `node`, `pip` | **kein** libsodium (`/opt/homebrew/lib`), **kein** `emcc` |

## Die Wahl

Eigenbau scheidet aus: eine Python- oder JS-Neuschreibung von `crypto_spake.c`
auf einzelnen libsodium-Aufrufen waere genau die Kryptografie, die der
Auftrag ausschliesst — und ohne die Referenz-Vektoren des Upstreams
(`tst_spake_exchange.cpp` prueft nur Rundlauf, keine festen Antworten) liesse
sich nicht belegen, dass sie auch zu NereusSDRs iPhone passt.

**Empfohlen: dieselbe Datei, unveraendert, in beide Welten.**

1. *Station (Python):* `crypto_spake.c` + `crypto_spake.h` + `pushpop.h`
   (per URL und `SHA-256 20d63587…4bb6` angeheftet, wie bei Upstream) mit `clang`
   gegen libsodium zu einer kleinen Bibliothek bauen, per `ctypes` aufrufen.
   Neue Abhaengigkeit: **libsodium** auf dem Mac der Station (`brew install
   libsodium`), dazu der Bau-Schritt.
2. *Seite (JS):* dieselbe C-Datei und libsodium mit **emscripten** zu einer
   einzigen `.wasm` uebersetzen und im Baum ablegen. Neue Abhaengigkeit:
   emscripten **nur als Bauwerkzeug**; die Seite braucht danach nichts
   Fremdes nachzuladen (sie wird ueber https ausgeliefert, kein CDN).
3. *Pruefung:* `crypto_spake.c` hat im Upstream keine Festwerte. Der Beleg
   ist ein Rundlauf Python-Station gegen JS/WASM-Seite UND gegen NereusSDRs
   `nereus_pairing_peer` (liegt im Upstream, `tests/tools/`) — der einzige
   Weg, auch die Verdrahtung (Namen, Reihenfolge, Boxen) zu belegen.

Das ist kein Nachmittag: Bau-Kette, ein Wortlistenmodul (`pairing-words-v1.txt`,
256 Woerter, Normalisierung — reine Textarbeit), das Koppelfenster der Station
mit Fehlzaehlern und Pausen (3.6), `pair.*` ueber das Mailbox-Rendezvous (6.5)
und die Eingabe auf der Seite. Vorher braucht es Martins Ja zu den beiden
neuen Abhaengigkeiten.

## Was bis dahin gilt

`koppeln.py` („ein Tipp" im eigenen Netz) bleibt der Weg: einmal zu Hause
koppeln, danach traegt der Fernzugriff von ueberall. Ein *weiteres* Telefon
braucht dafuer weiter das Heimnetz — genau die Luecke, die die Kopplung per
Code schliessen wuerde.
