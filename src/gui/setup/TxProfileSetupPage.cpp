// =================================================================
// src/gui/setup/TxProfileSetupPage.cpp  (Longpath)
// =================================================================
//
// Longpath-original Setup → Audio → TX Profile editor page.
// See TxProfileSetupPage.h for the full header + Thetis-semantics cites.
//
// Phase 3M-1c chunk J.3 + J.4.
//
// Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: Longpath-original file; Thetis-derived semantics are
// cited inline in the header.

#include "TxProfileSetupPage.h"

#include "core/MicProfileManager.h"
#include "core/ThetisProfilImport.h"
#include "core/ProfilHerkunft.h"
#include "core/ZeusProfilImport.h"
#include "core/ZeusAbfrage.h"
#include "models/RadioModel.h"
#include "gui/StyleConstants.h"
#include "gui/ScopedChildWidget.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include "models/TransmitModel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Longpath {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TxProfileSetupPage::TxProfileSetupPage(RadioModel* model,
                                         MicProfileManager* profileMgr,
                                         TransmitModel* tx,
                                         QWidget* parent)
    : SetupPage(QStringLiteral("TX Profile"), model, parent)
    , m_profileMgr(profileMgr)
    , m_tx(tx)
{
    buildUi();
    rebuildCombo();
    wireDirtyTracking();

    if (m_profileMgr) {
        // Refresh on external add/remove (e.g. CAT-imported profile).
        connect(m_profileMgr, &MicProfileManager::profileListChanged,
                this, &TxProfileSetupPage::rebuildCombo);
        // Refresh selection on external active-profile change.
        connect(m_profileMgr, &MicProfileManager::activeProfileChanged,
                this, [this](const QString& name) {
            if (!m_combo) { return; }
            QSignalBlocker blk(m_combo);
            m_seedingFromModel = true;
            const int idx = m_combo->findText(name);
            if (idx >= 0) { m_combo->setCurrentIndex(idx); }
            m_lastComboText = name;
            m_seedingFromModel = false;
            clearDirty();
        });
    }
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void TxProfileSetupPage::buildUi()
{
    QGroupBox* group = addSection(QStringLiteral("TX Profile"));

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);

    // Profile combo (editable so user could in principle type a new name —
    // but the canonical "Save" path is via the Save button).
    m_combo = new QComboBox(group);
    m_combo->setEditable(false);  // restrict edits to Save button path
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_combo->setToolTip(QStringLiteral(
        "Active TX Profile.  Switching prompts to save unsaved changes."));
    form->addRow(QStringLiteral("Active profile:"), m_combo);

    // Save / Delete buttons row.
    auto* btnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton(QStringLiteral("Save..."), group);
    m_saveBtn->setToolTip(QStringLiteral(
        "Save the current TransmitModel state under a profile name."));
    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), group);
    m_deleteBtn->setToolTip(QStringLiteral(
        "Delete the currently-selected profile."));
    // Thetis-Profile einlesen (2026-10-13). Zeus-Punkt aus 2.0.50:
    // "Use the Import button on the TX Audio Profile bar to load either
    // a Thetis XML profile export or a full Thetis database backup."
    //
    // Hier und nicht im TxApplet: an dieser Stelle liegen Auswahl,
    // Speichern und Loeschen schon beieinander, und ein Einlesen ist
    // dasselbe Geschaeft. Im Applet waere es eine vierte Taste neben
    // einem Auswahlfeld in einem Streifen, der ohnehin eng ist.
    m_importBtn = new QPushButton(tr("Import Thetis profiles…"),
                                  group);
    m_importBtn->setToolTip(tr("Import a Thetis profile export or "
                                           "a whole Thetis database. Nothing "
                                           "is applied and nothing is "
                                           "overwritten."));

    // ── Von Zeus holen (2026-10-31) ─────────────────────────────────
    //
    // Eine eigene Taste und nicht derselbe Dateidialog, weil es bei
    // Zeus gar keine Datei gibt: seine Profile stehen nur bei der
    // laufenden Maschine, abzuholen ueber deren oertliche Schnittstelle.
    // Die Begruendung in voller Laenge im Kopf von ZeusProfilImport.h.
    m_zeusBtn = new QPushButton(tr("Fetch from Zeus…"), group);
    m_zeusBtn->setToolTip(tr("Fetches the TX audio profiles from a Zeus "
                             "station engine running on this computer. "
                             "Zeus has no profile file — this asks the "
                             "running machine. Nothing is applied and "
                             "nothing is overwritten."));

    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_importBtn);
    btnRow->addWidget(m_zeusBtn);
    btnRow->addStretch(1);
    form->addRow(QString(), btnRow);

    if (auto* vlay = qobject_cast<QVBoxLayout*>(group->layout())) {
        vlay->addLayout(form);
    }

    connect(m_combo, &QComboBox::currentTextChanged,
            this, &TxProfileSetupPage::onComboTextChanged);
    connect(m_saveBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onSaveClicked);
    connect(m_deleteBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onDeleteClicked);
    connect(m_importBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onImportClicked);
    connect(m_zeusBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onZeusClicked);

    // ── Plan 4 Cluster C (Task 5 / D6): TX Filter group ──────────────────────
    // Low/High cutoff spinboxes for the TX bandpass filter.  Bidirectional with
    // TransmitModel::filterLow / filterHigh via filterChanged(int,int).
    // Cross-surface sync with TxApplet spinboxes happens automatically through
    // the shared TransmitModel::filterChanged signal — no extra wiring needed.
    if (m_tx) {
        QGroupBox* filterGroup = addSection(QStringLiteral("TX Filter"));
        auto* filterForm = new QFormLayout();
        filterForm->setContentsMargins(0, 0, 0, 0);

        auto* lowSpin = new QSpinBox(filterGroup);
        lowSpin->setRange(0, 5000);
        lowSpin->setSuffix(QStringLiteral(" Hz"));
        lowSpin->setStyleSheet(Style::spinBoxStyle());
        lowSpin->setToolTip(QStringLiteral(
            "TX bandpass filter lower cutoff (Hz).  0 Hz for voice SSB modes."));
        lowSpin->setValue(m_tx->filterLow());
        filterForm->addRow(QStringLiteral("Low cutoff:"), lowSpin);

        auto* highSpin = new QSpinBox(filterGroup);
        highSpin->setRange(200, 10000);
        highSpin->setSuffix(QStringLiteral(" Hz"));
        highSpin->setStyleSheet(Style::spinBoxStyle());
        highSpin->setToolTip(QStringLiteral(
            "TX bandpass filter upper cutoff (Hz).  2900 Hz for typical SSB voice."));
        highSpin->setValue(m_tx->filterHigh());
        filterForm->addRow(QStringLiteral("High cutoff:"), highSpin);

        if (auto* vlay = qobject_cast<QVBoxLayout*>(filterGroup->layout())) {
            vlay->addLayout(filterForm);
        }

        // UI → Model
        connect(lowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_tx, &TransmitModel::setFilterLow);
        connect(highSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_tx, &TransmitModel::setFilterHigh);

        // Model → UI (filterChanged carries both values)
        connect(m_tx, &TransmitModel::filterChanged,
                this, [lowSpin, highSpin](int low, int high) {
            {
                QSignalBlocker bLo(lowSpin);
                lowSpin->setValue(low);
            }
            {
                QSignalBlocker bHi(highSpin);
                highSpin->setValue(high);
            }
        });
    }
}

