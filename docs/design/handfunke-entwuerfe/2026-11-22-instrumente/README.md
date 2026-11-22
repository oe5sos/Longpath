# App: Instrumentenzeile S-Meter | SWR | Rotor-Kompass (2026-11-22)

Martins Wahl „C", dazu SWR; auf dem Desktop (Blatt A) ebenfalls in der linken Spalte.
Vorher/Nachher: iPhone 393 × 852 (2x) und Desktop 1400 × 900.

* SWR kommt aus `tx_sensors` (Feld 4, `TciSensorManager::formatTxSensors`) und existiert nur
  beim Senden; sonst „—". Die Seite las bisher die falschen Felder (1 und 2), jetzt 2 und 4
  (`pruefe-tx-sensors.mjs`).
* Der Kompass erscheint, sobald Longpath einen Rotor meldet; Antippen öffnet die Rotorsteuerung.
