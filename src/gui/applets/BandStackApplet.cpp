// no-port-check: Longpath-eigene Oberflaeche; die Technik dahinter steht in
// src/models/bandstack/ (aus Upstream NereusSDR, Regeln aus Thetis).
// =================================================================
// src/gui/applets/BandStackApplet.cpp  (Longpath)
// =================================================================
//
// Siehe BandStackApplet.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include "BandStackApplet.h"

#include <QActionGroup>
#include <QCursor>
#include <QEvent>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <cmath>

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackController.h"

namespace Longpath {

namespace {

const QString kMarkenSchluessel = QStringLiteral("BandStackShowOnSpectrum");

QColor rolle(const char* name, const char* rueckfall)
{
    return QColor(Style::role(name, rueckfall));
}

// Die Reihenfolge der Gruppen: Amateurbaender aufsteigend in der
// Frequenz, dann GEN und WWV, dann die Rundfunkbaender. XVTR hat keinen
// Stapel (Upstream V12).
const QList<Band>& anzeigeReihe()
{
    static const QList<Band> reihe = {
        Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m, Band::Band30m,
        Band::Band20m,  Band::Band17m, Band::Band15m, Band::Band12m, Band::Band10m,
        Band::Band6m,   Band::Band2m,  Band::GEN,     Band::WWV,
        Band::Band120m, Band::Band90m, Band::Band61m, Band::Band49m, Band::Band41m,
        Band::Band31m,  Band::Band25m, Band::Band22m, Band::Band19m, Band::Band16m,
        Band::Band14m,  Band::Band13m, Band::Band11m,
    };
    return reihe;
}

QString breiteText(int low, int high)
{
    const int breite = std::abs(high - low);
    if (breite <= 0) {
        return QString();
    }
    if (breite >= 1000) {
        return QString::number(breite / 1000.0, 'f', 1) + QLatin1Char('k');
    }
    return QString::number(breite);
}

void maleSchloss(QPainter& p, QPointF m, const QColor& farbe)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(farbe, 1.1));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(m.x() - 2.6, m.y() - 6.0, 5.2, 6.0), 0, 180 * 16);
    p.setPen(Qt::NoPen);
    p.setBrush(farbe);
    p.drawRoundedRect(QRectF(m.x() - 3.6, m.y() - 2.6, 7.2, 5.6), 1.0, 1.0);
    p.restore();
}

QString rueckkehrText(const BandStackController* stapel, Band band)
{
    // Die Rueckkehrart steht in der Ablage nur als Teil des Dokuments;
    // das „Always return here" ist der einzige Fall mit Eintrag.
    const QString id = stapel->store().specificId(band);
    if (!id.isEmpty()) {
        if (const BandStackEntry* e = stapel->store().entry(id)) {
            return QStringLiteral("Return: %1")
                .arg(BandStackStore::displayFrequency(e->frequencyHz));
        }
    }
    const QJsonObject bands = stapel->store().toJson().value(QStringLiteral("bands")).toObject();
    const QString art = bands.value(bandKeyName(band)).toObject()
                            .value(QStringLiteral("returnMode")).toString();
    if (art == QLatin1String("lastUsed")) {
        return QStringLiteral("Return: last frequency");
    }
    return QStringLiteral("Return: current spot");
}

} // namespace

// ═════════════════════════════════════════════════════════════════════
// BandStackListe
// ═════════════════════════════════════════════════════════════════════

BandStackListe::BandStackListe(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setObjectName(QStringLiteral("bandStackListe"));
}

void BandStackListe::setzeStapel(BandStackController* stapel, int sliceId)
{
    m_stapel = stapel;
    m_sliceId = sliceId;
    neuAufbauen();
}

bool BandStackListe::istOffen(Band band) const
{
    const int b = static_cast<int>(band);
    if (band == m_band) {
        return !m_zugeklappt.contains(b);
    }
    return m_aufgeklappt.contains(b);
}

void BandStackListe::setzeOffen(Band band, bool offen)
{
    const int b = static_cast<int>(band);
    if (band == m_band) {
        if (offen) { m_zugeklappt.remove(b); } else { m_zugeklappt.insert(b); }
    } else {
        if (offen) { m_aufgeklappt.insert(b); } else { m_aufgeklappt.remove(b); }
    }
    anordnen();
}

