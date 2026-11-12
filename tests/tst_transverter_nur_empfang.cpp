// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Transverter: nur Empfang (2026-11-08) ────────────────────────────────
//
// Die 2-m-Station: der TS-590 sendet ueber DRV in den MV-2 (IFi), Longpaths
// SDR haengt an IFo2 -- einem EMPFANGSausgang -- und zeigt das Band. Sendet
// das SDR, geht seine Leistung rueckwaerts in den Transverter.
//
// Der Bandplan-Waechter faengt das NICHT: er rechnet 28,300 auf 144,300 um,
// und 2 m ist in Region 1 erlaubt. Darum TransverterSetup::nurEmpfang, ab
// Werk an, und jeder Sendeweg fragt TransverterSetup::sperrtSenden():
//
//   * MOX und TUNE der HPSDR-Geraete ueber die MOX-Pruefung im
//     MoxController (setTune ruft setMox, setMox fragt die Pruefung)
//   * Flex: flexTasten und flexAbstimmen in RadioModel
//
// Die Flex-Wege brauchen eine Maschine; ohne sie kehren beide frueher um.
// Darum liest der Fall dort die Quelle (dasselbe Muster wie
// tst_swr_riegel::jederSendewegFragtDenRiegel).

#include <QtTest>
#include <QCheckBox>
#include <QFile>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TransverterSetup.h"
#include "gui/setup/CompanionRigPage.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstTransverterNurEmpfang : public QObject {
    Q_OBJECT

private:
    static TransverterSetup zweiMeter(bool nurEmpfang)
    {
        TransverterSetup t;
        t.enabled    = true;
        t.ifLowHz    = 28000000;
        t.ifHighHz   = 32000000;
        t.offsetHz   = 116000000;
        t.nurEmpfang = nurEmpfang;
        return t;
    }

private slots:
    void init()    { AppSettings::instance().clear(); TransverterSetup::setCurrent({}); }
    void cleanup() { AppSettings::instance().clear(); TransverterSetup::setCurrent({}); }

    // Ab Werk die sichere Seite -- aber nur wirksam, wenn der Transverter
    // ueberhaupt an ist. Ohne Transverter sendet Longpath wie immer.
    void abWerkNurEmpfangUndNurMitTransverter()
    {
        TransverterSetup t;
        QVERIFY(t.nurEmpfang);
        QVERIFY(!t.sperrtSenden());          // Transverter aus
        t = zweiMeter(true);
        QVERIFY(t.sperrtSenden());
        t.nurEmpfang = false;
        QVERIFY(!t.sperrtSenden());
    }

    // Eine Einstellungsdatei ohne den Schluessel (jede von vor dem
    // 2026-11-08) liest sich als "nur Empfang". Und eine Abwahl bleibt.
    void derSchluesselFehltAlsoNurEmpfang()
    {
        QVERIFY(TransverterSetup::load().nurEmpfang);
        TransverterSetup t = zweiMeter(false);
        t.save();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Transverter/RxOnly")).toString(),
                 QStringLiteral("False"));
        QVERIFY(!TransverterSetup::load().nurEmpfang);
    }

    // Die echte MOX-Pruefung aus RadioModel: MOX und TUNE werden mit Grund
    // abgewiesen. Ohne "nur Empfang" ist es NICHT dieser Grund.
    void moxUndTuneWerdenAbgewiesen()
    {
        RadioModel radio;
        radio.installBandPlanMoxCheckForTest();
        QVERIFY(radio.moxController());
        TransverterSetup::setCurrent(zweiMeter(true));

        QSignalSpy abgewiesen(radio.moxController(), &MoxController::moxRejected);
        radio.moxController()->setMox(true);
        QCOMPARE(abgewiesen.size(), 1);
        QVERIFY2(abgewiesen.last().first().toString().contains(QStringLiteral("receive only")),
                 qPrintable(abgewiesen.last().first().toString()));

        radio.moxController()->setTune(true);
        QCOMPARE(abgewiesen.size(), 2);
        QVERIFY2(abgewiesen.last().first().toString().contains(QStringLiteral("receive only")),
                 qPrintable(abgewiesen.last().first().toString()));
        radio.moxController()->setTune(false);

        TransverterSetup::setCurrent(zweiMeter(false));
        radio.moxController()->setMox(true);
        // Ohne Slice wird ohnehin abgewiesen -- aber nicht mit DIESEM Grund.
        if (abgewiesen.size() == 3) {
            QVERIFY(!abgewiesen.last().first().toString().contains(QStringLiteral("receive only")));
        }
        radio.moxController()->setMox(false);
    }

    void dieFlexWegeFragenDieSperre()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root) + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());
        for (const QString kopf : {QStringLiteral("bool RadioModel::flexTasten("),
                                   QStringLiteral("bool RadioModel::flexAbstimmen(")}) {
            const int a = src.indexOf(kopf);
            QVERIFY2(a >= 0, qPrintable(kopf));
            const int e = src.indexOf(QStringLiteral("\n}\n"), a);
            QVERIFY(e > a);
            QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("sperrtSenden()")),
                     qPrintable(kopf + QStringLiteral(" tastet, ohne nach nur Empfang zu fragen")));
        }
    }

    // Das Kaestchen auf Setup → Companion rig: da, ab Werk an, und eine
    // Abwahl landet in der Einstellung.
    void dasKaestchenAufDerSeite()
    {
        CompanionRigPage p;
        auto* box = p.findChild<QCheckBox*>(QStringLiteral("transverterRxOnly"));
        QVERIFY(box);
        QVERIFY(box->isChecked());
        box->setChecked(false);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Transverter/RxOnly")).toString(),
                 QStringLiteral("False"));
        QVERIFY(!TransverterSetup::load().nurEmpfang);
    }
};

QTEST_MAIN(TstTransverterNurEmpfang)
#include "tst_transverter_nur_empfang.moc"
