// no-port-check: Longpath-original, no Thetis logic.
//
// src/gui/SotaNachtragDialog.cpp  (Longpath) -- siehe .h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "gui/SotaNachtragDialog.h"

#include "core/SotaGipfel.h"
#include "gui/StyleConstants.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Longpath {

namespace {
enum Spalte { kHaken, kZeit, kRuf, kBand, kMode, kGipfel, kLocator, kS2s, kHinweis, kSpalten };
}

SotaNachtragDialog::SotaNachtragDialog(const QVector<LogEntry>& log, QWidget* parent)
    : QDialog(parent)
    , m_log(log)
{
    setWindowTitle(QStringLiteral("Summit references from SOTAdata"));
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QLatin1String(Style::kAppBg)));
    resize(860, 520);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(12, 12, 12, 12);
    col->setSpacing(8);

    m_erklaerung = new QLabel(QStringLiteral(
        "Fills in MY_SOTA_REF (and SOTA_REF for summit-to-summit contacts) from your "
        "SOTA activator log, and sets your grid (MY_GRIDSQUARE) to the summit's locator, "
        "so bearings, distances and path profiles start at the summit.\n"
        "On sotadata.org.uk: Logs → Activator → Download complete log (CSV). Contacts "
        "are matched by UTC date. Nothing is written until you press Apply, and the "
        "log is backed up first."), this);
    m_erklaerung->setWordWrap(true);
    m_erklaerung->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                    .arg(QLatin1String(Style::kTextSecondary)));
    col->addWidget(m_erklaerung);

    auto* waehlen = new QPushButton(QStringLiteral("Choose CSV file…"), this);
    waehlen->setStyleSheet(Style::buttonBaseStyle());
    connect(waehlen, &QPushButton::clicked, this, &SotaNachtragDialog::waehleDatei);
    auto* oben = new QHBoxLayout;
    oben->addWidget(waehlen);
    m_summe = new QLabel(this);
    m_summe->setWordWrap(true);
    m_summe->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                               .arg(QLatin1String(Style::kTextPrimary)));
    oben->addWidget(m_summe, 1);
    col->addLayout(oben);

    m_tabelle = new QTableWidget(0, kSpalten, this);
    m_tabelle->setHorizontalHeaderLabels({QString(), QStringLiteral("UTC"),
                                          QStringLiteral("Call"), QStringLiteral("Band"),
                                          QStringLiteral("Mode"), QStringLiteral("Summit"),
                                          QStringLiteral("Your grid"),
                                          QStringLiteral("S2S"), QStringLiteral("Note")});
    m_tabelle->verticalHeader()->setVisible(false);
    m_tabelle->setSelectionMode(QAbstractItemView::NoSelection);
    m_tabelle->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabelle->horizontalHeader()->setStretchLastSection(true);
    connect(m_tabelle, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) {
        if (it && it->column() == kHaken) { zaehle(); }
    });
    col->addWidget(m_tabelle, 1);

    auto* unten = new QHBoxLayout;
    unten->addStretch(1);
    auto* abbrechen = new QPushButton(QStringLiteral("Cancel"), this);
    m_anwenden = new QPushButton(QStringLiteral("Apply"), this);
    for (QPushButton* b : {abbrechen, m_anwenden}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        unten->addWidget(b);
    }
    m_anwenden->setEnabled(false);
    m_anwenden->setDefault(false);
    m_anwenden->setAutoDefault(false);
    abbrechen->setAutoDefault(false);
    connect(abbrechen, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_anwenden, &QPushButton::clicked, this, &QDialog::accept);
    col->addLayout(unten);

    m_summe->setText(QStringLiteral("No file chosen yet."));

    SotaGipfelVerzeichnis& v = SotaGipfelVerzeichnis::instanz();
    connect(&v, &SotaGipfelVerzeichnis::gipfelBekannt, this, &SotaNachtragDialog::zeigeLocatoren);
    connect(&v, &SotaGipfelVerzeichnis::gipfelFehler, this, &SotaNachtragDialog::zeigeLocatoren);
}

void SotaNachtragDialog::waehleDatei()
{
    const QString pfad = QFileDialog::getOpenFileName(
        this, QStringLiteral("SOTA activator log"), QString{},
        QStringLiteral("CSV (*.csv *.txt);;All files (*)"));
    if (!pfad.isEmpty()) { ladeCsv(pfad); }
}

bool SotaNachtragDialog::ladeCsv(const QString& pfad)
{
    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) {
        m_erg = {};
        zeige();
        m_summe->setText(QStringLiteral("Couldn't read %1: %2")
                             .arg(QFileInfo(pfad).fileName(), f.errorString()));
        return false;
    }
    return ladeCsvDaten(f.readAll(), QFileInfo(pfad).fileName());
}

bool SotaNachtragDialog::ladeCsvDaten(const QByteArray& csv, const QString& name)
{
    QString fehler;
    const SotaNachtrag::Datei d = SotaNachtrag::leseCsv(csv, &fehler);
    m_erg = SotaNachtrag::vorschlaege(m_log, d);
    // Die Gipfel nachschlagen (je Gipfel einmal) -- ihr Locator kommt in
    // MY_GRIDSQUARE.
    for (const SotaNachtrag::Vorschlag& v : m_erg.vorschlaege) {
        SotaGipfelVerzeichnis::instanz().anfragen(v.gipfel);
        SotaGipfelVerzeichnis::instanz().anfragen(v.gegenGipfel);
    }
    zeige();
    if (d.zeilen.isEmpty()) {
        m_summe->setText(QStringLiteral("%1: %2").arg(name, fehler));
        return false;
    }
    return true;
}

