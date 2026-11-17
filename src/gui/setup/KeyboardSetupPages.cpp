// =================================================================
// src/gui/setup/KeyboardSetupPages.cpp  (Longpath)
// =================================================================
//
// Siehe den Kopf in KeyboardSetupPages.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Platzhalter durch die echte Seite ersetzt von Martin
//                 Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — Schalter „F1…F12 als F-Tasten" (macOS), KI-gestuetzt
//                 ueber Anthropic Claude.
//   2026-11-17 — Gruppe „CW key (MIDI)", KI-gestuetzt ueber Anthropic
//                 Claude.
// =================================================================

#include "KeyboardSetupPages.h"

#include "core/cw/CwTastung.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/StyleConstants.h"
#include "gui/hotkeys/KeyboardRouter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Longpath {

namespace {

constexpr int kColAction = 0;
constexpr int kColKeys = 1;
constexpr int kIdRole = Qt::UserRole + 1;

// ── Der Fang einer neuen Taste ──────────────────────────────────────
//
// Ein kleines Fenster, das JEDE Taste nimmt -- auch Tab, Esc und
// Eingabe, die ein Dialog sonst selbst verbraucht. Abbrechen geht nur
// mit dem Knopf, sonst koennte man Esc nicht belegen (Esc stoppt die
// Ansage). Solange es offen ist, ist es modal, und die Tastaturweiche
// laesst alle Tasten in Ruhe (KeyboardRouter::windowTakesKeys).
class KeyCaptureDialog : public QDialog {
public:
    KeyCaptureDialog(const QString& actionName, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Change Shortcut"));
        setModal(true);
        auto* box = new QVBoxLayout(this);
        auto* prompt = new QLabel(
            QStringLiteral("Press the new key for “%1”.").arg(actionName), this);
        prompt->setWordWrap(true);
        box->addWidget(prompt);
        auto* hint = new QLabel(
            QStringLiteral("A key with Ctrl, Shift or Alt works best. "
                           "Transmit and voice-keyer keys don't take a plain letter, digit or arrow."),
            this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        box->addWidget(hint);

        auto* row = new QHBoxLayout;
        row->addStretch();
        auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
        cancel->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
        // Ohne Fokus, sonst nimmt der Knopf Leertaste und Eingabe selbst.
        cancel->setFocusPolicy(Qt::NoFocus);
        cancel->setAutoDefault(false);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        row->addWidget(cancel);
        box->addLayout(row);

        setFocusPolicy(Qt::StrongFocus);
        setMinimumWidth(320);
    }

    QString chord() const { return m_chord; }

protected:
    bool event(QEvent* e) override
    {
        // Ein Menue-Kuerzel soll hier nicht ausloesen, sondern als Taste
        // ankommen -- abgelehnt wird es dann mit Namen.
        if (e->type() == QEvent::ShortcutOverride) {
            e->accept();
            return true;
        }
        if (e->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(e);
            const QString chord = KeyboardRouter::chordFromEvent(ke);
            if (!chord.isEmpty() && !ke->isAutoRepeat()) {
                m_chord = chord;
                accept();
            }
            return true;
        }
        return QDialog::event(e);
    }

    void showEvent(QShowEvent* e) override
    {
        QDialog::showEvent(e);
        setFocus();
    }

private:
    QString m_chord;
};

QString keysText(const QStringList& keys)
{
    QStringList shown;
    for (const QString& k : keys) {
        shown.append(KeyboardShortcutsPage::nativeKeyText(k));
    }
    return shown.join(QStringLiteral(", "));
}

} // namespace

QString KeyboardShortcutsPage::nativeKeyText(const QString& chord)
{
    if (chord.isEmpty()) {
        return {};
    }
    const QString native = QKeySequence(chord, QKeySequence::PortableText).toString(QKeySequence::NativeText);
    return native.isEmpty() ? chord : native;
}

KeyboardShortcutsPage::KeyboardShortcutsPage(HotKeyBindings* bindings, CwTastung* cwKey,
                                             QWidget* parent)
    : SetupPage(QStringLiteral("Shortcuts"), parent)
    , m_bindings(bindings)
    , m_cwKey(cwKey)
{
    buildUI();
    buildCwKeyGroup();
    refresh();
    refreshCwKey();
    if (m_bindings != nullptr) {
        connect(m_bindings, &HotKeyBindings::bindingsChanged, this, &KeyboardShortcutsPage::refresh);
        connect(m_bindings, &HotKeyBindings::enabledChanged, this, [this](bool on) {
            QSignalBlocker block(m_enabledCheck);
            m_enabledCheck->setChecked(on);
            updateButtons();
        });
        connect(m_bindings, &HotKeyBindings::fKeysFirstChanged, this, [this](bool on) {
            QSignalBlocker block(m_fKeysCheck);
            m_fKeysCheck->setChecked(on);
            m_fKeysHoldCheck->setEnabled(on);
        });
        connect(m_bindings, &HotKeyBindings::fKeysWhileRunningChanged, this, [this](bool on) {
            QSignalBlocker block(m_fKeysHoldCheck);
            m_fKeysHoldCheck->setChecked(on);
        });
    }
}

void KeyboardShortcutsPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* group = new QGroupBox(QStringLiteral("Shortcuts"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    auto* vLayout = new QVBoxLayout(group);
    vLayout->setSpacing(6);

    m_enabledCheck = new QCheckBox(QStringLiteral("Use keyboard shortcuts"), group);
    m_enabledCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_enabledCheck->setToolTip(QStringLiteral(
        "One switch for every shortcut on this page. Menu shortcuts (Ctrl+K, Ctrl+R …) stay on."));
    vLayout->addWidget(m_enabledCheck);

    // Am Mac liefern F1…F12 ab Werk Helligkeit, Lautstaerke usw.; der
    // Sprachspeicher kaeme nur mit fn (Betreiber 2026-11-11). Siehe
    // FKeyPriority.
    m_fKeysCheck = new QCheckBox(
        QStringLiteral("F1…F12 are plain F keys while Longpath is in front"), group);
    m_fKeysCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_fKeysCheck->setToolTip(QStringLiteral(
        "Brightness, volume and the other Mac functions on these keys then need fn. "
        "As soon as another program is in front, or Longpath quits, the keys work as before."));
    vLayout->addWidget(m_fKeysCheck);

    // Betreiber 2026-11-14 („Ja, durchgehend"): nur-solange-vorne hat
    // den Mac zweimal in drei Tagen stumm gemacht -- F10 ist am Mac die
    // Stummtaste, und zwischen Fokuswechsel und Tastendruck stand der
    // Modus jedes Mal kurz auf der Mac-Belegung. Siehe FKeyPriority.
    m_fKeysHoldCheck = new QCheckBox(
        QStringLiteral("… and keep them that way while Longpath runs, not only in front"), group);
    m_fKeysHoldCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_fKeysHoldCheck->setToolTip(QStringLiteral(
        "Keeps F10 from muting the Mac when you reach for a voice-keyer key while another "
        "program is in front. The price: brightness and volume then need fn in EVERY program "
        "for as long as Longpath runs. Back to normal when Longpath quits."));
    vLayout->addWidget(m_fKeysHoldCheck);
#ifndef Q_OS_MACOS
    m_fKeysCheck->setVisible(false);
    m_fKeysHoldCheck->setVisible(false);
#endif

    auto* note = new QLabel(
        QStringLiteral("Keys work in the Longpath window and its panels — not while you type in a "
                       "text field, and not in a dialog. On a Mac, Ctrl is ⌘. Voice keyer: "
                       "F1…F10 sends a message, Ctrl+Shift+F1…F10 records it, Esc stops."),
        group);
    note->setWordWrap(true);
    note->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    vLayout->addWidget(note);

    m_searchEdit = new QLineEdit(group);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search shortcuts…"));
    m_searchEdit->setStyleSheet(Style::lineEditStyle());
    m_searchEdit->setClearButtonEnabled(true);
    vLayout->addWidget(m_searchEdit);

    m_table = new QTreeWidget(group);
    m_table->setColumnCount(2);
    m_table->setHeaderLabels({QStringLiteral("Action"), QStringLiteral("Key")});
    m_table->setStyleSheet(Style::tableStyle());
    m_table->header()->setFont(Style::capsFont(font(), Style::kFontCaption));
    m_table->header()->setSectionResizeMode(kColAction, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(kColKeys, QHeaderView::ResizeToContents);
    m_table->header()->setStretchLastSection(false);
    m_table->setRootIsDecorated(true);
    m_table->setUniformRowHeights(true);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setMinimumHeight(260);
    vLayout->addWidget(m_table, 1);

    // Die Zeilen: eine Gruppe je Abschnitt, darunter die Handlungen in
    // der Reihenfolge der Liste.
    QHash<QString, QTreeWidgetItem*> groups;
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        QTreeWidgetItem* parentItem = groups.value(action.group);
        if (parentItem == nullptr) {
            parentItem = new QTreeWidgetItem(m_table, QStringList{action.group});
            parentItem->setFlags(Qt::ItemIsEnabled);
            parentItem->setFirstColumnSpanned(true);
            parentItem->setExpanded(true);
            groups.insert(action.group, parentItem);
        }
        auto* item = new QTreeWidgetItem(parentItem, QStringList{action.name, QString()});
        item->setData(kColAction, kIdRole, action.id);
        if (action.transmits) {
            item->setToolTip(kColAction, QStringLiteral("Transmits."));
        }
        m_itemById.insert(action.id, item);
    }
    for (QTreeWidgetItem* g : std::as_const(groups)) {
        g->setExpanded(true);
    }

    m_status = new QLabel(group);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    vLayout->addWidget(m_status);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    const auto button = [group, btnRow](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, group);
        b->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
        b->setToolTip(tip);
        btnRow->addWidget(b);
        return b;
    };
    m_editButton = button(QStringLiteral("Change Key…"),
                          QStringLiteral("Press a new key for the selected action (or double-click the row)."));
    m_clearButton = button(QStringLiteral("Clear"), QStringLiteral("The selected action gets no key."));
    m_resetButton = button(QStringLiteral("Reset"), QStringLiteral("The selected action gets its default key back."));
    btnRow->addStretch();
    m_resetAllButton = button(QStringLiteral("Reset All"), QStringLiteral("Every action gets its default key back."));
    m_importButton = button(QStringLiteral("Import…"), QStringLiteral("Load shortcuts from a file."));
    m_exportButton = button(QStringLiteral("Export…"), QStringLiteral("Save the shortcuts to a file."));
    vLayout->addLayout(btnRow);

    contentLayout()->addWidget(group, 1);

    if (m_bindings == nullptr) {
        group->setEnabled(false);
        say(QStringLiteral("Shortcuts need the radio model, which this window doesn't have."), true);
        return;
    }

    connect(m_enabledCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_bindings->setEnabled(on);
        updateButtons();
    });
    connect(m_fKeysCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_bindings->setFKeysFirst(on);
        // Ohne „F-Tasten zuerst" hat das Durchhalten keinen Gegenstand.
        m_fKeysHoldCheck->setEnabled(on);
    });
    connect(m_fKeysHoldCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_bindings->setFKeysWhileRunning(on);
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &KeyboardShortcutsPage::applyFilter);
    connect(m_table, &QTreeWidget::itemSelectionChanged, this, &KeyboardShortcutsPage::updateButtons);
    connect(m_table, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item != nullptr && !item->data(kColAction, kIdRole).toString().isEmpty()) {
            changeSelected();
        }
    });
    connect(m_editButton, &QPushButton::clicked, this, &KeyboardShortcutsPage::changeSelected);
    connect(m_clearButton, &QPushButton::clicked, this, &KeyboardShortcutsPage::clearSelected);
    connect(m_resetButton, &QPushButton::clicked, this, &KeyboardShortcutsPage::resetSelected);
    connect(m_resetAllButton, &QPushButton::clicked, this, &KeyboardShortcutsPage::resetAll);
    connect(m_importButton, &QPushButton::clicked, this, &KeyboardShortcutsPage::importFile);
    connect(m_exportButton, &QPushButton::clicked, this, &KeyboardShortcutsPage::exportFile);
}

