// =================================================================
// src/gui/setup/HaliKeyGroup.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: HaliKeyGroup.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/setup/HaliKeyGroup.h"

#include "core/cw/CwTastung.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Longpath {

HaliKeyGroup::HaliKeyGroup(CwTastung* cwKey, QWidget* parent)
    : QGroupBox(parent)
    , m_cwKey(cwKey)
{
    buildUI();
    refresh();
}

void HaliKeyGroup::buildUI()
{
    setTitle(tr("Foot switch and CW key (HaliKey MIDI)"));
    setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* box = new QVBoxLayout(this);
    box->setSpacing(6);

    const auto check = [this, box](const QString& text, const QString& tip) {
        auto* c = new QCheckBox(text, this);
        c->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        c->setToolTip(tip);
        box->addWidget(c);
        return c;
    };
    const auto hint = [this, box](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setWordWrap(true);
        l->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        box->addWidget(l);
        return l;
    };

    m_cwEnabled = check(
        tr("Read a HaliKey MIDI (foot switch, paddle, straight key)"),
        tr("Longpath reads the HaliKey. Nothing goes to the radio unless the switch below is on."));
    m_cwKeyTx = check(
        tr("Key the transmitter"),
        tr("The foot switch transmits like the MOX button, in every mode. "
           "A Morse key puts a connected FlexRadio on the air in CW. "
           "Both go through the same transmit check as MOX."));
    hint(tr("Off: nothing goes to the radio — a Morse key only makes the sidetone, for practice. "
            "On: the foot switch (PTT input, or the key jack with key type “Foot switch”) holds "
            "transmit while it is down. A Morse key asks for transmit with the first element; if "
            "that is refused, nothing is keyed."));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(3, 1);
    int row = 0;
    const auto label = [this, grid](int r, int c, const QString& text) {
        grid->addWidget(new QLabel(text, this), r, c);
    };
    const auto spin = [this](int from, int to, const QString& suffix) {
        auto* s = new QSpinBox(this);
        s->setRange(from, to);
        s->setSuffix(suffix);
        s->setStyleSheet(Style::spinBoxStyle());
        s->setKeyboardTracking(false);
        return s;
    };

    label(row, 0, tr("MIDI port"));
    m_cwPort = new QComboBox(this);
    m_cwPort->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_cwPort->setToolTip(tr("Automatic takes the first port with “HaliKey” in its name — "
                            "never just any MIDI device."));
    grid->addWidget(m_cwPort, row, 1, 1, 2);
    m_cwRescan = new QPushButton(tr("Rescan"), this);
    m_cwRescan->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_cwRescan->setToolTip(tr("Read the MIDI ports again — after plugging the key in."));
    grid->addWidget(m_cwRescan, row, 3, Qt::AlignLeft);
    ++row;

    label(row, 0, tr("Key type"));
    m_cwType = new QComboBox(this);
    m_cwType->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_cwType->addItem(tr("Iambic A"), int(CwTastung::Art::JambusA));
    m_cwType->addItem(tr("Iambic B"), int(CwTastung::Art::JambusB));
    m_cwType->addItem(tr("Straight key"), int(CwTastung::Art::Handtaste));
    m_cwType->addItem(tr("Foot switch (PTT)"), int(CwTastung::Art::Fusstaster));
    m_cwType->setToolTip(tr("What is plugged into the HaliKey's key jack. The PTT input is "
                            "a foot switch in every setting."));
    grid->addWidget(m_cwType, row, 1);
    label(row, 2, tr("Speed"));
    m_cwWpm = spin(CwTastung::kWpmMin, CwTastung::kWpmMax, tr(" WPM"));
    grid->addWidget(m_cwWpm, row, 3, Qt::AlignLeft);
    ++row;

    m_cwSwap = new QCheckBox(tr("Swap paddles"), this);
    m_cwSwap->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    grid->addWidget(m_cwSwap, row, 1);
    label(row, 2, tr("Hang time"));
    m_cwHang = spin(CwTastung::kNachlaufMinMs, CwTastung::kNachlaufMaxMs, tr(" ms"));
    m_cwHang->setSingleStep(50);
    m_cwHang->setToolTip(tr("How long the radio stays on transmit after the last element."));
    grid->addWidget(m_cwHang, row, 3, Qt::AlignLeft);
    ++row;

    m_cwSidetone = new QCheckBox(tr("Sidetone"), this);
    m_cwSidetone->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_cwSidetone->setToolTip(tr("A local tone under your own key, on the Mac's default output. "
                                "The radio's own sidetone arrives too late over the network."));
    grid->addWidget(m_cwSidetone, row, 1);
    label(row, 2, tr("Pitch"));
    m_cwPitch = spin(300, 1200, tr(" Hz"));
    m_cwPitch->setSingleStep(10);
    grid->addWidget(m_cwPitch, row, 3, Qt::AlignLeft);
    ++row;

    label(row, 0, tr("Sidetone volume"));
    m_cwVolume = new QSlider(Qt::Horizontal, this);
    m_cwVolume->setRange(0, 100);
    m_cwVolume->setStyleSheet(QString::fromLatin1(Style::kSliderStyle));
    grid->addWidget(m_cwVolume, row, 1, 1, 2);
    ++row;

    label(row, 0, tr("MIDI notes"));
    auto* notes = new QHBoxLayout;
    notes->setSpacing(6);
    const auto note = [this, notes, &spin](const QString& text) {
        notes->addWidget(new QLabel(text, this));
        QSpinBox* s = spin(0, 127, QString());
        notes->addWidget(s);
        return s;
    };
    m_cwNoteDit = note(tr("Dit"));
    m_cwNoteDah = note(tr("Dah"));
    m_cwNoteKey = note(tr("Straight key"));
    m_cwNotePtt = note(tr("PTT"));
    notes->addStretch();
    grid->addLayout(notes, row, 1, 1, 3);
    box->addLayout(grid);

    m_cwStatus = new QLabel(this);
    m_cwStatus->setWordWrap(true);
    m_cwStatus->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    box->addWidget(m_cwStatus);

    if (m_cwKey == nullptr) {
        setEnabled(false);
        m_cwStatus->setText(tr("The key needs the radio model, which this window doesn't have."));
        return;
    }

    connect(m_cwEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_cwKey->setEingeschaltet(on);
        if (on) { refreshPorts(); }
    });
    connect(m_cwKeyTx, &QCheckBox::toggled, this, [this](bool on) { m_cwKey->setSenderTasten(on); });
    connect(m_cwPort, &QComboBox::activated, this, [this](int index) {
        m_cwKey->setPortWunsch(m_cwPort->itemData(index).toString());
    });
    connect(m_cwRescan, &QPushButton::clicked, this, &HaliKeyGroup::refreshPorts);
    connect(m_cwType, &QComboBox::activated, this, [this](int index) {
        m_cwKey->setArt(static_cast<CwTastung::Art>(m_cwType->itemData(index).toInt()));
    });
    connect(m_cwWpm, &QSpinBox::valueChanged, this, [this](int v) { m_cwKey->setWpm(v); });
    connect(m_cwSwap, &QCheckBox::toggled, this, [this](bool on) { m_cwKey->setVertauscht(on); });
    connect(m_cwHang, &QSpinBox::valueChanged, this, [this](int v) { m_cwKey->setNachlaufMs(v); });
    connect(m_cwSidetone, &QCheckBox::toggled, this, [this](bool on) { m_cwKey->setMithoerton(on); });
    connect(m_cwPitch, &QSpinBox::valueChanged, this, [this](int v) { m_cwKey->setTonhoeheHz(v); });
    connect(m_cwVolume, &QSlider::valueChanged, this, [this](int v) { m_cwKey->setTonLautstaerke(v); });
    const auto notesChanged = [this]() {
        MidiKeyInput::Noten n;
        n.punkt = m_cwNoteDit->value();
        n.strich = m_cwNoteDah->value();
        n.handtaste = m_cwNoteKey->value();
        n.ptt = m_cwNotePtt->value();
        m_cwKey->setNoten(n);
    };
    for (QSpinBox* s : {m_cwNoteDit, m_cwNoteDah, m_cwNoteKey, m_cwNotePtt}) {
        connect(s, &QSpinBox::valueChanged, this, notesChanged);
    }
    connect(m_cwKey, &CwTastung::zustandGeaendert, this, &HaliKeyGroup::refresh);
    refreshPorts();
}

