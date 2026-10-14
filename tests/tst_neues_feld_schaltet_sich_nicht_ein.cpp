// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall.
//
// Ein neues Feld schaltet sich nicht selbst in eine fertige Anordnung
// (2026-10-14).
//
// Der Betreiber, mit einem Bild: „der equalizer ist im hintergrund beim
// zweiten fenster" -- der Equalizer stand nach dem Start allein im sonst
// leeren Bild, VOR dem Verbinden, obwohl er ihn nie eingeschaltet hatte.
//
// URSACHE, nachgemessen und nicht geraten: ich hatte den Equalizer am
// 2026-10-11 mit `registerApplet("Eq", "Equalizer", true)` angemeldet --
// sichtbar ab Werk. Die Begruendung damals („ein neues Merkmal findet
// niemand, wenn es in einem Menue steckt") uebersieht, wie die Vorgabe
// wirkt: ein Layoutprofil traegt eine Liste der sichtbaren Felder, und
// fuer einen Schluessel, den es dort NICHT gibt, greift die Vorgabe.
// Sein Profil kennt keinen Eintrag „Eq" -- es ist aelter als das Feld --
// also schaltete sich der Equalizer selbst hinein.
//
// Es war NICHT die Verbindungsmaske: eine Sonde am echten Fenster hat
// gezeigt, dass der Equalizer in einem `PanelDocked`-Container sitzt,
// und `hideFloatingWindowsBehindConnectMask()` versteckt nur
// `Floating` und `OverlayDocked` -- zu Recht, denn die Weisung des
// Betreibers lautet „ALLE FLIEGENDEN Fenster gehoeren hinter die
// ConnectMaske". Der Equalizer flog nicht, er war nur sichtbar.
//
// Dieser Fall bewacht die Regel fuer ALLE Applets, nicht nur fuer den
// Equalizer: wer ein neues Feld dazubaut, soll es nicht ab Werk
// einschalten.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletVisibilityController.h"

using namespace Longpath;

class TstNeuesFeldSchaltetSichNichtEin : public QObject {
    Q_OBJECT

private slots:
    void derEqualizerIstNICHTSichtbarAbWerk()
    {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings::instance().clear();
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));

        auto* vis = mw.findChild<AppletVisibilityController*>();
        QVERIFY(vis);
        QVERIFY2(!vis->isVisible(QStringLiteral("Eq")),
                 "der Equalizer schaltet sich ab Werk selbst ein");
        // Er ist aber DA und ueber den Auswaehler zu finden -- sonst
        // waere aus dem einen Fehler der andere geworden.
        QVERIFY2(vis->isAvailable(QStringLiteral("Eq")),
                 "der Equalizer steht gar nicht im Auswaehler");
    }

    void dieSeitAugustGEISTERHAFTENFelderBleibenAus()
    {
        // Dieselbe Regel fuer die anderen, die aus demselben Grund
        // nachgeruestet wurden. CAT war von Anfang an richtig
        // angemeldet -- das ist der Vergleichsfall, der zeigt, dass die
        // Regel nicht neu ist, sondern dass ich sie beim Equalizer
        // gebrochen habe.
        QStandardPaths::setTestModeEnabled(true);
        AppSettings::instance().clear();
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        auto* vis = mw.findChild<AppletVisibilityController*>();
        QVERIFY(vis);
        QVERIFY2(!vis->isVisible(QStringLiteral("Cat")),
                 "CAT schaltet sich ab Werk selbst ein");
    }
};

QTEST_MAIN(TstNeuesFeldSchaltetSichNichtEin)
#include "tst_neues_feld_schaltet_sich_nicht_ein.moc"