void BandStackListe::neuAufbauen()
{
    m_zeilen.clear();
    m_stelle.clear();
    m_buchstabe = QChar();
    if (m_stapel.isNull() || !m_stapel->isReady() || m_sliceId < 0) {
        anordnen();
        return;
    }
    const Band vorher = m_band;
    m_band = m_stapel->stackBandFor(m_sliceId);
    if (m_band != vorher) {
        // Ein anderes Band ist jetzt das eigene: sein Zuklappen von
        // vorhin gilt nicht mehr.
        m_zugeklappt.clear();
    }
    m_stelle = m_stapel->shownPlace(m_sliceId, m_band);
    m_buchstabe = QChar(QLatin1Char('A' + m_sliceId));

    const BandStackStore& store = m_stapel->store();
    const auto gruppeFuer = [&](Band band) {
        Zeile g;
        g.art = Zeile::Gruppe;
        g.band = band;
        const QList<BandStackEntry> eintraege = store.entries(band);
        g.anzahl = static_cast<int>(eintraege.size());
        for (const BandStackEntry& e : eintraege) {
            if (e.locked) { ++g.gesperrt; }
        }
        g.offen = istOffen(band);
        if (g.offen) {
            g.rueckkehr = rueckkehrText(m_stapel, band);
        }
        m_zeilen.append(g);
        if (!g.offen) {
            return;
        }
        for (const BandStackEntry& e : eintraege) {
            Zeile z;
            z.art = Zeile::Eintrag;
            z.band = band;
            z.eintrag = e;
            z.stelle = band == m_band && e.id == m_stelle;
            m_zeilen.append(z);
        }
        if (band == m_band) {
            Zeile k;
            k.art = Zeile::Knopf;
            k.band = band;
            m_zeilen.append(k);
        }
    };

    // Das eigene Band zuerst, offen; die anderen darunter, zu.
    //
    // Die Rundfunkbaender (120 m bis 11 m) nur, wenn die Scheibe gerade
    // auf einem steht: Longpath hat fuer sie keine Bandtasten, und mit
    // ihren dreizehn Gruppen wurde das Applet im echten Fenster fast
    // 900 Punkte hoch -- der gewaehlte Entwurf A zeigt sie nicht.
    gruppeFuer(m_band);
    for (const Band band : anzeigeReihe()) {
        const bool rundfunk = static_cast<int>(band) >= static_cast<int>(Band::SwlFirst)
                              && static_cast<int>(band) <= static_cast<int>(Band::SwlLast);
        if (band != m_band && !rundfunk && !store.entries(band).isEmpty()) {
            gruppeFuer(band);
        }
    }
    anordnen();
}

void BandStackListe::anordnen()
{
    int y = 0;
    bool trennerGesetzt = false;
    for (Zeile& z : m_zeilen) {
        switch (z.art) {
        case Zeile::Gruppe:
            if (z.band != m_band && !trennerGesetzt) {
                y += 6;     // Abstand vor den anderen Baendern
                trennerGesetzt = true;
            }
            z.offen = istOffen(z.band);
            z.rect = QRect(0, y, width(), kGruppeH);
            y += kGruppeH + (z.offen ? 2 : 0);
            break;
        case Zeile::Eintrag:
            z.rect = QRect(4, y, width() - 8, kZeileH);
            y += kZeileH + kZeileLuft;
            break;
        case Zeile::Knopf:
            y += 4;
            z.rect = QRect(8, y, width() - 16, kKnopfH);
            y += kKnopfH + 6;
            break;
        }
    }
    if (m_hoehe != y) {
        m_hoehe = y;
        updateGeometry();
    }
    update();
}

QSize BandStackListe::sizeHint() const
{
    return {280, std::max(m_hoehe, kGruppeH)};
}

void BandStackListe::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    anordnen();
}

