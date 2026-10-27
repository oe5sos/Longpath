#pragma once

// =================================================================
// src/core/RigctldClient.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die andere Richtung ─────────────────────────────────────────────
//
// Longpath spricht rigctld bisher nur als SERVER: RigctldServer laesst
// Logbuch- und Contestprogramme Longpath steuern (#308). Dies ist das
// Gegenstueck -- Longpath fragt ein FREMDES Funkgeraet, wo es steht.
//
// Anlass, 2026-10-27, die 2-m-Conteststation des Betreibers: ein
// klassisches Geraet (TS-590S oder Elecraft K3) arbeitet als ZF hinter
// einem Transverter und sendet; Longpath haengt mit einem SDR am
// ZF-Ausgang des Transverters und zeigt das Band. Das SDR sieht
// 28-30 MHz, weiss aber nicht, wo im Band der Betreiber gerade
// zuhoert. Genau diese eine Zahl holt diese Klasse.
//
// ── Was sie NICHT tut: tasten ───────────────────────────────────────
//
// rigctld kann `T 1` -- Sender ein. Diese Klasse kann es nicht, und
// zwar mit Absicht und ohne Schalter: ein fremdes Geraet, das an einer
// Endstufe und einer Antenne haengt, wird nicht von einem Programm
// getastet, das nur zuschaut. Der PTT-Zustand wird GELESEN (`t`),
// damit die Anzeige weiss, dass gesendet wird; gesetzt wird er nie.
// Wer das anders will, muss es hier hinschreiben und dabei erklaeren,
// warum -- die Bandkanten-Regel aus CLAUDE.local.md gilt fuer einen
// fremden Sender genauso.
//
// Abstimmen (`F <hz>`) kann sie, denn das ist der eigentliche Nutzen
// eines Panadapters: hineinklicken, und das Geraet geht dorthin. Eine
// Abstimmung tastet nichts.
//
// ── Form ────────────────────────────────────────────────────────────
//
// Dieselbe wie RotctldClient (QTcpSocket, ein ausstehender Befehl,
// Taktgeber, Antwortwache, Wiederanlauf, Parser als reine
// Funktionen). Was rigctld spricht, steht in `man rigctld`:
//
//   f            -> "145000000\n"             Frequenz in Hz
//   F <hz>       -> "RPRT 0\n"                Frequenz setzen
//   m            -> "USB\n2400\n"             Modus, dann Durchlass
//   t            -> "0\n" / "1\n"             PTT-Zustand
//
// ── Die Falle, die die Vorlage noch hat ─────────────────────────────
//
// Die Modusantwort ist ZWEIzeilig. Das Contestprogramm (dessen
// RigctldClient dieselbe Form hat) wartet darauf unbedingt -- und
// haengt bis zur Antwortwache, wenn rigctld stattdessen einzeilig
// "RPRT -11" schickt, was bei einem Geraet ohne Modusabfrage
// vorkommt. Hier wird das abgefangen: eine RPRT-Zeile beendet das
// Warten sofort, auch wenn zwei Zeilen erwartet waren.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code. Form nach
//                src/core/RotctldClient.h; Befehlssatz aus
//                `man rigctld` (Hamlib 4.7.2).
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTcpSocket>

class QTimer;

namespace Longpath {

class RigctldClient : public QObject {
    Q_OBJECT
public:
    enum class State {
        Disconnected,   ///< Keine Verbindung, und es wird keine versucht.
        Connecting,     ///< Verbindung im Aufbau oder im Wiederanlauf.
        Connected,      ///< Verbindung steht, Antworten kommen.
    };
    Q_ENUM(State)

    explicit RigctldClient(QObject* parent = nullptr);
    ~RigctldClient() override;

