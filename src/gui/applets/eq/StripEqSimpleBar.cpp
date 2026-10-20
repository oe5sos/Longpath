// =================================================================
// src/gui/applets/eq/StripEqSimpleBar.cpp  (Longpath)
// =================================================================
// Siehe StripEqSimpleBar.h.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include "gui/applets/eq/StripEqSimpleBar.h"

#include "core/strip/ClientEq.h"
#include "core/strip/EqBandLayout.h"
#include "gui/StyleConstants.h"
#include "gui/applets/eq/EqHost.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

namespace Longpath {

namespace {

// Der Regler zaehlt in Zehntel-Dezibel, damit er ganzzahlig bleibt und
// trotzdem fein genug greift.
constexpr int kRange = 120;   // ±12,0 dB

QString hzText(float hz)
{
    if (hz >= 1000.0f) {
        const double k = double(hz) / 1000.0;
        return QStringLiteral("%1 kHz")
            .arg(k, 0, 'f', (std::fabs(k - std::round(k)) < 0.05) ? 0 : 1);
    }
    return QStringLiteral("%1 Hz").arg(int(std::lround(hz)));
}

QString dbText(float db)
{
    if (std::fabs(db) < 0.05f) { return QStringLiteral("0 dB"); }
    return QStringLiteral("%1%2 dB")
        .arg(db > 0 ? QStringLiteral("+") : QString())
        .arg(double(db), 0, 'f', 1);
}

} // namespace

const std::array<StripEqSimpleBar::Slot, StripEqSimpleBar::kCount>&
StripEqSimpleBar::bandSlots()
{
    // ── Warum gerade diese sieben ────────────────────────────────────
    //
    // Der Bandsatz hat vierzehn Plaetze (EqBandLayout.h). Die ersten
    // sieben gehoeren den Ausgangspunkten: ein Hochpass, drei
    // Brummkerben und drei geformte Baender, die zusammen den Klang
    // eines Profils ausmachen. Die letzten sieben sind ausdruecklich
    // „Griffe, die darauf warten, angefasst zu werden" -- flach,
    // ausgeschaltet, und in BEIDEN Wegen gleich: nach einem Profil
    // genauso wie nach Zuruecksetzen.
    //
    // Darum haengen die einfachen Regler an ihnen und nicht an den
    // ersten sieben. Zwei Folgen, beide gewollt:
    //
    //   * Der Ausgangspunkt bleibt, was er ist. Wer „Clean voice"
    //     gewaehlt hat, hoert weiter Clean voice -- die Regler legen
    //     sich darueber, statt es zu ueberschreiben.
    //   * Die Regler stehen beim ersten Oeffnen auf null. Das ist die
    //     ehrliche Aussage: das hier ist, was DU dazugetan hast.
    //
    // Die Frequenzen sind die der Griffe, nicht umgekehrt -- hier wird
    // benannt, nicht gesetzt.
    // ── Wie breit ein Regler greift ──────────────────────────────────
    //
    // „ich denke, dass die bereiche teilweise viel zu groß sind, die
    //  geändert werden."
    //
    // Stimmt, und es war eine geerbte Zahl: die Griffe kommen mit
    // Q = 0,707 auf die Welt (Butterworth, der neutrale Vorgabewert
    // aus ClientEq). Das sind rund ZWEI OKTAVEN -- wer bei 1,4 kHz
    // anhebt, hebt von 600 Hz bis 5 kHz an. Als Klangregler ist das
    // richtig, als „Praesenz" ist es die halbe Stimme.
    //
    // Q = 2,0 sind rund 0,7 Oktaven. Die Griffe stehen im Abstand von
    // etwa 0,58 Oktaven, also greifen Nachbarn noch ineinander (bei
    // Q = 2,5 wuerden sie sich gerade beruehren und die Summe bekaeme
    // Wellen) -- aber jeder Regler meint jetzt seine Gegend und nicht
    // die halbe Stimme.
    //
    // Gesetzt wird das erst, wenn an einem Regler GEZOGEN wird, nicht
    // beim Umschalten der Ansicht. Die Zusage „Umschalten aendert den
    // Ton nicht" gilt weiter.
    static constexpr float kQ = 2.0f;

    static const std::array<Slot, kCount> kSlots = {{
        { EqBandLayout::kFirstSpare + 0, "WÄRME",     300.0f,  kQ,
          "300 Hz. Hoch macht die Stimme voller, runter nimmt das"
          " Dumpfe." },
        { EqBandLayout::kFirstSpare + 1, "FÜLLE",     450.0f,  kQ,
          "450 Hz. Der Unterbau der Stimme." },
        { EqBandLayout::kFirstSpare + 2, "MULM",      650.0f,  kQ,
          "650 Hz. Runter, wenn es nach Pappkarton klingt." },
        { EqBandLayout::kFirstSpare + 3, "NASAL",     950.0f,  kQ,
          "950 Hz. Runter, wenn es durch die Nase klingt." },
        { EqBandLayout::kFirstSpare + 4, "KLARHEIT", 1400.0f,  kQ,
          "1,4 kHz. Verständlichkeit — das, was im Pile-up"
          " durchkommt." },
        { EqBandLayout::kFirstSpare + 5, "PRÄSENZ",  2500.0f,  kQ,
          "2,5 kHz. Das „vorne“. Zuviel davon wird anstrengend." },
        { EqBandLayout::kFirstSpare + 6, "SCHÄRFE",  3800.0f,  kQ,
          "3,8 kHz. Biss und S-Laute. Runter, wenn es zischt." },
    }};
    return kSlots;
}

StripEqSimpleBar::StripEqSimpleBar(QWidget* parent)
    : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(0);

