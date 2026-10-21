// no-port-check: Longpath-eigener Prüfstand.
// =================================================================
// tests/tst_gs232_rotor.cpp  (Longpath)
// =================================================================
//
// Der serielle GS-232A/B-Treiber, ohne Rotor am Tisch: statt der
// seriellen Schnittstelle hängt ein QBuffer drin (useDeviceForTest).
//
// Geprüft wird das, was am Draht falsch sein kann — und was an einer
// Peilung falsch sein DARF: nichts. Eine halb gelesene Zahl ist eine
// falsche Richtung, und eine alte Zahl ohne Hinweis ist schlimmer als
// gar keine.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Angelegt. Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/Gs232Rotor.h"

#include <cmath>
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

namespace {

// Ein QBuffer taugt hier NICHT: Treiber und Prüfstand schrieben in
// denselben Strom, und die Lesemarke lief durch das, was der Treiber
// gerade hinausgeschickt hatte ("AZ=14" + "C2" wurde zu
// "AZ=14=143 EL=000"). Diese Naht hat zwei getrennte Richtungen, wie
// eine echte Leitung.
class FakeWire : public QIODevice {
    Q_OBJECT
public:
    using QIODevice::QIODevice;

    /// Was der Controller antwortet.
    void antworte(const QByteArray& bytes)
    {
        m_rx.append(bytes);
        emit readyRead();
    }

    /// Was der Treiber hinausgeschickt hat.
    QByteArray gesendet() const { return m_tx; }
    void loescheGesendet() { m_tx.clear(); }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override
    {
        return m_rx.size() + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxlen) override
    {
        const qint64 n = qMin<qint64>(maxlen, m_rx.size());
        memcpy(data, m_rx.constData(), static_cast<size_t>(n));
        m_rx.remove(0, static_cast<int>(n));
        return n;
    }
    qint64 writeData(const char* data, qint64 len) override
    {
        m_tx.append(data, static_cast<int>(len));
        return len;
    }

private:
    QByteArray m_rx;
    QByteArray m_tx;
};

} // namespace

class TestGs232Rotor : public QObject {
    Q_OBJECT

private slots:
    // ── Die Befehle am Draht ─────────────────────────────────────────
    void fahrbefehl_hat_drei_ziffern_je_feld()
    {
        QCOMPARE(Gs232Rotor::moveCommand(292.0), QByteArrayLiteral("W292 000\r"));
        QCOMPARE(Gs232Rotor::moveCommand(7.0),   QByteArrayLiteral("W007 000\r"));
        QCOMPARE(Gs232Rotor::moveCommand(0.0),   QByteArrayLiteral("W000 000\r"));
    }

    void fahrbefehl_faltet_und_klemmt()
    {
        // 361 Grad ist 1 Grad, -5 Grad sind 355 — und 360 selbst ist 0,
        // nicht "360", das der Controller als Fehler lesen wuerde.
        QCOMPARE(Gs232Rotor::moveCommand(361.0), QByteArrayLiteral("W001 000\r"));
        QCOMPARE(Gs232Rotor::moveCommand(-5.0),  QByteArrayLiteral("W355 000\r"));
        QCOMPARE(Gs232Rotor::moveCommand(360.0), QByteArrayLiteral("W000 000\r"));
        // Hoehe geklemmt, nicht gefaltet: ein Rotor, der 200 Grad
        // hochfahren soll, soll am Anschlag stehen, nicht bei 20.
        QCOMPARE(Gs232Rotor::moveCommand(10.0, 200.0), QByteArrayLiteral("W010 180\r"));
        QCOMPARE(Gs232Rotor::moveCommand(10.0, -30.0), QByteArrayLiteral("W010 000\r"));
    }

    void abfrage_und_halt()
    {
        QCOMPARE(Gs232Rotor::pollCommand(), QByteArrayLiteral("C2\r"));
        QCOMPARE(Gs232Rotor::stopCommand(), QByteArrayLiteral("S\r"));
    }

