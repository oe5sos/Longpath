# ALC neben SWR und PWR (Blatt, 2026-11-26)

Echte Seite gegen `attrappe.py --sendet 3`, aufgenommen, während die Attrappe „sendet".
Nichts am echten Gerät getastet.

* Unter PWR eine Zeile **ALC** mit Wertchip in dB — dieselbe Zahl wie das ALC-Instrument
  am Mac (`TXA_ALC_AV`). Nur beim Senden und nur frisch, sonst „—".
* Longpath schickt sie als eigene Zeile `tx_alc:<rx>,<dB>;` im Takt von `tx_sensors`,
  nur an Clients, die `tx_alc_enable:true` geschickt haben. `tx_sensors` bleibt
  byte-gleich mit Thetis.
