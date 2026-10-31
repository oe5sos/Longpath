// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Versatz dort, wo er vor dem Betreiber landet.
//
// tst_transverter_versatz prueft die Rechnung. Hier geht es um die
// vier Stellen, die sie BENUTZEN: die Zahl in der VFO-Anzeige, die
// Frequenzachse im Panadapter, die Bandplan-Abschnitte darin und der
// Logbucheintrag.
//
// ── Worauf es hier wirklich ankommt ─────────────────────────────────
//
// Nicht darauf, dass 28,3 zu 144,3 wird -- das steht im anderen
// Pruefstand. Sondern darauf, dass OHNE Transverter nichts passiert.
//
// Das ist keine Formalie. Diese Aenderung greift in den Malcode des
// Panadapters, in die grosse VFO-Zahl und in das Logbuch eines
// Programms, das an Martins Mast haengt und mit dem er Contest fahrt.
// Eine Umrechnung, die „eigentlich nichts tut", tut im Zweifel doch
// etwas: ein llround hier, ein verschobenes Bandplanfenster dort.
// Darum ist die Gleichheit bei ausgeschaltetem Transverter hier die
// Hauptsache und der Transverterfall die Nebensache.
//
// Die Marke selbst wird am Widget geprueft (tst_begleitgeraet_marke).

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtTest>

#include "core/AppSettings.h"
#include "core/TransverterSetup.h"

using namespace Longpath;

namespace {

constexpr qint64 k28_300  = 28300000;
constexpr qint64 k144_300 = 144300000;
constexpr qint64 k14_200  = 14200000;

TransverterSetup zweiMeter()
{
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 28000000;
    t.ifHighHz = 30000000;
    t.offsetHz = 116000000;
    return t;
}

TransverterSetup dreiundzwanzigCm()
{
    // 1296 MHz mit 144-MHz-ZF. Der Fall, bei dem das ZF-Fenster selbst
    // schon ein VHF-Band ist.
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 144000000;
    t.ifHighHz = 146000000;
    t.offsetHz = 1152000000;
    return t;
}

} // namespace

