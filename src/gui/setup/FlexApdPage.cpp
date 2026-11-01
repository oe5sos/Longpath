// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/FlexApdPage.cpp  (Longpath-eigen)
// =================================================================
//
// Siehe FlexApdPage.h — dort steht, warum der Signalweg oben sitzt.
//
// Modification history (Longpath):
//   2026-11-01 -- Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Claude Code.
//   2026-11-01 -- Entwurf C umgesetzt (Signalweg oben).
// =================================================================

#include "FlexApdPage.h"

#include "core/FlexApdState.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPolygon>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace Longpath {

namespace {

/// Der Wert, mit dem die Maschine „kein Messpunkt gewaehlt" meldet —
/// und mit dem man die Wahl auch wieder zuruecknimmt.
constexpr auto kKeinMesspunkt = "INVALID";

// Der Weg wird gezeichnet, nicht gestylt — darum hier die Farben als
// QColor. Werte aus gui/StyleConstants.h bzw. docs/design/HAUSSTIL.md.
const QColor kWegRahmen   (31,  31,  35);   // kBorderSubtle
const QColor kWegGrund    (14,  14,  16);
const QColor kWegAktivR   (42,  95, 190);   // kBlueBorder
const QColor kWegAktivF   (18,  28,  44);
const QColor kWegLeise    (144, 144, 150);  // kTextTertiary
const QColor kWegStumm    ( 61,  61,  65);  // kBorderMuted
const QColor kWegAktivT   (207, 226, 245);
const QColor kWegBestaetigt(111, 163, 132); // kGreenText

}  // namespace

// ─────────────────────────────────────────────────────────────────────
// Der Signalweg
// ─────────────────────────────────────────────────────────────────────

FlexApdFlowWidget::FlexApdFlowWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(104);
}

void FlexApdFlowWidget::setZustand(bool laeuft, bool externerMesspunkt,
                                   const QString& messpunkt)
{
    if (laeuft == m_laeuft && externerMesspunkt == m_extern
        && messpunkt == m_messpunkt) {
        return;
    }
    m_laeuft    = laeuft;
    m_extern    = externerMesspunkt;
    m_messpunkt = messpunkt;
    update();
}

QSize FlexApdFlowWidget::sizeHint() const
{
    return QSize(520, 104);
}

void FlexApdFlowWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Vier Stationen, gleichmaessig ueber die Breite. Die Luecken
    // tragen die Pfeile.
    constexpr int kStationen = 4;
    const int lueck = 22;
    const int h     = 46;
    const int oben  = 4;
    const int w = qMax(70, (width() - (kStationen - 1) * lueck) / kStationen);

    const QString paText     = m_extern ? tr("external amp") : tr("own PA");
    const QString kopplerText= m_extern ? tr("amp sampler")  : tr("built-in");
    const QString rueckText  = m_extern && !m_messpunkt.isEmpty()
                                 ? m_messpunkt : tr("internal");

    const QString oberezeile[kStationen] = { tr("TX"), tr("PA"),
                                             tr("COUPLER"), tr("FEEDBACK") };
    const QString unterezeile[kStationen] = { tr("mic → radio"), paText,
                                              kopplerText, rueckText };

    int x = 0;
    int ersteMitte = 0;
    int letzteMitte = 0;
    for (int i = 0; i < kStationen; ++i) {
        const QRect r(x, oben, w, h);

        p.setPen(QPen(m_laeuft ? kWegAktivR : kWegRahmen, 1));
        p.setBrush(m_laeuft ? kWegAktivF : kWegGrund);
        p.drawRoundedRect(r, 3, 3);

        QFont f = p.font();
        f.setPointSizeF(10.5);
        f.setBold(true);
        p.setFont(f);
        p.setPen(m_laeuft ? kWegAktivT : kWegLeise);
        p.drawText(r.adjusted(2, 5, -2, -22), Qt::AlignCenter, oberezeile[i]);

        f.setPointSizeF(9.5);
        f.setBold(false);
        p.setFont(f);
        p.setPen(m_laeuft ? kWegLeise : kWegStumm);
        p.drawText(r.adjusted(2, 25, -2, -4),
                   Qt::AlignHCenter | Qt::AlignTop, unterezeile[i]);

        if (i == 0) { ersteMitte = r.center().x(); }
        if (i == kStationen - 1) { letzteMitte = r.center().x(); }

        if (i < kStationen - 1) {
            const int y = r.center().y();
            const int von = r.right() + 3;
            const int bis = r.right() + lueck - 3;
            p.setPen(QPen(m_laeuft ? kWegAktivR : kWegStumm, 1));
            p.drawLine(von, y, bis - 5, y);
            QPolygon spitze;
            spitze << QPoint(bis, y) << QPoint(bis - 6, y - 3)
                   << QPoint(bis - 6, y + 3);
            p.setPen(Qt::NoPen);
            p.setBrush(m_laeuft ? kWegAktivR : kWegStumm);
            p.drawPolygon(spitze);
        }
        x += w + lueck;
    }

    // Die Rueckfuehrung. Sie ist der Grund, warum das Ganze
    // funktioniert — und im Aus-Fall ist sie offen und grau.
    const int bogenY = oben + h + 14;
    p.setPen(QPen(m_laeuft ? kWegAktivR : kWegStumm, 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawLine(letzteMitte, oben + h, letzteMitte, bogenY);
    p.drawLine(letzteMitte, bogenY, ersteMitte, bogenY);
    p.drawLine(ersteMitte, bogenY, ersteMitte, oben + h);

    QFont f = p.font();
    f.setPointSizeF(9.5);
    f.setBold(false);
    p.setFont(f);
    p.setPen(m_laeuft ? kWegBestaetigt : kWegStumm);
    p.drawText(QRect(0, bogenY + 2, width(), 18),
               Qt::AlignHCenter | Qt::AlignTop,
               m_laeuft ? tr("correction applied in the radio")
                        : tr("loop open — predistortion off"));
}

// ─────────────────────────────────────────────────────────────────────
// Die Seite
// ─────────────────────────────────────────────────────────────────────

FlexApdPage::FlexApdPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    m_banner->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px; font-weight: bold;")
            .arg(QLatin1String(Style::kAmberText))
            .arg(Style::kFontSmall)));
    root->addWidget(m_banner);

    // ── Die Karte ────────────────────────────────────────────────────
    m_karte = new QFrame(this);
    m_karte->setObjectName(QStringLiteral("apdKarte"));
    m_karte->setStyleSheet(Style::themed(
        QStringLiteral("QFrame#apdKarte { background: %1; border: 1px solid %2; "
                       "border-radius: 4px; }")
            .arg(QLatin1String(Style::kPanelBg))
            .arg(QLatin1String(Style::kBorder))));
    auto* karte = new QVBoxLayout(m_karte);
    karte->setContentsMargins(14, 10, 14, 12);
    karte->setSpacing(8);
    root->addWidget(m_karte);

    // Kopfzeile: Name links, Schalter rechts — die Regel des
    // Betreibers, dass die Bedienung rechts oben im Kopf sitzt.
    {
        auto* kopf = new QHBoxLayout;
        kopf->setContentsMargins(0, 0, 0, 0);
        auto* titel = new QLabel(tr("PREDISTORTION"), m_karte);
        titel->setStyleSheet(Style::themed(
            QStringLiteral("color: %1; font-size: %2px; font-weight: bold;")
                .arg(QLatin1String(Style::kTextPrimary))
                .arg(Style::kFontSmall)));
        kopf->addWidget(titel);
        kopf->addStretch(1);

        m_enable = new QCheckBox(tr("on"), m_karte);
        m_enable->setToolTip(tr(
            "Sends 'apd enable=0|1'. The radio learns its correction curve "
            "while transmitting; nothing is learned and nothing changes until "
            "you key it. Longpath never switches this on by itself."));
        connect(m_enable, &QCheckBox::toggled, this, [this](bool on) {
            if (m_updatingFromModel || !m_model) { return; }
            m_model->flexApdEinschalten(on);
        });
        kopf->addWidget(m_enable);
        karte->addLayout(kopf);
    }

    m_flow = new FlexApdFlowWidget(m_karte);
    karte->addWidget(m_flow);

    m_samplerHost = new QWidget(m_karte);
    m_samplerForm = new QFormLayout(m_samplerHost);
    m_samplerForm->setContentsMargins(0, 2, 0, 0);
    m_samplerForm->setHorizontalSpacing(14);
    m_samplerForm->setVerticalSpacing(6);
    karte->addWidget(m_samplerHost);

    {
        auto* fuss = new QHBoxLayout;
        fuss->setContentsMargins(0, 0, 0, 0);
        m_messwerte = new QLabel(m_karte);
        m_messwerte->setStyleSheet(Style::themed(
            QStringLiteral("color: %1; font-size: %2px;")
                .arg(QLatin1String(Style::kAmberText))
                .arg(Style::kFontSmall)));
        fuss->addWidget(m_messwerte);
        fuss->addStretch(1);

        m_reset = new QPushButton(tr("Discard curve"), m_karte);
        m_reset->setToolTip(tr(
            "Sends 'apd reset'. The radio throws the curve away and starts "
            "learning again on the next transmission. Worth doing after "
            "changing the sampler, the antenna or the amplifier."));
        connect(m_reset, &QPushButton::clicked, this, [this] {
            if (m_model) { m_model->flexApdZuruecksetzen(); }
        });
        fuss->addWidget(m_reset);
        karte->addLayout(fuss);
    }

    // Die lange Erklaerung steht UNTER der Karte und klein: wer die
    // Bedienung kennt, soll sie nicht jedes Mal lesen muessen.
    m_helper = new QLabel(tr(
        "This is the radio's own predistortion, not PureSignal. On an "
        "OpenHPSDR radio Longpath computes the correction itself and puts it "
        "into its own transmit path; a FlexRadio modulates from the audio we "
        "send it, so the correction has to happen inside the radio.\n"
        "With no external sampler selected the radio uses its built-in "
        "directional coupler and linearises its own PA. To correct an "
        "external amplifier instead, feed that amplifier's sampler port into "
        "one of the receive inputs and select it above."), this);
    m_helper->setWordWrap(true);
    m_helper->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextTertiary))
            .arg(Style::kFontSmall)));
    root->addWidget(m_helper);

    root->addStretch(1);

    if (m_model) {
        connect(m_model, &RadioModel::flexApdGeaendert,
                this, &FlexApdPage::refreshFromRadio);
    }
    refreshFromRadio();
}

