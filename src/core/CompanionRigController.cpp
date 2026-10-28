// =================================================================
// src/core/CompanionRigController.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see CompanionRigController.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "core/CompanionRigController.h"

#include "core/AppSettings.h"
#include "core/RigctldProcess.h"
#include "core/TransverterSetup.h"

#include <QLoggingCategory>

namespace Longpath {

Q_LOGGING_CATEGORY(lcCompanionRig, "longpath.companionrig")

namespace {

/// AppSettings schreibt Wahrheitswerte als "True"/"False" (so wie
/// Thetis es tat), nicht als 0/1. Wer hier toBool() nimmt, bekommt bei
/// "False" ein true -- ein nicht leerer String ist wahr.
bool alsBool(const QVariant& v, bool vorgabe)
{
    if (!v.isValid()) { return vorgabe; }
    const QString s = v.toString().trimmed();
    if (s.isEmpty()) { return vorgabe; }
    return s.compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0
        || s == QStringLiteral("1");
}

QString mhz(qint64 hz)
{
    // QString::number, nicht arg(double) und nicht QLocale: in einem
    // deutschen Lauf traegt arg() ein Komma ein, und eine Frequenz mit
    // Komma hat in einem Logbuch oder einer Statuszeile nichts verloren.
    return QString::number(static_cast<double>(hz) / 1e6, 'f', 6);
}

} // namespace

CompanionRigController::CompanionRigController(QObject* parent)
    : QObject(parent)
{
    ladeEinstellungen();
}

CompanionRigController::~CompanionRigController()
{
    // Reihenfolge: erst der Client (er haengt am Socket), dann der
    // Daemon. Umgekehrt stirbt rigctld unter einer stehenden
    // Verbindung, und der Client meldet beim Abbau einen Fehler, den
    // niemand mehr liest.
    if (m_client) { m_client->disconnectFromRig(); }
    m_client.reset();
    if (m_proc) { m_proc->stop(); }
    m_proc.reset();
}

void CompanionRigController::ladeEinstellungen()
{
    auto& s = AppSettings::instance();

    m_eingeschaltet = alsBool(s.value(QStringLiteral("CompanionRigEnabled")), false);
    m_modell        = s.value(QStringLiteral("CompanionRigModel"), 0).toInt();
    m_anschluss     = s.value(QStringLiteral("CompanionRigDevice")).toString();
    m_baud          = s.value(QStringLiteral("CompanionRigBaud"), 0).toInt();
    m_daemonSelbstStarten =
        alsBool(s.value(QStringLiteral("CompanionRigStartDaemon")), false);
    m_host = s.value(QStringLiteral("CompanionRigHost"),
                     QStringLiteral("127.0.0.1")).toString().trimmed();
    m_port = static_cast<quint16>(
        s.value(QStringLiteral("CompanionRigPort"),
                RigctldProcess::kDefaultPort).toInt());
    m_geraeteName = s.value(QStringLiteral("CompanionRigName")).toString();

    // Der Versatz ist eine Stationseinstellung, keine des Fensters:
    // hier wird er geladen und prozessweit abgelegt, damit die vier
    // Leser (VFO-Zahl, Frequenzachse, Bandplan, Logbuch) ihn finden.
    TransverterSetup::setCurrent(TransverterSetup::load());
    emit versatzGeaendert();
}

void CompanionRigController::einstellungenGeaendert()
{
    const QString alterHost = m_host;
    const quint16 alterPort = m_port;
    const bool    liefSchon = verbunden();

    ladeEinstellungen();

    if (!m_eingeschaltet) {
        trenne();
        return;
    }

    // Umhaengen nur, wenn das Ziel wirklich ein anderes ist. Ein
    // Speichern, bei dem nur der Versatz geaendert wurde, soll eine
    // stehende Verbindung nicht abreissen -- der Bediener sitzt
    // womoeglich mitten in einem QSO.
    if (liefSchon && (alterHost != m_host || alterPort != m_port)) {
        trenne();
        verbinde();
    } else if (!liefSchon) {
        verbinde();
    }
}

void CompanionRigController::starteWennGewuenscht()
{
    if (!m_eingeschaltet) { return; }

    if (!AppSettings::darfVonSelbstVerbinden()) {
        qCInfo(lcCompanionRig)
            << "Begleitgeraet eingeschaltet, aber dieser Lauf verbindet nicht "
               "von selbst -- kein rigctld, kein Socket.";
        return;
    }
    verbinde();
}

void CompanionRigController::baueClient()
{
    if (m_client) { return; }

    m_client = std::make_unique<RigctldClient>();

    connect(m_client.get(), &RigctldClient::frequencyChanged, this,
            [this](qint64 hz) { emit zfFrequenzGeaendert(hz); });

    connect(m_client.get(), &RigctldClient::stateChanged, this,
            [this](RigctldClient::State s) {
        emit verbindungGeaendert(s == RigctldClient::State::Connected);
    });

    connect(m_client.get(), &RigctldClient::errorOccurred, this,
            [this](const QString& text) { emit problem(text); });

    connect(m_client.get(), &RigctldClient::replyTimedOut, this, [this]() {
        emit problem(tr("The companion rig stopped answering — reconnecting."));
    });
}

void CompanionRigController::starteDaemonWennGewuenscht()
{
    if (!m_daemonSelbstStarten) { return; }
    if (m_proc && m_proc->isRunning()) { return; }

    if (m_modell <= 0) {
        emit problem(tr("No Hamlib model picked — there is nothing to start."));
        return;
    }

    m_proc = std::make_unique<RigctldProcess>();
    QString fehler;
    if (!m_proc->start(m_modell, m_anschluss, m_baud, m_port, &fehler)) {
        emit problem(fehler);
        m_proc.reset();
        return;
    }

    // Der Prozess darf auf einen freien Port ausgewichen sein, wenn der
    // gewuenschte belegt war (die Lehre vom Rotor, 2026-09-16: ein
    // verwaistes rotctld sass auf 4533 mit dem falschen Geraet). Dann
    // gilt der, auf dem er wirklich lauscht -- und das ist immer
    // Loopback, weil der Prozess nur dort bindet.
    m_host = QStringLiteral("127.0.0.1");
    m_port = m_proc->listenPort();

    connect(m_proc.get(), &RigctldProcess::exited, this,
            [this](int code, const QString& err) {
        emit problem(tr("rigctld stopped (%1): %2")
                         .arg(QString::number(code),
                              RigctldProcess::grundAus(err)));
    });
}

// ── Longpath darf nicht sich selbst befragen ────────────────────────
//
// Longpath hat einen EIGENEN rigctld-Server (Setup -> CAT -> TCP/IP
// CAT), damit Fremdprogramme es wie ein Funkgeraet steuern koennen. Der
// bindet nach Vorgabe 127.0.0.1:4532 -- und 4532 ist auch die Vorgabe,
// die rigctld selbst benutzt, also die, die auf dieser Seite im Feld
// steht.
//
// Wer beides einschaltet und nichts umstellt, baut einen Kreis:
// Longpath fragt Longpath nach der Frequenz, bekommt seine eigene, die
// Marke klebt auf der eigenen Scheibe, und ein Klick im Panadapter
// schickt ein „F" an das eigene Programm. Nichts stuerzt ab, nichts
// warnt, und es sieht so lange richtig aus, bis der Betreiber merkt,
// dass das Funkgeraet sich nie bewegt.
//
// Geprueft wird gegen die EINSTELLUNG, nicht gegen einen laufenden
// Server: diese Klasse kennt MainWindow nicht, und sie soll ihn nicht
// kennen muessen. Faellt die Einstellung anders aus als die
// Wirklichkeit (Server aus, aber Schluessel noch auf True), verweigert
// die Pruefung einmal zu oft -- das ist die harmlose Richtung.
bool CompanionRigController::zieltAufUnsSelbst() const
{
    auto& s = AppSettings::instance();
    if (!alsBool(s.value(QStringLiteral("RigctldEnabled")), false)) {
        return false;
    }

    const quint16 eigener = static_cast<quint16>(
        s.value(QStringLiteral("RigctldPort"), 4532).toString().toUShort());
    if (m_port != eigener) { return false; }

    const QString h = m_host.trimmed();
    return h == QStringLiteral("127.0.0.1") || h == QStringLiteral("::1")
        || h.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0;
}

void CompanionRigController::verbinde()
{
    if (m_host.isEmpty() && !m_daemonSelbstStarten) {
        emit problem(tr("No address for the companion rig."));
        return;
    }

    // Vor dem Start des Daemons pruefen: der weicht bei belegtem Port
    // selbst aus und raeumt den Kreis damit auf.
    if (!m_daemonSelbstStarten && zieltAufUnsSelbst()) {
        emit problem(tr("That address is Longpath's own rigctld server "
                        "(Setup → CAT → TCP/IP CAT) — Longpath would be "
                        "asking itself. Point the companion rig at the other "
                        "rig's rigctld, or move one of the two ports."));
        return;
    }

    starteDaemonWennGewuenscht();
    baueClient();

    if (m_host.isEmpty()) {
        emit problem(tr("No address for the companion rig."));
        return;
    }

    m_client->setTarget(m_host, m_port);
    m_client->connectToRig();
}

void CompanionRigController::trenne()
{
    if (m_client) { m_client->disconnectFromRig(); }
    if (m_proc) {
        m_proc->stop();
        m_proc.reset();
    }
}

qint64 CompanionRigController::zfFrequenzHz() const
{
    if (!m_client || !m_client->hasFreshFrequency()) { return 0; }
    return m_client->frequencyHz();
}

qint64 CompanionRigController::rfFrequenzHz() const
{
    const qint64 zf = zfFrequenzHz();
    if (zf <= 0) { return 0; }
    return TransverterSetup::current().rfFrequencyHz(zf);
}

bool CompanionRigController::darfAbstimmen() const
{
    return m_eingeschaltet && verbunden();
}

void CompanionRigController::stimmeAbZf(qint64 ifHz)
{
    if (!darfAbstimmen() || ifHz <= 0) { return; }
    m_client->setFrequencyHz(ifHz);
}

QString CompanionRigController::beschreibung() const
{
    if (!m_eingeschaltet) { return tr("Companion rig off."); }
    if (!m_client) { return tr("Not connected."); }

    switch (m_client->state()) {
    case RigctldClient::State::Disconnected:
        return tr("Not connected.");
    case RigctldClient::State::Connecting:
        return tr("Connecting to %1…").arg(m_client->description());
    case RigctldClient::State::Connected:
        break;
    }

    const qint64 zf = zfFrequenzHz();
    if (zf <= 0) { return tr("Connected, waiting for a frequency."); }

    const qint64 rf = TransverterSetup::current().rfFrequencyHz(zf);
    QString text = m_geraeteName.isEmpty()
                       ? mhz(zf)
                       : QStringLiteral("%1, %2").arg(m_geraeteName, mhz(zf));
    if (rf != zf) {
        text += QStringLiteral(" → %1").arg(mhz(rf));
    }
    if (!m_client->mode().isEmpty()) {
        text += QStringLiteral(" (%1)").arg(m_client->mode());
    }
    return text;
}

} // namespace Longpath