// ── CW key (MIDI) ───────────────────────────────────────────────────
//
// Die Morsetaste am HaliKey MIDI (src/core/cw/CwTastung). Sie steht auf
// dieser Seite, weil sie ist, was die Seite sammelt: eine Taste, die
// etwas ausloest. Zwei Schalter stehen oben und getrennt -- lesen und
// senden sind nicht dasselbe, und der zweite ist ab Werk aus.
void KeyboardShortcutsPage::buildCwKeyGroup()
{
    m_cwGroup = new QGroupBox(tr("Foot switch and CW key (HaliKey MIDI)"), this);
    m_cwGroup->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* box = new QVBoxLayout(m_cwGroup);
    box->setSpacing(6);

    const auto check = [this, box](const QString& text, const QString& tip) {
        auto* c = new QCheckBox(text, m_cwGroup);
        c->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        c->setToolTip(tip);
        box->addWidget(c);
        return c;
    };
    const auto hint = [this, box](const QString& text) {
        auto* l = new QLabel(text, m_cwGroup);
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
        grid->addWidget(new QLabel(text, m_cwGroup), r, c);
    };
    const auto spin = [this](int from, int to, const QString& suffix) {
        auto* s = new QSpinBox(m_cwGroup);
        s->setRange(from, to);
        s->setSuffix(suffix);
        s->setStyleSheet(Style::spinBoxStyle());
        s->setKeyboardTracking(false);
        return s;
    };

    label(row, 0, tr("MIDI port"));
    m_cwPort = new QComboBox(m_cwGroup);
    m_cwPort->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_cwPort->setToolTip(tr("Automatic takes the first port with “HaliKey” in its name — "
                            "never just any MIDI device."));
    grid->addWidget(m_cwPort, row, 1, 1, 2);
    m_cwRescan = new QPushButton(tr("Rescan"), m_cwGroup);
    m_cwRescan->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_cwRescan->setToolTip(tr("Read the MIDI ports again — after plugging the key in."));
    grid->addWidget(m_cwRescan, row, 3, Qt::AlignLeft);
    ++row;

    label(row, 0, tr("Key type"));
    m_cwType = new QComboBox(m_cwGroup);
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

    m_cwSwap = new QCheckBox(tr("Swap paddles"), m_cwGroup);
    m_cwSwap->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    grid->addWidget(m_cwSwap, row, 1);
    label(row, 2, tr("Hang time"));
    m_cwHang = spin(CwTastung::kNachlaufMinMs, CwTastung::kNachlaufMaxMs, tr(" ms"));
    m_cwHang->setSingleStep(50);
    m_cwHang->setToolTip(tr("How long the radio stays on transmit after the last element."));
    grid->addWidget(m_cwHang, row, 3, Qt::AlignLeft);
    ++row;

    m_cwSidetone = new QCheckBox(tr("Sidetone"), m_cwGroup);
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
    m_cwVolume = new QSlider(Qt::Horizontal, m_cwGroup);
    m_cwVolume->setRange(0, 100);
    m_cwVolume->setStyleSheet(QString::fromLatin1(Style::kSliderStyle));
    grid->addWidget(m_cwVolume, row, 1, 1, 2);
    ++row;

    label(row, 0, tr("MIDI notes"));
    auto* notes = new QHBoxLayout;
    notes->setSpacing(6);
    const auto note = [this, notes, &spin](const QString& text) {
        notes->addWidget(new QLabel(text, m_cwGroup));
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

    m_cwStatus = new QLabel(m_cwGroup);
    m_cwStatus->setWordWrap(true);
    m_cwStatus->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    box->addWidget(m_cwStatus);

    contentLayout()->insertWidget(contentLayout()->count() - 1, m_cwGroup);

    if (m_cwKey == nullptr) {
        m_cwGroup->setEnabled(false);
        m_cwStatus->setText(tr("The CW key needs the radio model, which this window doesn't have."));
        return;
    }

    connect(m_cwEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_cwKey->setEingeschaltet(on);
        if (on) { refreshCwPorts(); }
    });
    connect(m_cwKeyTx, &QCheckBox::toggled, this, [this](bool on) { m_cwKey->setSenderTasten(on); });
    connect(m_cwPort, &QComboBox::activated, this, [this](int index) {
        m_cwKey->setPortWunsch(m_cwPort->itemData(index).toString());
    });
    connect(m_cwRescan, &QPushButton::clicked, this, &KeyboardShortcutsPage::refreshCwPorts);
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
    connect(m_cwKey, &CwTastung::zustandGeaendert, this, &KeyboardShortcutsPage::refreshCwKey);
    refreshCwPorts();
}

