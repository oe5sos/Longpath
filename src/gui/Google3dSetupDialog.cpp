// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/Google3dSetupDialog.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port. Siehe Google3dSetupDialog.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/Google3dSetupDialog.h"

#include "core/CredentialStore.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/Station3dSurface.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcGoogle3dSetup, "longpath.flug3d.einrichtung")

QPointer<Google3dSetupDialog>& openInstance()
{
    static QPointer<Google3dSetupDialog> p;
    return p;
}

QString small(const char* colour)
{
    return QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QLatin1String(colour));
}
} // namespace

Google3dSetupDialog* Google3dSetupDialog::showFor(QWidget* parent)
{
    QPointer<Google3dSetupDialog>& p = openInstance();
    if (!p) {
        // Am FENSTER aufhaengen, nicht an der Spalte, aus der geklickt
        // wurde: die Kartenspalte im Logbuch ist ein eingebettetes Widget,
        // und ein Dialog soll dem Fenster gehoeren, vor dem er aufgeht.
        p = new Google3dSetupDialog(parent ? parent->window() : nullptr);
        p->setAttribute(Qt::WA_DeleteOnClose);
    }
    p->show();
    p->raise();
    p->activateWindow();
    // Noch einmal nach dem Klick: Maus-Loslassen und die Fensterebenen
    // (AuxiliaryWindowLeveler) ordnen erst danach -- der Dialog soll
    // danach vorne liegen, nicht davor.
    QTimer::singleShot(0, p.data(), [d = p.data()]() {
        d->raise();
        d->activateWindow();
    });
    qCInfo(lcGoogle3dSetup).noquote()
        << "Google-3D-Einrichtung offen, vor"
        << (p->parentWidget() ? QString::fromLatin1(p->parentWidget()->metaObject()->className())
                              : QStringLiteral("none"));
    return p.data();
}

QString Google3dSetupDialog::guideHtml()
{
    // Verweise in der Akzentfarbe; jeder Schritt ein Satz, der sagt, wo
    // man klickt und woran man erkennt, dass es geklappt hat.
    const QString a = QStringLiteral("style=\"color:%1;\"").arg(QLatin1String(Style::kAccent));
    return QStringLiteral(
        "<ol style=\"margin-left:-20px;\">"
        "<li><b>Create a project.</b> Open the "
        "<a %1 href=\"https://console.cloud.google.com/projectcreate\">Google Cloud console</a>, "
        "sign in with your Google account and create a project, for example "
        "&ldquo;Longpath&rdquo;.</li>"
        "<li><b>Add a billing account.</b> Under "
        "<a %1 href=\"https://console.cloud.google.com/billing\">Billing</a>, link a "
        "billing account to the project. <b>Google requires one even for the free "
        "monthly quota</b> &mdash; without it the key is refused. A budget alert "
        "keeps you on the safe side.</li>"
        "<li><b>Enable the Map Tiles API.</b> Open "
        "<a %1 href=\"https://console.cloud.google.com/apis/library/tile.googleapis.com\">"
        "Map Tiles API</a> in the API library and press <i>Enable</i>.</li>"
        "<li><b>Create an API key.</b> Under "
        "<a %1 href=\"https://console.cloud.google.com/apis/credentials\">APIs &amp; "
        "Services &rarr; Credentials</a> choose <i>Create credentials &rarr; API key</i> "
        "and copy the key (it starts with &ldquo;AIza&rdquo;).</li>"
        "<li><b>Restrict the key.</b> Edit the key: under <i>API restrictions</i> pick "
        "<i>Restrict key</i> and tick only <i>Map Tiles API</i>. Leave "
        "<i>Application restrictions</i> at <i>None</i> &mdash; Longpath is not a "
        "website.</li>"
        "<li><b>Paste it below</b>, press <i>Test</i>, then <i>Save</i>.</li>"
        "</ol>").arg(a);
}

