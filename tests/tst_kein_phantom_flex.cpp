// SPDX-License-Identifier: GPL-3.0-or-later
//
// „habe soeben auf der maestro longpath mit flex gesehen ... stand auf
// der liste zum auswählen auf der console" (Betreiber, 2026-10-18) --
// und kurz darauf: „habe jedoch nichts geöffnet."
//
// Richtig gehandelt. Dahinter ist kein Radio.
//
// Aufgeklaert: in der Geraeteliste seines Maestro stand ein
// „FLEX-6400", das er nicht besitzt (er hat eine FLEX-8400, sonst
// nichts). Es war LONGPATH SELBST. `FlexRadioDiscoveryBroadcaster`
// ruft einmal je Sekunde per Broadcast auf UDP 4992 eine
// FlexRadio-Erkennung ins Netz -- Modellname, Spitzname „Longpath",
// Rufzeichen, Seriennummer aus der MAC im Flex-Format. Gebaut, damit
// die PGXL/TGXL-Endstufe Longpath in IHRER Liste findet; gesehen aber
// von JEDEM SmartSDR-Client im selben Netz.
//
// Der Fehler war das eine Tor: der Ausruf hing nur an
// `PGXL_BroadcastDiscovery`, und das steht per Vorgabe auf „True" --
// unabhaengig davon, ob ueberhaupt eine 4O3A-Endstufe da ist. Also
// legte jede Longpath-Installation ein Phantom-Flex in ihr Netz.
//
// Dieser Lauf haelt die REGEL fest, nicht die Verdrahtung: der
// 4O3A-Hauptschalter (Vorgabe „False") muss zustimmen.
//
// Gegenprobe: mit der alten Regel (nur der Feinschalter) faellt der
// erste Fall um -- und genau der ist Martins Maestro.

#include <QtTest>

#include "models/RadioModel.h"

using namespace Longpath;

class TstKeinPhantomFlex : public QObject
{
    Q_OBJECT

private slots:
    void ohneVierNullDreiASchweigtDerAusruf()
    {
        // Martins Lage am 2026-10-18: keine 4O3A-Endstufe, der
        // Feinschalter steht auf seiner Vorgabe „True".
        QVERIFY2(!RadioModel::flexAusrufSollLaufen(false, true),
                 "Longpath ruft sich als FlexRadio aus, obwohl keine "
                 "4O3A-Endstufe da ist -- das Phantom im Maestro");
    }

    void mitVierNullDreiARuftErWiederAus()
    {
        // Wer eine PGXL hat, soll sie weiter koppeln koennen.
        QVERIFY2(RadioModel::flexAusrufSollLaufen(true, true),
                 "mit 4O3A und Feinschalter muss der Ausruf laufen, "
                 "sonst findet die Endstufe Longpath nicht mehr");
    }

    void derFeinschalterBleibtEinSchalter()
    {
        // Wer eine 4O3A hat und den Ausruf trotzdem nicht will.
        QVERIFY(!RadioModel::flexAusrufSollLaufen(true, false));
        // Und beides aus ist erst recht still.
        QVERIFY(!RadioModel::flexAusrufSollLaufen(false, false));
    }
};

QTEST_MAIN(TstKeinPhantomFlex)
#include "tst_kein_phantom_flex.moc"
