// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Auto-SQL fuer die Flex (2026-11-08) ─────────────────────────────
//
// Betreiber: „sql muss schon fast bei 100 prozent sein, um das rauschen
// weg zu bekommen". Die Maschine liest die Stufe absolut (dBm + 160,
// AetherSDR SpectrumWidget.cpp:4642 [@d58e2b8a]); auf 40 m abends lag
// der Pegel der Scheibe bei -88 dBm. Jetzt: die Stufe folgt dem
// gemessenen Rauschen, der Regler stellt den Abstand.
//
//   1. Die Skala und Martins Messung (p10 -90,2 dBm + 20 dB = Stufe 90).
//   2. Eine sprechende Station hebt den Bezug nicht; ein Traeger ohne
//      Pause schon (die Grenze, festgehalten).
//   3. Faellt das Rauschen, folgt die Schwelle schnell, steigt es,
//      langsam.
//   4. Gesendet wird nicht bei jeder Probe.
//   5. RadioModel: gemessen wird immer, gesendet nur in AUTO.
//   6. Der Knopf: Aus -> SQL -> AUTO -> Aus an der Flex, sonst nur an/aus.
//   7. Der Flex-Weg schickt in AUTO keine Hand-Stufe hinterher.

#include <QtTest/QtTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStandardPaths>

#include "FensterFrist.h"
#include "core/AppSettings.h"
#include "core/FlexAutoRauschsperre.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;
using F = FlexAutoRauschsperre;

namespace {
// Rauschen der Scheibe wie in Martins Protokoll: um -88 dBm, ruhig
// (p10 -90,2 / p50 -88,2), zehn Proben je Sekunde.
double rauschen(int i, double mitte = -88.2)
{
    static const double muster[] = {-2.0, 0.4, -0.8, 1.3, -1.5, 0.2, 2.1, -0.3, 0.9, -1.1};
    return mitte + muster[i % 10];
}
void fuettere(F& f, qint64& t, int proben, const std::function<double(int)>& pegel)
{
    for (int i = 0; i < proben; ++i) {
        f.messung(pegel(i), t);
        t += 100;
    }
}
QPushButton* sqlKnopf(RxApplet& a)
{
    for (QPushButton* b : a.findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("SQL") || b->text() == QStringLiteral("AUTO")) {
            if (b->isCheckable()) { return b; }
        }
    }
    return nullptr;
}
QSlider* sqlRegler(RxApplet& a, QPushButton* btn)
{
    for (QSlider* s : a.findChildren<QSlider*>()) {
        if (s->parentWidget() == btn->parentWidget()
            && std::abs(s->geometry().center().y() - btn->geometry().center().y()) < 12) {
            return s;
        }
    }
    return nullptr;
}
} // namespace