Google3dSetupDialog::Google3dSetupDialog(QWidget* parent)
    : QDialog(parent)
    , m_probe(new GoogleTilesProbe(this))
{
    setWindowTitle(QStringLiteral("Google 3D map"));
    setModal(false);
    Style::applyDarkPageStyle(this);
    resize(560, 620);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(8);

    auto* intro = new QLabel(QStringLiteral(
        "<b>Optional.</b> With your own Google Maps Platform key, a station you "
        "look up in the logbook is shown in Google's photorealistic 3D: the map "
        "flies from your QTH along the great circle and comes down low enough to "
        "see the house and the antenna, wherever Google has 3D coverage.<br><br>"
        "Without a key everything keeps working as before &mdash; NASA imagery "
        "on the map, and a button that opens the station in Google Earth."), this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    intro->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; }")
                             .arg(QLatin1String(Style::kTextPrimary)));
    col->addWidget(intro);

    auto* howCap = new QLabel(QStringLiteral("HOW TO GET A KEY"), this);
    howCap->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; }")
                              .arg(QLatin1String(Style::kTextScale)));
    col->addWidget(howCap);

    auto* guide = new QLabel(guideHtml(), this);
    guide->setObjectName(QStringLiteral("google3dGuide"));
    guide->setWordWrap(true);
    guide->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    guide->setTextFormat(Qt::RichText);
    guide->setOpenExternalLinks(true);
    guide->setTextInteractionFlags(Qt::TextBrowserInteraction);
    guide->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; }")
                             .arg(QLatin1String(Style::kTextPrimary)));
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(guide);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    guide->setStyleSheet(guide->styleSheet()
                         + QStringLiteral("QLabel { background: transparent; }"));
    col->addWidget(scroll, 1);

    auto* costNote = new QLabel(QStringLiteral(
        "Each flight opens one 3D tiles session on your key; Google counts it "
        "against the free monthly quota. Pressing Test uses one, too."), this);
    costNote->setWordWrap(true);
    costNote->setStyleSheet(small(Style::kTextSecondary));
    col->addWidget(costNote);

    auto* keyCap = new QLabel(QStringLiteral("API KEY"), this);
    keyCap->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; }")
                              .arg(QLatin1String(Style::kTextScale)));
    col->addWidget(keyCap);

    m_keyEdit = new QLineEdit(GoogleMapsKey::load(), this);
    m_keyEdit->setEchoMode(QLineEdit::Password);
    // Kein Beispielschluessel als Platzhalter: ein grauer Wert in einem
    // leeren Pflichtfeld liest sich wie schon ausgefuellt (siehe QRZ).
    m_keyEdit->setPlaceholderText(QStringLiteral("paste your Google Maps Platform key"));
    m_keyEdit->setClearButtonEnabled(true);
    m_keyEdit->setStyleSheet(Style::lineEditStyle());
    col->addWidget(m_keyEdit);

    auto* where = new QLabel(CredentialStore::backendDescription(), this);
    where->setWordWrap(true);
    where->setStyleSheet(small(Style::kTextSecondary));
    col->addWidget(where);

    if (!Station3dSurface::isAvailable()) {
        auto* noWeb = new QLabel(QStringLiteral(
            "This build of Longpath has no 3D view (it was built without Qt "
            "WebEngine). The key is kept for a build that has one; until then "
            "the map offers Google Earth instead."), this);
        noWeb->setObjectName(QStringLiteral("google3dNoWebEngine"));
        noWeb->setWordWrap(true);
        noWeb->setStyleSheet(small(Style::kAmberText));
        col->addWidget(noWeb);
    }

    m_status = new QLabel(QString{}, this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(small(Style::kTextSecondary));
    col->addWidget(m_status);

    auto* row = new QHBoxLayout;
    m_testBtn = new QPushButton(QStringLiteral("Test"), this);
    m_testBtn->setToolTip(QStringLiteral(
        "Ask Google's Map Tiles API whether this key may load the 3D tiles"));
    m_removeBtn = new QPushButton(QStringLiteral("Remove key"), this);
    m_removeBtn->setToolTip(QStringLiteral(
        "Delete the key from the keychain — the map goes back to NASA imagery"));
    m_removeBtn->setEnabled(GoogleMapsKey::isSet());
    auto* closeBtn = new QPushButton(QStringLiteral("Cancel"), this);
    m_saveBtn = new QPushButton(QStringLiteral("Save"), this);
    m_saveBtn->setDefault(true);
    for (QPushButton* b : {m_testBtn, m_removeBtn, closeBtn, m_saveBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
    }
    row->addWidget(m_testBtn);
    row->addWidget(m_removeBtn);
    row->addStretch();
    row->addWidget(closeBtn);
    row->addWidget(m_saveBtn);
    col->addLayout(row);

    connect(m_testBtn, &QPushButton::clicked, this, &Google3dSetupDialog::runTest);
    connect(m_saveBtn, &QPushButton::clicked, this, &Google3dSetupDialog::save);
    connect(m_removeBtn, &QPushButton::clicked, this, &Google3dSetupDialog::removeKey);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_probe, &GoogleTilesProbe::finished,
            this, &Google3dSetupDialog::onProbeFinished);
    connect(m_keyEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        // Ein Hinweis beim Einfuegen, kein Urteil: das spricht Google.
        if (!t.trimmed().isEmpty() && !GoogleMapsKey::looksLikeApiKey(t)) {
            setStatus(QStringLiteral("That does not look like a Google API key "
                                     "(39 characters, starting with \"AIza\") — "
                                     "was it copied completely?"), false);
        } else {
            setStatus(QString{}, true);
        }
    });
}

void Google3dSetupDialog::setStatus(const QString& text, bool good)
{
    m_status->setText(text);
    m_status->setStyleSheet(small(good ? Style::kGreenText : Style::kAmberText));
}

void Google3dSetupDialog::runTest()
{
    const QString key = m_keyEdit->text().trimmed();
    if (key.isEmpty()) {
        setStatus(GoogleTilesProbe::sentence(GoogleTilesProbe::Verdict::Empty), false);
        return;
    }
    m_testBtn->setEnabled(false);
    setStatus(QStringLiteral("Asking Google…"), true);
    m_probe->check(key);
}

void Google3dSetupDialog::onProbeFinished(GoogleTilesProbe::Verdict v,
                                          const QString& message)
{
    m_testBtn->setEnabled(true);
    setStatus(message, v == GoogleTilesProbe::Verdict::Ok);
}

void Google3dSetupDialog::save()
{
    GoogleMapsKey::save(m_keyEdit->text());
    accept();
}

void Google3dSetupDialog::removeKey()
{
    GoogleMapsKey::erase();
    m_keyEdit->clear();
    m_removeBtn->setEnabled(false);
    setStatus(QStringLiteral("Key removed — the map uses NASA imagery again."), true);
}

} // namespace Longpath