    // ── Die Antworten ────────────────────────────────────────────────
    void liest_die_gs232b_form()
    {
        double az = 0.0, el = 0.0;
        QVERIFY(Gs232Rotor::parsePosition("AZ=302 EL=000", az, el));
        QCOMPARE(az, 302.0);
        QCOMPARE(el, 0.0);
        // Der ERC setzt zwei Leerzeichen.
        QVERIFY(Gs232Rotor::parsePosition("AZ=302  EL=045", az, el));
        QCOMPARE(el, 45.0);
        // Nur Azimut ist auch eine gueltige Antwort.
        QVERIFY(Gs232Rotor::parsePosition("AZ=007", az, el));
        QCOMPARE(az, 7.0);
        QCOMPARE(el, 0.0);
    }

    void liest_die_gs232a_form()
    {
        double az = 0.0, el = 0.0;
        QVERIFY(Gs232Rotor::parsePosition("+0302+0000", az, el));
        QCOMPARE(az, 302.0);
        QCOMPARE(el, 0.0);
    }

    void verweigert_alles_andere()
    {
        double az = 1.0, el = 1.0;
        // Teile, Rauschen, Quittungen -- nichts davon ist eine Peilung.
        for (const char* zeile : {"", "AZ=", "AZ=abc", "+030", "W292 000",
                                  "RPRT 0", "AZ=302 EL=", "?>"}) {
            QVERIFY2(!Gs232Rotor::parsePosition(zeile, az, el), zeile);
        }
        // und die Werte bleiben unberuehrt
        QCOMPARE(az, 1.0);
        QCOMPARE(el, 1.0);
    }

    // ── Am nachgestellten Draht ──────────────────────────────────────
    void lage_kommt_an_und_gilt_als_frisch()
    {
        FakeWire draht;
        QVERIFY(draht.open(QIODevice::ReadWrite));

        Gs232Rotor rotor;
        rotor.useDeviceForTest(&draht);
        QSignalSpy lage(&rotor, &Gs232Rotor::positionChanged);
        QSignalSpy hoehe(&rotor, &Gs232Rotor::elevationChanged);

        // Vor der ersten Lesung ist NICHTS frisch -- auch nicht die 0.
        QVERIFY(!rotor.hasFreshPosition());

        rotor.connectToRotor();
        QCOMPARE(rotor.state(), RotorController::State::Idle);
        // Beim Verbinden wird sofort gefragt, nicht erst nach 500 ms.
        QCOMPARE(draht.gesendet(), QByteArrayLiteral("C2\r"));

        draht.antworte("AZ=143 EL=000\r");
        QTRY_COMPARE(lage.count(), 1);
        QCOMPARE(rotor.azimuth(), 143.0);
        QVERIFY(rotor.hasFreshPosition());
        QCOMPARE(hoehe.count(), 0);   // 0 Grad ist der Anfangswert

        // Und eine Hoehe kommt als eigenes Signal.
        draht.antworte("AZ=143 EL=045\r");
        QTRY_COMPARE(hoehe.count(), 1);
        QCOMPARE(rotor.elevation(), 45.0);
    }

    void halbe_zeile_wird_nicht_als_peilung_gelesen()
    {
        FakeWire draht;
        QVERIFY(draht.open(QIODevice::ReadWrite));
        Gs232Rotor rotor;
        rotor.useDeviceForTest(&draht);
        QSignalSpy lage(&rotor, &Gs232Rotor::positionChanged);
        rotor.connectToRotor();

        // Erst die halbe Antwort: nichts darf daraus werden.
        draht.antworte("AZ=14");
        QCoreApplication::processEvents();
        QCOMPARE(lage.count(), 0);
        QVERIFY(!rotor.hasFreshPosition());

        // Dann der Rest, mit Zeilenende: JETZT ist es eine Peilung.
        draht.antworte("3 EL=000\r");
        QTRY_COMPARE(lage.count(), 1);
        QCOMPARE(rotor.azimuth(), 143.0);
    }

