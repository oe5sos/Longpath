// =================================================================
// tests/tst_pgxl_tgxl_abbau.cpp  (Longpath)
// =================================================================
// Longpath-eigener Prueffall, kein Gegenstueck bei AetherSDR -- dort
// steckt der Fehler, den dieser Fall festnagelt, noch drin.
//
// WAS HIER GEPRUEFT WIRD
//
// PgxlConnection und TgxlConnection erklaeren `QTcpSocket m_socket`
// VOR fuenf QTimer-Mitgliedern. Beim Abbau werden Mitglieder in
// UMGEKEHRTER Reihenfolge zerstoert: die Zeitgeber zuerst, der Socket
// danach. ~QTcpSocket ruft disconnectFromHost(), das loest
// disconnected() aus, und das haengt noch an onDisconnected() -- die
// QObject-Basis lebt zu dem Zeitpunkt ja noch. onDisconnected() greift
// dann nach m_pollTimer.stop() und m_keepaliveTimer.stop(), beide
// schon abgebaut, und meldet ausserdem disconnected() aus einem halb
// abgebauten Objekt heraus.
//
// WAS DIESER FALL IST -- UND WAS NICHT
//
// Der Fehler hat zwei Haelften, und sie sind NICHT gleich gut pruefbar.
// Beim ersten Entwurf dieser Datei stand hier, es sei ein Rauchmelder
// und kein Tor; die Gegenprobe hat das widerlegt, und darum steht es
// jetzt richtig da:
//
//   * DETERMINISTISCH, ein Tor. onDisconnected() meldet disconnected()
//     aus einem halb abgebauten Objekt heraus. Das ist von aussen
//     sichtbar: ein fremder Empfaenger, der den Abbau ueberlebt, zaehlt
//     die Meldung. Gemessen ohne die Behebung: 1 statt 0, bei PGXL und
//     TGXL gleich, in jedem Lauf.
//
//   * UNBESTIMMT, ein Rauchmelder. Der Zugriff auf die schon
//     abgebauten m_pollTimer / m_keepaliveTimer. Sieht der freigegebene
//     Speicher harmlos aus, passiert nichts Sichtbares. Beim
//     Geschwisterfall in SpeConnection meldete sich derselbe Fehler als
//     Warnung ("Timers cannot be started from another thread"), weil
//     dort ein abgebauter Zeitgeber GESTARTET wurde. Hier wird nur
//     gestoppt -- scheduleReconnect() nimmt QTimer::singleShot und
//     nicht das Mitglied --, und ein stop() auf abgebautem Speicher
//     geht meist stumm durch. Die Warnungspruefung unten faengt diese
//     Haelfte also nur, wenn man Glueck hat; deterministisch zeigt sie
//     nur der Debug-Bau mit AddressSanitizer.
//
// Das Tor allein genuegt: dieselbe eine Zeile im Destruktor
// (m_socket.disconnect(this)) behebt beide Haelften, und die eine
// Haelfte, die messbar ist, bewacht sie damit mit.
// =================================================================
// Modification history (Longpath):
//   2026-10-10  Created by Martin Fischer (OE5SOS), KI-gestuetzt via
//                 Anthropic Claude Code. Dritte und vierte Stelle
//                 derselben Fehlerform; die ersten zwei waren
//                 SpeConnection und MainWindow (2026-10-09).
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "core/PgxlConnection.h"
#include "core/TgxlConnection.h"

namespace {

// Nimmt eine Verbindung an und schickt genau EINE Zeile: die
// Versionsmeldung. Weniger geht nicht -- beide Klassen melden
// connected() erst, wenn die V-Zeile da ist (processLine), nicht schon
// beim TCP-Handschlag. Danach schweigt sie; der Fehler sitzt im Abbau,
// nicht im Gespraech.
class VersionsGegenstelle : public QObject
{
    Q_OBJECT
public:
    explicit VersionsGegenstelle(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            m_peer = m_server.nextPendingConnection();
            if (m_peer) { m_peer->write("V3.8.9\n"); }
        });
    }

    bool     hinstellen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16  port() const { return m_server.serverPort(); }

private:
    QTcpServer  m_server;
    QTcpSocket* m_peer{nullptr};
};

QStringList g_warnungen;
QtMessageHandler g_voriger = nullptr;

