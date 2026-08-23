#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include "core/Rf2ksConnection.h"

using namespace Longpath;

class RecordingAmpServer : public QTcpServer {
    Q_OBJECT
public:
    RecordingAmpServer() {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]{
            auto* s = nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, this, [this, s]{
                m_requests << s->readAll();
                s->write("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
                s->flush();
                s->disconnectFromHost();
            });
        });
    }
    quint16 port() const { return serverPort(); }
    QList<QByteArray> requests() const { return m_requests; }
private:
    QList<QByteArray> m_requests;
};

class Rf2ksConnectionControlTest : public QObject {
    Q_OBJECT
private slots:
    void putAntennasActive();
    void putOperateMode();
    void putOperationalInterface();
    void postErrorReset();
    void writeAcksAreNotParsedAsState();
};

// Find the first captured request that starts with the given prefix.
static QByteArray findRequest(const QList<QByteArray>& reqs, const QByteArray& prefix)
{
    for (const QByteArray& r : reqs) {
        if (r.startsWith(prefix)) {
            return r;
        }
    }
    return {};
}


// ── Warten BIS, nicht warten FUER ───────────────────────────────────
//
// Hier stand fuenfmal QTest::qWait(200) beziehungsweise (300): ein
// FESTER Zeitraum fuer einen HTTP-Umlauf zum oertlichen Pruefserver.
// Auf einer ruhigen Maschine reicht das; unter "ctest -j8" nicht
// immer, und dann faellt der Test aus einem Grund, der mit dem
// Geprueften nichts zu tun hat.
//
// Beobachtet am 2026-08-23, zweimal in etwa fuenfzehn Gate-Laeufen.
//
// QTRY_VERIFY_WITH_TIMEOUT wartet, BIS die Bedingung stimmt, hoechstens
// aber die genannte Zeit. Im Gutfall ist es SCHNELLER als das feste
// Warten (es kehrt zurueck, sobald die Anfrage da ist), im schlechten
// gibt es fuenf Sekunden statt zweihundert Millisekunden Luft.
//
// Die Pruefung selbst ist unveraendert: es geht weiter darum, ob die
// richtige Anfrage mit dem richtigen Rumpf auf der Leitung landet.

void Rf2ksConnectionControlTest::putAntennasActive() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    // High poll interval - keeps polling noise out of the 200 ms window.
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.setActiveAntenna(RfKitAntenna::Type::Internal, 2);
    QTRY_VERIFY_WITH_TIMEOUT(
        !findRequest(server.requests(), "PUT /antennas/active ").isEmpty(), 5000);
    const QByteArray req = findRequest(server.requests(), "PUT /antennas/active ");
    // QJsonObject sorts keys alphabetically: "number" < "type"
    QVERIFY(req.contains(R"("number":2)"));
    QVERIFY(req.contains(R"("type":"INTERNAL")"));
}

void Rf2ksConnectionControlTest::putOperateMode() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.setOperateMode("OPERATE");
    QTRY_VERIFY_WITH_TIMEOUT(
        !findRequest(server.requests(), "PUT /operate-mode ").isEmpty(), 5000);
    const QByteArray req = findRequest(server.requests(), "PUT /operate-mode ");
    QVERIFY(req.contains(R"("operate_mode":"OPERATE")"));
}

void Rf2ksConnectionControlTest::putOperationalInterface() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.setOperationalInterface("TCI");
    QTRY_VERIFY_WITH_TIMEOUT(
        !findRequest(server.requests(), "PUT /operational-interface ").isEmpty(), 5000);
    const QByteArray req = findRequest(server.requests(), "PUT /operational-interface ");
    QVERIFY(req.contains(R"("operational_interface":"TCI")"));
}

void Rf2ksConnectionControlTest::postErrorReset() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.resetError();
    QTRY_VERIFY_WITH_TIMEOUT(
        !findRequest(server.requests(), "POST /error/reset ").isEmpty(), 5000);
    const QByteArray req = findRequest(server.requests(), "POST /error/reset ");
}

// Review blocker [P2] on PR #291: successful write acknowledgements were
// routed back through onReplyFinished() -> handleResponse() using the same
// state paths as GET polling.  The amp answers a write with
// "Content-Length: 0" (see RecordingAmpServer above, which mirrors it), and
// QJsonDocument::fromJson("") yields an empty object -- so a successful
// PUT /operate-mode published operateModeUpdated("") and wiped the cached
// mode, and PUT /antennas/active published antenna number 0.
void Rf2ksConnectionControlTest::writeAcksAreNotParsedAsState() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    // High poll interval keeps GET polling out of the observation window, so
    // any emission below can only have come from the write acknowledgement.
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());

    QSignalSpy modeSpy(&conn, &Rf2ksConnection::operateModeUpdated);
    QSignalSpy antSpy(&conn, &Rf2ksConnection::activeAntennaUpdated);

    conn.setOperateMode(QStringLiteral("operate"));
    conn.setActiveAntenna(RfKitAntenna::Type::Internal, 2);
    // Auf BEIDE Schreibvorgaenge warten, nicht auf eine feste Zeit.
    QTRY_VERIFY_WITH_TIMEOUT(
        !findRequest(server.requests(), "PUT /operate-mode ").isEmpty(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(
        !findRequest(server.requests(), "PUT /antennas/active ").isEmpty(), 5000);

    // Danach noch eine kurze Runde, damit eine faelschlich ausgeloeste
    // Meldung Zeit haette anzukommen. Ohne die bestuende die Pruefung
    // unten auch dann, wenn das Fehlverhalten nur langsam ist.
    QTest::qWait(100);

    // ...without their empty-bodied acks being mistaken for state.
    QVERIFY2(modeSpy.isEmpty(),
             "write ack parsed as state: operateModeUpdated emitted from a "
             "Content-Length: 0 PUT reply");
    QVERIFY2(antSpy.isEmpty(),
             "write ack parsed as state: activeAntennaUpdated emitted from a "
             "Content-Length: 0 PUT reply");
}

QTEST_MAIN(Rf2ksConnectionControlTest)
#include "tst_rf2ks_connection_control.moc"