// ---------------------------------------------------------------------------
// rebuildCombo — populate from MicProfileManager.  Programmatic update;
// no prompt firing.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::rebuildCombo()
{
    if (!m_combo) { return; }

    QSignalBlocker blk(m_combo);
    m_seedingFromModel = true;

    m_combo->clear();
    if (m_profileMgr) {
        const QStringList names = m_profileMgr->profileNames();
        m_combo->addItems(names);
        const int idx = m_combo->findText(m_profileMgr->activeProfileName());
        if (idx >= 0) {
            m_combo->setCurrentIndex(idx);
            m_lastComboText = m_combo->currentText();
        } else if (!names.isEmpty()) {
            m_combo->setCurrentIndex(0);
            m_lastComboText = names.first();
        }
    }

    m_seedingFromModel = false;
    clearDirty();
}

// ---------------------------------------------------------------------------
// J.4 dirty tracking
// ---------------------------------------------------------------------------
//
// We connect to TransmitModel's *Changed signals.  Any emission while
// m_seedingFromModel == false marks the page dirty.  The dirty state is
// cleared on save / load / activeProfileChanged.
//
// The signal set covers the 23 live keys MicProfileManager captures
// (mic / VOX / MON / two-tone), plus a few non-captured but UI-relevant
// signals.  False positives (e.g. tunePowerByBandChanged) are acceptable
// in a v1 editor — the only consequence is an extra prompt.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::wireDirtyTracking()
{
    if (!m_tx) { return; }

    auto markDirty = [this]() {
        if (!m_seedingFromModel) {
            m_dirty = true;
        }
    };

    // Mic / VOX / MON.
    connect(m_tx, &TransmitModel::micGainDbChanged,         this, markDirty);
    connect(m_tx, &TransmitModel::micBoostChanged,          this, markDirty);
    connect(m_tx, &TransmitModel::micXlrChanged,            this, markDirty);
    connect(m_tx, &TransmitModel::lineInChanged,            this, markDirty);
    connect(m_tx, &TransmitModel::lineInBoostChanged,       this, markDirty);
    connect(m_tx, &TransmitModel::micTipRingChanged,        this, markDirty);
    connect(m_tx, &TransmitModel::micBiasChanged,           this, markDirty);
    connect(m_tx, &TransmitModel::micPttDisabledChanged,    this, markDirty);
    connect(m_tx, &TransmitModel::voxThresholdDbChanged,    this, markDirty);
    connect(m_tx, &TransmitModel::voxGainScalarChanged,     this, markDirty);
    connect(m_tx, &TransmitModel::voxHangTimeMsChanged,     this, markDirty);
    connect(m_tx, &TransmitModel::antiVoxGainDbChanged,     this, markDirty);
    // 3M-3a-iv post-bench refactor (Option A): antiVoxSourceVaxChanged dirty
    // wire dropped alongside the antiVoxSourceVax property.
    connect(m_tx, &TransmitModel::monitorVolumeChanged,     this, markDirty);
    connect(m_tx, &TransmitModel::micSourceChanged,         this, markDirty);

    // Two-tone (7 properties + 1 enum).
    connect(m_tx, &TransmitModel::twoToneFreq1Changed,           this, markDirty);
    connect(m_tx, &TransmitModel::twoToneFreq2Changed,           this, markDirty);
    connect(m_tx, &TransmitModel::twoToneLevelChanged,           this, markDirty);
    connect(m_tx, &TransmitModel::twoTonePowerChanged,           this, markDirty);
    connect(m_tx, &TransmitModel::twoToneFreq2DelayChanged,      this, markDirty);
    connect(m_tx, &TransmitModel::twoToneInvertChanged,          this, markDirty);
    connect(m_tx, &TransmitModel::twoTonePulsedChanged,          this, markDirty);
    connect(m_tx, &TransmitModel::twoToneDrivePowerSourceChanged, this, markDirty);
}

