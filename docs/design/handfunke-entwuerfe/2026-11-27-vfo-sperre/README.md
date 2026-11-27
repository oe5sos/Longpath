# VFO-Sperre (2026-11-27)

Dieselbe Sperre wie LOCK am Mac: `lock:rx,true|false` (TciProtocol::handleLockCommand → SliceModel::setLocked), Zustand aus `lock:`/`vfo_lock:`.
LOCK-Pille bei den Schritten (an = orange gefüllt), kleine Marke „LOCK“ über „MHz“.
Gesperrt schickt die App kein `vfo:` mehr: Wischen, Tippen, Spots, Band, Eintippen, Feinschritt; ein vergeblicher Griff lässt die Marke kurz aufleuchten.
Bilder: attrappe.py --gesperrt. Durchgespielt: gesperrt + Feinschritt → bleibt 14.074.000; LOCK aus → 14.074.100; LOCK an → Pille und Marke wieder da.