void FlexApdPage::refreshFromRadio()
{
    if (!m_model) { return; }

    const FlexApdState& apd = m_model->flexApd();
    const bool bekannt = apd.bekannt();

    // Abwesenheit als Abwesenheit zeigen: leere Regler sehen aus wie
    // „aus", und das waere eine Aussage ueber eine Maschine, die
    // nichts gesagt hat.
    m_karte->setVisible(bekannt);
    m_helper->setVisible(bekannt);
    m_banner->setVisible(!bekannt);

    if (!bekannt) {
        m_banner->setText(tr(
            "This radio reports no predistortion. FlexRadio's APD is in the "
            "8000 series from SmartSDR 4.2.18; older firmware and the 6000 "
            "series answer the subscription and send nothing. For an "
            "OpenHPSDR radio, use Tools > PureSignal instead."));
        return;
    }

    const QString ant = apd.antenne();
    const bool externerMesspunkt = !ant.isEmpty() && apd.hatMesspunkt(ant);

    // Kein Zustandssatz mehr, wenn die Maschine meldet: der Schalter
    // und der Weg sagen dasselbe, und zwar schneller. Der Banner
    // gehoert dem einen Fall, in dem es nichts zu sehen gibt.
    m_banner->setVisible(false);

    {
        const QSignalBlocker block(m_enable);
        m_enable->setChecked(apd.eingeschaltet());
    }

    m_flow->setZustand(apd.eingeschaltet(), externerMesspunkt,
                       externerMesspunkt ? apd.gewaehlterMesspunkt(ant) : QString());

    // Was die Maschine zuletzt gelernt hat, gilt je Antenne und Band —
    // nicht allgemein. Darum stehen beide Zahlen dabei.
    QStringList teile;
    if (!ant.isEmpty()) { teile << ant; }
    if (apd.frequenzMhz() > 0.0) {
        teile << tr("%1 MHz").arg(apd.frequenzMhz(), 0, 'f', 3);
    }
    teile << tr("%1 % power").arg(apd.leistungProzent());
    teile << (apd.kurveTraegt() ? tr("learned curve in circuit")
                                : tr("nothing learned yet"));
    m_messwerte->setText(teile.join(QStringLiteral(" · ")));

    rebuildSamplerRows();
}