void HaliKeyGroup::refreshPorts()
{
    if (m_cwKey == nullptr) {
        return;
    }
    const QStringList ports = m_cwKey->lesePortsNeu();
    const QString wish = m_cwKey->portWunsch();
    QSignalBlocker block(m_cwPort);
    m_cwPort->clear();
    m_cwPort->addItem(tr("Automatic (HaliKey)"), QString());
    for (const QString& p : ports) {
        m_cwPort->addItem(p, p);
    }
    // Ein gewuenschter Port, der gerade fehlt, bleibt in der Liste
    // stehen -- sonst waere die Wahl mit dem Abziehen vergessen.
    if (!wish.isEmpty() && !ports.contains(wish)) {
        m_cwPort->addItem(tr("%1 (not connected)").arg(wish), wish);
    }
    m_cwPort->setCurrentIndex(qMax(0, m_cwPort->findData(wish)));
}

void HaliKeyGroup::refresh()
{
    if (m_cwKey == nullptr) {
        return;
    }
    const auto set = [](QCheckBox* box, bool on) {
        QSignalBlocker block(box);
        box->setChecked(on);
    };
    const auto setValue = [](QSpinBox* box, int value) {
        QSignalBlocker block(box);
        box->setValue(value);
    };
    set(m_cwEnabled, m_cwKey->eingeschaltet());
    set(m_cwKeyTx, m_cwKey->senderTasten());
    set(m_cwSwap, m_cwKey->vertauscht());
    set(m_cwSidetone, m_cwKey->mithoerton());
    {
        QSignalBlocker block(m_cwType);
        m_cwType->setCurrentIndex(qMax(0, m_cwType->findData(int(m_cwKey->art()))));
    }
    setValue(m_cwWpm, m_cwKey->wpm());
    setValue(m_cwHang, m_cwKey->nachlaufMs());
    setValue(m_cwPitch, m_cwKey->tonhoeheHz());
    {
        QSignalBlocker block(m_cwVolume);
        m_cwVolume->setValue(m_cwKey->tonLautstaerke());
    }
    const MidiKeyInput::Noten n = m_cwKey->noten();
    setValue(m_cwNoteDit, n.punkt);
    setValue(m_cwNoteDah, n.strich);
    setValue(m_cwNoteKey, n.handtaste);
    setValue(m_cwNotePtt, n.ptt);

    // Der zweite Schalter hat ohne den ersten keinen Gegenstand.
    const bool on = m_cwKey->eingeschaltet();
    m_cwKeyTx->setEnabled(on);
    const CwTastung::Art art = m_cwKey->art();
    const bool paddle = art == CwTastung::Art::JambusA || art == CwTastung::Art::JambusB;
    const bool morse = art != CwTastung::Art::Fusstaster;
    m_cwWpm->setEnabled(paddle);
    m_cwSwap->setEnabled(paddle);
    m_cwHang->setEnabled(morse);
    m_cwSidetone->setEnabled(morse);
    m_cwPitch->setEnabled(morse);
    m_cwVolume->setEnabled(morse);
    m_cwStatus->setText(m_cwKey->zustandsText());
    // Bernstein, sobald die Taste den Sender erreicht -- gemessen ist
    // warm (Hausstil), und das ist der Zustand, den man sehen muss.
    m_cwStatus->setStyleSheet(on && m_cwKey->senderTasten()
        ? QStringLiteral("QLabel { color: %1; }").arg(QLatin1String(Style::kAmberText))
        : QString::fromLatin1(Style::kSecondaryLabelStyle));
}

} // namespace Longpath
