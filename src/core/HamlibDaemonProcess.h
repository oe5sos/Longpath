#pragma once

// =================================================================
// src/core/HamlibDaemonProcess.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Starts a Hamlib daemon so the operator does not have to.
//
// ── Woher das kommt ─────────────────────────────────────────────────
//
// Diese Klasse ist der Rumpf, der bis zum 2026-10-27 in
// RotctldProcess sass und nur rotctld starten konnte. Als der
// Begleitgeraet-Weg dazukam (Longpath folgt einem fremden Funkgeraet
// ueber rigctld), waere die Wahl gewesen: 250 Zeilen Prozesspflege
// ein zweites Mal hinschreiben, oder den Binaernamen zur
// Konstruktorfrage machen. Letzteres.
//
// Die beiden Daemons sind aus demselben Haus und nehmen dieselben
// Schalter (-m Modell, -r Geraet, -s Baudrate, -T/-t Adresse und
// Port). Was sie unterscheidet, ist ihr Name und ihr Vorgabeport --
// also genau das, was hier hereingegeben wird.
//
// ── Warum ueberhaupt ────────────────────────────────────────────────
//
// Der Weg ueber TCP ist der richtige -- ein Protokoll fuer sechzig
// Geraete, es laeuft ueber das Netz, und mit neuer Hardware haelt
// jemand anderes Schritt. Aber er machte die Anleitung zu "oeffne ein
// Fenster und tippe rotctld -m 603 -r /dev/tty.usbserial-1410", und
// das ist einem Entwickler zuzumuten, nicht jemandem, der eine
// Antenne drehen oder ein QSO fuehren will.
//
// Wer den Daemon selbst laufen laesst, merkt von dieser Klasse
// nichts: dieser Weg bleibt und hat weiter Vorrang.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Als RotctldProcess in C++20/Qt6 fuer NereusSDR
//                 angelegt, KI-gestuetzt ueber Anthropic Claude
//                 (Cowork), Betreiber Martin Fischer.
//   2026-09-16 — Nimmt einen freien Port, wenn der gewuenschte belegt
//                 ist (ein beim Absturz verwaistes rotctld sass live
//                 auf 4533 mit dem falschen Geraet); restart() fuer
//                 einen Daemon, dessen Geraeteverbindung gestorben
//                 ist; exited() feuert nicht mehr fuer ein stop(),
//                 das dieses Objekt selbst erbeten hat.
//   2026-10-27 — Aus RotctldProcess herausgezogen und ueber den
//                 Binaernamen allgemein gemacht, damit der
//                 CAT-Daemon (rigctld) denselben Rumpf benutzt statt
//                 einer Abschrift. Verhalten unveraendert.
//                 KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace Longpath {

class HamlibDaemonProcess : public QObject {
    Q_OBJECT
public:
    /// `binaryName` ist der Programmname ohne Pfad ("rotctld",
    /// "rigctld"), `defaultPort` der Port, den dieser Daemon von Haus
    /// aus belegt (4533 bzw. 4532).
    explicit HamlibDaemonProcess(QString binaryName, quint16 defaultPort,
                                 QObject* parent = nullptr);
    ~HamlibDaemonProcess() override;

    QString binaryName() const { return m_binaryName; }
    quint16 defaultPort() const { return m_defaultPort; }

    // Where the daemon lives, or empty if it cannot be found.
    //
    // PATH alone is not enough: a GUI application launched from Finder
    // inherits a PATH without /opt/homebrew/bin, so Hamlib installed
    // with brew is invisible to it while being perfectly present in the
    // operator's terminal.
    static QString findBinaryNamed(const QString& binaryName);

    // The command line, built where it can be checked. Public because
    // the dialog shows it to the operator: a person who can see the
    // exact command can run it by hand when the automatic path fails,
    // and can paste it into a bug report.
    //
    // Binaerneutral -- beide Daemons nehmen dieselben Schalter.
    static QStringList arguments(int hamlibModel, const QString& device,
                                 int baud, quint16 listenPort);

    // Der GRUND aus Hamlibs stderr, in einer Zeile fuer die Statusleiste.
    //
    // Warum nicht einfach die letzte Zeile: Hamlib schliesst mit „IO
    // error" ab, und genau das stand am 2026-10-06 in Martins Protokoll
    // unter „Rotator: rotctld exited". Die Zeile darueber hiess
    //
    //     connect to 192.168.x.16:4001 failed, (trying next interface):
    //     Network error 60: Operation timed out
    //
    // -- also Adresse UND Ursache. „IO error" sagt nichts, und wer nichts
    // erfaehrt, sucht beim Programm statt beim Kabel.
    //
    // Darum in zwei Stufen: zuerst die letzte Zeile, die eine Ursache
    // nennt (zeitueberschritten, abgewiesen, nicht vorhanden, belegt,
    // keine Berechtigung), dann die letzte, die von einem Fehlschlag
    // spricht UND dabei etwas benennt (Rechner, Port, Pfad, Nummer), und
    // erst zuletzt die letzte nichtleere Zeile.
    //
    // Die Bedingung „benennt etwas" hat der Pruefstand erzwungen: „IO
    // error" steht bei Hamlib immer zuletzt und enthaelt das Wort
    // „error", also gewann es in der zweiten Stufe jedes Mal gegen die
    // Zeile, die tatsaechlich etwas sagte.
    // Oeffentlich und ohne Zustand, damit es ohne Oberflaeche und ohne
    // laufenden Prozess pruefbar ist.
    static QString grundAus(const QString& stderrText);

    bool isRunning() const;

    // Start the daemon. Returns false and fills `error` if the binary is
    // missing or the process refuses to start; a daemon that starts and
    // then exits reports through exited() instead, because that failure
    // arrives later.
    //
    // `listenPort` is the port to prefer. If something already holds
    // it, a free one is taken instead — read listenPort() afterwards
    // for the port actually in use. 2026-09-16: found live — a rotctld
    // from an earlier, crashed session was still sitting on 4533 with
    // the wrong device; the new one could not bind, exited at once,
    // and the client then happily talked to the stale one, which never
    // answered. Sidestepping the port is the fix that needs nothing
    // from the operator; killing strangers' processes is not ours to do.
    bool start(int hamlibModel, const QString& device, int baud,
               quint16 listenPort, QString* error);

    // The port the running (or last started) daemon listens on.
    quint16 listenPort() const { return m_listenPort; }

    void stop();

    // Everything start() needs, kept so the owner can bounce the daemon
    // without re-collecting it: a daemon whose link to the device has
    // died (the ARCO drops a silent GS-232A session after ~20 s) never
    // recovers on its own and has to be started afresh.
    bool restart(QString* error);

signals:
    // The daemon stopped on its own. Carries whatever it wrote to
    // stderr, which is where Hamlib puts the reason — a wrong model
    // number or a serial port that is not there both come out here and
    // nowhere else. Not emitted for a stop() this object asked for.
    void exited(int exitCode, const QString& stderrText);

private:
    QString m_binaryName;
    quint16 m_defaultPort{0};

    QProcess m_proc;
    quint16  m_listenPort{0};
    bool     m_stopRequested{false};

    int     m_model{0};
    QString m_device;
    int     m_baud{0};
    quint16 m_preferredPort{0};
};

} // namespace Longpath
