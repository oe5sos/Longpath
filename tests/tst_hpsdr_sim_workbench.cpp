// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_hpsdr_sim_workbench.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// WERKBANK, keine Pruefung im ueblichen Sinn: faehrt Longpaths eigenen
// Verbindungsweg gegen einen HPSDR-Simulator (hpsdrsim aus pihpsdr /
// deskhpsdr, GPL — als Messgeraet benutzt, nicht als Quelle) und sagt,
// was dabei herauskommt. Discovery-Antwort → RadioInfo → RadioModel::
// connectToRadio() → Empfangsstrom → Frequenz setzen → trennen.
//
// Laeuft NUR, wenn LONGPATH_HPSDRSIM gesetzt ist („host:port", z.B.
// 127.0.0.1:1024) — die CI hat keinen Simulator, und ein Test, der
// einen braucht und keinen findet, wuerde dort still scheitern. Mit
// LONGPATH_HPSDRSIM_LOG=<Datei> liest er hinterher das Protokoll des
// Simulators und zeigt, welche Steuerwerte dort ankamen (RX FREQ, Rate,
// PTT, HL2-Bits) — das ist der Teil, den kein Signal-Spy sieht.
//
// Stationen der Werkbank: Discovery → Verbinden → Empfangsstrom →
// Frequenz → Abtastrate → TUNE ein/aus (PTT am Simulator) → Trennen.
// Am Ende muss der Simulator PTT=0 gesehen haben — ein Traeger, der
// nach dem Trennen weiterlaeuft, ist der eine Fehler, den kein
// Hardware-Test verzeiht.
//
// Mit LONGPATH_HPSDRSIM_NO_TX=1 bleibt die TUNE-Station aus (fuer
// Simulator-Varianten ohne Sendezweig).

#include <QtTest>

#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QFile>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSignalSpy>

#include <algorithm>

using namespace Longpath;

class TstHpsdrSimWorkbench : public QObject { Q_OBJECT
private slots:
    void discover_connect_receive_tune_disconnect()
    {
        const QString target = qEnvironmentVariable("LONGPATH_HPSDRSIM");
        if (target.isEmpty()) {
            QSKIP("LONGPATH_HPSDRSIM nicht gesetzt — Werkbank, kein CI-Test.");
        }
        const QStringList hp = target.split(QLatin1Char(':'));
        const QHostAddress addr(hp.value(0, QStringLiteral("127.0.0.1")));
        const quint16 port = static_cast<quint16>(hp.value(1, QStringLiteral("1024")).toUInt());

        // ── 1. Discovery per Unicast-Probe ───────────────────────────
        RadioDiscovery disc;
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);
        QSignalSpy failed(&disc, &RadioDiscovery::probeFailed);
        disc.probeAddress(addr, port);
        QTRY_VERIFY_WITH_TIMEOUT(found.count() > 0 || failed.count() > 0, 8000);
        QVERIFY2(found.count() > 0, "Der Simulator hat auf die Probe nicht geantwortet");
        const RadioInfo info = found.first().first().value<RadioInfo>();
        qInfo() << "DISCOVERED" << info.displayName() << "board" << int(info.boardType)
                << "protocol" << int(info.protocol) << "fw" << info.firmwareVersion
                << "mac" << info.macAddress << "at" << info.address.toString();

        // ── 2. Verbinden ─────────────────────────────────────────────
        RadioModel model;
        QSignalSpy state(&model, &RadioModel::connectionStateChanged);
        QSignalSpy iq(&model, &RadioModel::rawIqData);
        model.connectToRadio(info);
        QTRY_VERIFY_WITH_TIMEOUT(model.connectionState() == ConnectionState::Connected
                                 || model.connectionState() == ConnectionState::Disconnected
                                    && state.count() > 1, 15000);
        qInfo() << "STATE" << int(model.connectionState()) << "transitions" << state.count();
        QVERIFY2(model.connectionState() == ConnectionState::Connected,
                 "Verbindung kam nicht zustande (siehe STATE oben)");