    const QString nameCss = QStringLiteral(
        "QLabel { color: %1; font-size: 9px; font-weight: bold;"
        " letter-spacing: 1px; background: transparent; }")
        .arg(QString::fromLatin1(Style::kTextScale));
    const QString freqCss = QStringLiteral(
        "QLabel { color: %1; font-size: 9px; background: transparent; }")
        .arg(QString::fromLatin1(Style::kTextInactive));
    const QString valueCss = QStringLiteral(
        "QLabel { color: %1; font-size: 11px; background: transparent; }")
        .arg(QString::fromLatin1(Style::kTextPrimary));

    for (int i = 0; i < kCount; ++i) {
        const Slot& s = bandSlots()[static_cast<size_t>(i)];

        auto* col = new QVBoxLayout;
        col->setSpacing(2);
        col->setContentsMargins(0, 0, 0, 0);

        auto* name = new QLabel(QString::fromUtf8(s.name), this);
        name->setAlignment(Qt::AlignCenter);
        name->setStyleSheet(nameCss);
        name->setToolTip(QString::fromUtf8(s.what));
        col->addWidget(name);

        auto* freq = new QLabel(QStringLiteral("—"), this);
        freq->setAlignment(Qt::AlignCenter);
        freq->setStyleSheet(freqCss);
        col->addWidget(freq);
        m_freqLabels[static_cast<size_t>(i)] = freq;

        auto* sl = new QSlider(Qt::Vertical, this);
        sl->setRange(-kRange, kRange);
        sl->setValue(0);
        sl->setMinimumHeight(96);
        sl->setStyleSheet(Style::sliderVStyle());
        sl->setToolTip(QString::fromUtf8(s.what));
        // Doppelklick kann ein QSlider nicht; die Mittenstellung
        // bekommt man ueber das Rad oder die Tasten genau. Ein Schritt
        // ist 0,5 dB, eine Seite 3 dB.
        sl->setSingleStep(5);
        sl->setPageStep(30);
        col->addWidget(sl, 1, Qt::AlignHCenter);
        m_sliders[static_cast<size_t>(i)] = sl;

        // Die Zielzeile steht ZWISCHEN Name und Regler, nicht unten:
        // dort schaut man hin, bevor man anfasst.
        auto* ziel = new QLabel(QStringLiteral("—"), this);
        ziel->setAlignment(Qt::AlignCenter);
        ziel->setStyleSheet(freqCss);
        col->addWidget(ziel);
        m_zielLabels[static_cast<size_t>(i)] = ziel;

        auto* val = new QLabel(QStringLiteral("0 dB"), this);
        val->setAlignment(Qt::AlignCenter);
        val->setStyleSheet(valueCss);
        col->addWidget(val);
        m_valueLabels[static_cast<size_t>(i)] = val;

        row->addLayout(col, 1);

        connect(sl, &QSlider::valueChanged, this, [this, i](int v) {
            if (m_reloading) { return; }
            applyBand(i, v);
        });
    }
}

void StripEqSimpleBar::setEq(ClientEq* eq)
{
    m_eq = eq;
    // Die sieben Griffe muessen da sein, sonst greifen die Regler ins
    // Leere. ensureSeeded() haengt nur an, es ruehrt kein vorhandenes
    // Band an -- siehe EqBandLayout.h.
    if (m_eq != nullptr) { EqBandLayout::ensureSeeded(*m_eq); }
    reload();
}

