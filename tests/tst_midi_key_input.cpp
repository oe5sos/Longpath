// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_midi_key_input.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Die Morsetaste und der Fusstaster am HaliKey MIDI
// (src/core/midi/MidiKeyInput): was eine MIDI-Botschaft bedeutet und
// welcher Port genommen wird -- beides reine Funktionen, ohne Geraet.
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test.

#include <QtTest>

#include <QSignalSpy>

#include "core/midi/MidiKeyInput.h"

using namespace Longpath;
using E = MidiKeyInput::Ereignis;

class TstMidiKeyInput : public QObject {
    Q_OBJECT

private slots:
    // Die Noten des Handbuchs: 20 Punkt, 21 Strich, 30 Handtaste, 31 PTT.
    void dieNotenDesHandbuchs()
    {
        const MidiKeyInput::Noten n;
        QCOMPARE(MidiKeyInput::deute({0x90, 20, 100}, n), E::PunktAn);
        QCOMPARE(MidiKeyInput::deute({0x80, 20, 0}, n), E::PunktAus);
        QCOMPARE(MidiKeyInput::deute({0x90, 21, 100}, n), E::StrichAn);
        QCOMPARE(MidiKeyInput::deute({0x80, 21, 64}, n), E::StrichAus);
        QCOMPARE(MidiKeyInput::deute({0x90, 30, 1}, n), E::TasteAn);
        QCOMPARE(MidiKeyInput::deute({0x80, 30, 0}, n), E::TasteAus);
        QCOMPARE(MidiKeyInput::deute({0x90, 31, 127}, n), E::PttAn);
        QCOMPARE(MidiKeyInput::deute({0x80, 31, 0}, n), E::PttAus);
    }

    // Viele Geraete schicken das Loslassen als Note On mit Anschlag 0.
    // Wer das als „gedrueckt" liest, laesst den Sender stehen.
    void noteOnMitAnschlagNullIstLoslassen()
    {
        const MidiKeyInput::Noten n;
        QCOMPARE(MidiKeyInput::deute({0x90, 31, 0}, n), E::PttAus);
        QCOMPARE(MidiKeyInput::deute({0x90, 20, 0}, n), E::PunktAus);
    }

    // Der Kanal zaehlt nicht: das Handbuch nennt keinen.
    void jederKanal()
    {
        const MidiKeyInput::Noten n;
        for (unsigned char kanal = 0; kanal < 16; ++kanal) {
            QCOMPARE(MidiKeyInput::deute({static_cast<unsigned char>(0x90 | kanal), 31, 90}, n),
                     E::PttAn);
            QCOMPARE(MidiKeyInput::deute({static_cast<unsigned char>(0x80 | kanal), 31, 0}, n),
                     E::PttAus);
        }
    }

    void allesAndereIstNichts()
    {
        const MidiKeyInput::Noten n;
        QCOMPARE(MidiKeyInput::deute({0x90, 60, 100}, n), E::Keins);   // fremde Note
        QCOMPARE(MidiKeyInput::deute({0xB0, 20, 100}, n), E::Keins);   // Regler
        QCOMPARE(MidiKeyInput::deute({0xC0, 20}, n), E::Keins);        // zu kurz
        QCOMPARE(MidiKeyInput::deute({}, n), E::Keins);
    }

    void eigeneNoten()
    {
        MidiKeyInput::Noten n;
        n.ptt = 64;
        QCOMPARE(MidiKeyInput::deute({0x90, 64, 100}, n), E::PttAn);
        QCOMPARE(MidiKeyInput::deute({0x90, 31, 100}, n), E::Keins);
    }

    // Nie einfach der erste Port: ein Klavier ist keine Morsetaste.
    void portwahl()
    {
        const QStringList ports{QStringLiteral("IAC-Treiber Bus 1"),
                                QStringLiteral("Digital Piano"),
                                QStringLiteral("HaliKey MIDI")};
        QCOMPARE(MidiKeyInput::waehlePort(ports, QString()), 2);
        QCOMPARE(MidiKeyInput::waehlePort(ports, QStringLiteral("Digital Piano")), 1);
        // Der Wunsch fehlt: zurueck auf die Suche nach dem Namen.
        QCOMPARE(MidiKeyInput::waehlePort(ports, QStringLiteral("gibt es nicht")), 2);
        QCOMPARE(MidiKeyInput::waehlePort({QStringLiteral("halikey midi port 1")}, QString()), 0);
        QCOMPARE(MidiKeyInput::waehlePort({QStringLiteral("Digital Piano")}, QString()), -1);
        QCOMPARE(MidiKeyInput::waehlePort({}, QString()), -1);
    }

    // Die Senken bekommen den STAND beider Kontakte, nicht die Kante.
    void senkenUndSignale()
    {
        MidiKeyInput eingang;
        QList<QPair<bool, bool>> paddle;
        QList<bool> taste;
        eingang.setPaddleSenke([&paddle](bool p, bool s) { paddle.append({p, s}); });
        eingang.setTastenSenke([&taste](bool u) { taste.append(u); });
        QSignalSpy ptt(&eingang, &MidiKeyInput::ptt);

        eingang.verarbeite({0x90, 20, 100});
        eingang.verarbeite({0x90, 21, 100});
        eingang.verarbeite({0x80, 20, 0});
        eingang.verarbeite({0x80, 21, 0});
        const QList<QPair<bool, bool>> soll{{true, false}, {true, true}, {false, true}, {false, false}};
        QCOMPARE(paddle, soll);

        eingang.verarbeite({0x90, 30, 100});
        eingang.verarbeite({0x90, 30, 0});
        QCOMPARE(taste, (QList<bool>{true, false}));

        eingang.verarbeite({0x90, 31, 100});
        eingang.verarbeite({0x80, 31, 0});
        QCOMPARE(ptt.count(), 2);
        QCOMPARE(ptt.at(0).at(0).toBool(), true);
        QCOMPARE(ptt.at(1).at(0).toBool(), false);
    }

    // schliesse() laesst los -- der Besitzer soll es erfahren.
    void schliessenLaesstLos()
    {
        MidiKeyInput eingang;
        QList<QPair<bool, bool>> paddle;
        QList<bool> taste;
        eingang.setPaddleSenke([&paddle](bool p, bool s) { paddle.append({p, s}); });
        eingang.setTastenSenke([&taste](bool u) { taste.append(u); });
        eingang.verarbeite({0x90, 20, 100});
        paddle.clear();
        eingang.schliesse();
        const QList<QPair<bool, bool>> soll{{false, false}};
        QCOMPARE(paddle, soll);
        QCOMPARE(taste, (QList<bool>{false}));
    }

    // Der Abbau dagegen ruft niemanden mehr: die Senke gehoert meist
    // jemandem, der sich gerade selbst abbaut. So gefunden auf der
    // macOS-CI (SIGABRT am Ende von senkenUndSignale, 2026-11-17).
    void abbauRuftKeineSenke()
    {
        int rufe = 0;
        {
            MidiKeyInput eingang;
            eingang.setPaddleSenke([&rufe](bool, bool) { ++rufe; });
            eingang.setTastenSenke([&rufe](bool) { ++rufe; });
            eingang.verarbeite({0x90, 20, 100});   // ein Kontakt steht noch
            rufe = 0;
        }
        QCOMPARE(rufe, 0);
    }
};

QTEST_GUILESS_MAIN(TstMidiKeyInput)
#include "tst_midi_key_input.moc"
