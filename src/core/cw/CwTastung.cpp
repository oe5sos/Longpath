// =================================================================
// src/core/cw/CwTastung.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: CwTastung.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/cw/CwTastung.h"

#include "core/AppSettings.h"
#include "core/cw/CwMithoerton.h"
#include "core/cw/IambicKeyer.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcCwTaste, "longpath.cw.taste")

namespace Longpath {

namespace {

const QString kEin        = QStringLiteral("CwKey/Enabled");
const QString kSender     = QStringLiteral("CwKey/KeyTransmitter");
const QString kArt        = QStringLiteral("CwKey/Mode");
const QString kWpm        = QStringLiteral("CwKey/Wpm");
const QString kTausch     = QStringLiteral("CwKey/SwapPaddles");
const QString kNachlauf   = QStringLiteral("CwKey/HangMs");
const QString kTon        = QStringLiteral("CwKey/Sidetone");
const QString kTonHz      = QStringLiteral("CwKey/SidetoneHz");
const QString kTonLaut    = QStringLiteral("CwKey/SidetoneVolume");
const QString kPort       = QStringLiteral("CwKey/MidiPort");
const QString kNotePunkt  = QStringLiteral("CwKey/NoteDit");
const QString kNoteStrich = QStringLiteral("CwKey/NoteDah");
const QString kNoteTaste  = QStringLiteral("CwKey/NoteKey");
const QString kNotePtt    = QStringLiteral("CwKey/NotePtt");

QString wahr(bool an) { return an ? QStringLiteral("True") : QStringLiteral("False"); }

bool liesBool(const QString& schluessel, bool vorgabe)
{
    return AppSettings::instance().value(schluessel, wahr(vorgabe)).toString()
           == QStringLiteral("True");
}

int liesZahl(const QString& schluessel, int vorgabe, int von, int bis)
{
    bool ok = false;
    const int wert = AppSettings::instance().value(schluessel, vorgabe).toString().toInt(&ok);
    return ok ? std::clamp(wert, von, bis) : vorgabe;
}

} // namespace

CwTastung::CwTastung(QObject* eltern)
    : QObject(eltern)
    , m_eingang(std::make_unique<MidiKeyInput>())
    , m_geber(std::make_unique<IambicKeyer>())
    , m_ton(std::make_unique<CwMithoerton>())
{
    m_nachlauf.setSingleShot(true);
    m_wache.setSingleShot(true);
    m_eingangsWache.setSingleShot(true);
    m_entsperren.setSingleShot(true);
    m_pttWache.setSingleShot(true);
    connect(&m_pttWache, &QTimer::timeout, this, &CwTastung::pttWacheAbgelaufen);
    connect(&m_nachlauf, &QTimer::timeout, this, &CwTastung::nachlaufAbgelaufen);
    connect(&m_wache, &QTimer::timeout, this, &CwTastung::wacheAbgelaufen);
    connect(&m_eingangsWache, &QTimer::timeout, this, &CwTastung::wacheAbgelaufen);
    connect(&m_entsperren, &QTimer::timeout, this, [this]() {
        m_sperre = false;
        emit zustandGeaendert();
    });

    // Der Geber ruft von SEINEM Faden, die Senken vom MIDI-Faden.
    m_geber->setOnKeyDownChange(
        [this](bool unten, std::chrono::steady_clock::time_point) { kanteVomFaden(unten); });
    m_eingang->setPaddleSenke([this](bool punkt, bool strich) {
        if (!m_aktiv.load(std::memory_order_acquire)) { return; }
        switch (static_cast<Art>(m_artFaden.load(std::memory_order_acquire))) {
        case Art::Fusstaster:
            // Der Fusstaster an der Tastenbuchse: jeder Kontakt ist PTT.
            pttVomFaden(punkt || strich);
            return;
        case Art::Handtaste:
            // Eine Handtaste an der Spitze kommt als Note 20 (Handbuch).
            kanteVomFaden(punkt);
            return;
        case Art::JambusA:
        case Art::JambusB:
            break;
        }
        m_geber->setPaddleState(punkt, strich);
    });
    m_eingang->setTastenSenke([this](bool unten) {
        if (!m_aktiv.load(std::memory_order_acquire)) { return; }
        if (static_cast<Art>(m_artFaden.load(std::memory_order_acquire)) == Art::Fusstaster) {
            pttVomFaden(unten);
            return;
        }
        kanteVomFaden(unten);
    });

    // Die Signale kommen vom MIDI-Faden und landen hier in der Schlange.
    connect(m_eingang.get(), &MidiKeyInput::ptt, this, &CwTastung::pttGeaendert);
    connect(m_eingang.get(), &MidiKeyInput::paddle, this,
            [this](bool punkt, bool strich) {
                m_paddleGedrueckt = punkt || strich;
                eingangBewegt();
            });
    connect(m_eingang.get(), &MidiKeyInput::handtaste, this, [this](bool unten) {
        m_handtasteGedrueckt = unten;
        eingangBewegt();
    });
    connect(m_eingang.get(), &MidiKeyInput::geoeffnet, this, &CwTastung::zustandGeaendert);
    connect(m_eingang.get(), &MidiKeyInput::geschlossen, this, &CwTastung::zustandGeaendert);
}

CwTastung::~CwTastung()
{
    // Das Ziel wird hier NICHT mehr angefasst: wer die Tastung besitzt,
    // ruft vorher trenneZiel() -- beim Abbau ist nicht sicher, was vom
    // Geraet noch steht.
    m_aktiv.store(false, std::memory_order_release);
    m_eingang->schliesse();
    m_geber->stop();
    m_ton->stop();
}

void CwTastung::trenneZiel()
{
    lasseLos(/*sendenZurueck=*/true);
    m_ziel = nullptr;
}

// ── Einstellungen ────────────────────────────────────────────────────

void CwTastung::lade()
{
    m_eingeschaltet = liesBool(kEin, false);
    m_senderTasten  = liesBool(kSender, false);
    m_art           = static_cast<Art>(liesZahl(kArt, int(Art::JambusB), 0, 3));
    m_wpm           = liesZahl(kWpm, 20, kWpmMin, kWpmMax);
    m_vertauscht    = liesBool(kTausch, false);
    m_nachlaufMs    = liesZahl(kNachlauf, 600, kNachlaufMinMs, kNachlaufMaxMs);
    m_mithoerton    = liesBool(kTon, true);
    m_ton->setTonhoeheHz(liesZahl(kTonHz, 600, 300, 1200));
    m_ton->setLautstaerke(liesZahl(kTonLaut, 40, 0, 100));
    m_portWunsch    = AppSettings::instance().value(kPort, QString()).toString();

    MidiKeyInput::Noten n;
    n.punkt     = liesZahl(kNotePunkt, n.punkt, 0, 127);
    n.strich    = liesZahl(kNoteStrich, n.strich, 0, 127);
    n.handtaste = liesZahl(kNoteTaste, n.handtaste, 0, 127);
    n.ptt       = liesZahl(kNotePtt, n.ptt, 0, 127);
    m_eingang->setNoten(n);

    wendeAn();
    starteOderStoppe();
    emit zustandGeaendert();
}

void CwTastung::sichere() const
{
    AppSettings& s = AppSettings::instance();
    s.setValue(kEin, wahr(m_eingeschaltet));
    s.setValue(kSender, wahr(m_senderTasten));
    s.setValue(kArt, QString::number(int(m_art)));
    s.setValue(kWpm, QString::number(m_wpm));
    s.setValue(kTausch, wahr(m_vertauscht));
    s.setValue(kNachlauf, QString::number(m_nachlaufMs));
    s.setValue(kTon, wahr(m_mithoerton));
    s.setValue(kTonHz, QString::number(m_ton->tonhoeheHz()));
    s.setValue(kTonLaut, QString::number(m_ton->lautstaerke()));
    s.setValue(kPort, m_portWunsch);
    const MidiKeyInput::Noten n = m_eingang->noten();
    s.setValue(kNotePunkt, QString::number(n.punkt));
    s.setValue(kNoteStrich, QString::number(n.strich));
    s.setValue(kNoteTaste, QString::number(n.handtaste));
    s.setValue(kNotePtt, QString::number(n.ptt));
}

void CwTastung::wendeAn()
{
    m_geber->setMode(m_art == Art::JambusA ? IambicKeyer::Mode::IambicA
                                           : IambicKeyer::Mode::IambicB);
    m_geber->setWpm(m_wpm);
    m_geber->setSwapPaddles(m_vertauscht);
    m_artFaden.store(int(m_art), std::memory_order_release);
    m_tonAn.store(m_mithoerton, std::memory_order_release);
}

void CwTastung::setEingeschaltet(bool an)
{
    if (m_eingeschaltet == an) { return; }
    m_eingeschaltet = an;
    sichere();
    starteOderStoppe();
    emit zustandGeaendert();
}

void CwTastung::setSenderTasten(bool an)
{
    if (m_senderTasten == an) { return; }
    // Beim Ausschalten zuerst loslassen -- solange der Schalter noch
    // steht, sonst bliebe ein Traeger stehen, den niemand mehr loest.
    if (!an) { lasseLos(/*sendenZurueck=*/true); }
    m_senderTasten = an;
    m_letzteMeldung.clear();
    sichere();
    emit zustandGeaendert();
}

void CwTastung::setArt(Art a)
{
    if (m_art == a) { return; }
    // Ein laufendes Zeichen gehoert zur alten Art -- und ein gehaltener
    // Fusstaster auch.
    lasseLos(/*sendenZurueck=*/true);
    m_geber->setPaddleState(false, false);
    m_geber->reset();
    m_art = a;
    wendeAn();
    sichere();
    starteOderStoppe();
    emit zustandGeaendert();
}

void CwTastung::setWpm(int wpm)
{
    wpm = std::clamp(wpm, kWpmMin, kWpmMax);
    if (m_wpm == wpm) { return; }
    m_wpm = wpm;
    wendeAn();
    sichere();
}

void CwTastung::setVertauscht(bool an)
{
    if (m_vertauscht == an) { return; }
    m_vertauscht = an;
    wendeAn();
    sichere();
}

void CwTastung::setNachlaufMs(int ms)
{
    ms = std::clamp(ms, kNachlaufMinMs, kNachlaufMaxMs);
    if (m_nachlaufMs == ms) { return; }
    m_nachlaufMs = ms;
    sichere();
}

void CwTastung::setMithoerton(bool an)
{
    if (m_mithoerton == an) { return; }
    m_mithoerton = an;
    wendeAn();
    if (!an) { m_ton->setTaste(false); }
    sichere();
    starteOderStoppe();
}

int CwTastung::tonhoeheHz() const { return m_ton->tonhoeheHz(); }

void CwTastung::setTonhoeheHz(int hz)
{
    if (m_ton->tonhoeheHz() == std::clamp(hz, 300, 1200)) { return; }
    m_ton->setTonhoeheHz(hz);
    sichere();
}

int CwTastung::tonLautstaerke() const { return m_ton->lautstaerke(); }

void CwTastung::setTonLautstaerke(int prozent)
{
    if (m_ton->lautstaerke() == std::clamp(prozent, 0, 100)) { return; }
    m_ton->setLautstaerke(prozent);
    sichere();
}

void CwTastung::setPortWunsch(const QString& name)
{
    if (m_portWunsch == name) { return; }
    m_portWunsch = name;
    sichere();
    if (m_eingeschaltet && !m_ohneGeraete) {
        m_eingang->oeffne(m_portWunsch);
    }
    emit zustandGeaendert();
}

MidiKeyInput::Noten CwTastung::noten() const { return m_eingang->noten(); }

void CwTastung::setNoten(const MidiKeyInput::Noten& n)
{
    MidiKeyInput::Noten neu;
    neu.punkt     = std::clamp(n.punkt, 0, 127);
    neu.strich    = std::clamp(n.strich, 0, 127);
    neu.handtaste = std::clamp(n.handtaste, 0, 127);
    neu.ptt       = std::clamp(n.ptt, 0, 127);
    m_eingang->setNoten(neu);
    sichere();
}

QStringList CwTastung::lesePortsNeu()
{
    if (m_ohneGeraete) { return {}; }
    const QStringList namen = m_eingang->ports();
    if (m_eingeschaltet) {
        // Immer neu oeffnen, auch wenn der Port „offen" ist: nach
        // Abziehen und Anstecken ist der alte Griff tot, und das System
        // sagt es nicht. Vorher loslassen -- ein gehaltener Fusstaster
        // bekaeme sein Note Off sonst nie.
        lasseLos(/*sendenZurueck=*/true);
        m_eingang->oeffne(m_portWunsch);
    }
    emit zustandGeaendert();
    return namen;
}

void CwTastung::starteOderStoppe()
{
    m_aktiv.store(m_eingeschaltet, std::memory_order_release);
    if (m_eingeschaltet) {
        m_geber->start();
        if (!m_ohneGeraete) {
            if (!m_eingang->istOffen()) { m_eingang->oeffne(m_portWunsch); }
            // Der Fusstaster braucht keinen Ton -- und eine offene
            // Tonausgabe, die nie klingt, ist nur ein Geraet mehr.
            if (m_mithoerton && m_art != Art::Fusstaster) { m_ton->start(); } else { m_ton->stop(); }
        }
        return;
    }
    m_eingang->schliesse();
    m_geber->setPaddleState(false, false);
    m_geber->stop();
    m_ton->stop();
    m_paddleGedrueckt = false;
    m_handtasteGedrueckt = false;
    m_eingangsWache.stop();
    lasseLos(/*sendenZurueck=*/true);
}

// ── Die Kante ────────────────────────────────────────────────────────

void CwTastung::kanteVomFaden(bool unten)
{
    // Zwei Quellen (Geber, Handtaste) auf einer Leitung: nur der Wechsel
    // zaehlt.
    if (m_leitung.exchange(unten, std::memory_order_acq_rel) == unten) { return; }
    // Der Ton SOFORT und hier -- siehe den Kopf.
    m_ton->setTaste(unten && m_tonAn.load(std::memory_order_acquire));
    QMetaObject::invokeMethod(this, [this, unten]() { kante(unten); }, Qt::QueuedConnection);
}

bool CwTastung::fordereSendenAn()
{
    QString warum;
    if (!m_ziel->bereit(&warum)) {
        melde(warum.isEmpty() ? tr("The radio is not ready for CW keying.") : warum);
        return false;
    }
    if (m_ziel->sendet()) { return true; }
    m_ziel->fordereSenden(true);
    if (!m_ziel->sendet()) {
        // Die Freigabe hat nein gesagt (Bandplan, Stehwelle, Sperre). Ihr
        // Grund steht dort, wo jede Ablehnung steht; hier geht nichts
        // hinaus.
        melde(tr("The radio did not go to transmit — nothing was keyed."));
        return false;
    }
    m_selbstAngefordert = true;
    return true;
}

void CwTastung::kante(bool unten)
{
    if (!unten) {
        if (m_sperre) { m_entsperren.start(m_nachlaufMs); }
        if (m_tasteUnten) {
            if (m_ziel) { m_ziel->taste(false); }
            m_tasteUnten = false;
            m_wache.stop();
            emit tasteGeaendert(false);
        }
        if (m_selbstAngefordert && !m_pttGehalten) { m_nachlauf.start(m_nachlaufMs); }
        return;
    }

    if (!m_eingeschaltet) { return; }
    // Ueben: der Ton ist schon gelaufen, zum Geraet geht nichts.
    if (!m_senderTasten || m_ziel == nullptr) { return; }
    if (m_tasteUnten) { return; }

    // Nach einem Ende von aussen erst wieder, wenn die Taste losgelassen
    // war: wer MOX ausschaltet, waehrend die Hand noch gibt, will nicht
    // einen Punkt spaeter wieder auf Sendung sein.
    if (m_sperre) {
        m_entsperren.stop();
        return;
    }
    // Das Senden gehoerte uns und ist weg, ohne dass es jemand gemeldet
    // haette: dasselbe Ende von aussen, nur frueher bemerkt.
    if (m_selbstAngefordert && !m_ziel->sendet()) {
        vonAussenBeendet();
        return;
    }

    m_nachlauf.stop();
    if (!fordereSendenAn()) { return; }

    m_ziel->taste(true);
    m_tasteUnten = true;
    m_letzteMeldung.clear();
    m_wache.start(m_wacheMs);
    emit tasteGeaendert(true);
}

void CwTastung::pttVomFaden(bool an)
{
    QMetaObject::invokeMethod(this, [this, an]() { pttGeaendert(an); }, Qt::QueuedConnection);
}

void CwTastung::pttGeaendert(bool an)
{
    if (an) {
        if (m_pttGehalten) { return; }
        if (!m_eingeschaltet || !m_senderTasten || m_ziel == nullptr) { return; }
        m_nachlauf.stop();
        // Wie „Hold to talk": gedrueckt heisst senden. Die Freigabe
        // entscheidet; sagt sie nein, steht ihr Grund dort, wo jede
        // Ablehnung steht.
        m_ziel->fordereSenden(true);
        if (!m_ziel->freigabeSteht()) {
            melde(tr("The radio did not go to transmit."));
            return;
        }
        // Gehalten gilt der Schalter nur, wenn das Senden auch steht.
        m_pttGehalten = true;
        m_letzteMeldung.clear();
        m_pttWache.start(m_pttWacheMs);
        emit zustandGeaendert();
        return;
    }
    if (!m_pttGehalten) { return; }
    m_pttGehalten = false;
    m_pttWache.stop();
    m_nachlauf.stop();
    // Erst die Taste, dann das Senden.
    if (m_tasteUnten) {
        if (m_ziel) { m_ziel->taste(false); }
        m_tasteUnten = false;
        m_wache.stop();
        emit tasteGeaendert(false);
    }
    m_selbstAngefordert = false;
    if (m_ziel) { m_ziel->fordereSenden(false); }
    emit zustandGeaendert();
}

void CwTastung::pttWacheAbgelaufen()
{
    if (!m_pttGehalten) { return; }
    qCWarning(lcCwTaste).nospace()
        << "Fusstaster: seit " << m_pttWacheMs / 1000 << " s gedrueckt — die "
           "Wache gibt das Senden zurueck (fehlendes Note Off, abgezogenes Kabel?)";
    pttGeaendert(false);
    melde(tr("The foot switch was held for %1 min and was released.")
              .arg(m_pttWacheMs / 60000));
}

void CwTastung::nachlaufAbgelaufen()
{
    if (m_tasteUnten || m_pttGehalten) { return; }
    if (!m_selbstAngefordert) { return; }
    m_selbstAngefordert = false;
    if (m_ziel) { m_ziel->fordereSenden(false); }
}

void CwTastung::lasseLos(bool sendenZurueck)
{
    m_nachlauf.stop();
    m_wache.stop();
    m_pttWache.stop();
    m_leitung.store(false, std::memory_order_release);
    m_ton->setTaste(false);
    if (m_tasteUnten) {
        if (m_ziel) { m_ziel->taste(false); }
        m_tasteUnten = false;
        emit tasteGeaendert(false);
    }
    // Das Senden gehoert uns, wenn die Taste ODER der Fusstaster es
    // geholt hat.
    const bool unsers = m_selbstAngefordert || m_pttGehalten;
    m_selbstAngefordert = false;
    m_pttGehalten = false;
    if (unsers && sendenZurueck && m_ziel) { m_ziel->fordereSenden(false); }
}

void CwTastung::vonAussenBeendet()
{
    m_nachlauf.stop();
    m_wache.stop();
    m_pttWache.stop();
    if (m_tasteUnten) {
        if (m_ziel) { m_ziel->taste(false); }
        m_tasteUnten = false;
        emit tasteGeaendert(false);
    }
    m_selbstAngefordert = false;
    // Ein noch gedrueckter Fusstaster holt das Senden NICHT zurueck: er
    // fordert nur auf der Kante, und die naechste ist sein Loslassen.
    m_pttGehalten = false;
    m_sperre = true;
    m_entsperren.start(m_nachlaufMs);
    qCInfo(lcCwTaste) << "CW-Taste: das Senden wurde von aussen beendet — "
                         "getastet wird erst wieder nach dem Loslassen";
    emit zustandGeaendert();
}

void CwTastung::sendenBeendet()
{
    // Ein Ende, das zu spaet kommt: das Geraet sendet laengst wieder
    // (MoxController meldet am ENDE seines Rueckwegs, und dazwischen kann
    // die naechste Kante das Senden neu geholt haben).
    if (m_ziel != nullptr && m_ziel->freigabeSteht()) { return; }
    // Unser eigenes Zurueckgeben nach dem Nachlauf: nichts zu tun.
    if (!m_tasteUnten && !m_selbstAngefordert && !m_pttGehalten) { return; }
    vonAussenBeendet();
}

void CwTastung::eingangBewegt()
{
    // Der Fusstaster hat seine eigene, viel laengere Wache: einen
    // Durchgang haelt man absichtlich laenger als 15 s.
    if (m_art == Art::Fusstaster) {
        m_eingangsWache.stop();
        return;
    }
    if (m_paddleGedrueckt || m_handtasteGedrueckt) {
        m_eingangsWache.start(m_wacheMs);
    } else {
        m_eingangsWache.stop();
    }
}

void CwTastung::wacheAbgelaufen()
{
    qCWarning(lcCwTaste).nospace()
        << "CW-Taste: seit " << m_wacheMs << " ms unveraendert gedrueckt — "
           "die Wache nimmt sie hoch (fehlendes Note Off, abgezogenes Kabel?)";
    m_eingangsWache.stop();
    m_paddleGedrueckt = false;
    m_handtasteGedrueckt = false;
    m_geber->setPaddleState(false, false);
    m_geber->reset();
    m_wache.stop();
    m_leitung.store(false, std::memory_order_release);
    m_ton->setTaste(false);
    if (m_tasteUnten) {
        if (m_ziel) { m_ziel->taste(false); }
        m_tasteUnten = false;
        emit tasteGeaendert(false);
    }
    // Das Senden geht den gewohnten Weg zurueck: ueber den Nachlauf. Ein
    // gehaltener Fusstaster bleibt, was er ist -- er hat seine eigene
    // Wache.
    if (m_selbstAngefordert && !m_pttGehalten) { m_nachlauf.start(m_nachlaufMs); }
    melde(tr("The key was held for %1 s without a change and was released.")
              .arg(m_wacheMs / 1000));
}

void CwTastung::melde(const QString& text)
{
    if (text == m_letzteMeldung) { return; }   // einmal, nicht je Punkt
    m_letzteMeldung = text;
    qCInfo(lcCwTaste).noquote() << "CW-Taste:" << text;
    emit zustandGeaendert();
}

QString CwTastung::zustandsText() const
{
    if (!m_eingeschaltet) { return tr("Off."); }
    QStringList teile;
    if (m_eingang->istOffen()) {
        teile << tr("Key: %1.").arg(m_eingang->portName());
    } else if (m_ohneGeraete) {
        teile << tr("No MIDI port in use.");
    } else {
        teile << tr("No MIDI key found — plug it in and press Rescan.");
    }
    if (!m_senderTasten) {
        teile << tr("Practice: nothing goes to the radio.");
    } else if (m_ziel == nullptr) {
        teile << tr("No radio to key.");
    } else if (m_art == Art::Fusstaster) {
        teile << tr("The foot switch transmits (PTT), in every mode.");
    } else {
        QString warum;
        if (m_ziel->bereit(&warum)) {
            teile << tr("Keys the transmitter.");
        } else {
            teile << tr("Would key the transmitter, but: %1").arg(warum);
        }
    }
    if (m_pttGehalten) { teile << tr("Foot switch down."); }
    if (m_sperre) { teile << tr("Transmit was ended from outside — release the key."); }
    if (!m_letzteMeldung.isEmpty()) { teile << m_letzteMeldung; }
    return teile.join(QLatin1Char(' '));
}

} // namespace Longpath
