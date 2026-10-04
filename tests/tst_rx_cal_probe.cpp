// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test tool, no Thetis logic.
//
// ── Empfangs-Kalibrierung am echten Geraet (Werkbank, 2026-09-26) ──────
//
// Fuer den Test von PR #102 ("dBm-Skala: Kalibrierung auf die Daten"),
// ohne dass jemand am Rechner sitzt. Verbindet das echte MainWindow mit
// einem Geraet (oder dem HPSDR-Simulator), stellt einen stetigen Traeger
// ein und schaltet den Stufenabschwaecher durch. Je Stufe:
//   - S-Meter (WDSP SignalAvg + rxMeterOffsetDb, wie TCI rx_sensors)
//   - Kurve im Durchlass, so wie man sie an der Skala abliest
//   - MaxBin fuers S-Meter (roh + Kalibrierung, wie MeterPoller)
// und ein Bild des Panadapters. Richtig ist: alle drei bleiben stehen
// (+-1..2 dB), waehrend der Abschwaecher springt.
//
// NUR EMPFANG. Dieses Werkzeug ruft keine Sende-Funktion auf (kein MOX,
// kein TUNE, kein Zweiton) und prueft am Ende, dass nie gesendet wurde.
//
// Laeuft nur mit LONGPATH_RX_CAL_TARGET=ip[:port], sonst QSKIP (CI).
//   LONGPATH_RX_CAL_FREQ   Traeger in Hz (Vorgabe 14100000, HL2-Simulator)
//   LONGPATH_RX_CAL_STEPS  Abschwaecher-Stufen, z. B. "0,10,20,30"
//   LONGPATH_GRAB_DIR      Bilder und Tabelle (rx_cal.csv)
//
// Ohne Antenne (nur Rauschboden) geht es auch: dann bleibt das Rohsignal
// am Wandler fast gleich, und die Kalibrierung hebt die Ablesung mit jeder
// Stufe an. Richtig ist dann, dass Kurve und S-Meter GEMEINSAM steigen.
// Deshalb gibt das Werkzeug zusaetzlich den Gleichlauf aus, die Spanne von
// (Kurve - S-Meter). Er muss klein sein, mit und ohne Antenne. Vor PR #102
// war er so gross wie die Abschwaecher-Spanne.
//
// Der HL2-Simulator (hpsdrsim -hermeslite2) daempft den Testton wirklich.
// Dort muss also wie mit Antenne alles stehen bleiben.

#include <QtTest>

#include <cmath>

#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/MoxController.h"
#include "core/StepAttenuatorController.h"
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QSignalSpy>

using namespace Longpath;

namespace {

struct Reading {
    int attDb = 0;
    double sMeter = 0.0;
    double trace = 0.0;
    double maxBin = 0.0;
};

// Was man auf der Skala ABLIEST: der hoechste angezeigte Pixelwert im
// Durchlass. Die Pixelwerte liegen auf derselben Achse wie Gitter und
// Zahlen -- vor PR #102 roh (die Achse wanderte mit der Kalibrierung),
// danach kalibriert (die Achse steht). Nicht aus peakDbmInSlicePassband()
// + Kalibrierung ausrechnen: das waere eine Zahl, nicht die Ablesung.
double shownPassbandPeak(const SpectrumWidget* sw, double vfoHz)
{
    const QVector<float>& px = sw->renderedPixels();
    const int n = px.size();
    const double bw = sw->bandwidth();
    if (n < 2 || bw <= 0.0) { return -400.0; }
    const double left = sw->centerFrequency() - bw / 2.0;
    const double hzPerPx = bw / (n - 1);
    const int a = qBound(0, int(std::lround((vfoHz + sw->filterLowHz() - left) / hzPerPx)), n - 1);
    const int b = qBound(0, int(std::lround((vfoHz + sw->filterHighHz() - left) / hzPerPx)), n - 1);
    float peak = -400.0f;
    for (int i = qMin(a, b); i <= qMax(a, b); ++i) { peak = qMax(peak, px[i]); }
    return peak;
}

// Mittel ueber ein paar Sekunden, damit Rauschen und Mittelung sich legen.
Reading measure(RadioModel* model, SpectrumWidget* sw, double vfoHz, int attDb,
                int settleMs, int sampleMs)
{
    QTest::qWait(settleMs);
    Reading r;
    r.attDb = attDb;
    int n = 0;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < sampleMs) {
        double s = -140.0;
        double mb = -400.0;
        if (auto* wdsp = model->wdspEngine()) {
            if (auto* rx = wdsp->rxChannel(0)) {
                s = rx->getMeter(RxMeterType::SignalAvg) + model->rxMeterOffsetDb();
            }
            const double raw = wdsp->getMaxBinDbm(0);
            mb = raw > -400.0 ? raw + model->rxMeterOffsetDb() : raw;
        }
        const double shown = shownPassbandPeak(sw, vfoHz);
        r.sMeter += s;
        r.maxBin += mb;
        r.trace += shown;
        ++n;
        QTest::qWait(100);
    }
    if (n > 0) { r.sMeter /= n; r.maxBin /= n; r.trace /= n; }
    return r;
}

} // namespace

