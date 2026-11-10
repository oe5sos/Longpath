// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/Google3dSetupDialog.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port. Siehe Google3dSetupDialog.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-10 — Quelle waehlbar: Cesium ion (kostenlos) oder Google.
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
#include <QRadioButton>
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

QString Google3dSetupDialog::guideHtml(Map3dAccess::Source s)
{
    // Verweise in der Akzentfarbe; jeder Schritt ein Satz, der sagt, wo
    // man klickt und woran man erkennt, dass es geklappt hat.
    const QString a = QStringLiteral("style=\"color:%1;\"").arg(QLatin1String(Style::kAccent));
    if (s == Map3dAccess::Source::CesiumIon) {
        return QStringLiteral(
            "<ol style=\"margin-left:-20px;\">"
            "<li><b>Create a free account.</b> Sign up at "
            "<a %1 href=\"https://ion.cesium.com/signup/\">ion.cesium.com</a> with your "
            "e-mail address. The <i>Community</i> plan is free for personal, "
            "non-commercial use &mdash; amateur radio is exactly that. Longpath needs no "
            "payment details: if a page asks for a card, you are looking at a paid plan "
            "&mdash; go back.</li>"
            "<li><b>Copy your token.</b> Open "
            "<a %1 href=\"https://ion.cesium.com/tokens\">Access Tokens</a>, click the "
            "<i>Default</i> token and copy it (a long text starting with &ldquo;eyJ&rdquo;).</li>"
            "<li><b>Check the Google tiles.</b> Under "
            "<a %1 href=\"https://ion.cesium.com/assets/\">My Assets</a> there should be "
            "&ldquo;Google Photorealistic 3D Tiles&rdquo;. If not, add it from the "
            "<a %1 href=\"https://ion.cesium.com/assetdepot/2275207\">Asset Depot</a>.</li>"
            "<li><b>Paste it below</b>, press <i>Test</i>, then <i>Save</i>.</li>"
            "</ol>").arg(a);
    }
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
    resize(580, 680);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(8);

    auto caption = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; }")
                             .arg(QLatin1String(Style::kTextScale)));
        return l;
    };

    auto* intro = new QLabel(QStringLiteral(
        "<b>Optional.</b> A station you look up in the logbook is shown in Google's "
        "photorealistic 3D: the map flies from your QTH along the great circle and "
        "comes down low enough to see the house and the antenna, wherever Google has "
        "3D coverage. It needs an access of your own &mdash; a free Cesium ion account "
        "is enough.<br><br>"
        "Without one everything keeps working as before &mdash; NASA imagery on the "
        "map, and a button that opens the station in Google Earth."), this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    intro->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; }")
                             .arg(QLatin1String(Style::kTextPrimary)));
    col->addWidget(intro);

    // ── Die Quelle ───────────────────────────────────────────────────
    col->addWidget(caption(QStringLiteral("ACCESS")));
    m_ionChoice = new QRadioButton(QStringLiteral(
        "Cesium ion — free account, no payment details"), this);
    m_ionChoice->setObjectName(QStringLiteral("google3dSourceIon"));
    m_googleChoice = new QRadioButton(QStringLiteral(
        "Google Maps Platform — own key, needs a billing account"), this);
    m_googleChoice->setObjectName(QStringLiteral("google3dSourceGoogle"));
    for (QRadioButton* r : {m_ionChoice, m_googleChoice}) {
        r->setStyleSheet(QStringLiteral("QRadioButton { color: %1; font-size: 13px; }")
                             .arg(QLatin1String(Style::kTextPrimary)));
        col->addWidget(r);
    }

    col->addWidget(caption(QStringLiteral("HOW TO GET IT")));
    m_guide = new QLabel(QString(), this);
    m_guide->setObjectName(QStringLiteral("google3dGuide"));
    m_guide->setWordWrap(true);
    m_guide->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_guide->setTextFormat(Qt::RichText);
    m_guide->setOpenExternalLinks(true);
    m_guide->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_guide->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; "
                                          "background: transparent; }")
                               .arg(QLatin1String(Style::kTextPrimary)));
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_guide);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    col->addWidget(scroll, 1);

    m_costNote = new QLabel(QString(), this);
    m_costNote->setWordWrap(true);
    m_costNote->setStyleSheet(small(Style::kTextSecondary));
    col->addWidget(m_costNote);

    m_keyCaption = caption(QString());
    col->addWidget(m_keyCaption);

    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setEchoMode(QLineEdit::Password);
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
            "WebEngine). The access is kept for a build that has one; until then "
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
    m_removeBtn = new QPushButton(QStringLiteral("Remove"), this);
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
    // toggled, nicht clicked: auch die Bedienhilfe schaltet so um.
    connect(m_ionChoice, &QRadioButton::toggled, this, [this](bool on) {
        if (on) { showSource(Map3dAccess::Source::CesiumIon); }
    });
    connect(m_googleChoice, &QRadioButton::toggled, this, [this](bool on) {
        if (on) { showSource(Map3dAccess::Source::Google); }
    });
    connect(m_keyEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        // Ein Hinweis beim Einfuegen, kein Urteil: das spricht der Dienst.
        const QString v = t.trimmed();
        if (v.isEmpty()) { setStatus(QString{}, true); return; }
        if (m_source == Map3dAccess::Source::CesiumIon && !CesiumIonToken::looksLikeToken(v)) {
            setStatus(QStringLiteral("That does not look like a Cesium ion token (a long "
                                     "text in three parts, starting with \"eyJ\") — was "
                                     "it copied completely?"), false);
        } else if (m_source == Map3dAccess::Source::Google
                   && !GoogleMapsKey::looksLikeApiKey(v)) {
            setStatus(QStringLiteral("That does not look like a Google API key "
                                     "(39 characters, starting with \"AIza\") — "
                                     "was it copied completely?"), false);
        } else {
            setStatus(QString{}, true);
        }
    });

    // Die zuletzt gewaehlte Quelle zeigen (Vorgabe: Cesium ion).
    const Map3dAccess::Source start = Map3dAccess::chosenSource();
    (start == Map3dAccess::Source::Google ? m_googleChoice : m_ionChoice)->setChecked(true);
    showSource(start);
}

