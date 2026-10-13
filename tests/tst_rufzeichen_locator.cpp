// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall, kein Thetis-Bezug.
//
// Rufzeichen und Locator in den Einstellungen (2026-10-13).
//
// Beide Felder in "Startup & Preferences" waren Attrappen ("future"),
// und das hatte vier Folgen, die niemandem auffielen, weil nichts eine
// Fehlermeldung gab:
//
//   * Der FlexRadio-Rundruf sendete "LONGPATH" statt des Rufzeichens --
//     das sehen SmartSDR und der PGXL im Netz.
//   * Der KiwiSDR meldete sich mit LEEREM Rufzeichen beim Betreiber des
//     Empfaengers an.
//   * Die QSO-Karte zeigte die eigene Station nicht.
//   * Der Satellitendienst rechnet Ueberfluege aus dem Locator. Ohne
//     gueltigen Locator gibt inView() IMMER eine leere Liste zurueck --
//     ein stiller Totalausfall der Satellitenfunktion.
//
// Der letzte Punkt ist der, der hier am genauesten geprueft wird: er ist
// der einzige, dessen Ausfall man am Verhalten des Programms sieht.

#include <QtTest/QtTest>
#include <QLineEdit>

#include "core/AppSettings.h"
#include "core/Maidenhead.h"
#include "core/sat/SatelliteService.h"
#include "gui/setup/GeneralSetupPages.h"

using namespace Longpath;

namespace {

/// Das Eingabefeld hinter einer Beschriftung finden.
QLineEdit* feldMit(QWidget& seite, const QString& platzhalter)
{
    for (QLineEdit* e : seite.findChildren<QLineEdit*>()) {
        if (e->placeholderText().contains(platzhalter)) { return e; }
    }
    return nullptr;
}

} // namespace

