// =================================================================
// src/gui/applets/QsoRecorderApplet.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "QsoRecorderApplet.h"

#include "gui/StyleConstants.h"
#include "gui/styles/PopupMenuStyle.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QDir>
#include <QAction>
#include <QUrl>
#include <QDesktopServices>
#include <QMenu>
#include <QStorageInfo>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// mm:ss — Sekunden allein sind ab einer Minute nicht mehr lesbar.
// Punktgroesse nur anfassen, wenn es eine gibt. Kopflos laeuft Qt mit
// Pixelgroessen, dort ist pointSizeF() == -1, und -1 minus eins ist eine
// Warnung je Beschriftung — im Test hundertfach, im Betrieb still.
void nudgePointSize(QFont& f, double delta)
{
    if (f.pointSizeF() > 0.0) { f.setPointSizeF(f.pointSizeF() + delta); }
}

QString clockText(double seconds)
{
    const int total = static_cast<int>(seconds);
    return QStringLiteral("%1:%2")
        .arg(total / 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

QsoRecorderApplet::QsoRecorderApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    if (m_model) {
        connect(&m_model->qsoRecorder(),
                &QsoRecorderController::recordingChanged,
                this, [this](bool) { refreshState(); });
        connect(&m_model->qsoRecorder(),
                &QsoRecorderController::secondsChanged,
                this, [this](double) { refreshState(); });
        connect(&m_model->qsoRecorder(),
                &QsoRecorderController::samplesLost,
                this, [this]() {
            // Eine Luecke in einer Aufnahme faellt sonst erst auf, wenn
            // man sie braucht.
            if (m_lossLabel) {
                m_lossLabel->setText(QStringLiteral(
                    "⚠ samples were lost — the recording has a gap"));
                m_lossLabel->setVisible(true);
            }
        });
    }

    refreshState();
    refreshList();
}

QString QsoRecorderApplet::recordingFolder() const
{
    // Neben die Einstellungen, wie beim Sprachspeicher — ein Ort, den
    // man wiederfindet, ohne zu suchen.
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir().mkpath(base + QStringLiteral("/recordings"));
    return base + QStringLiteral("/recordings");
}

void QsoRecorderApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(6, 4, 6, 6);
    vbox->setSpacing(5);

    // ── Kopfzeile: Knopf, Uhr mit Ziel, was mitgeschrieben wird ──────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        m_recBtn = styledButton(QStringLiteral("● Record"), 82);
        m_recBtn->setToolTip(QStringLiteral(
            "Record this QSO: what you hear on the left channel, your own "
            "voice on the right. Nothing is transmitted — both taps only "
            "listen in."));
        connect(m_recBtn, &QPushButton::clicked,
                this, &QsoRecorderApplet::onRecordClicked);
        row->addWidget(m_recBtn);

        m_clock = new QLabel(QStringLiteral("00:00"), body);
        QFont f = m_clock->font();
        f.setStyleHint(QFont::Monospace);
        nudgePointSize(f, 2.0);
        f.setBold(true);
        m_clock->setFont(f);
        m_clock->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                   .arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_clock);

        // Der Deckel steht daneben, bevor er zuschlaegt.
        m_capLabel = new QLabel(
            QStringLiteral("of %1:00").arg(QsoRecorder::kMaxMinutes), body);
        m_capLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }")
            .arg(QLatin1String(Style::kTextScale)));
        row->addWidget(m_capLabel);

        row->addStretch(1);

        // Dieselben Angaben, die in die Beschreibungsdatei wandern.
        m_headInfo = new QLabel(body);
        QFont hf = m_headInfo->font();
        hf.setStyleHint(QFont::Monospace);
        nudgePointSize(hf, -1.0);
        m_headInfo->setFont(hf);
        m_headInfo->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; }")
            .arg(QLatin1String(Style::kTextScale)));
        row->addWidget(m_headInfo);

        vbox->addLayout(row);
    }

    // ── Zwei Spuren, gleich gross ────────────────────────────────────
    //
    // Der ganze Grund fuer die Aufteilung: eine gemeinsame Anzeige
    // laesst Stille nicht von „Mikrofon aus" unterscheiden.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        auto makeTrack = [&](const QString& title, const char* colour,
                             QLabel** peakOut, QWidget** barOut,
                             QWidget** fillOut) {
            auto* box = new QWidget(body);
            box->setStyleSheet(QStringLiteral(
                "QWidget { background: %1; border: 1px solid %2;"
                " border-radius: 3px; }")
                .arg(QLatin1String(Style::kInsetBg),
                     QLatin1String(Style::kBorderSubtle)));
            auto* col = new QVBoxLayout(box);
            col->setContentsMargins(6, 4, 6, 6);
            col->setSpacing(4);

            auto* head = new QHBoxLayout;
            auto* name = new QLabel(title, box);
            name->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 10px; border: none; }")
                .arg(QLatin1String(Style::kTextSecondary)));
            head->addWidget(name);
            head->addStretch(1);

            auto* peak = new QLabel(QStringLiteral("—"), box);
            QFont pf = peak->font();
            pf.setStyleHint(QFont::Monospace);
            nudgePointSize(pf, -1.0);
            peak->setFont(pf);
            peak->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; border: none; }")
                .arg(QLatin1String(colour)));
            head->addWidget(peak);
            col->addLayout(head);

            auto* bar = new QWidget(box);
            bar->setFixedHeight(7);
            bar->setStyleSheet(QStringLiteral(
                "QWidget { background: %1; border: none; border-radius: 2px; }")
                .arg(QLatin1String(Style::kAppBg)));
            auto* barLay = new QHBoxLayout(bar);
            barLay->setContentsMargins(0, 0, 0, 0);
            barLay->setSpacing(0);
            auto* fill = new QWidget(bar);
            fill->setStyleSheet(QStringLiteral(
                "QWidget { background: %1; border: none; border-radius: 2px; }")
                .arg(QLatin1String(colour)));
            fill->setFixedWidth(0);
            barLay->addWidget(fill);
            barLay->addStretch(1);
            col->addWidget(bar);

            *peakOut = peak;
            *barOut  = bar;
            *fillOut = fill;
            return box;
        };

        row->addWidget(makeTrack(QStringLiteral("OTHER STATION"),
                                 Style::kAccent,
                                 &m_rxPeak, &m_rxBar, &m_rxFill), 1);
        row->addWidget(makeTrack(QStringLiteral("YOUR VOICE"),
                                 Style::kAmberWarn,
                                 &m_txPeak, &m_txBar, &m_txFill), 1);
        vbox->addLayout(row);
    }

    m_lossLabel = new QLabel(body);
    m_lossLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(QLatin1String(Style::kAmberText)));
    m_lossLabel->setVisible(false);
    vbox->addWidget(m_lossLabel);

    // ── Was schon aufgenommen wurde ──────────────────────────────────
    //
    // Die Antwort auf „wo ist die Aufnahme von vorhin". Kein zweites
    // Fenster dafuer.
    m_list = new QListWidget(body);
    m_list->setMaximumHeight(96);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2;"
        " border-radius: 3px; color: %3; font-size: 11px; }"
        "QListWidget::item { padding: 1px 4px; }"
        "QListWidget::item:selected { background: %4; color: %5; }")
        .arg(QLatin1String(Style::kInsetBg),
             QLatin1String(Style::kBorderSubtle),
             QLatin1String(Style::kTextSecondary),
             QLatin1String(Style::kBlueBg),
             QLatin1String(Style::kBlueText)));
    // Loeschen aus der Liste. Aus der Thetis-Durchsicht
    // (DeleteRecording, clsAudioRecordPlayback.cs:776
    // [v2.10.3.15-5-g852bf0e]) — dort raeumt sie auch den Ordner mit
    // weg, wenn er leer bleibt; das lassen wir, unser Ordner ist fest.
    //
    // Mit Rueckfrage: eine geloeschte Aufnahme kommt nicht wieder, und
    // die Beschreibung geht mit.
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        QListWidgetItem* item = m_list->itemAt(pos);
        if (!item) { return; }
        const QString file = item->data(Qt::UserRole).toString();
        if (file.isEmpty()) { return; }

        QMenu menu(this);
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
        QAction* reveal = menu.addAction(QStringLiteral("Show in folder"));
        menu.addSeparator();
        QAction* del = menu.addAction(QStringLiteral("Delete recording"));

        const QAction* chosen = menu.exec(m_list->mapToGlobal(pos));
        if (chosen == reveal) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(recordingFolder()));
        } else if (chosen == del) {
            if (QMessageBox::question(this, QStringLiteral("QSO Recorder"),
                    QStringLiteral("Delete this recording and its "
                                   "description? This cannot be undone."))
                != QMessageBox::Yes) {
                return;
            }
            QFile::remove(file);
            QString side = file;
            if (side.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
                side.chop(4);
                QFile::remove(side + QStringLiteral(".json"));
            }
            refreshList();
        }
    });

    m_list->setToolTip(QStringLiteral(
        "Recordings on disk. Left channel is the other station, right "
        "channel is your own voice — any audio editor can split them."));
    vbox->addWidget(m_list);

    vbox->addStretch();
    root->addWidget(body);
}