        // ── 3. Empfangsstrom ─────────────────────────────────────────
        QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= 20, 10000);
        qInfo() << "IQ blocks" << iq.count() << "slices" << model.slices().size()
                << "active" << (model.activeSlice() ? "yes" : "no")
                << "isConnected" << model.isConnected()
                << "rateHz" << model.connectionSampleRateHz();
        QVERIFY(model.activeSlice());

        // ── 4. Abstimmen ─────────────────────────────────────────────
        SliceModel* s = model.activeSlice();
        const double before = s->frequency();
        s->setFrequency(14'200'000.0);
        QTest::qWait(600);
        qInfo() << "FREQ before" << before << "after" << s->frequency()
                << "dspMode" << int(s->dspMode());
        QCOMPARE(int(s->frequency()), 14'200'000);

        // ── 4b. Abtastrate ───────────────────────────────────────────
        // Der HPSDR-Weg kennt eine Rate fuer alle Stroeme; wir nehmen die
        // hoechste erlaubte, die nicht schon anliegt, damit im Protokoll
        // ein Wechsel sichtbar wird.
        const QVector<int> rates = model.allowedStreamSampleRates();
        const int rateBefore = model.connectionSampleRateHz();
        int rateWanted = 0;
        for (const int r : rates) {
            if (r != rateBefore) { rateWanted = std::max(rateWanted, r); }
        }
        qInfo() << "RATES allowed" << rates << "before" << rateBefore << "wanted" << rateWanted;
        if (rateWanted > 0) {
            model.requestSliceSampleRate(s->sliceIndex(), rateWanted);
            QTRY_COMPARE_WITH_TIMEOUT(model.connectionSampleRateHz(), rateWanted, 8000);
            QTRY_VERIFY_WITH_TIMEOUT(model.connectionState() == ConnectionState::Connected, 8000);
            const int iqBeforeRate = iq.count();
            QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= iqBeforeRate + 20, 10000);
            qInfo() << "RATE now" << model.connectionSampleRateHz()
                    << "text" << model.connectionSampleRateText()
                    << "IQ blocks since" << (iq.count() - iqBeforeRate);
        }

        // ── 4c. TUNE ein/aus — PTT am Simulator ──────────────────────
        const bool noTx = qEnvironmentVariableIsSet("LONGPATH_HPSDRSIM_NO_TX");
        bool tuned = false;
        if (!noTx) {
            QSignalSpy refused(&model, &RadioModel::tuneRefused);
            model.setTune(true);
            QTest::qWait(1200);
            qInfo() << "TUNE on: isTune" << model.isTune() << "mox" << model.mox()
                    << "refused" << (refused.isEmpty() ? QString() : refused.first().first().toString());
            tuned = model.isTune();
            model.setTune(false);
            QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 8000);
            QTest::qWait(400);
            qInfo() << "TUNE off: isTune" << model.isTune() << "mox" << model.mox();
            QVERIFY2(!model.isTune(), "TUNE liess sich nicht wieder ausschalten");
        }

        // ── 5. Trennen ───────────────────────────────────────────────
        model.disconnectFromRadio();
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Disconnected, 8000);

        // ── 6. Was der Simulator gesehen hat ─────────────────────────
        const QString logPath = qEnvironmentVariable("LONGPATH_HPSDRSIM_LOG");
        if (!logPath.isEmpty()) {
            QTest::qWait(300);
            QFile f(logPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
                QStringList seen;
                for (const QString& l : lines) {
                    if (l.contains(QLatin1String("FREQ")) || l.contains(QLatin1String("Rate"), Qt::CaseInsensitive)
                        || l.contains(QLatin1String("PTT")) || l.contains(QLatin1String("Hermes"))
                        || l.contains(QLatin1String("DEVICE")) || l.contains(QLatin1String("Start"))
                        || l.contains(QLatin1String("Stop")) || l.contains(QLatin1String("ALEX"))
                        || l.contains(QLatin1String("DRIVE")) || l.contains(QLatin1String("RECEIVERS"))
                        || l.contains(QLatin1String("ATT"))) {
                        seen << l.trimmed();
                    }
                }
                const QString all = seen.join(QLatin1Char('\n'));
                qInfo().noquote() << "SIMULATOR SAW:\n  " + seen.join(QStringLiteral("\n  "));
                QVERIFY2(all.contains(QStringLiteral("14200000")),
                         "Die Frequenz 14,2 MHz kam beim Simulator nie an");

                // Abtastrate: Bit-Paar 0..3 = 48/96/192/384 kHz.
                if (rateWanted > 0) {
                    const int code = (rateWanted == 384000) ? 3 : (rateWanted == 192000) ? 2
                                   : (rateWanted == 96000) ? 1 : 0;
                    const QRegularExpression rateRe(
                        QStringLiteral("SampleRate= 0000000%1").arg(code));
                    QVERIFY2(all.contains(rateRe),
                             qPrintable(QStringLiteral("Die Abtastrate %1 (Code %2) kam beim Simulator nie an")
                                        .arg(rateWanted).arg(code)));
                }

                // PTT: bei TUNE muss 1 gekommen sein — und die LETZTE
                // PTT-Zeile muss 0 sein, sonst sendet das Geraet nach dem
                // Trennen weiter.
                QString lastPtt;
                for (const QString& l : seen) {
                    if (l.contains(QLatin1String("PTT="))) { lastPtt = l; }
                }
                if (tuned) {
                    QVERIFY2(all.contains(QStringLiteral("PTT= 00000001")),
                             "TUNE war an, aber der Simulator sah nie PTT=1");
                }
                if (!lastPtt.isEmpty()) {
                    QVERIFY2(lastPtt.contains(QStringLiteral("PTT= 00000000")),
                             qPrintable(QStringLiteral("Letzter PTT-Stand am Simulator ist nicht 0: %1").arg(lastPtt)));
                }
            }
        }
    }
};

QTEST_MAIN(TstHpsdrSimWorkbench)
#include "tst_hpsdr_sim_workbench.moc"