void warnungenSammeln(QtMsgType typ, const QMessageLogContext& ctx,
                      const QString& text)
{
    if (typ == QtWarningMsg || typ == QtCriticalMsg || typ == QtFatalMsg) {
        g_warnungen << text;
    }
    if (g_voriger) { g_voriger(typ, ctx, text); }
}

}  // namespace

class TstPgxlTgxlAbbau : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void pgxlRuftBeimAbbauNichtZurueck();
    void tgxlRuftBeimAbbauNichtZurueck();

private:
    VersionsGegenstelle* m_gegen{nullptr};
};

void TstPgxlTgxlAbbau::init()
{
    m_gegen = new VersionsGegenstelle(this);
    QVERIFY2(m_gegen->hinstellen(), "Die Gegenstelle kam nicht an den Port");
    g_warnungen.clear();
}

void TstPgxlTgxlAbbau::cleanup()
{
    if (g_voriger) {
        qInstallMessageHandler(g_voriger);
        g_voriger = nullptr;
    }
    delete m_gegen;
    m_gegen = nullptr;
}

void TstPgxlTgxlAbbau::pgxlRuftBeimAbbauNichtZurueck()
{
    // Selbstverbinden EIN, damit onDisconnected() den vollen Weg geht und
    // nicht vorzeitig aussteigt -- genau dieser Weg greift nach den
    // abgebauten Zeitgebern.
    Longpath::AppSettings::instance().setValue("PGXL_AutoReconnect", "True");

    auto* verb = new Longpath::PgxlConnection();
    QSignalSpy auf(verb, &Longpath::PgxlConnection::connected);
    verb->connectToPgxl(QStringLiteral("127.0.0.1"), m_gegen->port());
    QVERIFY2(auf.wait(3000), "Die Verbindung zur Gegenstelle kam nicht zustande");

    // Ein Empfaenger, der sich MELDET, wenn aus dem halb abgebauten
    // Objekt noch etwas herauskommt. Fremdes Objekt, damit er den Abbau
    // ueberlebt.
    QObject horcher;
    int wegGemeldet = 0;
    QObject::connect(verb, &Longpath::PgxlConnection::disconnected, &horcher,
                     [&wegGemeldet] { ++wegGemeldet; });

    g_warnungen.clear();
    g_voriger = qInstallMessageHandler(warnungenSammeln);
    delete verb;                       // <- hier passiert es
    qInstallMessageHandler(g_voriger);
    g_voriger = nullptr;

    for (const QString& w : g_warnungen) {
        QVERIFY2(!w.contains(QStringLiteral("Timers cannot"))
                     && !w.contains(QStringLiteral("startTimer"))
                     && !w.contains(QStringLiteral("QObject::connect")),
                 qPrintable(QStringLiteral("Beim Abbau wurde noch in das halb "
                                           "abgebaute Objekt zurueckgerufen: %1")
                                .arg(w)));
    }
    QCOMPARE(wegGemeldet, 0);
}

void TstPgxlTgxlAbbau::tgxlRuftBeimAbbauNichtZurueck()
{
    Longpath::AppSettings::instance().setValue("TGXL_AutoReconnect", "True");

    auto* verb = new Longpath::TgxlConnection();
    QSignalSpy auf(verb, &Longpath::TgxlConnection::connected);
    verb->connectToTgxl(QStringLiteral("127.0.0.1"), m_gegen->port());
    QVERIFY2(auf.wait(3000), "Die Verbindung zur Gegenstelle kam nicht zustande");

    QObject horcher;
    int wegGemeldet = 0;
    QObject::connect(verb, &Longpath::TgxlConnection::disconnected, &horcher,
                     [&wegGemeldet] { ++wegGemeldet; });

    g_warnungen.clear();
    g_voriger = qInstallMessageHandler(warnungenSammeln);
    delete verb;
    qInstallMessageHandler(g_voriger);
    g_voriger = nullptr;

    for (const QString& w : g_warnungen) {
        QVERIFY2(!w.contains(QStringLiteral("Timers cannot"))
                     && !w.contains(QStringLiteral("startTimer"))
                     && !w.contains(QStringLiteral("QObject::connect")),
                 qPrintable(QStringLiteral("Beim Abbau wurde noch in das halb "
                                           "abgebaute Objekt zurueckgerufen: %1")
                                .arg(w)));
    }
    QCOMPARE(wegGemeldet, 0);
}

QTEST_MAIN(TstPgxlTgxlAbbau)
#include "tst_pgxl_tgxl_abbau.moc"