void KeyboardShortcutsPage::refreshCwPorts()
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

void KeyboardShortcutsPage::refreshCwKey()
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

void KeyboardShortcutsPage::refresh()
{
    if (m_bindings == nullptr) {
        return;
    }
    {
        QSignalBlocker block(m_enabledCheck);
        m_enabledCheck->setChecked(m_bindings->enabled());
    }
    {
        QSignalBlocker block(m_fKeysCheck);
        m_fKeysCheck->setChecked(m_bindings->fKeysFirst());
    }
    {
        QSignalBlocker block(m_fKeysHoldCheck);
        m_fKeysHoldCheck->setChecked(m_bindings->fKeysWhileRunning());
        m_fKeysHoldCheck->setEnabled(m_bindings->fKeysFirst());
    }
    for (auto it = m_itemById.cbegin(); it != m_itemById.cend(); ++it) {
        const QStringList keys = m_bindings->keysFor(it.key());
        it.value()->setText(kColKeys, keys.isEmpty() ? QStringLiteral("—") : keysText(keys));
        it.value()->setToolTip(kColKeys, keys.join(QStringLiteral(", ")));
    }
    applyFilter(m_searchEdit->text());
    updateButtons();
}

void KeyboardShortcutsPage::applyFilter(const QString& text)
{
    const QString needle = text.trimmed();
    for (int g = 0; g < m_table->topLevelItemCount(); ++g) {
        QTreeWidgetItem* const group = m_table->topLevelItem(g);
        const bool groupMatches = !needle.isEmpty() && group->text(kColAction).contains(needle, Qt::CaseInsensitive);
        int shown = 0;
        for (int i = 0; i < group->childCount(); ++i) {
            QTreeWidgetItem* const item = group->child(i);
            const bool match = needle.isEmpty() || groupMatches
                || item->text(kColAction).contains(needle, Qt::CaseInsensitive)
                || item->text(kColKeys).contains(needle, Qt::CaseInsensitive)
                || item->toolTip(kColKeys).contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
            shown += match ? 1 : 0;
        }
        group->setHidden(shown == 0);
    }
}

