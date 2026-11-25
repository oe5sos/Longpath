# Sendeleistung neben dem SWR (Blatt, 2026-11-25)

Echte Seite gegen `attrappe.py --sendet 3`, aufgenommen während die Attrappe „sendet"
(sie schickt dann `tx_sensors` mit 12,5 W). Nichts am echten Gerät getastet.

* Unter dem SWR-Zeiger eine zweite Kopfzeile **PWR** mit Wertchip (rms-Watt aus
  `tx_sensors` Feld 2), gleiche Regel wie das SWR: nur beim Senden und nur frisch, sonst „—".
* **ALC fehlt:** Longpaths TCI-Server liefert sie nicht (`tx_sensors` = Mikrofon, rms-W,
  Spitzen-W, SWR). Erst bauen, wenn der Server sie meldet.
