// no-port-check: Longpath-original test file.
//
// Der 590 geht aus oder der USB-Stecker faellt -- und kommt wieder.
//
// Bis zum 12.11. blieb Longpath danach stumm: rigctld stand mit einem
// toten Anschluss (oder hatte sich beendet), der Client verband sich
// endlos neu mit etwas, das nie antwortete, und erst Trennen/Verbinden
// von Hand half. Mitten im Contest ist das ein verlorener Sender.
//
// Geprueft mit einem ECHTEN rigctld am Hamlib-Dummy (Modell 1), weil die
// Frage gerade ist, was mit dem Prozess passiert:
//   1. rigctld stirbt (SIGKILL)        -> Wache startet ihn neu.
//   2. rigctld haengt (SIGSTOP)        -> Wache startet ihn neu.
//   3. Gesagt wird es einmal beim Ausfall und einmal beim Wiederkommen.
//
// Ohne rigctld auf dem Rechner wird uebersprungen, nicht gefaelscht.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-12 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>

#include <csignal>
#include <sys/types.h>
#include <signal.h>

#include "core/AppSettings.h"
#include "core/CompanionRigController.h"
#include "core/RigctldClient.h"

using namespace Longpath;

namespace {

void stelleEin(const QString& schluessel, const QString& wert)
{
    AppSettings::instance().setValue(schluessel, wert);
}

quint16 freierPort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost, 0);
    return s.serverPort();
}

int zaehle(const QSignalSpy& spy, const QString& teil)
{
    int n = 0;
    for (const QList<QVariant>& a : spy) {
        if (a.value(0).toString().contains(teil)) { ++n; }
    }
    return n;
}

} // namespace

class Tst590KommtWieder : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("rigctld")).isEmpty()) {
            QSKIP("kein rigctld auf diesem Rechner");
        }
        stelleEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        stelleEin(QStringLiteral("CompanionRigStartDaemon"), QStringLiteral("True"));
        stelleEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
        stelleEin(QStringLiteral("CompanionRigPort"), QString::number(freierPort()));
        stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("1"));
        stelleEin(QStringLiteral("CompanionRigDevice"), QString());
        stelleEin(QStringLiteral("CompanionRigName"), QStringLiteral("Dummy"));
    }

    void gestorben_und_gehaengt_kommt_wieder()
    {
        CompanionRigController rig;
        rig.setWacheMsForTest(300);
        QSignalSpy meldung(&rig, &CompanionRigController::problem);
        rig.setCatBetrieb(true);
        rig.client()->setPollIntervalMs(100);

        QTRY_VERIFY_WITH_TIMEOUT(rig.client()->hasFreshFrequency(), 20000);
        QVERIFY(!rig.ausfallForTest());

        // 1. Gestorben.
        const qint64 erster = rig.daemonPidForTest();
        QVERIFY(erster > 0);
        QCOMPARE(::kill(static_cast<pid_t>(erster), SIGKILL), 0);
        QTRY_VERIFY_WITH_TIMEOUT(rig.ausfallForTest(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.ausfallForTest(), 30000);
        QVERIFY(rig.client()->hasFreshFrequency());
        const qint64 zweiter = rig.daemonPidForTest();
        QVERIFY(zweiter > 0 && zweiter != erster);
        QCOMPARE(zaehle(meldung, QStringLiteral("does not answer")), 1);
        QCOMPARE(zaehle(meldung, QStringLiteral("answers again")), 1);

        // 2. Gehaengt: der Prozess lebt, der Anschluss ist offen, nichts
        // kommt zurueck -- so sieht ein toter USB-Anschluss von aussen aus.
        QCOMPARE(::kill(static_cast<pid_t>(zweiter), SIGSTOP), 0);
        QTRY_VERIFY_WITH_TIMEOUT(rig.ausfallForTest(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.ausfallForTest(), 30000);
        QVERIFY(rig.client()->hasFreshFrequency());
        const qint64 dritter = rig.daemonPidForTest();
        QVERIFY(dritter > 0 && dritter != zweiter);
        ::kill(static_cast<pid_t>(zweiter), SIGKILL);   // falls er noch steht
        QCOMPARE(zaehle(meldung, QStringLiteral("does not answer")), 2);
        QCOMPARE(zaehle(meldung, QStringLiteral("answers again")), 2);

        rig.trenne();
    }

    void getrennt_startet_die_wache_nichts()
    {
        CompanionRigController rig;
        rig.setWacheMsForTest(100);
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.client()->hasFreshFrequency(), 20000);
        rig.trenne();
        QCOMPARE(rig.daemonPidForTest(), qint64(0));
        QTest::qWait(800);
        QCOMPARE(rig.daemonPidForTest(), qint64(0));
        QVERIFY(!rig.ausfallForTest());
    }
};

QTEST_MAIN(Tst590KommtWieder)
#include "tst_590_kommt_wieder.moc"