QString KeyboardShortcutsPage::selectedId() const
{
    const QList<QTreeWidgetItem*> sel = m_table->selectedItems();
    return sel.isEmpty() ? QString() : sel.first()->data(kColAction, kIdRole).toString();
}

void KeyboardShortcutsPage::updateButtons()
{
    const bool have = m_bindings != nullptr;
    const bool row = have && !selectedId().isEmpty();
    m_editButton->setEnabled(row);
    m_clearButton->setEnabled(row && !m_bindings->keysFor(selectedId()).isEmpty());
    m_resetButton->setEnabled(row);
    m_resetAllButton->setEnabled(have);
    m_importButton->setEnabled(have);
    m_exportButton->setEnabled(have);
    m_fKeysCheck->setEnabled(have && m_bindings->enabled());
}

QString KeyboardShortcutsPage::shownKeysFor(const QString& id) const
{
    QTreeWidgetItem* const item = m_itemById.value(id);
    return item != nullptr ? item->text(kColKeys) : QString();
}

void KeyboardShortcutsPage::say(const QString& text, bool warning)
{
    m_status->setText(text);
    m_status->setStyleSheet(warning
        ? QStringLiteral("QLabel { color: %1; }").arg(QLatin1String(Style::kAmberText))
        : QString::fromLatin1(Style::kSecondaryLabelStyle));
}