void QsoRecorderApplet::onRecordClicked()
{
    if (!m_model) { return; }
    QsoRecorderController& rec = m_model->qsoRecorder();

    if (rec.isRecording()) {
        rec.stop();

        const QString stamp = rec.recorder().info().utcStart
                                  .toString(QStringLiteral("yyyyMMdd-HHmmss"));
        const QString call = rec.recorder().info().callsign;
        const QString name = call.isEmpty()
            ? QStringLiteral("qso-%1.wav").arg(stamp)
            : QStringLiteral("qso-%1-%2.wav").arg(stamp, call);
        const QString path = recordingFolder() + QLatin1Char('/') + name;

        // Eine leere Aufnahme ist kein Fehler, sondern ein Versehen —
        // jemand hat zweimal geklickt. Ein modaler Kasten dafuer ist
        // zu viel; die Zeile darunter sagt es und geht wieder weg.
        if (rec.recorder().rxFrames() == 0
            && rec.recorder().txFrames() == 0) {
            if (m_lossLabel) {
                m_lossLabel->setText(QStringLiteral(
                    "nothing was recorded — no audio arrived"));
                m_lossLabel->setVisible(true);
            }
        } else {
            QString err;
            if (!rec.recorder().save(path, &err)) {
                // Hier SCHON ein Kasten: es lag etwas vor und ist nicht
                // auf die Platte gekommen. Das darf man nicht uebersehen.
                QMessageBox::warning(this, QStringLiteral("QSO Recorder"),
                    QStringLiteral("Nothing was written: %1").arg(err));
            }
        }
        refreshList();
    } else {
        // ── Passt das ueberhaupt noch? ───────────────────────────
        //
        // Aus der Thetis-Durchsicht vom 2026-08-19: Thetis prueft mit
        // OkToRecord() vor dem Start und noch einmal WAEHREND der
        // Aufnahme auf einem Zeitgeber, und stoppt sich selbst, wenn
        // der Platz knapp wird (clsAudioRecordPlayback.cs:633 + :662
        // [v2.10.3.15-5-g852bf0e]).
        //
        // Bei uns reicht die Pruefung VOR dem Start: wir sammeln im
        // Arbeitsspeicher und schreiben erst am Ende. Der laufende
        // Zeitgeber waere hier eine Antwort auf eine Frage, die sich
        // nicht stellt.
        //
        // Gerechnet wird gegen die VOLLE halbe Stunde, nicht gegen den
        // Augenblick. Eine Aufnahme, die nach 25 Minuten am Platz
        // scheitert, ist schlimmer als eine, die gar nicht erst
        // anfaengt.
        {
            const QStorageInfo disk(recordingFolder());
            const qint64 needed = static_cast<qint64>(QsoRecorder::kMaxMinutes)
                                * 60 * 48000 * 4;   // Stereo, 16 Bit
            if (disk.isValid() && disk.bytesAvailable() > 0
                && disk.bytesAvailable() < needed) {
                QMessageBox::warning(this, QStringLiteral("QSO Recorder"),
                    QStringLiteral(
                        "Not enough room for a full recording: %1 MB free, "
                        "%2 MB needed for %3 minutes.")
                        .arg(disk.bytesAvailable() / (1024 * 1024))
                        .arg(needed / (1024 * 1024))
                        .arg(QsoRecorder::kMaxMinutes));
                return;
            }
        }

        if (m_lossLabel) { m_lossLabel->setVisible(false); }

        QsoRecordingInfo info;
        info.utcStart = QDateTime::currentDateTimeUtc();

        // Was gespeichert wird, kommt aus dem Modell — und steht
        // gleichzeitig im Kopf. Was man sieht, ist was in der
        // Beschreibungsdatei landet.
        // Die aktive Scheibe, nicht die erste: aufgenommen wird, was
        // man gerade hoert.
        SliceModel* s = m_model->activeSlice();
        if (!s) { s = m_model->sliceById(0); }
        if (s) {
            info.frequency = QString::number(s->frequency() / 1.0e6, 'f', 6);
            info.mode      = SliceModel::modeName(s->dspMode());
        }
        rec.setSliceId(s ? s->sliceIndex() : 0);
        rec.start(info);
    }
    refreshState();
}