    void fahren_und_halten_gehen_hinaus()
    {
        FakeWire draht;
        QVERIFY(draht.open(QIODevice::ReadWrite));
        Gs232Rotor rotor;
        rotor.useDeviceForTest(&draht);
        rotor.connectToRotor();
        draht.loescheGesendet();

        rotor.moveTo(292.0);
        QCOMPARE(draht.gesendet(), QByteArrayLiteral("W292 000\r"));
        QCOMPARE(rotor.state(), RotorController::State::Moving);

        draht.loescheGesendet();
        rotor.stop();
        QCOMPARE(draht.gesendet(), QByteArrayLiteral("S\r"));
        QCOMPARE(rotor.state(), RotorController::State::Idle);
    }

    void ohne_verbindung_wird_nicht_gedreht()
    {
        Gs232Rotor rotor;
        QSignalSpy fehler(&rotor, &Gs232Rotor::errorOccurred);
        rotor.moveTo(180.0);
        QCOMPARE(fehler.count(), 1);
        QCOMPARE(rotor.state(), RotorController::State::Disconnected);
    }

    // ── Erreichbar? ──────────────────────────────────────────────────
    //
    // Ein Treiber, den die Oberflaeche nie anlegt, ist kein Treiber.
    // Dieser Lauf setzt die Einstellung und sieht nach, WAS entsteht.
    void die_einstellung_waehlt_den_treiber()
    {
        AppSettings& s = AppSettings::instance();
        const QString vorher = s.value(QStringLiteral("Rotor/Backend"),
                                       QStringLiteral("rotctld")).toString();

        s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("gs232"));
        s.setValue(QStringLiteral("Rotor/SerialPort"),
                   QStringLiteral("/dev/tty.usbserial-PRUEF"));
        s.setValue(QStringLiteral("Rotor/Host"), QString{});
        {
            RotorLogbookPanel panel(nullptr, nullptr, nullptr);
            panel.ensureRotorForTest();
            QVERIFY(qobject_cast<Gs232Rotor*>(panel.rotorForTest()) != nullptr);
            QCOMPARE(panel.rotorForTest()->description(),
                     QStringLiteral("GS-232 /dev/tty.usbserial-PRUEF, 9600 Bd"));
        }

        // Und mit Netzziel gewinnt das Netz -- der Fall dieser Station.
        s.setValue(QStringLiteral("Rotor/Host"), QStringLiteral("172.30.30.16"));
        s.setValue(QStringLiteral("Rotor/NetPort"), 4001);
        {
            RotorLogbookPanel panel(nullptr, nullptr, nullptr);
            panel.ensureRotorForTest();
            auto* gs = qobject_cast<Gs232Rotor*>(panel.rotorForTest());
            QVERIFY(gs != nullptr);
            QCOMPARE(gs->description(),
                     QStringLiteral("GS-232 über 172.30.30.16:4001"));
            // Angelegt heisst NICHT verbunden: der Lauf macht hier keine
            // Verbindung zur Station auf.
            QCOMPARE(gs->state(), RotorController::State::Disconnected);
        }
        s.setValue(QStringLiteral("Rotor/Host"), QString{});