void Google3dSetupDialog::showSource(Map3dAccess::Source s)
{
    m_source = s;
    const bool ion = s == Map3dAccess::Source::CesiumIon;
    m_guide->setText(guideHtml(s));
    m_costNote->setText(ion
        ? QStringLiteral("Cesium's free plan includes 1,000 Google 3D tile requests a "
                         "month; Longpath makes one when the 3D view starts, not one "
                         "per station. Pressing Test uses one, too. With no payment "
                         "details on file, nothing can be charged.")
        : QStringLiteral("Each start of the 3D view opens one 3D tiles session on your "
                         "key; Google counts it against the free monthly quota. "
                         "Pressing Test uses one, too."));
    m_keyCaption->setText(ion ? QStringLiteral("ACCESS TOKEN") : QStringLiteral("API KEY"));
    // Kein Beispielwert als Platzhalter: ein grauer Wert in einem leeren
    // Pflichtfeld liest sich wie schon ausgefuellt (siehe QRZ).
    m_keyEdit->setPlaceholderText(ion ? QStringLiteral("paste your Cesium ion access token")
                                      : QStringLiteral("paste your Google Maps Platform key"));
    {
        QSignalBlocker b(m_keyEdit);
        m_keyEdit->setText(Map3dAccess::secretFor(s));
    }
    m_removeBtn->setEnabled(!Map3dAccess::secretFor(s).isEmpty());
    m_removeBtn->setToolTip(ion
        ? QStringLiteral("Delete the Cesium ion token from the keychain")
        : QStringLiteral("Delete the Google key from the keychain"));
    m_testBtn->setToolTip(ion
        ? QStringLiteral("Ask Cesium ion whether this token may open Google's 3D tiles")
        : QStringLiteral("Ask Google's Map Tiles API whether this key may load the 3D tiles"));
    setStatus(QString{}, true);
}

void Google3dSetupDialog::setStatus(const QString& text, bool good)
{
    m_status->setText(text);
    m_status->setStyleSheet(small(good ? Style::kGreenText : Style::kAmberText));
}

void Google3dSetupDialog::runTest()
{
    const QString secret = m_keyEdit->text().trimmed();
    if (secret.isEmpty()) {
        setStatus(GoogleTilesProbe::sentenceFor(m_source, GoogleTilesProbe::Verdict::Empty),
                  false);
        return;
    }
    m_testBtn->setEnabled(false);
    setStatus(m_source == Map3dAccess::Source::CesiumIon ? QStringLiteral("Asking Cesium ion…")
                                                          : QStringLiteral("Asking Google…"),
              true);
    m_probe->check(m_source, secret);
}

void Google3dSetupDialog::onProbeFinished(GoogleTilesProbe::Verdict v,
                                          const QString& message)
{
    m_testBtn->setEnabled(true);
    setStatus(message, v == GoogleTilesProbe::Verdict::Ok);
}

void Google3dSetupDialog::save()
{
    if (m_source == Map3dAccess::Source::CesiumIon) {
        CesiumIonToken::save(m_keyEdit->text());
    } else {
        GoogleMapsKey::save(m_keyEdit->text());
    }
    Map3dAccess::setChosenSource(m_source);
    accept();
}

void Google3dSetupDialog::removeKey()
{
    if (m_source == Map3dAccess::Source::CesiumIon) {
        CesiumIonToken::erase();
    } else {
        GoogleMapsKey::erase();
    }
    m_keyEdit->clear();
    m_removeBtn->setEnabled(false);
    setStatus(QStringLiteral("Removed — without an access the map uses NASA imagery."), true);
}

} // namespace Longpath