void KeyboardShortcutsPage::changeSelected()
{
    const QString id = selectedId();
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (m_bindings == nullptr || action == nullptr) {
        return;
    }
    KeyCaptureDialog dlg(action->name, this);
    if (dlg.exec() != QDialog::Accepted || dlg.chord().isEmpty()) {
        return;
    }
    const QString chord = dlg.chord();
    HotKeyBindOutcome outcome = m_bindings->bind(id, chord, false);
    if (outcome.result == HotKeyBindResult::Conflict) {
        const HotKeyAction* const owner = HotKeyActionList::find(outcome.conflictId);
        const QString ownerName = owner != nullptr ? owner->name : outcome.conflictId;
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Key in Use"),
            QStringLiteral("%1 is already “%2”.\n\nMove it to “%3”?")
                .arg(nativeKeyText(chord), ownerName, action->name));
        if (answer != QMessageBox::Yes) {
            return;
        }
        outcome = m_bindings->bind(id, chord, true);
    }
    if (outcome.result != HotKeyBindResult::Bound) {
        say(outcome.reason, true);
        return;
    }
    say(QStringLiteral("“%1” is now %2.").arg(action->name, nativeKeyText(chord)), false);
}

void KeyboardShortcutsPage::clearSelected()
{
    const QString id = selectedId();
    if (m_bindings == nullptr || id.isEmpty()) {
        return;
    }
    m_bindings->clear(id);
}

