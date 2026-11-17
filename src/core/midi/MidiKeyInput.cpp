// =================================================================
// src/core/midi/MidiKeyInput.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: MidiKeyInput.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/midi/MidiKeyInput.h"

#include <QLoggingCategory>

#include <RtMidi.h>

Q_LOGGING_CATEGORY(lcMidiKey, "longpath.midikey")

namespace Longpath {

MidiKeyInput::Ereignis MidiKeyInput::deute(const std::vector<unsigned char>& b,
                                           const Noten& noten)
{
    if (b.size() < 3) { return Ereignis::Keins; }
    const unsigned char art = b[0] & 0xF0;   // Kanal (untere vier Bit) zaehlt nicht
    const int note = b[1];
    const int anschlag = b[2];

    bool an = false;
    if (art == 0x90) {
        an = anschlag > 0;                    // Note On mit Anschlag 0 = Note Off
    } else if (art == 0x80) {
        an = false;
    } else {
        return Ereignis::Keins;               // Regler, Programmwechsel, …
    }

    // Die Reihenfolge entscheidet, wenn jemand zwei Rollen auf dieselbe
    // Note legt: das Paddle gewinnt vor der Handtaste, die Taste vor PTT.
    if (note == noten.punkt)     { return an ? Ereignis::PunktAn  : Ereignis::PunktAus; }
    if (note == noten.strich)    { return an ? Ereignis::StrichAn : Ereignis::StrichAus; }
    if (note == noten.handtaste) { return an ? Ereignis::TasteAn  : Ereignis::TasteAus; }
    if (note == noten.ptt)       { return an ? Ereignis::PttAn    : Ereignis::PttAus; }
    return Ereignis::Keins;
}

int MidiKeyInput::waehlePort(const QStringList& ports, const QString& wunsch)
{
    if (!wunsch.isEmpty()) {
        const int i = ports.indexOf(wunsch);
        if (i >= 0) { return i; }
    }
    for (int i = 0; i < ports.size(); ++i) {
        if (ports.at(i).contains(QStringLiteral("HaliKey"), Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

MidiKeyInput::MidiKeyInput(QObject* eltern) : QObject(eltern) {}

MidiKeyInput::~MidiKeyInput()
{
    schliesse();
}

QStringList MidiKeyInput::ports() const
{
    QStringList namen;
    try {
        RtMidiIn sonde;
        const unsigned int n = sonde.getPortCount();
        for (unsigned int i = 0; i < n; ++i) {
            namen << QString::fromStdString(sonde.getPortName(i));
        }
    } catch (const RtMidiError& e) {
        qCWarning(lcMidiKey).noquote()
            << "MIDI-Eingaenge lassen sich nicht auflisten:"
            << QString::fromStdString(e.getMessage());
    }
    return namen;
}

bool MidiKeyInput::oeffne(const QString& wunsch)
{
    schliesse();
    m_fehler.clear();
    try {
        m_eingang = std::make_unique<RtMidiIn>();
        QStringList namen;
        const unsigned int n = m_eingang->getPortCount();
        for (unsigned int i = 0; i < n; ++i) {
            namen << QString::fromStdString(m_eingang->getPortName(i));
        }
        const int welcher = waehlePort(namen, wunsch);
        if (welcher < 0) {
            m_fehler = namen.isEmpty()
                ? QStringLiteral("kein MIDI-Eingang am Rechner")
                : QStringLiteral("kein HaliKey unter den MIDI-Eingaengen (%1)")
                      .arg(namen.join(QStringLiteral(", ")));
            qCInfo(lcMidiKey).noquote() << "MIDI-Taste:" << m_fehler;
            m_eingang.reset();
            return false;
        }
        // Nur Noten: Takt, Aktiv-Abtastung und SysEx bleiben draussen.
        m_eingang->ignoreTypes(true, true, true);
        m_eingang->setCallback(
            [](double, std::vector<unsigned char>* botschaft, void* selbst) {
                if (botschaft && selbst) {
                    static_cast<MidiKeyInput*>(selbst)->verarbeite(*botschaft);
                }
            },
            this);
        m_eingang->openPort(static_cast<unsigned int>(welcher), "Longpath CW key");
        m_portName = namen.at(welcher);
        m_offen.store(true);
        qCInfo(lcMidiKey).noquote() << "MIDI-Taste: offen an" << m_portName;
        emit geoeffnet(m_portName);
        return true;
    } catch (const RtMidiError& e) {
        m_fehler = QString::fromStdString(e.getMessage());
        qCWarning(lcMidiKey).noquote() << "MIDI-Taste: Oeffnen gescheitert —" << m_fehler;
        m_eingang.reset();
        return false;
    }
}

void MidiKeyInput::schliesse()
{
    const bool war = m_offen.exchange(false);
    if (m_eingang) {
        try {
            m_eingang->cancelCallback();
            m_eingang->closePort();
        } catch (const RtMidiError&) {
            // Beim Schliessen ist nichts mehr zu retten.
        }
        m_eingang.reset();
    }
    // Ein Kontakt, der beim Abziehen „gedrueckt" stand, darf nicht
    // haengen bleiben: alles loslassen.
    if (m_punkt.exchange(false) | m_strich.exchange(false)) {
        if (m_paddleSenke) { m_paddleSenke(false, false); }
        emit paddle(false, false);
    }
    if (m_tastenSenke) { m_tastenSenke(false); }
    if (war) {
        m_portName.clear();
        emit geschlossen();
    }
}

void MidiKeyInput::setNoten(const Noten& n)
{
    m_notePunkt.store(n.punkt);
    m_noteStrich.store(n.strich);
    m_noteTaste.store(n.handtaste);
    m_notePtt.store(n.ptt);
}

MidiKeyInput::Noten MidiKeyInput::noten() const
{
    Noten n;
    n.punkt = m_notePunkt.load();
    n.strich = m_noteStrich.load();
    n.handtaste = m_noteTaste.load();
    n.ptt = m_notePtt.load();
    return n;
}

void MidiKeyInput::setPaddleSenke(std::function<void(bool, bool)> senke)
{
    m_paddleSenke = std::move(senke);
}

void MidiKeyInput::setTastenSenke(std::function<void(bool)> senke)
{
    m_tastenSenke = std::move(senke);
}

void MidiKeyInput::verarbeite(const std::vector<unsigned char>& botschaft)
{
    switch (deute(botschaft, noten())) {
    case Ereignis::PunktAn:
    case Ereignis::PunktAus:
    case Ereignis::StrichAn:
    case Ereignis::StrichAus: {
        const Ereignis e = deute(botschaft, noten());
        if (e == Ereignis::PunktAn)   { m_punkt.store(true); }
        if (e == Ereignis::PunktAus)  { m_punkt.store(false); }
        if (e == Ereignis::StrichAn)  { m_strich.store(true); }
        if (e == Ereignis::StrichAus) { m_strich.store(false); }
        const bool p = m_punkt.load();
        const bool s = m_strich.load();
        if (m_paddleSenke) { m_paddleSenke(p, s); }
        emit paddle(p, s);
        break;
    }
    case Ereignis::TasteAn:
        if (m_tastenSenke) { m_tastenSenke(true); }
        emit handtaste(true);
        break;
    case Ereignis::TasteAus:
        if (m_tastenSenke) { m_tastenSenke(false); }
        emit handtaste(false);
        break;
    case Ereignis::PttAn:  emit ptt(true);  break;
    case Ereignis::PttAus: emit ptt(false); break;
    case Ereignis::Keins:  break;
    }
}

} // namespace Longpath