void FlexApdPage::rebuildSamplerRows()
{
    if (!m_model) { return; }
    const FlexApdState& apd = m_model->flexApd();

    // Ohne waehlbaren Messpunkt (`configurable=0`) gibt es nichts zu
    // waehlen — dann bleiben die Zeilen weg, statt ungenutzt dazustehen.
    m_samplerHost->setVisible(apd.einstellbar());

    // Die Zeilen haengen daran, WELCHE Sendeantennen die Maschine
    // fuehrt. Sie meldet sie einzeln und nicht unbedingt auf einmal,
    // also wird die Liste neu gebaut, sobald sie sich aendert.
    const QStringList antennen = apd.sendeantennen();
    bool passt = antennen.size() == m_samplerCombos.size();
    if (passt) {
        for (const QString& a : antennen) {
            if (!m_samplerCombos.contains(a)) { passt = false; break; }
        }
    }
    if (!passt) {
        while (m_samplerForm->rowCount() > 0) {
            m_samplerForm->removeRow(0);
        }
        m_samplerCombos.clear();

        for (const QString& ant : antennen) {
            auto* combo = new QComboBox(m_samplerHost);
            combo->setToolTip(tr(
                "Where the radio listens to its own output while transmitting "
                "on this antenna. 'Internal coupler' corrects the radio's own "
                "PA; the other entries are receive inputs you can feed from an "
                "external amplifier's sampler port."));
            connect(combo, &QComboBox::currentIndexChanged, this,
                    [this, ant, combo](int) {
                if (m_updatingFromModel || !m_model) { return; }
                const QString punkt = combo->currentData().toString();
                if (punkt.isEmpty()) { return; }
                m_model->flexApdMesspunktSetzen(ant, punkt);
            });
            m_samplerCombos.insert(ant, combo);
            m_samplerForm->addRow(tr("Sampler %1").arg(ant), combo);
        }
    }

    // Inhalte fuellen, ohne dass ein Feld dabei einen Befehl ausloest.
    m_updatingFromModel = true;
    for (auto it = m_samplerCombos.cbegin(); it != m_samplerCombos.cend(); ++it) {
        QComboBox* combo = it.value();
        const QSignalBlocker block(combo);
        combo->clear();
        combo->addItem(tr("Internal coupler (own PA)"),
                       QString::fromLatin1(kKeinMesspunkt));
        for (const QString& punkt : apd.moeglicheMesspunkte(it.key())) {
            combo->addItem(punkt, punkt);
        }
        const QString gewaehlt = apd.gewaehlterMesspunkt(it.key());
        const int idx = combo->findData(gewaehlt.isEmpty()
                                            ? QString::fromLatin1(kKeinMesspunkt)
                                            : gewaehlt);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_updatingFromModel = false;
}

}  // namespace Longpath
