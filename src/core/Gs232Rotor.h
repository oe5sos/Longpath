#pragma once

// =================================================================
// src/core/Gs232Rotor.h  (Longpath)
// =================================================================
//
// Ein Yaesu GS-232A/B direkt an der seriellen Schnittstelle — ohne
// Hamlib, ohne rotctld dazwischen.
//
// Longpath fuhr Rotoren bisher nur ueber rotctld (RotctldClient, dazu
// RotctldProcess, der den Dienst selbst startet). Das ist der Weg, der
// JEDEN Rotor erreicht, den Hamlib kennt — aber fuer den haeufigsten
// Fall ist es ein Dienst mehr, der laufen, gefunden und ueberwacht
// werden muss. RotorController wurde 2026-08-07 ausdruecklich als
// Schnittstelle angelegt, damit genau dieser Treiber spaeter als
// Unterklasse dazukommen kann und kein Umbau wird; das ist er.
//
// Das Protokoll ist uebernommen aus dem Upstream NereusSDR
// (`src/core/RotorConnection.cpp` @ 9a20b6b0c, Fassung 2026.10.1, J.J.
// Boyd KG4VCF), der es seinerseits gegen Hamlibs `gs232a.c`/`gs232b.c`
// und einen ERC-Controller an der Bank gestellt hat:
//
//   Abfrage   `C2\r`        -> `AZ=aaa EL=eee` (GS-232B; der ERC setzt
//                              zwei Leerzeichen vor EL) oder
//                              `+0aaa+0eee` (GS-232A, Hamlibs Form;
//                              an keinem Geraet beobachtet)
//   Fahren    `Waaa eee\r`  -> je drei Ziffern, Hoehe 000 am reinen
//                              Azimutrotor
//   Halt      `S\r`
//
// Die Schnittstelle laeuft 8N1 ohne Handschlag.
//
// Zur Ehrlichkeit ueber die Lage gilt derselbe Satz wie in
// RotorController.h: eine Nadel, die eine alte Zahl zeigt, ohne das zu
// sagen, ist schlimmer als eine Nadel, die nichts zeigt. Darum traegt
// jede Lesung ihren Zeitpunkt, und `hasFreshPosition()` ist die
// Antwort darauf, nicht `isConnected()`.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Angelegt. Longpath, KI-gestuetzt via Anthropic Claude
//                Code; Protokoll uebernommen aus Upstream NereusSDR.
// =================================================================

#include "RotorController.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

class QIODevice;
class QTimer;

namespace Longpath {

class Gs232Rotor : public RotorController {
    Q_OBJECT
public:
    explicit Gs232Rotor(QObject* parent = nullptr);
    ~Gs232Rotor() override;

    /// Schnittstelle und Geschwindigkeit. Vorgabe 9600 Baud — das ist,
    /// was der GS-232 ab Werk spricht.
    void setPort(const QString& portName, int baudRate = 9600);
    QString portName() const { return m_portName; }
    int baudRate() const { return m_baud; }

    /// Derselbe Controller, aber ueber einen seriellen Netzserver
    /// (ser2net, Moxa und Verwandte): die Bytes sind dieselben, nur der
    /// Draht ist ein TCP-Strom. Gemessen an Martins Station am
    /// 2026-10-21: der GS-232 haengt dort NICHT lokal, sondern auf
    /// 172.30.30.16:4001, und rotctld wird mit genau diesem Ziel
    /// gestartet (`-r 172.30.30.16:4001 -s 9600`). Ein Treiber nur fuer
    /// lokale Schnittstellen haette diese Station nie erreicht.
    ///
    /// Gesetzt schlaegt das Netzziel die serielle Schnittstelle.
    void setNetworkTarget(const QString& host, quint16 port = 4001);
    QString host() const { return m_host; }
    quint16 networkPort() const { return m_netPort; }

    /// Wie oft die Lage abgefragt wird. Wie beim rotctld-Weg 500 ms:
    /// ein Rotor dreht rund 5 Grad je Sekunde, schneller zu fragen
    /// bringt keine neue Zahl.
    void setPollIntervalMs(int ms);

    QString description() const override;
    State   state() const override { return m_state; }
    double  azimuth() const override { return m_azimuth; }
    bool    hasFreshPosition() const override;

    /// Die Hoehe aus derselben Meldung. 0 am reinen Azimutrotor.
    double elevation() const { return m_elevation; }

    /// Aelter als das, gilt die Lage als nicht mehr steuerbar. Dieselbe
    /// Frist wie beim rotctld-Weg (RotctldClient::kStaleAfterMs): drei
    /// ausgefallene Abfragen bei 500 ms.
    static constexpr int kStaleAfterMs = 1500;

    void connectToRotor() override;
    void disconnectFromRotor() override;
    void moveTo(double azimuthDeg) override;
    void stop() override;

    // ── Protokoll als reine Funktionen ───────────────────────────────

    /// `AZ=302  EL=000` oder `+0302+0000` -> 302 / 0.
    /// Gibt false fuer alles andere zurueck — besonders fuer eine halbe
    /// Zeile, denn eine halb gelesene Zahl ist eine falsche Peilung.
    static bool parsePosition(const QByteArray& zeile,
                              double& azDeg, double& elDeg);

    /// `W292 000\r`. Der Azimut wird auf 0..359 gefaltet, die Hoehe auf
    /// 0..180 geklemmt.
    static QByteArray moveCommand(double azimuthDeg, double elevationDeg = 0.0);

    static QByteArray pollCommand() { return QByteArrayLiteral("C2\r"); }
    static QByteArray stopCommand() { return QByteArrayLiteral("S\r"); }

#ifdef LONGPATH_BUILD_TESTS
    /// Prüfstandsnaht: statt der seriellen Schnittstelle ein beliebiges
    /// Geraet (etwa ein QBuffer) einhaengen. Der Rotor uebernimmt den
    /// Besitz NICHT.
    void useDeviceForTest(QIODevice* dev);
#endif

signals:
    /// Die Hoehe aus derselben Meldung wie positionChanged — gleiche
    /// Trennung wie bei RotctldClient, damit RotorControllers Vertrag
    /// azimutweise bleibt.
    void elevationChanged(double elevationDeg);

private:
    void onReadyRead();
    void poll();
    void send(const QByteArray& command);
    void setState(State s);
    void fail(const QString& why);

    QIODevice* m_dev{nullptr};
    bool       m_ownsDev{true};
    QString    m_portName;
    int        m_baud{9600};
    QString    m_host;
    quint16    m_netPort{4001};

    QByteArray m_buffer;
    QTimer*    m_poll{nullptr};
    int        m_pollMs{500};

    State      m_state{State::Disconnected};
    double     m_azimuth{0.0};
    double     m_elevation{0.0};
    QDateTime  m_lastReading;
};

} // namespace Longpath