class TstRufzeichenLocator : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Pflicht: ohne Testmodus schreibt AppSettings in die echte
        // Einstellungsdatei des Betreibers.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        auto& st = AppSettings::instance();
        st.setValue(QStringLiteral("StationCallsign"), QString{});
        st.setValue(QStringLiteral("StationGridSquare"), QString{});
    }

    // ── Die Schluessel ──────────────────────────────────────────────────────

    void dieFelderSchreibenGENAUDieSchluesselDieGelesenWerden()
    {
        // Der eigentliche Fehler war nicht "nicht verdrahtet", sondern:
        // das Feld hiess "Callsign", und die vier Leser lesen
        // "StationCallsign". Waere es je unter dem Feldnamen verdrahtet
        // worden, haette es unter einem fuenften, ungelesenen Namen
        // gespeichert -- und wieder haette niemand etwas gemerkt.
        StartupPrefsPage seite(nullptr, nullptr);
        QLineEdit* ruf = feldMit(seite, QStringLiteral("OE5SOS"));
        QLineEdit* loc = feldMit(seite, QStringLiteral("JN78"));
        QVERIFY2(ruf, "Kein Rufzeichenfeld gefunden");
        QVERIFY2(loc, "Kein Locatorfeld gefunden");

        ruf->setText(QStringLiteral("oe5sos"));
        emit ruf->editingFinished();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("StationCallsign")).toString(),
                 QStringLiteral("OE5SOS"));     // gross geschrieben

        loc->setText(QStringLiteral("jn78dc"));
        emit loc->editingFinished();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("StationGridSquare")).toString(),
                 QStringLiteral("JN78DC"));
    }

    void dieFelderZeigenBeimOeffnenWasGespeichertIst()
    {
        auto& st = AppSettings::instance();
        st.setValue(QStringLiteral("StationCallsign"), QStringLiteral("OE5SOS"));
        st.setValue(QStringLiteral("StationGridSquare"), QStringLiteral("JN78DC"));
        StartupPrefsPage seite(nullptr, nullptr);
        QCOMPARE(feldMit(seite, QStringLiteral("OE5SOS"))->text(),
                 QStringLiteral("OE5SOS"));
        QCOMPARE(feldMit(seite, QStringLiteral("JN78"))->text(),
                 QStringLiteral("JN78DC"));
    }

    void einUngueltigerLocatorWirdNICHTGespeichert()
    {
        // Sonst stuende Unsinn in den Einstellungen, der Satellitendienst
        // faende weiter nichts, und niemand wuesste warum. Das Feld sagt
        // es stattdessen mit einem roten Rahmen.
        StartupPrefsPage seite(nullptr, nullptr);
        QLineEdit* loc = feldMit(seite, QStringLiteral("JN78"));
        QVERIFY(loc);
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                        QStringLiteral("JN78DC"));
        loc->setText(QStringLiteral("ZZ99zz"));
        emit loc->editingFinished();
        // Der alte, gueltige Wert bleibt stehen.
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("StationGridSquare")).toString(),
                 QStringLiteral("JN78DC"));
        QVERIFY2(!loc->styleSheet().isEmpty(),
                 "Das Feld zeigt den ungueltigen Locator nicht an");
    }

    void einLeererLocatorIstErlaubt()
    {
        StartupPrefsPage seite(nullptr, nullptr);
        QLineEdit* loc = feldMit(seite, QStringLiteral("JN78"));
        QVERIFY(loc);
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                        QStringLiteral("JN78DC"));
        loc->setText(QString{});
        emit loc->editingFinished();
        QVERIFY(AppSettings::instance()
                    .value(QStringLiteral("StationGridSquare")).toString().isEmpty());
        QVERIFY(loc->styleSheet().isEmpty());
    }

    // ── Die Wirkung: der Satellitendienst ───────────────────────────────────

    void derSatellitendienstFindetErstMitLocatorEtwas()
    {
        // DER Fall dieser Datei. Ohne gueltigen Locator gibt
        // SatelliteService::observerFor() false, und inView() damit IMMER
        // eine leere Liste -- gleich wie viele Satelliten am Himmel
        // stehen. Das ist der eine der vier Ausfaelle, den man am
        // Verhalten des Programms sieht.
        auto& st = AppSettings::instance();
        SatelliteService dienst;

        st.setValue(QStringLiteral("StationGridSquare"), QString{});
        Observer o{};
        QVERIFY2(!dienst.observerFor(QString{}, &o),
                 "Ohne Locator liefert observerFor einen Standort -- woher?");

        // Jetzt ueber das Feld setzen, wie der Betreiber es tut.
        StartupPrefsPage seite(nullptr, nullptr);
        QLineEdit* loc = feldMit(seite, QStringLiteral("JN78"));
        QVERIFY(loc);
        loc->setText(QStringLiteral("JN78dc"));
        emit loc->editingFinished();

        QVERIFY2(dienst.observerFor(QString{}, &o),
                 "Mit gesetztem Locator findet observerFor noch keinen Standort");
        // JN78 ist das Maidenhead-Quadrat 14..16 Grad Ost, 48..49 Grad
        // Nord (Feld J = 0..20 O, N = 40..50 N; Quadrat 7 = +14..16,
        // 8 = +48..49). Nachgerechnet, nicht geschaetzt -- der erste
        // Versuch stand auf 15..18 Ost und fiel darum.
        //
        // Grob geprueft, nicht auf die Stelle: die Umrechnung selbst hat
        // ihren eigenen Prueffall. Hier geht es nur darum, dass der
        // Dienst UEBERHAUPT einen plausiblen Standort bekommt.
        QVERIFY2(o.latDeg > 48.0 && o.latDeg < 49.0,
                 qPrintable(QStringLiteral("Breite %1 liegt nicht in JN78")
                                .arg(o.latDeg)));
        QVERIFY2(o.lonDeg > 14.0 && o.lonDeg < 16.0,
                 qPrintable(QStringLiteral("Laenge %1 liegt nicht in JN78")
                                .arg(o.lonDeg)));
    }

    void einUngueltigerLocatorInDenEinstellungenHilftNicht()
    {
        // Gegenstueck: steht dort Unsinn (von Hand eingetragen, oder aus
        // einer alten Fassung), muss der Dienst das als "kein Standort"
        // behandeln und nicht irgendwo rechnen.
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                        QStringLiteral("NICHTSNUTZ"));
        SatelliteService dienst;
        Observer o{};
        QVERIFY(!dienst.observerFor(QString{}, &o));
    }
};

QTEST_MAIN(TstRufzeichenLocator)
#include "tst_rufzeichen_locator.moc"