int BandStackListe::zeileBei(const QPoint& pos) const
{
    for (int i = 0; i < m_zeilen.size(); ++i) {
        if (m_zeilen.at(i).rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

QRect BandStackListe::nameRect(const Zeile& z) const
{
    const int x = z.rect.left() + 10 + 14 + 84 + 44 + 40;
    const int rechts = z.rect.right() - (z.stelle ? 32 : 8);
    return QRect(x - 4, z.rect.top() + 2, std::max(40, rechts - x + 4), z.rect.height() - 4);
}

QRect BandStackListe::namensfeldFuer(const QString& id) const
{
    for (const Zeile& z : m_zeilen) {
        if (z.art == Zeile::Eintrag && z.eintrag.id == id) {
            return nameRect(z);
        }
    }
    return {};
}

QRect BandStackListe::zeileRectForTest(const QString& id) const
{
    for (const Zeile& z : m_zeilen) {
        if (z.art == Zeile::Eintrag && z.eintrag.id == id) {
            return z.rect;
        }
    }
    return {};
}

QRect BandStackListe::gruppeRectForTest(Band band) const
{
    for (const Zeile& z : m_zeilen) {
        if (z.art == Zeile::Gruppe && z.band == band) {
            return z.rect;
        }
    }
    return {};
}

QRect BandStackListe::knopfRectForTest() const
{
    for (const Zeile& z : m_zeilen) {
        if (z.art == Zeile::Knopf) {
            return z.rect;
        }
    }
    return {};
}

void BandStackListe::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    bool trenner = false;
    for (int i = 0; i < m_zeilen.size(); ++i) {
        const Zeile& z = m_zeilen.at(i);
        if (z.art == Zeile::Gruppe && z.band != m_band && !trenner) {
            p.setPen(rolle("border-subtle", Style::kBorderSubtle));
            p.drawLine(8, z.rect.top() - 3, width() - 8, z.rect.top() - 3);
            trenner = true;
        }
        switch (z.art) {
        case Zeile::Gruppe:  maleGruppe(p, z); break;
        case Zeile::Eintrag: maleEintrag(p, z, i == m_schwebt); break;
        case Zeile::Knopf:   maleKnopf(p, z); break;
        }
    }
}

void BandStackListe::maleGruppe(QPainter& p, const Zeile& z) const
{
    const QRect r = z.rect;
    p.setFont(Style::capsFont(font(), Style::kFontCaption));
    p.setPen(z.offen ? rolle("text-secondary", Style::kTextSecondary)
                     : rolle("text-scale", Style::kTextScale));
    p.drawText(QRect(r.left() + 10, r.top(), 56, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
               bandLabel(z.band));
    p.setFont(Style::monoFont(font(), Style::kFontSmall));
    p.setPen(rolle("text-scale", Style::kTextScale));
    p.drawText(QRect(r.left() + 66, r.top(), 28, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
               QString::number(z.anzahl));
    if (z.gesperrt > 0) {
        maleSchloss(p, QPointF(r.left() + 100, r.center().y() + 1),
                    rolle("text-inactive", Style::kTextInactive));
        p.drawText(QRect(r.left() + 107, r.top(), 24, r.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, QString::number(z.gesperrt));
    }
    if (!z.rueckkehr.isEmpty()) {
        QFont f = font();
        f.setPixelSize(Style::kFontSmall);
        p.setFont(f);
        p.drawText(QRect(r.left(), r.top(), r.width() - 26, r.height()),
                   Qt::AlignVCenter | Qt::AlignRight, z.rueckkehr);
    }
    QFont f = font();
    f.setPixelSize(Style::kFontSmall);
    p.setFont(f);
    p.setPen(rolle("text-scale", Style::kTextScale));
    p.drawText(QRect(r.right() - 20, r.top(), 14, r.height()), Qt::AlignCenter,
               z.offen ? QStringLiteral("▾") : QStringLiteral("▸"));
}

void BandStackListe::maleEintrag(QPainter& p, const Zeile& z, bool schwebt) const
{
    const QRect r = z.rect;
    const BandStackEntry& e = z.eintrag;
    if (z.stelle) {
        // Die Stelle ist eine Mulde (Hausstil, „Kein Auswahl-Blau"),
        // Blau nur als schmaler Strich.
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        // Die Konstanten direkt: die Rolle "sel-bg" der Themendatei
        // traegt noch das alte Blau.
        g.setColorAt(0, QColor(Style::kGlassSelTop));
        g.setColorAt(1, QColor(Style::kGlassSelBot));
        p.setPen(QPen(QColor(Style::kGlassSelBorder), 1.0));
        p.setBrush(g);
        p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        p.fillRect(QRect(r.left() + 1, r.top() + 5, 2, r.height() - 10),
                   rolle("accent", Style::kAccent));
    } else if (schwebt) {
        p.setPen(Qt::NoPen);
        p.setBrush(rolle("button", Style::kButtonBg));
        p.drawRoundedRect(QRectF(r), 6, 6);
    }

    const QColor wert = z.stelle ? QColor(Style::kGlassSelText)
                                 : rolle("text", Style::kTextPrimary);
    int x = r.left() + 10;
    if (e.locked) {
        maleSchloss(p, QPointF(x + 4, r.center().y() + 1), rolle("text-scale", Style::kTextScale));
    }
    x += 14;
    p.setFont(Style::monoFont(font(), Style::kFontBody,
                              z.stelle ? QFont::DemiBold : QFont::Normal));
    p.setPen(wert);
    p.drawText(QRect(x, r.top(), 80, r.height()), Qt::AlignVCenter | Qt::AlignRight,
               BandStackStore::displayFrequency(e.frequencyHz));
    x += 84;
    p.setFont(Style::capsFont(font(), Style::kFontCaption));
    p.setPen(z.stelle ? rolle("text", Style::kTextPrimary)
                      : rolle("text-secondary", Style::kTextSecondary));
    p.drawText(QRect(x, r.top(), 42, r.height()), Qt::AlignVCenter | Qt::AlignLeft, e.mode);
    x += 44;
    p.setFont(Style::monoFont(font(), Style::kFontSmall));
    p.setPen(rolle("text-scale", Style::kTextScale));
    p.drawText(QRect(x, r.top(), 38, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
               breiteText(e.filterLowHz, e.filterHighHz));
    x += 40;
    const int flagge = z.stelle ? 26 : 0;
    const int nameBreite = r.right() - 8 - flagge - x;
    if (nameBreite > 16 && !e.name.isEmpty()) {
        QFont f = font();
        f.setPixelSize(Style::kFontSmall);
        p.setFont(f);
        p.setPen(rolle("text-tertiary", Style::kTextTertiary));
        p.drawText(QRect(x, r.top(), nameBreite, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(e.name, Qt::ElideRight, nameBreite));
    }
    if (z.stelle && !m_buchstabe.isNull()) {
        const QRectF f(r.right() - 26, r.center().y() - 8, 18, 16);
        p.setPen(QPen(rolle("accent", Style::kAccent), 1.0));
        p.setBrush(rolle("app-bg", Style::kAppBg));
        p.drawRoundedRect(f, 4, 4);
        QFont s = font();
        s.setPixelSize(Style::kFontSmall);
        s.setBold(true);
        p.setFont(s);
        p.setPen(rolle("text", Style::kTextPrimary));
        p.drawText(f, Qt::AlignCenter, QString(m_buchstabe));
    }
}

void BandStackListe::maleKnopf(QPainter& p, const Zeile& z) const
{
    Style::paintRaisedChip(p, z.rect, 7, m_knopfGedrueckt);
    QFont f = font();
    f.setPixelSize(Style::kFontSmall);
    p.setFont(f);
    p.setPen(rolle("text", Style::kTextPrimary));
    p.drawText(z.rect, Qt::AlignCenter, QStringLiteral("+  Add current spot"));
}

void BandStackListe::mousePressEvent(QMouseEvent* event)
{
    const int i = zeileBei(event->position().toPoint());
    if (i < 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    const Zeile& z = m_zeilen.at(i);
    if (event->button() == Qt::RightButton) {
        if (z.art == Zeile::Eintrag) {
            emit zeilenMenue(z.eintrag.id, event->globalPosition().toPoint());
        } else if (z.art == Zeile::Gruppe) {
            emit bandMenue(z.band, event->globalPosition().toPoint());
        }
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    switch (z.art) {
    case Zeile::Gruppe:
        setzeOffen(z.band, !istOffen(z.band));
        neuAufbauen();
        break;
    case Zeile::Eintrag:
        emit zeileGeklickt(z.eintrag.id);
        break;
    case Zeile::Knopf:
        m_knopfGedrueckt = true;
        update(z.rect);
        break;
    }
}

void BandStackListe::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_knopfGedrueckt) {
        m_knopfGedrueckt = false;
        update();
        const int i = zeileBei(event->position().toPoint());
        if (i >= 0 && m_zeilen.at(i).art == Zeile::Knopf && event->button() == Qt::LeftButton) {
            emit hinzufuegenGeklickt();
        }
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void BandStackListe::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int i = zeileBei(event->position().toPoint());
    if (i >= 0 && m_zeilen.at(i).art == Zeile::Eintrag && event->button() == Qt::LeftButton) {
        emit umbenennenGewuenscht(m_zeilen.at(i).eintrag.id, nameRect(m_zeilen.at(i)));
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void BandStackListe::mouseMoveEvent(QMouseEvent* event)
{
    int i = zeileBei(event->position().toPoint());
    if (i >= 0 && m_zeilen.at(i).art != Zeile::Eintrag) {
        i = -1;
    }
    if (i != m_schwebt) {
        m_schwebt = i;
        update();
    }
    if (i >= 0) {
        const BandStackEntry& e = m_zeilen.at(i).eintrag;
        QString tip = QStringLiteral("%1 %2").arg(BandStackStore::displayFrequency(e.frequencyHz),
                                                 e.mode);
        if (!e.name.isEmpty()) {
            tip += QStringLiteral(" — ") + e.name;
        }
        setToolTip(tip);
    } else {
        setToolTip(QString());
    }
    QWidget::mouseMoveEvent(event);
}

void BandStackListe::leaveEvent(QEvent* event)
{
    m_schwebt = -1;
    update();
    QWidget::leaveEvent(event);
}

// ═════════════════════════════════════════════════════════════════════
// BandStackApplet
// ═════════════════════════════════════════════════════════════════════

BandStackApplet::BandStackApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    setObjectName(QStringLiteral("BandStackApplet"));
    auto* box = new QVBoxLayout(this);
    box->setContentsMargins(4, 6, 4, 6);
    box->setSpacing(6);

    // Hausstil-Regel 6: ein Modul erklaert sich in zwei Zeilen.
    m_erklaerung = new QLabel(this);
    m_erklaerung->setWordWrap(true);
    m_erklaerung->setStyleSheet(QStringLiteral("QLabel { color: %1; padding: 0 8px; }")
                                    .arg(Style::role("text-scale", Style::kTextScale)));
    {
        QFont f = font();
        f.setPixelSize(Style::kFontSmall);
        m_erklaerung->setFont(f);
    }
    box->addWidget(m_erklaerung);

    m_liste = new BandStackListe(this);
    box->addWidget(m_liste);
    box->addStretch(1);

    connect(m_liste, &BandStackListe::zeileGeklickt, this, &BandStackApplet::zeileGeklickt);
    connect(m_liste, &BandStackListe::hinzufuegenGeklickt, this, &BandStackApplet::hinzufuegen);
    connect(m_liste, &BandStackListe::umbenennenGewuenscht, this, &BandStackApplet::umbenennen);
    connect(m_liste, &BandStackListe::zeilenMenue, this,
            [this](const QString& id, const QPoint& global) {
        QMenu* menue = baueZeilenMenue(id, this);
        menue->setAttribute(Qt::WA_DeleteOnClose);
        menue->popup(global);
    });
    connect(m_liste, &BandStackListe::bandMenue, this, [this](Band band, const QPoint& global) {
        QMenu* menue = baueBandMenue(band, this);
        menue->setAttribute(Qt::WA_DeleteOnClose);
        menue->popup(global);
    });

    initReceiverBinding();
    connect(this, &AppletWidget::boundSliceChanged, this, [this](SliceModel*) { auffrischen(); });
    if (BandStackController* s = stapel()) {
        connect(s, &BandStackController::changed, this, &BandStackApplet::auffrischen);
    }
    if (model) {
        connect(model, &RadioModel::sliceAdded, this, [this](int) { auffrischen(); });
        connect(model, &RadioModel::sliceRemoved, this, [this](int) { auffrischen(); });
    }
    auffrischen();
}

BandStackController* BandStackApplet::stapel() const
{
    return m_model ? m_model->bandStack() : nullptr;
}

int BandStackApplet::sliceId() const
{
    const SliceModel* s = boundSlice();
    return s ? s->sliceIndex() : -1;
}

bool BandStackApplet::markenImSpektrum()
{
    return AppSettings::instance().value(kMarkenSchluessel, QStringLiteral("True")).toString()
           == QLatin1String("True");
}

void BandStackApplet::setzeMarkenImSpektrum(bool an)
{
    AppSettings::instance().setValue(kMarkenSchluessel,
                                     an ? QStringLiteral("True") : QStringLiteral("False"));
}

QWidget* BandStackApplet::kopfleistenWidget()
{
    if (!m_stand) {
        // Hausstil-Regel 5: der Zustand als umrandete Kapsel rechts oben.
        m_stand = new QLabel(this);
        m_stand->setObjectName(QStringLiteral("bandStackStand"));
        m_stand->setAlignment(Qt::AlignCenter);
        m_stand->setMinimumWidth(64);
        m_stand->setFixedHeight(18);
        m_stand->setFont(Style::monoFont(font(), Style::kFontSmall));
        m_stand->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: %2; border: 1px solid %3;"
            " border-radius: 9px; padding: 0px 8px; }")
            .arg(QLatin1String(Style::kTextPrimary), Style::role("app-bg", Style::kAppBg),
                 QLatin1String(Style::kGlassSelBorder)));
        m_stand->setToolTip(QStringLiteral(
            "Band, your place in its stack and how many entries it has. "
            "\"–\" means you tuned here by hand: leaving saves nothing."));
        auffrischen();
    }
    return m_stand;
}

void BandStackApplet::auffrischen()
{
    BandStackController* s = stapel();
    const int id = sliceId();
    const bool bereit = s && s->isReady() && id >= 0;
    m_liste->setzeStapel(s, id);
    m_liste->setVisible(bereit);

    QString stand;
    if (bereit) {
        stand = s->positionText(id);
        stand.replace(QStringLiteral(" -/"), QStringLiteral(" –/"));
    }
    if (m_stand) {
        m_stand->setText(stand);
        m_stand->setVisible(!stand.isEmpty());
    }

    if (!bereit) {
        m_erklaerung->setText(s ? s->unavailableReason()
                                : QStringLiteral("Connect to a radio to use band stacks."));
    } else if (s->shownPlace(id, s->stackBandFor(id)).isEmpty()
               && stand.contains(QStringLiteral("–/"))) {
        // Keine Stelle auf diesem Band: von einem anderen Band her von Hand
        // hineingestimmt, oder die eigene Stelle wurde geloescht. Am
        // Geraet am 2026-11-11 gesehen: nach „Delete" stand hier
        // „Tuned here by hand" -- die Folge stimmte, der Grund nicht.
        m_erklaerung->setText(QStringLiteral("No saved spot here: leaving %1 saves nothing.")
                                  .arg(bandLabel(s->stackBandFor(id))));
    } else {
        m_erklaerung->setText(
            QStringLiteral("Saved spots per band. Press the band again for the next."));
    }
    updateGeometry();
}

bool BandStackApplet::eventFilter(QObject* watched, QEvent* event)
{
    // Escape verwirft den neuen Namen; editingFinished kommt beim
    // Verstecken zwar noch, findet das Feld dann aber unsichtbar.
    if (watched == m_namensfeld && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        m_umbenennenId.clear();
        m_namensfeld->hide();
        return true;
    }
    return AppletWidget::eventFilter(watched, event);
}

void BandStackApplet::melde(bool ok, const QString& grund)
{
    if (!ok && !grund.isEmpty()) {
        emit hinweis(grund, true);
    }
}

void BandStackApplet::zeileGeklickt(const QString& id)
{
    BandStackController* s = stapel();
    if (!s) { return; }
    QString grund;
    melde(s->goTo(sliceId(), id, {}, &grund), grund);
}

void BandStackApplet::hinzufuegen()
{
    BandStackController* s = stapel();
    if (!s) { return; }
    QString grund;
    const bool ok = s->add(sliceId(), {}, &grund);
    melde(ok, grund);
    if (ok) {
        emit hinweis(QStringLiteral("Band stack: added %1").arg(s->positionText(sliceId())), false);
    }
}

void BandStackApplet::umbenennen(const QString& id, const QRect& nameRect)
{
    BandStackController* s = stapel();
    const BandStackEntry* e = s ? s->store().entry(id) : nullptr;
    if (!e) { return; }
    if (!m_namensfeld) {
        m_namensfeld = new QLineEdit(m_liste);
        m_namensfeld->setObjectName(QStringLiteral("bandStackName"));
        m_namensfeld->setStyleSheet(Style::glassFieldStyle());
        m_namensfeld->setPlaceholderText(QStringLiteral("Name"));
        m_namensfeld->installEventFilter(this);
        connect(m_namensfeld, &QLineEdit::editingFinished, this, [this] {
            if (!m_namensfeld->isVisible()) { return; }
            const QString name = m_namensfeld->text().trimmed();
            const QString id = m_umbenennenId;
            m_namensfeld->hide();
            m_umbenennenId.clear();
            BandStackController* s = stapel();
            const BandStackEntry* e = s ? s->store().entry(id) : nullptr;
            if (e && e->name != name) {
                QString grund;
                melde(s->edit(id, name, std::nullopt, &grund), grund);
            }
        });
    }
    m_umbenennenId = id;
    m_namensfeld->setText(e->name);
    m_namensfeld->setGeometry(nameRect);
    m_namensfeld->show();
    m_namensfeld->raise();
    m_namensfeld->setFocus();
    m_namensfeld->selectAll();
}

QMenu* BandStackApplet::baueZeilenMenue(const QString& id, QWidget* parent)
{
    auto* menue = new QMenu(parent);
    BandStackController* s = stapel();
    const BandStackEntry* e = s ? s->store().entry(id) : nullptr;
    if (!e) {
        menue->addAction(QStringLiteral("This entry no longer exists"))->setEnabled(false);
        return menue;
    }
    const QString eintragId = id;
    const Band band = e->band;
    menue->addAction(QStringLiteral("Go there"), this, [this, eintragId] { zeileGeklickt(eintragId); });
    QAction* hier = menue->addAction(QStringLiteral("Save current spot here"), this, [this, eintragId] {
        BandStackController* s = stapel();
        if (!s) { return; }
        QString grund;
        melde(s->saveHere(sliceId(), eintragId, {}, &grund), grund);
    });
    hier->setEnabled(!e->locked && s->stackBandFor(sliceId()) == band);
    if (e->locked) {
        hier->setToolTip(QStringLiteral("The entry is locked."));
    }
    menue->addAction(QStringLiteral("Rename"), this, [this, eintragId] {
        const QRect r = m_liste->namensfeldFuer(eintragId);
        if (r.isValid()) {
            umbenennen(eintragId, r);
        }
    });
    menue->addAction(e->locked ? QStringLiteral("Unlock") : QStringLiteral("Lock"), this,
                     [this, eintragId, gesperrt = e->locked] {
        BandStackController* s = stapel();
        if (!s) { return; }
        QString grund;
        melde(s->edit(eintragId, std::nullopt, !gesperrt, &grund), grund);
    });
    QAction* immer = menue->addAction(QStringLiteral("Always return here"));
    immer->setCheckable(true);
    immer->setChecked(s->store().specificId(band) == id);
    connect(immer, &QAction::toggled, this, [this, eintragId, band](bool an) {
        BandStackController* s = stapel();
        if (!s) { return; }
        QString grund;
        melde(s->setReturn(band, an ? BandStackReturnMode::Specific : BandStackReturnMode::Current,
                           an ? eintragId : QString(), &grund),
              grund);
    });
    menue->addSeparator();
    menue->addAction(QStringLiteral("Delete"), this, [this, eintragId] {
        BandStackController* s = stapel();
        if (!s) { return; }
        QString grund;
        melde(s->remove(eintragId, &grund), grund);
    });
    return menue;
}

QMenu* BandStackApplet::baueBandMenue(Band band, QWidget* parent)
{
    auto* menue = new QMenu(parent);
    BandStackController* s = stapel();
    if (!s) {
        return menue;
    }
    QMenu* zurueck = menue->addMenu(QStringLiteral("Return to"));
    auto* gruppe = new QActionGroup(zurueck);
    const QString art = s->store().toJson().value(QStringLiteral("bands")).toObject()
                            .value(bandKeyName(band)).toObject()
                            .value(QStringLiteral("returnMode")).toString();
    const QString specific = s->store().specificId(band);
    const auto wahl = [&](const QString& text, BandStackReturnMode mode, bool an) {
        QAction* a = zurueck->addAction(text);
        a->setCheckable(true);
        a->setChecked(an);
        gruppe->addAction(a);
        connect(a, &QAction::triggered, this, [this, band, mode, specific] {
            BandStackController* s = stapel();
            if (!s) { return; }
            QString grund;
            melde(s->setReturn(band, mode,
                               mode == BandStackReturnMode::Specific ? specific : QString(),
                               &grund),
                  grund);
        });
        return a;
    };
    wahl(QStringLiteral("Current spot"), BandStackReturnMode::Current,
         specific.isEmpty() && art != QLatin1String("lastUsed"));
    wahl(QStringLiteral("Last frequency used"), BandStackReturnMode::LastUsed,
         specific.isEmpty() && art == QLatin1String("lastUsed"));
    QAction* fest = wahl(QStringLiteral("The entry marked \"Always return here\""),
                         BandStackReturnMode::Specific, !specific.isEmpty());
    fest->setEnabled(!specific.isEmpty());

    menue->addAction(m_liste->istOffen(band) ? QStringLiteral("Collapse")
                                             : QStringLiteral("Expand"),
                     this, [this, band] {
        m_liste->setzeOffen(band, !m_liste->istOffen(band));
        m_liste->neuAufbauen();
    });
    menue->addSeparator();
    menue->addAction(QStringLiteral("Clear unlocked entries on %1…").arg(bandLabel(band)), this,
                     [this, band] {
        const auto antwort = QMessageBox::question(
            this, QStringLiteral("Band Stack"),
            QStringLiteral("Remove every unlocked entry on %1? Locked entries stay.")
                .arg(bandLabel(band)));
        if (antwort != QMessageBox::Yes) { return; }
        if (BandStackController* s = stapel()) {
            s->clear(band, nullptr);
        }
    });
    return menue;
}

QMenu* BandStackApplet::baueOptionenMenue(QWidget* parent)
{
    auto* menue = new QMenu(parent);
    BandStackController* s = stapel();

    QAction* marken = menue->addAction(QStringLiteral("Show on the spectrum"));
    marken->setCheckable(true);
    marken->setChecked(markenImSpektrum());
    marken->setToolTip(QStringLiteral("A small mark on the frequency scale for every entry."));
    connect(marken, &QAction::toggled, this, [this](bool an) {
        setzeMarkenImSpektrum(an);
        emit markenSchalterGeaendert(an);
    });

    if (s && s->isReady()) {
        const BandStackOptions o = s->store().options();
        QAction* dupes = menue->addAction(QStringLiteral("Don't save over a spot already here"));
        dupes->setCheckable(true);
        dupes->setChecked(o.dupeProtection);
        dupes->setToolTip(QStringLiteral("Two entries within 100 Hz are the same spot."));
        connect(dupes, &QAction::toggled, this, [this](bool an) {
            if (BandStackController* s = stapel()) {
                QString grund;
                melde(s->options(an, std::nullopt, std::nullopt, &grund), grund);
            }
        });

        QMenu* dwell = menue->addMenu(QStringLiteral("Auto-save after"));
        auto* dwellGruppe = new QActionGroup(dwell);
        for (const int sek : {0, 10, 30, 60}) {
            QAction* a = dwell->addAction(sek == 0 ? QStringLiteral("Off")
                                                   : QStringLiteral("%1 s on a spot").arg(sek));
            a->setCheckable(true);
            a->setChecked(o.dwellSeconds == sek);
            dwellGruppe->addAction(a);
            connect(a, &QAction::triggered, this, [this, sek] {
                if (BandStackController* s = stapel()) {
                    QString grund;
                    melde(s->options(std::nullopt, std::nullopt, sek, &grund), grund);
                }
            });
        }

        QMenu* verfall = menue->addMenu(QStringLiteral("Auto-expiry"));
        auto* verfallGruppe = new QActionGroup(verfall);
        for (const int min : {0, 5, 15, 30, 60}) {
            QAction* a = verfall->addAction(min == 0 ? QStringLiteral("Off")
                                                     : QStringLiteral("After %1 min").arg(min));
            a->setCheckable(true);
            a->setChecked(o.expiryMinutes == min);
            verfallGruppe->addAction(a);
            connect(a, &QAction::triggered, this, [this, min] {
                if (BandStackController* s = stapel()) {
                    QString grund;
                    melde(s->options(std::nullopt, min, std::nullopt, &grund), grund);
                }
            });
        }

        menue->addSeparator();
        menue->addAction(QStringLiteral("Reset to region defaults…"), this, [this] {
            BandStackController* s = stapel();
            if (!s) { return; }
            const auto antwort = QMessageBox::question(
                this, QStringLiteral("Band Stack"),
                QStringLiteral("Replace the unlocked entries with the defaults for %1? "
                               "Your locked entries stay.")
                    .arg(s->store().region()));
            if (antwort == QMessageBox::Yes) {
                s->reseed(nullptr);
            }
        });
        menue->addAction(QStringLiteral("Clear unlocked entries…"), this, [this] {
            const auto antwort = QMessageBox::question(
                this, QStringLiteral("Band Stack"),
                QStringLiteral("Remove every unlocked entry on every band? Locked entries stay."));
            if (antwort != QMessageBox::Yes) { return; }
            if (BandStackController* s = stapel()) {
                s->clear(std::nullopt, nullptr);
            }
        });
    }
    return menue;
}

void BandStackApplet::openExtendedSettings()
{
    QMenu* menue = baueOptionenMenue(this);
    menue->setAttribute(Qt::WA_DeleteOnClose);
    menue->popup(QCursor::pos());
}

} // namespace Longpath