class TstTransverterAnzeige : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        TransverterSetup::setCurrent(TransverterSetup{});
    }

    // ── Die Hauptsache: aus heisst aus ──────────────────────────────

    void ausgeschaltetRechnetNicht()
    {
        QVERIFY(!TransverterSetup::current().active());

        // Beide Richtungen, und zwar bitgleich -- nicht „fast".
        QCOMPARE(TransverterSetup::displayHz(k28_300), k28_300);
        QCOMPARE(TransverterSetup::panHz(k28_300),     k28_300);
        QCOMPARE(TransverterSetup::displayHz(k14_200), k14_200);
        QCOMPARE(TransverterSetup::panHz(k144_300),    k144_300);
    }

    void ausgeschaltetLaesstDasLogbuchInRuhe()
    {
        double  mhz  = 28.3;
        QString band = QStringLiteral("10m");

        QVERIFY(!TransverterSetup::logbuchFrequenz(k28_300, &mhz, &band));
        QCOMPARE(mhz,  28.3);
        QCOMPARE(band, QStringLiteral("10m"));
    }

    void eingeschaltetAberAusserhalbRechnetAuchNicht()
    {
        TransverterSetup::setCurrent(zweiMeter());

        // 20 m mit eingeschaltetem 2-m-Transverter: der Betreiber will
        // 14 MHz sehen, nicht 130. Eine Rechnung, die immer addiert,
        // waere schlimmer als keine.
        QCOMPARE(TransverterSetup::displayHz(k14_200), k14_200);

        double  mhz  = 14.2;
        QString band = QStringLiteral("20m");
        QVERIFY(!TransverterSetup::logbuchFrequenz(k14_200, &mhz, &band));
        QCOMPARE(mhz,  14.2);
        QCOMPARE(band, QStringLiteral("20m"));
    }

    // ── Der Logbucheintrag ──────────────────────────────────────────

    void logbuchTraegtAntenneUndBand()
    {
        TransverterSetup::setCurrent(zweiMeter());

        double  mhz  = 28.3;
        QString band = QStringLiteral("10m");

        QVERIFY(TransverterSetup::logbuchFrequenz(k28_300, &mhz, &band));
        QCOMPARE(mhz,  144.3);
        QCOMPARE(band, QStringLiteral("2m"));
    }

    void logbuchLaesstEinUnbekanntesBandStehen()
    {
        // Ein Versatz, der nirgends in ein Amateurband faellt. Dann
        // bleibt das Band des Aufrufers stehen: ein FALSCHES Band ist
        // schlimmer als ein ungenaues, weil ein ADIF-Satz in ein
        // Diplom-Programm wandert.
        TransverterSetup t;
        t.enabled  = true;
        t.ifLowHz  = 28000000;
        t.ifHighHz = 30000000;
        t.offsetHz = 72000000;       // 28,3 -> 100,3 MHz, kein Band
        TransverterSetup::setCurrent(t);

        double  mhz  = 28.3;
        QString band = QStringLiteral("10m");

        // Gerechnet wird trotzdem -- die Frequenz ist richtig, nur der
        // Bandname fehlt.
        QVERIFY(TransverterSetup::logbuchFrequenz(k28_300, &mhz, &band));
        QCOMPARE(mhz,  100.3);
        QCOMPARE(band, QStringLiteral("10m"));
    }

    void logbuchNimmtAuchNullzeiger()
    {
        TransverterSetup::setCurrent(zweiMeter());
        // Kein Absturz, wenn ein Aufrufer nur die Frequenz will.
        double mhz = 0.0;
        QVERIFY(TransverterSetup::logbuchFrequenz(k28_300, &mhz, nullptr));
        QCOMPARE(mhz, 144.3);
        QVERIFY(TransverterSetup::logbuchFrequenz(k28_300, nullptr, nullptr));
    }

    // ── Die Bandnamen ───────────────────────────────────────────────

    void bandnamenTreffen()
    {
        QCOMPARE(TransverterSetup::adifBand(144300000), QStringLiteral("2m"));
        QCOMPARE(TransverterSetup::adifBand(432200000), QStringLiteral("70cm"));
        QCOMPARE(TransverterSetup::adifBand(1296200000), QStringLiteral("23cm"));
        QCOMPARE(TransverterSetup::adifBand(50150000),  QStringLiteral("6m"));
        QCOMPARE(TransverterSetup::adifBand(70200000),  QStringLiteral("4m"));
    }

    void bandnamenAnDenKanten()
    {
        // Die Grenzen sind die der ADIF-Spezifikation, nicht die des
        // oesterreichischen Bandplans -- ein Austauschformat richtet
        // sich nicht nach dem Land. Was gesendet werden DARF,
        // entscheidet BandPlanGuard, nicht diese Tabelle.
        QCOMPARE(TransverterSetup::adifBand(144000000), QStringLiteral("2m"));
        QCOMPARE(TransverterSetup::adifBand(148000000), QStringLiteral("2m"));
        QVERIFY(TransverterSetup::adifBand(143999999).isEmpty());
        QVERIFY(TransverterSetup::adifBand(148000001).isEmpty());
    }

    void keinBandBeiUnsinn()
    {
        QVERIFY(TransverterSetup::adifBand(0).isEmpty());
        QVERIFY(TransverterSetup::adifBand(-1).isEmpty());
        QVERIFY(TransverterSetup::adifBand(100300000).isEmpty());  // UKW-Radio
    }

    // ── 23 cm: die ZF ist selbst ein VHF-Band ───────────────────────

    void zfKannSelbstEinBandSein()
    {
        TransverterSetup::setCurrent(dreiundzwanzigCm());

        QCOMPARE(TransverterSetup::displayHz(k144_300), qint64(1296300000));

        double  mhz  = 144.3;
        QString band = QStringLiteral("2m");
        QVERIFY(TransverterSetup::logbuchFrequenz(k144_300, &mhz, &band));
        QCOMPARE(mhz,  1296.3);
        QCOMPARE(band, QStringLiteral("23cm"));
    }

    // ── Die prozessweite Fassung ────────────────────────────────────

    void dieFassungFaengtAusAn()
    {
        // Absichtlich NICHT aus der Einstellungsdatei vorbelegt: sonst
        // haengt jeder Pruefstand, der eine Zahl malt, an einer Datei,
        // und in einem Lauf ohne Einstellungen entscheidet die
        // Aufrufreihenfolge, ob der Versatz greift.
        QVERIFY(!TransverterSetup::current().active());
    }

    void setCurrentWirktSofort()
    {
        QCOMPARE(TransverterSetup::displayHz(k28_300), k28_300);
        TransverterSetup::setCurrent(zweiMeter());
        QCOMPARE(TransverterSetup::displayHz(k28_300), k144_300);
        TransverterSetup::setCurrent(TransverterSetup{});
        QCOMPARE(TransverterSetup::displayHz(k28_300), k28_300);
    }

    void halbEingestelltIstAus()
    {
        // Eingeschaltet, aber ohne brauchbares Fenster. configured()
        // faengt das; ohne diese Pruefung waere der Versatz 0 und
        // alles „zufaellig richtig" -- bis jemand den Versatz setzt
        // und das Fenster vergisst.
        TransverterSetup t;
        t.enabled  = true;
        t.ifLowHz  = 0;
        t.ifHighHz = 0;
        t.offsetHz = 116000000;
        TransverterSetup::setCurrent(t);

        QVERIFY(!TransverterSetup::current().active());
        QCOMPARE(TransverterSetup::displayHz(k28_300), k28_300);
    }
    // ── Die Spots muessen mit (2026-10-31) ──────────────────────────
    //
    // Betreiber fragte nach dem Stand fuer 2-m-Contest. Nachgesehen:
    // der Panadapter rechnete Achse, Bandplan und die Marke des
    // Begleitgeraets durch den Transverter -- die SPOTS aber nicht.
    // `drawSpotMarkers` nahm `hzToX(spot.freqMhz * 1e6)` roh.
    //
    // Folge: ein Spot von 144,300 landet auf einer Achse, die 28 MHz
    // zeigt, also weit auszerhalb des Bildes; und ein echter
    // 28-MHz-Spot saesze faelschlich mitten im 2-m-Band. Ein Klick
    // darauf haette die Scheibe auf 144 MHz gestellt -- eine
    // Frequenz, die das Geraet gar nicht kann.
    //
    // Der Lauf liest die Quelle: ein `SpectrumWidget` braucht einen
    // Schirm und eine GPU, und die CI hat keine (2026-10-21).
    void dieSpotsGehenDurchDenTransverter()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/SpectrumWidget.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        const int a = src.indexOf(QStringLiteral("void SpectrumWidget::drawSpotMarkers"));
        QVERIFY2(a > 0, "drawSpotMarkers nicht gefunden");
        const int e = src.indexOf(QStringLiteral("\n}\n"), a);
        QVERIFY(e > a);
        const QString rumpf = src.mid(a, e - a);

        QVERIFY2(!rumpf.contains(QStringLiteral("hzToX(spot.freqMhz * 1.0e6")),
                 "der Spot wird roh auf die Achse gelegt — mit Transverter "
                 "landet er auszerhalb des Bildes");
        QVERIFY2(rumpf.contains(QStringLiteral("zfFuerAchse(spot.freqMhz")),
                 "die Spotfrequenz geht nicht durch den Transverter");

        // Und der Klick stimmt die ZF ab, nicht die Antennenfrequenz.
        const int k = src.indexOf(QStringLiteral("const double freqHz   = "));
        QVERIFY2(k > 0, "die Klickfrequenz ist nicht mehr zu finden");
        QVERIFY2(src.mid(k, 120).contains(QStringLiteral("zfFuerAchse")),
                 "ein Klick auf den Spot stellt die Scheibe auf die "
                 "Antennenfrequenz — die das Geraet nicht kann");
    }

    // Der Bandplan reichte nur bis 54 MHz; mit Transverter zeigte der
    // Panadapter auf 2 m darum gar keine Bandgrenze.
    void derBandplanKenntJetztZweiMeter()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/resources/bandplans/iaru-region1.json"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        QVERIFY2(!d.isNull(), "der Bandplan ist kein gueltiges JSON");
        const QJsonArray segs = d.object().value(QStringLiteral("segments")).toArray();
        bool zweiMeter = false;
        for (const QJsonValue& v : segs) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("low")).toDouble() == 144.0
                && o.value(QStringLiteral("high")).toDouble() == 146.0) {
                zweiMeter = true;
            }
        }
        QVERIFY2(zweiMeter, "144-146 MHz fehlt — auf 2 m zeigt der "
                            "Panadapter keine Bandgrenze");
    }
};

QTEST_MAIN(TstTransverterAnzeige)
#include "tst_transverter_anzeige.moc"