    void setTarget(const QString& host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    /// Wie oft nach der Frequenz gefragt wird. Die Mitte der Anzeige
    /// soll der Hand am Knopf folgen, nicht hinterherkriechen; ein
    /// Viertelsekundentakt auf der Rueckschleife kostet nichts.
    void setPollIntervalMs(int ms);
    int  pollIntervalMs() const { return m_pollMs; }

    /// Aelter als das, und die Frequenz wird nicht mehr angezeigt.
    /// Drei Takte: eine ausgefallene Antwort ist ein Schluckauf, drei
    /// sind ein Geraet, das nicht mehr da ist.
    static constexpr int kStaleAfterMs = 1500;

    /// Wie lange ein ausstehender Befehl auf seine Antwort warten darf,
    /// bevor die Verbindung fuer tot erklaert und getrennt wird. Grosszuegig
    /// neben kStaleAfterMs, weil das die letzte Rettung fuer einen
    /// eingefrorenen Daemon ist, kein Laufzeitbudget.
    static constexpr int kReplyTimeoutMs = 4000;

    static constexpr int kRetryAfterDropMs    = 3000;
    static constexpr int kRetryAfterRefusedMs = 1000;

    State   state() const { return m_state; }
    bool    isConnected() const { return m_state == State::Connected; }

    /// Zuletzt gemeldete Frequenz in Hz, oder 0, wenn noch keine kam.
    /// Das ist die ZF-Frequenz, wie das Geraet sie zeigt -- die
    /// Umrechnung auf die Antenne macht TransverterSetup, nicht diese
    /// Klasse.
    qint64  frequencyHz() const { return m_frequencyHz; }
    QString mode() const { return m_mode; }
    int     passbandHz() const { return m_passbandHz; }
    bool    ptt() const { return m_ptt; }

    /// Ist die letzte Frequenzmeldung frisch genug, um sie anzuzeigen?
    bool hasFreshFrequency() const;

    /// Eine Zeile fuer die Statusanzeige ("rigctld 127.0.0.1:4532").
    QString description() const;

    void connectToRig();
    void disconnectFromRig();

    /// Das Geraet auf diese ZF-Frequenz stellen. Tut nichts, wenn
    /// keine Verbindung steht.
    void setFrequencyHz(qint64 hz);

    // ── Protokoll, als reine Funktionen ─────────────────────────────

    /// "145000000\n" -> 145000000. Liefert false fuer alles, was keine
    /// Zahl ist -- insbesondere fuer eine RPRT-Zeile, die sonst als
    /// Frequenz -1 durchginge.
    static bool parseFrequency(const QByteArray& reply, qint64& hz);

    /// "USB\n2400\n" -> "USB", 2400. Die zweite Zeile darf fehlen;
    /// dann bleibt die Durchlassbreite 0. Eine RPRT-Zeile ist kein
    /// Modus und liefert false.
    static bool parseMode(const QByteArray& reply, QString& mode,
                          int& passbandHz);

    /// "0\n" -> false, "1\n" -> true. Alles andere: false zurueck.
    static bool parsePtt(const QByteArray& reply, bool& keyed);

    /// "RPRT 0" -> 0, "RPRT -1" -> -1. false, wenn es keine RPRT-Zeile ist.
    static bool parseReport(const QByteArray& reply, int& code);

    /// Hamlibs Fehlernummern als etwas, das ein Betreiber lesen kann.
    static QString describeReport(int code);

    /// Der Befehl, der ein Geraet abstimmt. Hier gebaut, damit das
    /// Zahlenformat eine Entscheidung ist: rigctld will ganze Hertz,
    /// und ein Gebietsschema mit Tausenderpunkten waere eine
    /// Ablehnung.
    static QByteArray setFrequencyCommand(qint64 hz);

signals:
    void stateChanged(State s);

    /// Frische Frequenz vom Geraet, in Hz, wie das Geraet sie zeigt.
    void frequencyChanged(qint64 hz);

    /// Modus und Durchlassbreite aus derselben Antwort.
    void modeChanged(const QString& mode, int passbandHz);

    /// Der PTT-Zustand des fremden Geraets. NUR gelesen -- siehe Kopf.
    void pttChanged(bool keyed);

    /// Etwas, das der Betreiber sehen soll: eine Zeile, kein Code.
    void errorOccurred(const QString& message);

    /// Die Antwortwache hat angeschlagen: ein ausstehender Befehl blieb
    /// laenger als kReplyTimeoutMs unbeantwortet, die Verbindung wurde
    /// getrennt. Getrennt von errorOccurred, damit der Besitzer des
    /// rigctld-Prozesses „der Daemon haengt" von jedem anderen Grund
    /// unterscheiden kann -- ein haengender Daemon braucht einen
    /// Neustart, keinen neuen Verbindungsversuch.
    void replyTimedOut();

private:
    enum class Pending { None, Frequency, Mode, Ptt, Report };

    void send(const QByteArray& command, Pending expect);
    void pump();
    void poll();
    void onReadyRead();
    void setState(State s);
    void fail(const QString& why);

    /// Wieviele Zeilen diese Antwort hat, wenn sie vollstaendig ist.
    static int expectedLines(Pending p);

    struct Command {
        QByteArray bytes;
        Pending    expect;
    };

    QTcpSocket m_socket;
    // KEINE Vorgabeadresse. 127.0.0.1 waere die richtige Vermutung und
    // zugleich die falsche Vorgabe: mit ihr ist „keine Adresse
    // gesetzt" kein Zustand mehr, die Pruefung in connectToRig() toter
    // Code, und die Klasse verbindet sich ungefragt irgendwohin. Der
    // Vorschlag gehoert in die Einstellungsseite, nicht hierher.
    // (Vom Pruefstand gefunden, 2026-10-27.)
    QString    m_host;
    quint16    m_port{4532};            // rigctlds Vorgabe

    QQueue<Command> m_queue;
    Pending    m_awaiting{Pending::None};
    QByteArray m_buffer;

    QTimer* m_poll{nullptr};
    QTimer* m_retry{nullptr};
    QTimer* m_deadline{nullptr};
    int     m_pollMs{250};
    quint64 m_tick{0};

    State     m_state{State::Disconnected};
    bool      m_wanted{false};          ///< Hat jemand connectToRig() gerufen?
    qint64    m_frequencyHz{0};
    QString   m_mode;
    int       m_passbandHz{0};
    bool      m_ptt{false};
    QDateTime m_lastFrequency;
};

} // namespace Longpath