void TxProfileSetupPage::clearDirty()
{
    m_dirty = false;
}

// ---------------------------------------------------------------------------
// applyProfileByName — load profile values into TransmitModel.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::applyProfileByName(const QString& name)
{
    if (!m_profileMgr || !m_tx) { return; }
    m_seedingFromModel = true;
    m_profileMgr->setActiveProfile(name, m_tx);
    m_seedingFromModel = false;
    clearDirty();
}

// ---------------------------------------------------------------------------
// J.4 — handleUnsavedPrompt: returns true if the switch should proceed.
// Yes → save, No → discard, Cancel → revert.
// ---------------------------------------------------------------------------
bool TxProfileSetupPage::handleUnsavedPrompt(const QString& /*newName*/)
{
    if (!m_dirty) { return true; }

    UnsavedPromptResult result = UnsavedPromptResult::Cancel;
    const QString currentName = m_profileMgr
        ? m_profileMgr->activeProfileName()
        : QString();

    if (m_unsavedPromptHook) {
        result = m_unsavedPromptHook(currentName);
    } else {
        const auto answer = QMessageBox::question(
            this,
            tr("Unsaved Profile Changes"),
            tr("The current profile (\"%1\") has been modified.\n"
               "Save before switching?").arg(currentName),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (answer) {
            case QMessageBox::Yes:    result = UnsavedPromptResult::Yes;    break;
            case QMessageBox::No:     result = UnsavedPromptResult::No;     break;
            default:                  result = UnsavedPromptResult::Cancel; break;
        }
    }

    switch (result) {
        case UnsavedPromptResult::Yes:
            if (m_profileMgr && m_tx && !currentName.isEmpty()) {
                m_profileMgr->saveProfile(currentName, m_tx);
            }
            return true;
        case UnsavedPromptResult::No:
            return true;
        case UnsavedPromptResult::Cancel:
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Combo selection change handler.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::onComboTextChanged(const QString& newName)
{
    if (m_seedingFromModel) { return; }
    if (newName.isEmpty()) { return; }

    // Focus-gated: only run the unsaved-prompt path on user-driven changes.
    const bool userDriven = m_simulateUserChangeForTest
        || (m_combo && m_combo->hasFocus());

    if (!userDriven) {
        // Programmatic change: load unconditionally, no prompt.
        applyProfileByName(newName);
        m_lastComboText = newName;
        return;
    }

    // User-driven change.  Run the J.4 unsaved-changes prompt flow.
    if (!handleUnsavedPrompt(newName)) {
        // Cancel: revert combo to the previous selection.
        QSignalBlocker blk(m_combo);
        m_seedingFromModel = true;
        const int idx = m_combo->findText(m_lastComboText);
        if (idx >= 0) {
            m_combo->setCurrentIndex(idx);
        }
        m_seedingFromModel = false;
        return;
    }

    // Yes / No path: load the new profile.
    applyProfileByName(newName);
    m_lastComboText = newName;
}

// ---------------------------------------------------------------------------
// Save button handler.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::onSaveClicked()
{
    if (!m_profileMgr || !m_tx) { return; }

    const QString currentName = m_profileMgr->activeProfileName();

    SavePromptResult prompt;
    if (m_savePromptHook) {
        prompt = m_savePromptHook(currentName);
    } else {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this,
            tr("Save TX Profile"),
            tr("New profile name:"),
            QLineEdit::Normal,
            currentName,
            &ok);
        prompt = SavePromptResult{ok, name};
    }

    if (!prompt.accepted) { return; }
    const QString trimmed = prompt.name.trimmed();
    if (trimmed.isEmpty()) {
        // Empty / whitespace-only name → no-op (validation).
        return;
    }

    // If the name already exists, ask for overwrite confirmation.
    const QStringList existing = m_profileMgr->profileNames();
    if (existing.contains(trimmed)) {
        bool overwrite = false;
        if (m_overwriteConfirmHook) {
            overwrite = m_overwriteConfirmHook();
        } else {
            const auto answer = QMessageBox::question(
                this,
                tr("Overwrite TX Profile"),
                tr("A profile named \"%1\" already exists.  Overwrite?").arg(trimmed),
                QMessageBox::Yes | QMessageBox::No);
            overwrite = (answer == QMessageBox::Yes);
        }
        if (!overwrite) { return; }
    }

    m_profileMgr->saveProfile(trimmed, m_tx);
    clearDirty();
}

// ---------------------------------------------------------------------------
// Delete button handler.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::onDeleteClicked()
{
    if (!m_profileMgr || !m_combo) { return; }

    const QString target = m_combo->currentText();
    if (target.isEmpty()) { return; }

    // Confirm.
    bool confirmed = false;
    if (m_deleteConfirmHook) {
        confirmed = m_deleteConfirmHook();
    } else {
        const auto answer = QMessageBox::question(
            this,
            tr("Delete TX Profile"),
            tr("Delete profile \"%1\"?").arg(target),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }

    if (!confirmed) { return; }

    const bool ok = m_profileMgr->deleteProfile(target);
    if (!ok) {
        // MicProfileManager refused (last-remaining-profile guard from F.3).
        // Surface the verbatim Thetis string per chunk-J spec.
        const QString msg = QStringLiteral(
            "It is not possible to delete the last remaining TX profile");
        if (m_rejectionMessageHook) {
            m_rejectionMessageHook(msg);
        } else {
            QMessageBox::information(this, tr("Cannot Delete Profile"), msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Test seam: drive a "user-driven" combo change without focus events.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::simulateUserComboChangeForTest(const QString& newName)
{
    if (!m_combo) { return; }
    m_simulateUserChangeForTest = true;
    m_combo->setCurrentText(newName);
    m_simulateUserChangeForTest = false;
}

// ---------------------------------------------------------------------------
// Test hook setters.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::setSavePromptHook(SavePromptHook hook)
{
    m_savePromptHook = std::move(hook);
}

void TxProfileSetupPage::setOverwriteConfirmHook(OverwriteConfirmHook hook)
{
    m_overwriteConfirmHook = std::move(hook);
}

void TxProfileSetupPage::setDeleteConfirmHook(DeleteConfirmHook hook)
{
    m_deleteConfirmHook = std::move(hook);
}

void TxProfileSetupPage::setRejectionMessageHook(RejectionMessageHook hook)
{
    m_rejectionMessageHook = std::move(hook);
}

void TxProfileSetupPage::setUnsavedPromptHook(UnsavedPromptHook hook)
{
    m_unsavedPromptHook = std::move(hook);
}


// ── Thetis-Profile einlesen (2026-10-13) ───────────────────────────────
//
// Dritter Gang. Der LESER steht in src/core/ThetisProfilImport.{h,cpp}
// (kennt AppSettings nicht), das ANLEGEN in
// MicProfileManager::einleseThetisProfile. Hier ist nur der Weg vom
// Dateidialog zum Bericht.
//
// Geteilt in zwei Methoden, und das ist Absicht: `onImportClicked()`
// holt die Datei, `einleseThetisDateiFuerTest()` tut die Arbeit. Ein
// Prueffall kann die zweite fahren, ohne einen Dateidialog aufzumachen
// -- ein modaler Dialog in einem Prueflauf haengt, bis die Zeit ablaeuft
// (auf diesem Baum schon erlebt).
void TxProfileSetupPage::onImportClicked()
{
    const QString pfad = QFileDialog::getOpenFileName(
        this,
        tr("Import Thetis profiles"),
        QString(),
        // Keine Endungsfilterung auf etwas Enges: Zeus erkennt die Datei
        // selbst, "even if it has been renamed", und bei uns macht das
        // der Leser am INHALT. Ein Filter, der nur *.xml zeigt, versteckt
        // genau die umbenannte Datei, um die es dabei geht.
        tr("Thetis files (*.xml *.XML);;All files (*)"));
    if (pfad.isEmpty()) { return; }

    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(
            this, tr("Import failed"),
            tr("The file could not be opened:\n%1\n\n%2")
                .arg(pfad, f.errorString()));
        return;
    }
    const QByteArray inhalt = f.readAll();
    f.close();

    // ── Die Gegenrichtung (2026-10-31) ───────────────────────────────
    //
    // Dasselbe wie im Kanalzugfenster, nur andersherum: wer sein
    // AetherSDR-Kanalzugprofil hier hereinzieht, soll hoeren, dass
    // Longpath es lesen kann — zwei Fenster weiter. Vorher bekam er
    // „Nothing imported." und keinen Hinweis, dass die Datei in
    // Ordnung ist. Siehe ProfilHerkunft.h.
    const ProfilHerkunft::Befund b = ProfilHerkunft::erkenne(inhalt);
    // Eine abgelegte Zeus-Antwort gehoert hierher — sie fuehrt
    // dieselben Felder. Dass der uebliche Weg zu Zeus die Taste
    // daneben ist, aendert nichts daran: wer seine Antwort in eine
    // Datei gelegt hat, soll sie auch von hier einlesen koennen.
    if (b.art == ProfilHerkunft::Art::ZeusTxAudio) {
        const QString bericht = einleseZeusAntwortFuerTest(inhalt);
        ScopedChildWidget<QMessageBox> zOwner(this);
        QMessageBox& zk = *zOwner.get();
        zk.setWindowTitle(tr("Import Thetis profiles"));
        zk.setIcon(QMessageBox::Information);
        zk.setText(bericht);
        zk.exec();
        return;
    }
    if (b.art == ProfilHerkunft::Art::AetherKanalzug) {
        QMessageBox::information(
            this, tr("Import Thetis profiles"),
            tr("That is a channel-strip profile from AetherSDR. It "
               "belongs in the channel strip, not here: open the "
               "Channel Strip window and use “Profile…” in the profile "
               "row. The two are different chains — that one runs on "
               "the microphone, these set the radio's own processing."));
        return;
    }

    const QString bericht = einleseThetisDateiFuerTest(inhalt);
    // Ein Bericht und kein stilles "fertig": was eingelesen wurde, was
    // umbenannt werden musste und was liegengeblieben ist, entscheidet
    // der Betreiber -- aber nur, wenn er es sieht.
    ScopedChildWidget<QMessageBox> dlgOwner(this);   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    QMessageBox& k = *dlgOwner.get();
    k.setWindowTitle(tr("Import Thetis profiles"));
    k.setIcon(QMessageBox::Information);
    k.setText(bericht.section(QLatin1Char('\n'), 0, 0));
    k.setDetailedText(bericht);
    k.exec();
}

// ── Von Zeus holen (2026-10-31) ──────────────────────────────────────
//
// Zwei Schritte, und der erste ist eine Frage: WO. Zeus bindet seinen
// Port dynamisch, es gibt keine feste Nummer zum Raten (siehe
// ZeusAbfrage.h), also wird der Betreiber gefragt — mit dem, was
// zuletzt ging, als Vorgabe.
void TxProfileSetupPage::onZeusClicked()
{
    bool ja = false;
    const QString adresse = QInputDialog::getText(
        this, tr("Fetch from Zeus"),
        tr("Address of the Zeus station engine on this computer.\n"
           "The port is not fixed — read it off the Zeus window."),
        QLineEdit::Normal, ZeusAbfrage::basis(), &ja);
    if (!ja || adresse.trimmed().isEmpty()) { return; }

    QString grund;
    if (!ZeusAbfrage::istOertlich(adresse.trimmed(), &grund)) {
        QMessageBox::warning(this, tr("Fetch from Zeus"), grund);
        return;
    }
    ZeusAbfrage::setzeBasis(adresse.trimmed());

    // Der Holer haengt am Fenster, nicht am Stapel: die Antwort kommt
    // spaeter, und ein Holer, der vorher stirbt, meldet nie etwas.
    auto* holer = new ZeusAbfrage(this);
    m_zeusBtn->setEnabled(false);

    connect(holer, &ZeusAbfrage::gescheitert, this,
            [this, holer](const QString& g) {
                m_zeusBtn->setEnabled(true);
                holer->deleteLater();
                QMessageBox::warning(this, tr("Fetch from Zeus"), g);
            });
    connect(holer, &ZeusAbfrage::gekommen, this,
            [this, holer](const QByteArray& json) {
                m_zeusBtn->setEnabled(true);
                holer->deleteLater();

                const QString bericht = einleseZeusAntwortFuerTest(json);
                ScopedChildWidget<QMessageBox> dlgOwner(this);
                QMessageBox& k = *dlgOwner.get();
                k.setWindowTitle(tr("Fetch from Zeus"));
                k.setIcon(QMessageBox::Information);
                k.setText(bericht);
                k.exec();
            });
    holer->hole();
}

QString TxProfileSetupPage::einleseZeusAntwortFuerTest(const QByteArray& json)
{
    const ZeusProfilImport::Ergebnis gelesen = ZeusProfilImport::lies(json);

    QStringList z;
    if (!gelesen.fehler.isEmpty()) {
        z << tr("Nothing imported.");
        for (const QString& f : gelesen.fehler) {
            z << QStringLiteral("  ") + f;
        }
        return z.join(QLatin1Char('\n'));
    }
    if (!m_profileMgr) {
        z << tr("There is no profile manager here -- nothing created.");
        return z.join(QLatin1Char('\n'));
    }

    // DERSELBE Schreiber wie beim Thetis-Weg. Nicht aus Sparsamkeit:
    // zwei Schreiber laufen beim naechsten Feld auseinander, und dann
    // legt dieselbe Einstellung aus zwei Quellen zwei verschiedene
    // Profile an.
    const MicProfileManager::ThetisEinleseErgebnis erg =
        m_profileMgr->einleseThetisProfile(gelesen.profile);

    z << tr("%1 profile(s) created from Zeus, nothing applied and "
            "nothing overwritten.").arg(erg.anzahl());
    z << QString();

    for (auto it = erg.angelegt.constBegin();
         it != erg.angelegt.constEnd(); ++it) {
        if (it.key() == it.value()) {
            z << tr("  created: %1").arg(it.value());
        } else {
            z << tr("  created: %1  (in Zeus: %2)")
                     .arg(it.value(), it.key());
        }
        for (const QString& h : erg.hinweise.value(it.value())) {
            z << QStringLiteral("      ") + h;
        }
    }
    for (auto it = erg.ausgelassen.constBegin();
         it != erg.ausgelassen.constEnd(); ++it) {
        z << tr("  NOT created: %1 -- %2").arg(it.key(), it.value());
    }
    if (!gelesen.hinweise.isEmpty()) {
        z << QString() << tr("About the answer:");
        for (const QString& h : gelesen.hinweise) {
            z << QStringLiteral("  ") + h;
        }
    }

    // Was Zeus fuehrt und wir nicht — einmal am Ende, nicht je Profil.
    // Ohne diesen Block sieht der Betreiber „angelegt" und weiss
    // nicht, dass Zeus' halbe Kette gar nicht dabei sein KANN.
    z << QString() << tr("Zeus carries these, Longpath does not:");
    for (const QString& h : ZeusProfilImport::nichtUebernommen()) {
        z << QStringLiteral("  ") + h;
    }
    return z.join(QLatin1Char('\n'));
}

QString TxProfileSetupPage::einleseThetisDateiFuerTest(const QByteArray& inhalt)
{
    const ThetisProfilImport::Ergebnis gelesen =
        ThetisProfilImport::lies(inhalt);

    QStringList z;
    if (!gelesen.fehler.isEmpty()) {
        z << tr("Nothing imported.");
        for (const QString& f : gelesen.fehler) { z << QStringLiteral("  ") + f; }
        return z.join(QLatin1Char('\n'));
    }

    switch (gelesen.art) {
    case ThetisProfilImport::Art::Profilausfuhr:
        z << tr("Recognised: a Thetis profile export.");
        break;
    case ThetisProfilImport::Art::Datenbanksicherung:
        z << tr("Recognised: a whole Thetis database.");
        break;
    case ThetisProfilImport::Art::Unbekannt:
        z << tr("Nothing imported.");
        return z.join(QLatin1Char('\n'));
    }

    if (!m_profileMgr) {
        z << tr("There is no profile manager here "
               "-- nothing created.");
        return z.join(QLatin1Char('\n'));
    }

    const MicProfileManager::ThetisEinleseErgebnis erg =
        m_profileMgr->einleseThetisProfile(gelesen.profile);

    z[0] = tr("%1 profile(s) created, nothing applied and "
             "nothing overwritten.").arg(erg.anzahl());
    z << QString();
    switch (gelesen.art) {
    case ThetisProfilImport::Art::Profilausfuhr:
        z << tr("File: a Thetis profile export."); break;
    case ThetisProfilImport::Art::Datenbanksicherung:
        z << tr("File: a whole Thetis database."); break;
    default: break;
    }

    for (auto it = erg.angelegt.constBegin(); it != erg.angelegt.constEnd(); ++it) {
        if (it.key() == it.value()) {
            z << tr("  created: %1").arg(it.value());
        } else {
            z << tr("  created: %1  (in the file: %2)")
                     .arg(it.value(), it.key());
        }
        for (const QString& h : erg.hinweise.value(it.value())) {
            z << QStringLiteral("      ") + h;
        }
    }
    for (auto it = erg.ausgelassen.constBegin();
         it != erg.ausgelassen.constEnd(); ++it) {
        z << tr("  NOT created: %1 -- %2")
                 .arg(it.key(), it.value());
    }
    if (!gelesen.hinweise.isEmpty()) {
        z << QString() << tr("About the file:");
        for (const QString& h : gelesen.hinweise) {
            z << QStringLiteral("  ") + h;
        }
    }
    return z.join(QLatin1Char('\n'));
}

} // namespace Longpath