void QsoRecorderApplet::refreshState()
{
    if (!m_model) { return; }
    const QsoRecorderController& rec = m_model->qsoRecorder();
    const bool on = rec.isRecording();

    if (m_recBtn) {
        m_recBtn->setText(on ? QStringLiteral("■ Stop")
                             : QStringLiteral("● Record"));
        // Rot NUR beim Aufnehmen. Ein dauerhaft roter Knopf ist keine
        // Auskunft, sondern Farbe.
        m_recBtn->setStyleSheet(on
            ? QStringLiteral(
                "QPushButton { background: %1; border: 1px solid %2;"
                " border-radius: 6px; color: %3; font-size: 11px;"
                " padding: 3px 8px; }")
                .arg(QLatin1String(Style::kRedBg),
                     QLatin1String(Style::kRedBorder),
                     QLatin1String(Style::kRedText))
            : QString());
    }

    if (m_clock) { m_clock->setText(clockText(rec.recorder().recordedSeconds())); }

    if (m_headInfo) {
        const QsoRecordingInfo& i = rec.recorder().info();
        m_headInfo->setText(i.frequency.isEmpty()
            ? QStringLiteral("no radio")
            : QStringLiteral("%1 · %2").arg(i.frequency, i.mode));
    }

    // Die Balken. Breite in Anteilen der Spurbreite, damit sie mit dem
    // Fenster mitgehen.
    auto setFill = [](QWidget* bar, QWidget* fill, float peak) {
        if (!bar || !fill) { return; }
        const int w = qMax(0, bar->width());
        fill->setFixedWidth(static_cast<int>(w * qBound(0.0f, peak, 1.0f)));
    };
    setFill(m_rxBar, m_rxFill, on ? rec.lastRxPeak() : 0.0f);
    setFill(m_txBar, m_txFill, on ? rec.lastTxPeak() : 0.0f);

    auto setPeakText = [on](QLabel* l, float peak) {
        if (!l) { return; }
        if (!on || peak <= 0.0f) { l->setText(QStringLiteral("—")); return; }
        const double db = 20.0 * std::log10(static_cast<double>(peak));
        l->setText(QStringLiteral("%1 dB").arg(db, 0, 'f', 0));
    };
    setPeakText(m_rxPeak, rec.lastRxPeak());
    setPeakText(m_txPeak, rec.lastTxPeak());
}