        // Und die Vorgabe bleibt der rotctld-Weg: wer nichts einstellt,
        // merkt von der Weiche nichts.
        s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("rotctld"));
        {
            RotorLogbookPanel panel(nullptr, nullptr, nullptr);
            panel.ensureRotorForTest();
            QVERIFY(qobject_cast<Gs232Rotor*>(panel.rotorForTest()) == nullptr);
            QVERIFY(panel.rotorForTest() != nullptr);
        }

        s.setValue(QStringLiteral("Rotor/Backend"), vorher);
    }

    // ── Am ECHTEN Gerät ─────────────────────────────────────────────
    //
    // Läuft nur, wenn LONGPATH_GS232_HOST gesetzt ist — sonst
    // übersprungen, damit CI und jeder Rechner ohne Rotor grün bleiben.
    // Es wird ausschließlich GELESEN: `C2` fragt die Lage, nichts dreht
    // sich. Ein Fahrbefehl gehört nicht in einen Prüflauf.
    void am_echten_geraet_wenn_eines_da_ist()
    {
        const QByteArray host = qgetenv("LONGPATH_GS232_HOST");
        if (host.isEmpty()) {
            QSKIP("kein LONGPATH_GS232_HOST gesetzt — kein Gerät am Tisch");
        }
        const quint16 port = static_cast<quint16>(
            qEnvironmentVariableIntValue("LONGPATH_GS232_PORT") ?: 4001);

        Gs232Rotor rotor;
        rotor.setNetworkTarget(QString::fromLatin1(host), port);
        QSignalSpy lage(&rotor, &Gs232Rotor::positionChanged);
        QSignalSpy fehler(&rotor, &Gs232Rotor::errorOccurred);

        rotor.connectToRotor();
        QTRY_VERIFY_WITH_TIMEOUT(lage.count() > 0 || fehler.count() > 0, 8000);
        if (fehler.count() > 0) {
            QFAIL(qPrintable(fehler.at(0).at(0).toString()));
        }
        QVERIFY(rotor.hasFreshPosition());
        qInfo() << "GS-232 am Draht:" << rotor.description()
                << "meldet" << rotor.azimuth() << "Grad, Höhe"
                << rotor.elevation();
        rotor.disconnectFromRotor();
    }

    // Der Verbinden-Knopf im Rotordialog kann den Treiber wechseln,
    // waehrend das Fenster offen ist. Das ist der Mechanismus dahinter.
    void treiberwechsel_im_laufenden_fenster()
    {
        AppSettings& s = AppSettings::instance();
        const QString vorher = s.value(QStringLiteral("Rotor/Backend"),
                                       QStringLiteral("rotctld")).toString();

        s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("rotctld"));
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        panel.ensureRotorForTest();
        QVERIFY(qobject_cast<Gs232Rotor*>(panel.rotorForTest()) == nullptr);
        RotorController* alt = panel.rotorForTest();

        // Umgestellt wie im Dialog: Einstellung schreiben, dann neu bauen.
        s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("gs232"));
        s.setValue(QStringLiteral("Rotor/Host"), QStringLiteral("172.30.30.16"));
        s.setValue(QStringLiteral("Rotor/NetPort"), 4001);
        panel.rebuildRotorBackendForTest();

        auto* neu = qobject_cast<Gs232Rotor*>(panel.rotorForTest());
        QVERIFY(neu != nullptr);
        QVERIFY(panel.rotorForTest() != alt);
        QCOMPARE(neu->description(), QStringLiteral("GS-232 über 172.30.30.16:4001"));
        QCOMPARE(neu->state(), RotorController::State::Disconnected);

        // Und zurueck -- der Weg darf in beide Richtungen gehen.
        s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("rotctld"));
        panel.rebuildRotorBackendForTest();
        QVERIFY(qobject_cast<Gs232Rotor*>(panel.rotorForTest()) == nullptr);
        QVERIFY(panel.rotorForTest() != nullptr);

        s.setValue(QStringLiteral("Rotor/Host"), QString{});
        s.setValue(QStringLiteral("Rotor/Backend"), vorher);
    }

    // ── Ein Fahrbefehl am ECHTEN Rotor ──────────────────────────────
    //
    // Braucht ZWEI Schalter: LONGPATH_GS232_HOST (wie der Lesetest) und
    // zusätzlich LONGPATH_GS232_MOVE=1. Ohne den zweiten passiert hier
    // nichts — dieser Lauf dreht eine echte Antenne, und das darf nicht
    // aus Versehen geschehen.
    //
    // Er fährt fünf Grad weiter und danach auf den Ausgangswert zurück.
    // Fünf Grad, weil es messbar ist und innerhalb jeder Keule liegt;
    // zurück, weil die Antenne danach stehen soll, wo sie stand.
    void fahrbefehl_am_echten_geraet()
    {
        const QByteArray host = qgetenv("LONGPATH_GS232_HOST");
        if (host.isEmpty() || qEnvironmentVariableIntValue("LONGPATH_GS232_MOVE") != 1) {
            QSKIP("kein LONGPATH_GS232_MOVE=1 — es wird nichts gedreht");
        }
        const quint16 port = static_cast<quint16>(
            qEnvironmentVariableIntValue("LONGPATH_GS232_PORT") ?: 4001);

        Gs232Rotor rotor;
        rotor.setNetworkTarget(QString::fromLatin1(host), port);
        QSignalSpy lage(&rotor, &Gs232Rotor::positionChanged);
        rotor.connectToRotor();
        QTRY_VERIFY_WITH_TIMEOUT(rotor.hasFreshPosition(), 8000);

        const double start = rotor.azimuth();
        const double ziel  = std::fmod(start + 5.0 + 360.0, 360.0);
        qInfo() << "Rotor steht auf" << start << "Grad, fahre auf" << ziel;

        auto wartetBis = [&](double soll) {
            QElapsedTimer uhr; uhr.start();
            while (uhr.elapsed() < 90000) {
                QTest::qWait(500);
                double d = std::fabs(rotor.azimuth() - soll);
                if (d > 180.0) { d = 360.0 - d; }
                if (d <= 2.0) { return true; }
            }
            return false;
        };

        rotor.moveTo(ziel);
        const bool angekommen = wartetBis(ziel);
        qInfo() << "nach dem Fahrbefehl:" << rotor.azimuth() << "Grad";

        // Zurueck, auch wenn der Hinweg misslang -- die Antenne gehoert
        // dem Betreiber, nicht dem Prueflauf.
        rotor.moveTo(start);
        const bool zurueck = wartetBis(start);
        // Den ENDSTAND nennen, nicht nur "angekommen": die Frist oben
        // gibt bei zwei Grad Rest frei, und der Rotor laeuft danach noch
        // etwas weiter. Am 2026-10-21 stand er darum am Ende auf 295,
        // wo er mit 297 angefangen hatte -- der Lauf meldete trotzdem
        // "zurueck", und genau diese Luecke schliesst die Zeile hier.
        QTest::qWait(3000);          // ausrollen lassen
        const double ende = rotor.azimuth();
        double rest = std::fabs(ende - start);
        if (rest > 180.0) { rest = 360.0 - rest; }
        qInfo() << "zurueck auf:" << ende << "Grad -- Rest zum Ausgangswert:"
                << rest << "Grad";

        rotor.disconnectFromRotor();
        QVERIFY2(angekommen, "der Rotor hat das Ziel nicht erreicht");
        QVERIFY2(zurueck, "der Rotor ist nicht auf den Ausgangswert zurueck");
        // Mehr als fuenf Grad Rest waere kein Nachlauf mehr, sondern ein
        // Fehler im Fahrbefehl.
        QVERIFY2(rest <= 5.0,
                 qPrintable(QStringLiteral("Rest zum Ausgangswert: %1 Grad")
                                .arg(rest)));
    }

    void beschreibung_sagt_die_schnittstelle()
    {
        Gs232Rotor rotor;
        QVERIFY(rotor.description().contains(QStringLiteral("keine Schnittstelle")));
        rotor.setPort(QStringLiteral("/dev/tty.usbserial-A1"), 9600);
        QCOMPARE(rotor.description(),
                 QStringLiteral("GS-232 /dev/tty.usbserial-A1, 9600 Bd"));
    }
};

QTEST_MAIN(TestGs232Rotor)
#include "tst_gs232_rotor.moc"