class TstFlexAutoSql : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void dieSkalaDerMaschine()
    {
        QCOMPARE(F::stufeFuerDbm(-70.2), 90);
        QCOMPARE(F::stufeFuerDbm(-60.0), 100);
        QCOMPARE(F::stufeFuerDbm(-30.0), 100);     // geklemmt
        QCOMPARE(F::stufeFuerDbm(-200.0), 1);      // 0 waere „immer offen"
        QCOMPARE(F::dbmFuerStufe(90), -70.0);
    }

    // Martins Abend auf 40 m: mit der Vorgabe landet die Automatik dort,
    // wo er den Regler von Hand hinstellte (83..98, zuletzt um 90).
    void martinsMessungErgibtStufeNeunzig()
    {
        F f;
        qint64 t = 1000;
        fuettere(f, t, F::kMindestProben - 1, [](int i) { return rauschen(i); });
        QVERIFY(!f.rauschDbm());                   // zu wenig Proben
        fuettere(f, t, 200, [](int i) { return rauschen(i); });
        QVERIFY(f.rauschDbm());
        QVERIFY2(*f.rauschDbm() < -89.5 && *f.rauschDbm() > -91.0,
                 qPrintable(QString::number(*f.rauschDbm())));
        QCOMPARE(f.marge(), F::kMargeVorgabeDb);
        QCOMPARE(*f.stufe(), 90);
        QCOMPARE(*f.faellig(t), 90);
    }

    // Eine Station spricht im Durchlass, 70 % der Zeit bei -60 dBm, in
    // den Pausen Rauschen. Der Bezug bleibt das Rauschen -- die Sperre
    // laesst sie durch (Stufe unter -60 + 160 = 100).
    void eineSprechendeStationHebtDenBezugNicht()
    {
        F f;
        qint64 t = 1000;
        fuettere(f, t, 300, [](int i) { return (i % 10) < 7 ? -60.0 : rauschen(i); });
        QVERIFY(f.rauschDbm());
        QVERIFY2(*f.rauschDbm() < -86.0, qPrintable(QString::number(*f.rauschDbm())));
        QVERIFY(*f.stufe() < F::stufeFuerDbm(-60.0));
    }

    // Die Grenze, festgehalten: ein Traeger ohne jede Pause ueber das
    // ganze Fenster wird selbst zum Bezug.
    void einTraegerOhnePauseWirdZumBezug()
    {
        F f;
        qint64 t = 1000;
        fuettere(f, t, 100, [](int i) { return rauschen(i); });
        fuettere(f, t, 210, [](int) { return -60.0; });
        QCOMPARE(*f.rauschDbm(), -60.0);
    }

    void faelltSchnellSteigtLangsam()
    {
        F f;
        qint64 t = 1000;
        fuettere(f, t, 200, [](int i) { return rauschen(i, -80.0); });
        // 10 dB weniger: nach 2,5 s ist der Bezug unten, bis auf gut
        // einen dB -- das Zehntel des Fensters besteht dann aus den
        // neuen Proben, aber aus deren oberem Teil.
        fuettere(f, t, 25, [](int i) { return rauschen(i, -90.0); });
        QVERIFY2(*f.rauschDbm() < -88.0, qPrintable(QString::number(*f.rauschDbm())));
        // 10 dB mehr: nach 5 s steht er noch unten ...
        fuettere(f, t, 200, [](int i) { return rauschen(i, -90.0); });
        fuettere(f, t, 50, [](int i) { return rauschen(i, -80.0); });
        QVERIFY2(*f.rauschDbm() < -90.0, qPrintable(QString::number(*f.rauschDbm())));
        // ... nach 20 s ist er oben.
        fuettere(f, t, 160, [](int i) { return rauschen(i, -80.0); });
        QVERIFY2(*f.rauschDbm() > -83.0, qPrintable(QString::number(*f.rauschDbm())));
    }

    // Die Zeit des Sendens ist hier eine eigene Uhr: faellig() fragt
    // nur, wie lange die letzte Stufe her ist.
    void gesendetWirdNichtBeiJederProbe()
    {
        F f;
        qint64 t = 1000;
        fuettere(f, t, 50, [](int) { return -90.0; });
        QCOMPARE(*f.faellig(6000), 90);                // die erste: sofort
        QVERIFY(!f.faellig(9000));                     // gleich: nichts
        // Eine Stufe Unterschied: erst nach zehn Sekunden.
        fuettere(f, t, 250, [](int) { return -89.0; });
        QCOMPARE(*f.stufe(), 91);
        QVERIFY(!f.faellig(11000));
        QCOMPARE(*f.faellig(16000), 91);
        // Vier Stufen: nach einer Sekunde.
        fuettere(f, t, 250, [](int) { return -93.0; });
        QVERIFY(!f.faellig(16500));
        QCOMPARE(*f.faellig(17000), 87);
        // Den Abstand verstellen: sofort.
        f.setMarge(10);
        QCOMPARE(*f.faellig(17001), 77);
        // Neu messen: ohne Proben keine Stufe, danach sofort.
        f.zuruecksetzen();
        QVERIFY(!f.faellig(30000));
        fuettere(f, t, F::kMindestProben, [](int) { return -93.0; });
        QCOMPARE(*f.faellig(17002), 77);
    }

    void derAbstandIstGeklemmtUndGespeichert()
    {
        F f;
        f.setMarge(0);
        QCOMPARE(f.marge(), F::kMargeMinDb);
        f.setMarge(99);
        QCOMPARE(f.marge(), F::kMargeMaxDb);
        F::speichereMarge(15);
        QCOMPARE(F::ladeMarge(), 15);
        F::speichereMarge(F::kMargeVorgabeDb);
    }

    // ── RadioModel: gemessen wird immer, gesendet nur in AUTO ────────
    void radioModelStelltDieStufeErstInAuto()
    {
        RadioModel radio;
        QSignalSpy gestellt(&radio, &RadioModel::flexAutoRauschsperreGestellt);
        qint64 t = 1000;
        for (int i = 0; i < 50; ++i) { radio.flexPegelFuerTest(-90.0, t); t += 100; }
        QCOMPARE(gestellt.count(), 0);
        radio.setFlexAutoMarge(F::kMargeVorgabeDb);
        radio.setFlexAutoRauschsperre(true);
        QCOMPARE(gestellt.count(), 1);                         // sofort, schon gemessen
        QCOMPARE(gestellt.at(0).at(1).toInt(), 90);
        radio.setFlexAutoMarge(10);
        QCOMPARE(gestellt.count(), 2);
        QCOMPARE(gestellt.at(1).at(1).toInt(), 80);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("FlexAutoSqlMarginDb")).toInt(), 10);
        radio.setFlexAutoRauschsperre(false);
        for (int i = 0; i < 50; ++i) { radio.flexPegelFuerTest(-70.0, t); t += 100; }
        QCOMPARE(gestellt.count(), 2);
        radio.setFlexAutoMarge(F::kMargeVorgabeDb);
    }

    // ── Der Knopf ────────────────────────────────────────────────────
    void derKnopfHatAnDerFlexDreiStellungen()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        SliceModel* s = model.slices().at(a);
        s->setDspMode(DSPMode::LSB);
        s->setSsqlThresh(40.0);
        RxApplet applet(s, &model);
        applet.resize(400, 600);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        bool flex = true;
        applet.setAutoSqlVerfuegbar([&flex]() { return flex; });
        applet.setAutoSqlMarge(20);
        QPushButton* btn = sqlKnopf(applet);
        QVERIFY(btn);
        QSlider* regler = sqlRegler(applet, btn);
        QVERIFY(regler);
        QSignalSpy marge(&applet, &RxApplet::autoSqlMargeGeaendert);

        btn->click();                                         // Aus -> SQL
        QVERIFY(s->ssqlEnabled());
        QVERIFY(!s->ssqlAuto());
        QCOMPARE(btn->text(), QStringLiteral("SQL"));
        QCOMPARE(regler->maximum(), 100);
        QCOMPARE(regler->value(), 40);

        btn->click();                                         // SQL -> AUTO
        QVERIFY(s->ssqlEnabled());
        QVERIFY(s->ssqlAuto());
        QVERIFY(btn->isChecked());
        QCOMPARE(btn->text(), QStringLiteral("AUTO"));
        QCOMPARE(regler->minimum(), F::kMargeMinDb);
        QCOMPARE(regler->maximum(), F::kMargeMaxDb);
        QCOMPARE(regler->value(), 20);

        regler->setValue(25);                                 // Abstand, nicht Stufe
        QCOMPARE(marge.count(), 1);
        QCOMPARE(marge.at(0).at(0).toInt(), 25);
        QCOMPARE(s->ssqlThresh(), 40.0);
        applet.setAutoSqlStand(-90.2, 85);
        QVERIFY(regler->toolTip().contains(QStringLiteral("level 85")));

        btn->click();                                         // AUTO -> Aus
        QVERIFY(!s->ssqlEnabled());
        QVERIFY(!s->ssqlAuto());
        QVERIFY(!btn->isChecked());
        QCOMPARE(btn->text(), QStringLiteral("SQL"));
        QCOMPARE(regler->maximum(), 100);
        QCOMPARE(regler->value(), 40);

        // Ohne Flex: an und aus, nie AUTO.
        flex = false;
        btn->click();
        btn->click();
        QVERIFY(!s->ssqlEnabled());
        QVERIFY(!s->ssqlAuto());
        QCOMPARE(btn->text(), QStringLiteral("SQL"));
    }

    // ── Der Flex-Weg ─────────────────────────────────────────────────
    //
    // MainWindow_Flex verdrahtet die Rauschsperre selbst (siehe
    // tst_flex_rauschsperre). In AUTO darf von dort keine Hand-Stufe
    // hinausgehen -- sie ueberschriebe die Automatik bei jeder
    // Reglerbewegung.
    void derFlexWegSchicktInAutoKeineHandStufe()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root) + QStringLiteral("/src/gui/MainWindow_Flex.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());
        const int a = src.indexOf(QStringLiteral("auto sperreHinaus"));
        QVERIFY2(a > 0, "sperreHinaus ist weg");
        const int e = src.indexOf(QStringLiteral("\n        };"), a);
        QVERIFY(e > a);
        const QString rumpf = src.mid(a, e - a);
        const int auto_ = rumpf.indexOf(QStringLiteral("if (automatik) { return; }"));
        const int hand  = rumpf.indexOf(QStringLiteral("sendeFlexRauschsperre"));
        QVERIFY2(rumpf.contains(QStringLiteral("setFlexAutoRauschsperre")),
                 "der Flex-Weg schaltet die Automatik nicht");
        QVERIFY2(auto_ > 0 && hand > auto_,
                 "in AUTO geht die Hand-Stufe trotzdem hinaus");
        QVERIFY2(src.contains(QStringLiteral("&SliceModel::ssqlAutoChanged")),
                 "der Wechsel nach AUTO erreicht die Maschine nicht");
    }
};

QTEST_MAIN(TstFlexAutoSql)
#include "tst_flex_auto_sql.moc"