void SotaNachtragDialog::zeige()
{
    QSignalBlocker block(m_tabelle);
    m_tabelle->setRowCount(0);
    m_tabelle->setRowCount(m_erg.vorschlaege.size());
    for (int r = 0; r < m_erg.vorschlaege.size(); ++r) {
        const SotaNachtrag::Vorschlag& v = m_erg.vorschlaege.at(r);
        const LogEntry& e = m_log.at(v.index);
        auto* haken = new QTableWidgetItem;
        haken->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        haken->setCheckState(v.vorgewaehlt ? Qt::Checked : Qt::Unchecked);
        m_tabelle->setItem(r, kHaken, haken);
        auto text = [this, r](int c, const QString& s) {
            m_tabelle->setItem(r, c, new QTableWidgetItem(s));
        };
        text(kZeit, e.timeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        text(kRuf, e.call);
        text(kBand, e.band);
        text(kMode, e.mode);
        text(kGipfel, v.gipfel);
        text(kLocator, locatorText(v));
        text(kS2s, v.gegenGipfel);
        text(kHinweis, v.hinweis);
    }
    m_tabelle->resizeColumnsToContents();
    zaehle();
}

void SotaNachtragDialog::zaehle()
{
    const int n = gewaehlt().size();
    const bool wartet = wartetAufGipfel();
    // Erst wenn die Locatoren der Gipfel da sind -- sonst kaeme der
    // Gipfel ohne seinen Locator ins Log.
    m_anwenden->setEnabled(n > 0 && !wartet);
    if (wartet) {
        m_anwenden->setText(QStringLiteral("Looking up summits…"));
    } else {
        m_anwenden->setText(n > 0 ? QStringLiteral("Apply to %1 contact%2")
                                        .arg(n).arg(n == 1 ? QString() : QStringLiteral("s"))
                                  : QStringLiteral("Apply"));
    }
    m_summe->setText(zusammenfassung());
}

QString SotaNachtragDialog::locatorText(const SotaNachtrag::Vorschlag& v) const
{
    const SotaGipfelVerzeichnis& g = SotaGipfelVerzeichnis::instanz();
    const QString jetzt = m_log.at(v.index).myGridSquare.trimmed().toUpper();
    const QString gipfel = g.gipfelLocator(v.gipfel);
    if (!gipfel.isEmpty()) {
        if (jetzt.startsWith(gipfel)) { return jetzt; }
        return jetzt.isEmpty() ? QStringLiteral("→ %1").arg(gipfel)
                               : QStringLiteral("%1 → %2").arg(jetzt, gipfel);
    }
    if (g.wartet(v.gipfel)) { return QStringLiteral("%1 (looking up summit…)").arg(jetzt); }
    if (g.istUnbekannt(v.gipfel)) {
        return QStringLiteral("%1 (summit not in the SOTA list)").arg(jetzt);
    }
    return QStringLiteral("%1 (couldn't look up the summit)").arg(jetzt);
}

bool SotaNachtragDialog::wartetAufGipfel() const
{
    const SotaGipfelVerzeichnis& g = SotaGipfelVerzeichnis::instanz();
    for (const SotaNachtrag::Vorschlag& v : m_erg.vorschlaege) {
        if (g.wartet(v.gipfel) || g.wartet(v.gegenGipfel)) { return true; }
    }
    return false;
}

void SotaNachtragDialog::zeigeLocatoren()
{
    for (int r = 0; r < m_tabelle->rowCount() && r < m_erg.vorschlaege.size(); ++r) {
        if (QTableWidgetItem* it = m_tabelle->item(r, kLocator)) {
            it->setText(locatorText(m_erg.vorschlaege.at(r)));
        }
    }
    m_tabelle->resizeColumnToContents(kLocator);
    zaehle();
}

QString SotaNachtragDialog::zusammenfassung() const
{
    QStringList s;
    s << QStringLiteral("%1 contact%2 on activation days")
             .arg(m_erg.vorschlaege.size())
             .arg(m_erg.vorschlaege.size() == 1 ? QString() : QStringLiteral("s"));
    if (m_erg.schonGesetzt > 0) {
        s << QStringLiteral("%1 already have a summit (left as they are)").arg(m_erg.schonGesetzt);
    }
    if (!m_erg.mehrdeutig.isEmpty()) {
        s << QStringLiteral("not assigned, two summits on one day: %1")
                 .arg(m_erg.mehrdeutig.join(QStringLiteral("; ")));
    }
    if (m_erg.tageOhneQso > 0) {
        s << QStringLiteral("%1 activation day%2 without contacts in this log")
                 .arg(m_erg.tageOhneQso)
                 .arg(m_erg.tageOhneQso == 1 ? QString() : QStringLiteral("s"));
    }
    return s.join(QStringLiteral(" · "));
}

QVector<SotaNachtrag::Vorschlag> SotaNachtragDialog::gewaehlt() const
{
    QVector<SotaNachtrag::Vorschlag> aus;
    for (int r = 0; r < m_tabelle->rowCount() && r < m_erg.vorschlaege.size(); ++r) {
        const QTableWidgetItem* it = m_tabelle->item(r, kHaken);
        if (it && it->checkState() == Qt::Checked) { aus << m_erg.vorschlaege.at(r); }
    }
    return aus;
}

} // namespace Longpath