void StripEqSimpleBar::setHost(EqHost* host) { m_host = host; }

void StripEqSimpleBar::reload()
{
    m_reloading = true;
    for (int i = 0; i < kCount; ++i) {
        const Slot& s = bandSlots()[static_cast<size_t>(i)];
        const size_t k = static_cast<size_t>(i);
        if (m_eq == nullptr) {
            m_sliders[k]->setValue(0);
            m_freqLabels[k]->setText(QStringLiteral("—"));
            m_valueLabels[k]->setText(QStringLiteral("0 dB"));
            continue;
        }
        const auto bp = m_eq->band(s.band);
        // Ein ausgeschaltetes Band steht auf 0, egal was in seinem
        // Pegel gespeichert ist -- sonst zeigt der Regler eine
        // Anhebung an, die nicht zu hoeren ist.
        const float db = bp.enabled ? bp.gainDb : 0.0f;
        m_sliders[k]->setValue(int(std::lround(db * 10.0f)));
        m_freqLabels[k]->setText(hzText(bp.freqHz));
        m_valueLabels[k]->setText(dbText(db));
    }
    m_reloading = false;
}

void StripEqSimpleBar::tidy()
{
    if (m_eq == nullptr) { return; }
    for (const Slot& s : bandSlots()) {
        auto bp = m_eq->band(s.band);
        bp.freqHz = s.freqHz;
        bp.q      = s.q;
        m_eq->setBand(s.band, bp);
    }
    if (m_host != nullptr) { m_host->saveClientEqSettings(); }
    reload();
    emit changed();
}

void StripEqSimpleBar::setZiel(const QVector<double>& abweichungDb)
{
    const bool gueltig = abweichungDb.size() == kCount;
    for (int i = 0; i < kCount; ++i) {
        QLabel* l = m_zielLabels[static_cast<size_t>(i)];
        if (l == nullptr) { continue; }
        if (!gueltig) {
            // „Unbekannt ist ein Strich, keine Null" (HAUSSTIL §7).
            l->setText(QStringLiteral("—"));
            l->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 9px;"
                " background: transparent; }")
                .arg(QString::fromLatin1(Style::kTextInactive)));
            continue;
        }
        const double d = abweichungDb[i];
        QString text;
        const char* farbe = Style::kTextInactive;
        // Drei Dezibel Spielraum. Enger wäre eine Genauigkeit, die
        // weder die Messung noch das Ohr hergibt -- und sie würde zu
        // einer Jagd nach einer Zahl einladen, die nichts bedeutet.
        if (d > 3.0) {
            text  = QStringLiteral("%1 zuviel").arg(-d, 0, 'f', 0);
            farbe = Style::kAmberText;
        } else if (d < -3.0) {
            text  = QStringLiteral("+%1 fehlt").arg(-d, 0, 'f', 0);
            farbe = Style::kGlassSelBorder;
        } else {
            text  = QStringLiteral("passt");
            farbe = Style::kGreenText;
        }
        l->setText(text);
        l->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; background: transparent; }")
            .arg(QString::fromLatin1(farbe)));
    }
}

void StripEqSimpleBar::applyBand(int i, int sliderValue)
{
    if (m_eq == nullptr) { return; }
    const Slot& s = bandSlots()[static_cast<size_t>(i)];
    const float db = float(sliderValue) / 10.0f;

    auto bp = m_eq->band(s.band);
    bp.gainDb  = db;
    // Angefasst heisst auch: dieser Griff ist jetzt das, was sein
    // Name sagt -- auf seiner Frequenz und in seiner Breite. Ohne das
    // bleibt ein in der Profi-Ansicht verschobener Griff unter einem
    // Namen stehen, der nicht mehr stimmt (gesehen: NASAL auf 2 kHz,
    // oberhalb von KLARHEIT auf 1,5 kHz -- die Reihe stand nicht mehr
    // der Reihe nach).
    bp.freqHz  = s.freqHz;
    bp.q       = s.q;
    // Angefasst heisst eingeschaltet. Bei 0 dB ist das hoerbar
    // dasselbe wie aus, und es erspart den Fall „Regler steht auf +4,
    // Band ist aus".
    bp.enabled = true;
    m_eq->setBand(s.band, bp);

    m_valueLabels[static_cast<size_t>(i)]->setText(dbText(db));
    if (m_host != nullptr) { m_host->saveClientEqSettings(); }
    emit changed();
}

} // namespace Longpath
