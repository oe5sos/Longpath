#pragma once

// Warten, bis der TCI-Init-Burst durch ist — statt auf eine Zahl von
// Millisekunden zu hoffen.
//
// Warum es das gibt: Longpath schickt jedem frisch verbundenen Client rund
// 98 Rahmen Anfangszustand. Darin steht unter anderem
//
//     trx:0,false;  tune:0,false;  audio_stream_sample_type:;  …
//
// also genau die Namen, auf die viele Pruefstaende danach horchen. Wer nach
// dem Anmelden blind `QTest::qWait(80)` schreibt und dann mitschreibt,
// misst auf einer schnellen Maschine das Richtige und auf einer langsamen
// den Burst.
//
// Am 2026-10-01/02 ist das zweimal passiert, beide Male nur auf dem
// CI-Laeufer und nie hier:
//
//   tst_tci_sendesperre      sechs Faelle rot, und zwar in BEIDE Richtungen
//                            — die Sperre schien zu versagen UND die
//                            Freigabe nicht durchzugehen. Beides zugleich
//                            kaputt gibt es nicht; es war der Burst, der
//                            `trx:0,false;` mitbringt.
//   tst_tci_audio_roundtrip  "Das Echo muss mulaw8 nennen — bekommen:
//                            protocol:…; device:…; …", also der Burst
//                            selbst, mitten abgeschnitten.
//
// Der Burst endet mit `ready;`. Darauf laesst sich warten, und das haengt
// an nichts als am Protokoll.
//
// Neu ist der Fund nicht: tst_tci_silent_error_invariant hat ihn am
// 2026-09-17 schon einmal gemacht ("on a loaded CI runner the tail of the
// burst arrived AFTER the spy was cleared") und sich mit einer eigenen
// Ruhe-Erkennung beholfen. Dass es danach zweimal dieselbe Falle gab,
// liegt daran, dass die Loesung dort privat blieb. Darum hier als Helfer
// fuer alle.

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWebSocket>

namespace TciTest {

/** Wartet, bis der Server `ready;` geschickt hat.
 *
 *  @param client   die bereits verbundene Verbindung
 *  @param grenzeMs Zeitlimit; kommt in dieser Zeit kein `ready;`, ist etwas
 *                  anderes kaputt und der Pruefstand soll das sagen, statt
 *                  stumm weiterzulaufen.
 *  @param nachlaufMs kurze Ruhe danach, damit ein Nachzuegler hinter
 *                  `ready;` nicht doch noch in die Messung faellt.
 *  @return true, wenn der Burst vollstaendig durch ist.
 *
 *  Erst NACH dieser Funktion mitschreiben — alles davor gehoert dem Burst.
 */
inline bool warteAufReady(QWebSocket& client, int grenzeMs = 8000,
                          int nachlaufMs = 60)
{
    bool durch = false;
    const QMetaObject::Connection horcher =
        QObject::connect(&client, &QWebSocket::textMessageReceived,
                         [&durch](const QString& nachricht) {
                             for (const QString& teil :
                                  nachricht.split(QLatin1Char(';'))) {
                                 if (teil.trimmed().compare(
                                         QLatin1String("ready"),
                                         Qt::CaseInsensitive) == 0) {
                                     durch = true;
                                 }
                             }
                         });
    QElapsedTimer uhr;
    uhr.start();
    while (!durch && uhr.elapsed() < grenzeMs) { QTest::qWait(20); }
    QObject::disconnect(horcher);
    if (durch && nachlaufMs > 0) { QTest::qWait(nachlaufMs); }
    return durch;
}

/** Wartet, bis eine Antwort mit diesem Namen eintrifft.
 *
 *  Gegenstueck zu warteAufReady fuer die Messung danach: ein Befehl wird
 *  geschickt und die Bestaetigung kommt, wenn sie kommt — nicht nach einer
 *  festen Zahl von Millisekunden. Am 2026-10-02 fiel genau daran noch ein
 *  Fall um, nachdem der Burst schon sauber abgewartet wurde: der Besitzer
 *  war gesetzt (der Befehl war also durch), aber das Echo war nach 80 ms
 *  noch unterwegs.
 *
 *  @param gesammelt Liste, in die ein textMessageReceived-Horcher schreibt
 *  @param name      erwarteter Befehlsname, z. B. "trx"
 *  @return true, sobald eine Zeile mit `name:` eintrifft.
 *
 *  Fuer den umgekehrten Fall — es darf NICHTS kommen — taugt diese Funktion
 *  nicht: dort muss man eine Weile warten und danach pruefen, und genau das
 *  tut der Aufrufer weiterhin selbst.
 */
inline bool warteAufAntwort(const QStringList& gesammelt, const QString& name,
                            int grenzeMs = 4000)
{
    QElapsedTimer uhr;
    uhr.start();
    while (uhr.elapsed() < grenzeMs) {
        for (const QString& zeile : gesammelt) {
            for (const QString& teil : zeile.split(QLatin1Char(';'))) {
                if (teil.trimmed().startsWith(name + QLatin1Char(':'),
                                              Qt::CaseInsensitive)) {
                    return true;
                }
            }
        }
        QTest::qWait(20);
    }
    return false;
}

}  // namespace TciTest
