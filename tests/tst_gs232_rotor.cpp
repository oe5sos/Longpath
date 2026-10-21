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
        {
            RotorLogbookPanel panel(nullptr, nullptr, nullptr);
            panel.ensureRotorForTest();
            QVERIFY(qobject_cast<Gs232Rotor*>(panel.rotorForTest()) != nullptr);
            QCOMPARE(panel.rotorForTest()->description(),
                     QStringLiteral("GS-232 /dev/tty.usbserial-PRUEF, 9600 Bd"));
        }

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