void KeyboardShortcutsPage::resetSelected()
{
    const QString id = selectedId();
    if (m_bindings == nullptr || id.isEmpty()) {
        return;
    }
    m_bindings->resetToDefault(id);
}

void KeyboardShortcutsPage::resetAll()
{
    if (m_bindings == nullptr) {
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Reset All Shortcuts"),
        QStringLiteral("Every action gets its default key back. Keys you changed are lost.\n\nReset all?"));
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_bindings->resetAll();
    say(QStringLiteral("All shortcuts are back to their defaults."), false);
}

void KeyboardShortcutsPage::exportFile()
{
    if (m_bindings == nullptr) {
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Shortcuts"), dir + QStringLiteral("/longpath-shortcuts.txt"),
        QStringLiteral("Text files (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        say(QStringLiteral("Could not write %1: %2").arg(path, file.errorString()), true);
        return;
    }
    QTextStream out(&file);
    out << m_bindings->exportText();
    say(QStringLiteral("Shortcuts saved to %1.").arg(path), false);
}

void KeyboardShortcutsPage::importFile()
{
    if (m_bindings == nullptr) {
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Shortcuts"), dir, QStringLiteral("Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        say(QStringLiteral("Could not read %1: %2").arg(path, file.errorString()), true);
        return;
    }
    const QStringList skipped = m_bindings->importText(QString::fromUtf8(file.readAll()));
    if (skipped.isEmpty()) {
        say(QStringLiteral("Shortcuts loaded from %1.").arg(path), false);
        return;
    }
    say(QStringLiteral("Loaded, but %1 line(s) were skipped.").arg(skipped.size()), true);
    QMessageBox::information(this, QStringLiteral("Import Shortcuts"),
                             QStringLiteral("These lines were skipped:\n\n%1").arg(skipped.join(QLatin1Char('\n'))));
}

} // namespace Longpath