void QsoRecorderApplet::refreshList()
{
    if (!m_list) { return; }
    m_list->clear();

    QDir dir(recordingFolder());
    const auto files = dir.entryInfoList(QStringList{QStringLiteral("*.wav")},
                                         QDir::Files, QDir::Time);
    for (const QFileInfo& f : files) {
        // Die Beschreibung neben der Datei, wenn es sie gibt. Ein
        // Dateiname ist in einem halben Jahr keine Auskunft; „DL1ABC ·
        // 14.205.000 · LSB · 6:41" ist eine.
        //
        // Aus der Thetis-Durchsicht (GetJSONDetailsFromFile,
        // clsAudioRecordPlayback.cs:1102 [v2.10.3.15-5-g852bf0e]).
        const QsoRecordingInfo info = readQsoDescription(f.absoluteFilePath());

        QStringList parts;
        parts << (info.utcStart.isValid()
                      ? info.utcStart.toLocalTime()
                            .toString(QStringLiteral("dd.MM. HH:mm"))
                      : f.lastModified().toString(
                            QStringLiteral("dd.MM. HH:mm")));

        if (!info.callsign.isEmpty())  { parts << info.callsign; }
        if (!info.frequency.isEmpty()) { parts << info.frequency; }
        if (!info.mode.isEmpty())      { parts << info.mode; }
        if (info.seconds > 0.0)        { parts << clockText(info.seconds); }

        // Ohne Beschreibung bleibt der Dateiname — besser als eine
        // Zeile, die nur ein Datum traegt.
        if (parts.size() == 1) { parts << f.completeBaseName(); }

        auto* item = new QListWidgetItem(parts.join(QStringLiteral("  ·  ")));
        item->setData(Qt::UserRole, f.absoluteFilePath());
        item->setToolTip(f.absoluteFilePath());
        m_list->addItem(item);
    }
    if (files.isEmpty()) {
        m_list->addItem(QStringLiteral("no recordings yet"));
    }
}

void QsoRecorderApplet::syncFromModel()
{
    refreshState();
}

} // namespace NereusSDR