class TstRxCalProbe : public QObject { Q_OBJECT
private slots:
    void attenuatorStepsKeepTheReading()
    {
        const QString target = qEnvironmentVariable("LONGPATH_RX_CAL_TARGET");
        if (target.isEmpty()) {
            QSKIP("LONGPATH_RX_CAL_TARGET nicht gesetzt -- Werkbank, kein CI-Test.");
        }
        const QStringList hp = target.split(QLatin1Char(':'));
        const QHostAddress addr(hp.value(0));
        const quint16 port = static_cast<quint16>(hp.value(1, QStringLiteral("1024")).toUInt());
        const double freqHz = qEnvironmentVariable("LONGPATH_RX_CAL_FREQ", QStringLiteral("14100000")).toDouble();
        QList<int> steps;
        for (const QString& s : qEnvironmentVariable("LONGPATH_RX_CAL_STEPS", QStringLiteral("0,10,20,30"))
                                    .split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            steps << s.trimmed().toInt();
        }
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) { QDir().mkpath(dir); }

        auto* mw = new MainWindow();
        mw->resize(1680, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);

        bool everTransmitted = false;
        if (MoxController* mox = model->moxController()) {
            connect(mox, &MoxController::moxStateChanged, this,
                    [&everTransmitted](bool on) { if (on) { everTransmitted = true; } });
        }

        RadioDiscovery disc;
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);
        disc.probeAddress(addr, port);
        QTRY_VERIFY_WITH_TIMEOUT(found.count() > 0, 10000);
        const RadioInfo info = found.first().first().value<RadioInfo>();
        qInfo().noquote() << "GERAET" << info.displayName();

        model->connectToRadio(info);
        QTRY_VERIFY_WITH_TIMEOUT(model->connectionState() == ConnectionState::Connected, 20000);
        QTest::qWait(1500);

        SliceModel* slice = model->activeSlice();
        QVERIFY(slice);
        slice->setDspMode(DSPMode::AM);
        slice->setFrequency(freqHz);

        SpectrumWidget* sw = mw->activeSpectrumWidget();
        QVERIFY(sw);
        StepAttenuatorController* att = model->stepAttController();
        QVERIFY2(att, "kein Stufenabschwaecher");
        const bool attWasEnabled = att->stepAttEnabled();
        const int attWas = att->attenuatorDb();
        att->setStepAttEnabled(true);

        QList<Reading> readings;
        for (int db : steps) {
            att->setAttenuation(db);
            Reading r = measure(model, sw, slice->frequency(), db, 3500, 2500);
            r.attDb = att->attenuatorDb();
            readings << r;
            qInfo().noquote() << QStringLiteral("ATT %1 dB | S-Meter %2 | Kurve abgelesen %3 | MaxBin %4 | Kal %5")
                                     .arg(r.attDb, 3)
                                     .arg(r.sMeter, 7, 'f', 1)
                                     .arg(r.trace, 7, 'f', 1)
                                     .arg(r.maxBin, 7, 'f', 1)
                                     .arg(sw->dbmCalOffset(), 6, 'f', 1);
            if (!dir.isEmpty()) {
                const QImage img = sw->grabFramebuffer();
                if (!img.isNull()) {
                    img.save(dir + QStringLiteral("/rx_cal_att%1.png").arg(r.attDb, 2, 10, QLatin1Char('0')));
                }
            }
        }

        // Zurueck, wie es war.
        att->setAttenuation(attWas);
        att->setStepAttEnabled(attWasEnabled);
        QTest::qWait(500);

        if (!dir.isEmpty()) {
            QFile f(dir + QStringLiteral("/rx_cal.csv"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write("att_db;smeter_dbm;trace_dbm;maxbin_dbm;trace_minus_smeter_db\n");
                for (const Reading& r : readings) {
                    f.write(QStringLiteral("%1;%2;%3;%4;%5\n").arg(r.attDb)
                                .arg(r.sMeter, 0, 'f', 2).arg(r.trace, 0, 'f', 2)
                                .arg(r.maxBin, 0, 'f', 2).arg(r.trace - r.sMeter, 0, 'f', 2).toUtf8());
                }
            }
        }

        // Spannweite je Groesse ueber alle Stufen -- die eigentliche Antwort.
        auto span = [&readings](double Reading::*m) {
            double lo = 1e9, hi = -1e9;
            for (const Reading& r : readings) { lo = qMin(lo, r.*m); hi = qMax(hi, r.*m); }
            return hi - lo;
        };
        qInfo().noquote() << QStringLiteral("SPANNE ueber die Stufen: S-Meter %1 dB, Kurve %2 dB, MaxBin %3 dB")
                                 .arg(span(&Reading::sMeter), 0, 'f', 1)
                                 .arg(span(&Reading::trace), 0, 'f', 1)
                                 .arg(span(&Reading::maxBin), 0, 'f', 1);
        // Gleichlauf: gilt auch ohne Antenne, wo die Ablesung mit der Stufe
        // steigen MUSS (siehe Kopf).
        double lo = 1e9, hi = -1e9;
        for (const Reading& r : readings) {
            lo = qMin(lo, r.trace - r.sMeter);
            hi = qMax(hi, r.trace - r.sMeter);
        }
        qInfo().noquote() << QStringLiteral("GLEICHLAUF Kurve - S-Meter: Spanne %1 dB").arg(hi - lo, 0, 'f', 1);

        model->disconnectFromRadio();
        QTest::qWait(1000);
        QVERIFY2(!everTransmitted, "Es wurde gesendet -- darf in diesem Werkzeug nie passieren");
        delete mw;
    }
};

QTEST_MAIN(TstRxCalProbe)
#include "tst_rx_cal_probe.moc"
