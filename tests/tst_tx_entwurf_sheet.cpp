// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: rendert drei Entwuerfe fuer das TX-Feld
// in wirklicher Groesse, je Entwurf ein eigenes Blatt, je Blatt drei
// Betriebsfaelle (Ruhe / Senden gut / Senden mit hohem SWR).
//
// Anlass, 2026-09-02: "wie können wir das TX Widget besser gestalten".
// Das gebaute Feld stapelt vierzehn Zeilen in einer Spalte und braucht
// dafuer rund 420 Punkt Hoehe -- im angedockten Zustand sind 154 da,
// der Rest liegt hinter einem Rollbalken. Die Blaetter zeigen drei
// Wege heraus.

#include <QtTest>
#include <functional>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "gui/HGauge.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

namespace {

// Die gemessene Groesse aus dem Bildschirmfoto vom 2026-09-02:
// das TX-Feld ist im angedockten Zustand 520 x 172 Punkt gross,
// davon 18 Kopfleiste. Alle Entwuerfe muessen da hinein -- ohne
// Rollbalken.
constexpr int kPanelW = 520;
constexpr int kTitleH = 18;
constexpr int kBodyH  = 154;

enum class Fall { Ruhe, Senden, Hoch };

// ── Ein Balken: Beschriftung links, Mulde, Zahl rechts ──────────────
// Ersetzt in Entwurf B und C die beiden HGauge-Streifen. Der
// Unterschied ist nicht die Zeichnung, sondern dass die ZAHL dabei
// steht: "38 W" liest man im Vorbeigehen, eine Fuellhoehe nicht.
class Balken : public QWidget {
public:
    Balken(QString name, QWidget* parent = nullptr)
        : QWidget(parent), m_name(std::move(name)) { setFixedHeight(24); }

    void setzen(double anteil, QString zahl, bool warnung)
    { m_anteil = anteil; m_zahl = std::move(zahl); m_warn = warnung; update(); }

    void setNameBreite(int w) { m_nameW = w; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int h = height();

        QFont f = font();
        f.setPixelSize(9);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        p.drawText(QRect(0, 0, m_nameW, h), Qt::AlignLeft | Qt::AlignVCenter, m_name);

        const int zahlW = 54;
        const int x = m_nameW + 4;
        const int w = width() - x - zahlW - 4;
        const int barH = 8;
        const int y = (h - barH) / 2;

        p.setPen(QColor(Style::kBorder));
        p.setBrush(QColor(Style::kInsetBg));
        p.drawRoundedRect(x, y, w, barH, 2, 2);

        const int fuell = int(qBound(0.0, m_anteil, 1.0) * (w - 2));
        if (fuell > 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(m_warn ? Style::kGaugeDanger : Style::kAmberDim));
            p.drawRoundedRect(x + 1, y + 1, fuell, barH - 2, 1, 1);
        }

        f.setPixelSize(13);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(m_warn ? Style::kGaugeDanger
                               : (m_zahl == QStringLiteral("—") ? Style::kTextInactive
                                                                : Style::kTextPrimary)));
        p.drawText(QRect(width() - zahlW, 0, zahlW - 2, h),
                   Qt::AlignRight | Qt::AlignVCenter, m_zahl);
    }

private:
    QString m_name, m_zahl{QStringLiteral("—")};
    double  m_anteil = 0.0;
    bool    m_warn = false;
    int     m_nameW = 52;
};

// ── Die grosse Ziffer fuer Entwurf C ────────────────────────────────
class Ziffernblock : public QWidget {
public:
    explicit Ziffernblock(QWidget* parent = nullptr) : QWidget(parent) {}

    void setzen(QString watt, double wAnteil, QString swr, double sAnteil, bool warn)
    { m_w = std::move(watt); m_wa = wAnteil; m_s = std::move(swr);
      m_sa = sAnteil; m_warn = warn; update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width();

        QFont f = font();
        // Vorlauf: die eine Zahl, die waehrend des Sendens zaehlt.
        const bool leer = (m_w == QStringLiteral("—"));
        f.setPixelSize(leer ? 22 : 38);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(leer ? Style::kTextInactive : Style::kTextPrimary));
        const QRect wattR(0, -2, w - 26, 42);
        p.drawText(wattR, Qt::AlignRight | Qt::AlignVCenter, m_w);

        f.setPixelSize(11);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        p.drawText(QRect(w - 24, 12, 24, 24), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("W"));
        p.drawText(QRect(0, 0, 60, 16), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("VORLAUF"));

        // Haarbalken darunter -- Feinzeichnung, keine zweite Anzeige.
        strich(p, 44, m_wa, false);

        f.setPixelSize(leer ? 15 : 22);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(m_warn ? Style::kGaugeDanger
                               : (leer ? Style::kTextInactive : Style::kTextPrimary)));
        p.drawText(QRect(0, 50, w - 4, 26), Qt::AlignRight | Qt::AlignVCenter, m_s);

        f.setPixelSize(9);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        p.drawText(QRect(0, 50, 60, 26), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("SWR"));

        strich(p, 78, m_sa, m_warn);
    }

private:
    void strich(QPainter& p, int y, double anteil, bool warn)
    {
        const int w = width();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Style::kGroove));
        p.drawRect(0, y, w, 3);
        const int fuell = int(qBound(0.0, anteil, 1.0) * w);
        if (fuell > 0) {
            p.setBrush(QColor(warn ? Style::kGaugeDanger : Style::kAmberDim));
            p.drawRect(0, y, fuell, 3);
        }
    }

    QString m_w{QStringLiteral("—")}, m_s{QStringLiteral("—")};
    double m_wa = 0.0, m_sa = 0.0;
    bool m_warn = false;
};

// ── Bausteine im Hausstil ───────────────────────────────────────────

// Eingerastet, aber leise: dunkler Grund, Akzentrand, heller Text.
// Der Hausstil laesst genau EINE kraeftige Stelle im Feld zu, und die
// gehoert MOX. Alles andere zeigt seinen Zustand ueber den Rand.
QString stillStil()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2;"
        " border: 1px solid %3; }")
        .arg(Style::kBadgeInfoBg, Style::kTextPrimary, Style::kAccent);
}

QPushButton* knopf(QWidget* p, const QString& t, int h, bool an,
                   const QString& extra = QString())
{
    auto* b = new QPushButton(t, p);
    b->setCheckable(true);
    b->setChecked(an);
    b->setFixedHeight(h);
    b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    b->setStyleSheet(Style::buttonBaseStyle()
                     + (extra.isEmpty() ? stillStil() : extra));
    return b;
}

void festeBreite(QPushButton* b, int w)
{
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    b->setFixedWidth(w);
}

QPushButton* chip(QWidget* p, const QString& t, bool an)
{
    auto* b = knopf(p, t, 20, an);
    b->setStyleSheet(Style::buttonBaseStyle()
                     + QStringLiteral("QPushButton { font-size: 10px; padding: 1px 4px; }")
                     + stillStil());
    return b;
}

QLabel* wert(QWidget* p, const QString& t, int w)
{
    auto* l = new QLabel(t, p);
    l->setFixedWidth(w);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    l->setStyleSheet(Style::insetValueStyle());
    return l;
}

QHBoxLayout* reglerZeile(QWidget* p, const QString& name, int prozent,
                         const QString& zahl, int nameW = 40, int wertW = 42)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* l = new QLabel(name, p);
    l->setFixedWidth(nameW);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }")
                         .arg(Style::kTextScale));
    row->addWidget(l);
    auto* s = new QSlider(Qt::Horizontal, p);
    s->setRange(0, 100);
    s->setValue(prozent);
    s->setFixedHeight(16);
    s->setStyleSheet(Style::sliderHStyle());
    row->addWidget(s, 1);
    row->addWidget(wert(p, zahl, wertW));
    return row;
}

// MOX ist die einzige Stelle im Feld, die kraeftig sein darf
// (HAUSSTIL: "nur MOX/TX darf kraeftiger, #c25a5c").
// ── Die Toene fuer die Sendetaste ───────────────────────────────────
//
// Der Hausstil setzt fuer "sendet" Rot (#c25a5c). Der Einwand vom
// 2026-09-02 -- "das rot finde ich nicht sehr schoen" -- trifft aber
// einen wunden Punkt, der nicht nur Geschmack ist: der sendende MOX
// und die SWR-Warnung tragen heute DIESELBE Farbfamilie. Nimmt man
// MOX aus dem Rot heraus, bedeutet Rot im Feld wieder genau eine
// Sache: es stimmt etwas nicht.
enum class Ton { Rot, Messing, Bernstein, Kupfer, Glimmen, Kontur, Stahl, Lampe };

QString moxStil(Ton t = Ton::Rot)
{
    switch (t) {
    case Ton::Rot:
        return QStringLiteral(
            "QPushButton:checked { background: %1; color: #ffffff;"
            " border: 1px solid %2; }").arg(Style::kTxRed, Style::kRedBorder);

    case Ton::Messing:
        // Der Panelakzent des Hausstils, aber als Flaeche. Dunkle
        // Schrift darauf -- eine gravierte Taste, kein Leuchtschild.
        return QStringLiteral(
            "QPushButton:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            " stop:0 #a87a3c, stop:1 #8a6a30); color: #17130a;"
            " border: 1px solid #c2924f; }");

    case Ton::Bernstein:
        // Eine Stufe heller und lauter als Messing.
        return QStringLiteral(
            "QPushButton:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            " stop:0 #d8a55f, stop:1 #b8873f); color: #1b1408;"
            " border: 1px solid #e8c48a; }");

    case Ton::Kupfer:
        // Die Bruecke zum bisherigen Rot: warm und kraeftig, aber
        // erdig statt alarmierend.
        return QStringLiteral(
            "QPushButton:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            " stop:0 #a3653f, stop:1 #834e31); color: #fbeee4;"
            " border: 1px solid #c08055; }");

    case Ton::Glimmen:
        // Das Instrumentenglimmen aus HAUSSTIL.md, auf einen Knopf
        // gelegt: Licht von innen, heller Ring. Die Taste geht an wie
        // eine Roehre, nicht wie eine Warnlampe.
        return QStringLiteral(
            "QPushButton:checked { background: qradialgradient(cx:0.5, cy:0.55,"
            " radius:0.85, fx:0.5, fy:0.55, stop:0 #8a6f3a, stop:0.5 #4a3f28,"
            " stop:1 #241f18); color: #f7f2e6;"
            " border: 1px solid #c2924f; }");

    case Ton::Kontur:
        // Am sparsamsten: die Flaeche bleibt dunkel, nur Ring und
        // Schrift gehen an. Haelt die Zwei-Prozent-Regel am saubersten
        // ein -- und ist am leisesten, was bei MOX auch ein Risiko ist.
        return QStringLiteral(
            "QPushButton:checked { background: #1d1a12; color: #d8a55f;"
            " border: 2px solid #c2924f; }");

    case Ton::Stahl:
        // Die Akzentfamilie als Flaeche. Ruhig und teuer aussehend --
        // aber Blau heisst im Hausstil "anfassbar", nicht "aktiv".
        return QStringLiteral(
            "QPushButton:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            " stop:0 #3d6d9c, stop:1 #2c5075); color: #eaf2fa;"
            " border: 1px solid #6a9dc8; }");

    case Ton::Lampe:
        // Die Flaeche bleibt neutral, links steht ein Leuchtstreifen --
        // dasselbe Mittel wie der 3-px-Panelakzent im Kopf.
        return QStringLiteral(
            "QPushButton:checked { background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            " stop:0 #c2924f, stop:0.045 #c2924f, stop:0.05 #26262b,"
            " stop:1 #1e1e22); color: #e8d3ae;"
            " border: 1px solid #3a3a41; }");
    }
    return {};
}

QWidget* rahmen(QWidget* body, const QString& titel)
{
    auto* panel = new QWidget;
    panel->setFixedSize(kPanelW, kTitleH + kBodyH);
    panel->setObjectName(QStringLiteral("Entwurfsrahmen"));
    panel->setStyleSheet(QStringLiteral("#Entwurfsrahmen { background: %1; }")
                             .arg(Style::kPanelBg));
    auto* v = new QVBoxLayout(panel);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* bar = new QWidget(panel);
    bar->setFixedHeight(kTitleH);
    bar->setStyleSheet(Style::titleBarStyle());
    auto* hb = new QHBoxLayout(bar);
    hb->setContentsMargins(4, 0, 6, 0);
    hb->setSpacing(6);
    auto* grip = new QLabel(QStringLiteral("⋮⋮"), bar);
    grip->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px;"
                                       " background: transparent; }").arg(Style::kTextScale));
    hb->addWidget(grip);
    auto* lab = new QLabel(titel, bar);
    lab->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px;"
                                      " font-weight: bold; background: transparent; }")
                           .arg(Style::kTitleText));
    hb->addWidget(lab);
    hb->addStretch();
    // Was heute eine eigene Zeile im Feld belegt, gehoert hier hin:
    // Mikrofonquelle und Profil sind Zustand, kein Bedienelement.
    auto* nb = new QLabel(QStringLiteral("PC mic  ·  Default"), bar);
    nb->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px;"
                                     " background: transparent; }").arg(Style::kTextScale));
    hb->addWidget(nb);
    v->addWidget(bar);
    v->addWidget(body, 1);
    return panel;
}

// ════════════════════════════════════════════════════════════════════
// Entwurf A — Zwei Spalten. Nichts faellt weg, nur umgebrochen.
// ════════════════════════════════════════════════════════════════════
QWidget* entwurfA(Fall fall)
{
    const bool tx = fall != Fall::Ruhe;
    const bool warn = fall == Fall::Hoch;

    auto* body = new QWidget;
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* h = new QHBoxLayout(body);
    h->setContentsMargins(6, 4, 6, 4);
    h->setSpacing(8);

    auto* links = new QVBoxLayout;
    links->setSpacing(3);
    auto* fwd = new HGauge(body);
    fwd->setTitle(QStringLiteral("RF Pwr"));
    fwd->setRange(0, 15);
    fwd->setYellowStart(11);
    fwd->setRedStart(13);
    fwd->setValue(tx ? 3.4 : 0.0);
    links->addWidget(fwd);
    auto* swr = new HGauge(body);
    swr->setTitle(QStringLiteral("SWR"));
    swr->setRange(1.0, 3.0);
    swr->setYellowStart(2.0);
    swr->setRedStart(2.5);
    swr->setValue(tx ? (warn ? 2.8 : 1.4) : 1.0);
    swr->setTickLabels({QStringLiteral("1"), QStringLiteral("1.5"),
                        QStringLiteral("2.5"), QStringLiteral("3")});
    links->addWidget(swr);
    links->addLayout(reglerZeile(body, QStringLiteral("RF"), 25, QStringLiteral("3 W")));
    links->addLayout(reglerZeile(body, QStringLiteral("Tune"), 10, QStringLiteral("1 W")));
    links->addLayout(reglerZeile(body, QStringLiteral("Mon"), 50, QStringLiteral("50")));
    links->addStretch();
    h->addLayout(links, 1);

    auto* strich = new QWidget(body);
    strich->setFixedWidth(1);
    strich->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kBorderSubtle));
    h->addWidget(strich);

    auto* rechts = new QVBoxLayout;
    rechts->setSpacing(3);
    auto* z1 = new QHBoxLayout;
    z1->setSpacing(3);
    z1->addWidget(knopf(body, QStringLiteral("TUNE"), 26, false, moxStil()), 1);
    z1->addWidget(knopf(body, QStringLiteral("MOX"), 26, tx, moxStil()), 1);
    rechts->addLayout(z1);

    auto* z2 = new QHBoxLayout;
    z2->setSpacing(3);
    for (auto&& p : {std::pair{QStringLiteral("VOX"), true},
                     std::pair{QStringLiteral("MON"), false},
                     std::pair{QStringLiteral("LEV"), true},
                     std::pair{QStringLiteral("EQ"), false},
                     std::pair{QStringLiteral("CFC"), false}}) {
        z2->addWidget(chip(body, p.first, p.second), 1);
    }
    rechts->addLayout(z2);
    rechts->addLayout(reglerZeile(body, QStringLiteral("VOX"), 60,
                                  QStringLiteral("\u221220 dB"), 28, 52));

    auto* z4 = new QHBoxLayout;
    z4->setSpacing(3);
    auto* bw = new QLabel(QStringLiteral("TX BW"), body);
    bw->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }")
                          .arg(Style::kTextScale));
    z4->addWidget(bw);
    for (int v : {100, 2900}) {
        auto* sp = new QSpinBox(body);
        sp->setRange(0, 10000);
        sp->setValue(v);
        sp->setSuffix(QStringLiteral(" Hz"));
        sp->setFixedHeight(20);
        sp->setStyleSheet(Style::spinBoxStyle());
        z4->addWidget(sp, 1);
    }
    rechts->addLayout(z4);

    auto* z5 = new QHBoxLayout;
    z5->setSpacing(3);
    z5->addWidget(chip(body, QStringLiteral("2-Tone"), false), 1);
    z5->addWidget(chip(body, QStringLiteral("PS-A"), false), 1);
    auto* prot = new QLabel(QStringLiteral("SWR Prot"), body);
    prot->setFixedWidth(64);
    prot->setAlignment(Qt::AlignCenter);
    prot->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 9px; font-weight: bold; background: %1;"
        " border: 1px solid %2; border-radius: 6px; color: %3; }")
        .arg(warn ? QLatin1String(Style::kBadgeTxBg) : QLatin1String(Style::kInsetBg),
             warn ? QLatin1String(Style::kRedBorder) : QLatin1String(Style::kInsetBorder),
             warn ? QLatin1String(Style::kTxRed) : QLatin1String(Style::kTextInactive)));
    z5->addWidget(prot);
    rechts->addLayout(z5);
    rechts->addStretch();
    h->addLayout(rechts, 1);

    return rahmen(body, QStringLiteral("TX"));
}

// ════════════════════════════════════════════════════════════════════
// Entwurf B — Sendeleiste. Ein Messblock, ein grosser MOX,
// eine einzige Reihe gleicher Schalter, Rest hinter dem Zahnrad.
// ════════════════════════════════════════════════════════════════════
QWidget* entwurfB(Fall fall, Ton ton)
{
    const bool tx = fall != Fall::Ruhe;
    const bool warn = fall == Fall::Hoch;

    auto* body = new QWidget;
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(6, 5, 6, 5);
    v->setSpacing(5);

    auto* oben = new QHBoxLayout;
    oben->setSpacing(8);

    auto* mess = new QVBoxLayout;
    mess->setSpacing(2);
    auto* b1 = new Balken(QStringLiteral("VORLAUF"), body);
    b1->setzen(tx ? 3.4 / 15.0 : 0.0,
               tx ? QStringLiteral("3,4 W") : QStringLiteral("—"), false);
    mess->addWidget(b1);
    auto* b2 = new Balken(QStringLiteral("SWR"), body);
    b2->setzen(tx ? (warn ? 0.9 : 0.2) : 0.0,
               tx ? (warn ? QStringLiteral("2,8") : QStringLiteral("1,4"))
                  : QStringLiteral("—"), warn);
    mess->addWidget(b2);
    mess->addLayout(reglerZeile(body, QStringLiteral("Leistung"), 25,
                                QStringLiteral("3 W"), 50));
    mess->addLayout(reglerZeile(body, QStringLiteral("Tune"), 10,
                                QStringLiteral("1 W"), 50));
    oben->addLayout(mess, 1);

    auto* keys = new QVBoxLayout;
    keys->setSpacing(4);
    auto* mox = knopf(body, QStringLiteral("MOX"), 58, tx, moxStil(ton));
    festeBreite(mox, 104);
    mox->setStyleSheet(mox->styleSheet()
                       + QStringLiteral("QPushButton { font-size: 15px; }"));
    keys->addWidget(mox);
    auto* tune = knopf(body, QStringLiteral("TUNE"), 24, false, moxStil(ton));
    festeBreite(tune, 104);
    keys->addWidget(tune);
    oben->addLayout(keys);
    v->addLayout(oben);

    auto* unten = new QHBoxLayout;
    unten->setSpacing(3);
    for (auto&& p : {std::pair{QStringLiteral("VOX"), true},
                     std::pair{QStringLiteral("MON"), false},
                     std::pair{QStringLiteral("LEV"), true},
                     std::pair{QStringLiteral("EQ"), false},
                     std::pair{QStringLiteral("CFC"), false},
                     std::pair{QStringLiteral("2T"), false},
                     std::pair{QStringLiteral("PS"), false}}) {
        unten->addWidget(chip(body, p.first, p.second), 1);
    }
    auto* zahn = chip(body, QStringLiteral("⚙"), false);
    festeBreite(zahn, 26);
    unten->addWidget(zahn);
    v->addLayout(unten);

    auto* fuss = new QLabel(
        warn ? QStringLiteral("SWR-Schutz aktiv — Leistung gedrosselt")
             : QStringLiteral("TX 100–2900 Hz  ·  2,8 k BW"), body);
    fuss->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; }")
        .arg(warn ? QLatin1String(Style::kTxRed) : QLatin1String(Style::kTextScale)));
    v->addWidget(fuss);
    v->addStretch();

    return rahmen(body, QStringLiteral("TX"));
}

// ════════════════════════════════════════════════════════════════════
// Entwurf C — Instrumententafel. Die Zahl zuerst, alles Einstellbare
// hinter dem Zahnrad. Am wenigsten sichtbare Teile.
// ════════════════════════════════════════════════════════════════════
QWidget* entwurfC(Fall fall)
{
    const bool tx = fall != Fall::Ruhe;
    const bool warn = fall == Fall::Hoch;

    auto* body = new QWidget;
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(8, 6, 8, 6);
    v->setSpacing(6);

    auto* oben = new QHBoxLayout;
    oben->setSpacing(10);
    auto* zb = new Ziffernblock(body);
    zb->setFixedSize(190, 82);
    zb->setzen(tx ? QStringLiteral("3,4") : QStringLiteral("—"),
               tx ? 3.4 / 15.0 : 0.0,
               tx ? (warn ? QStringLiteral("2,8") : QStringLiteral("1,4"))
                  : QStringLiteral("—"),
               tx ? (warn ? 0.9 : 0.2) : 0.0, warn);
    oben->addWidget(zb);

    auto* mox = knopf(body, QStringLiteral("MOX"), 82, tx, moxStil());
    mox->setStyleSheet(mox->styleSheet()
                       + QStringLiteral("QPushButton { font-size: 17px; }"));
    oben->addWidget(mox, 3);
    auto* tune = knopf(body, QStringLiteral("TUNE"), 82, false, moxStil());
    oben->addWidget(tune, 2);
    v->addLayout(oben);

    v->addLayout(reglerZeile(body, QStringLiteral("Leistung"), 25,
                             QStringLiteral("3 W"), 54));

    auto* unten = new QHBoxLayout;
    unten->setSpacing(3);
    for (auto&& p : {std::pair{QStringLiteral("VOX"), true},
                     std::pair{QStringLiteral("MON"), false},
                     std::pair{QStringLiteral("LEV"), true},
                     std::pair{QStringLiteral("EQ"), false},
                     std::pair{QStringLiteral("CFC"), false}}) {
        unten->addWidget(chip(body, p.first, p.second), 1);
    }
    auto* zahn = chip(body, QStringLiteral("⚙ Mehr"), false);
    festeBreite(zahn, 62);
    unten->addWidget(zahn);
    v->addLayout(unten);
    v->addStretch();

    return rahmen(body, QStringLiteral("TX"));
}

// ── Das ECHTE Widget ────────────────────────────────────────────────
//
// Der wichtigste Blatt-Typ ueberhaupt: ein freihand entworfenes
// Panel sieht eingebaut anders aus (echte Schrift, echte Abstaende,
// echte Knopfhoehen). Erst dieses Blatt beweist, dass der Entwurf
// auch gebaut so aussieht.
QWidget* echtesFeld(Fall fall, RadioModel* modell)
{
    auto* applet = new TxApplet(modell);

    const bool tx = fall != Fall::Ruhe;
    const bool warn = fall == Fall::Hoch;

    const QList<HGauge*> anzeigen = applet->findChildren<HGauge*>();
    if (anzeigen.size() >= 2) {
        // Ein QRP-Geraet wie das ANVELINA am Platz des Betreibers:
        // 15-W-Skala statt der 100-W-Voreinstellung.
        anzeigen.at(0)->setRange(0.0, 15.0);
        anzeigen.at(0)->setYellowStart(11.0);
        anzeigen.at(0)->setRedStart(13.0);
        anzeigen.at(0)->setValue(tx ? 3.4 : 0.0);
        anzeigen.at(1)->setValue(tx ? (warn ? 2.8 : 1.4) : 1.0);
    }
    for (QPushButton* b : applet->findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("MOX")) { b->setChecked(tx); }
        if (b->text() == QStringLiteral("VOX")) { b->setChecked(true); }
        if (b->text() == QStringLiteral("LEV")) { b->setChecked(true); }
    }
    for (QLabel* l : applet->findChildren<QLabel*>()) {
        if (l->accessibleName() == QStringLiteral("TX filter status")) {
            l->setText(QStringLiteral("100–2900 Hz · 2.8k BW"));
        }
        if (l->accessibleName() == QStringLiteral("SWR protection indicator")) {
            l->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 9px; font-weight: bold; }")
                .arg(QLatin1String(warn ? Style::kSwrProtActive
                                        : Style::kTextInactive)));
        }
    }
    return rahmen(applet, QStringLiteral("TX"));
}

QImage blatt(const std::function<QWidget*(Fall)>& bauen, const QString& kopf)
{
    const QStringList namen{QStringLiteral("Ruhe — nicht verbunden"),
                            QStringLiteral("Senden, SWR 1,4"),
                            QStringLiteral("Senden, SWR 2,8 — Schutz greift")};
    const Fall faelle[3] = {Fall::Ruhe, Fall::Senden, Fall::Hoch};

    const int rand = 14;
    const int kopfH = 26;
    const int panelH = kTitleH + kBodyH;
    const int zeile = 14 + panelH + 12;

    QImage img(QSize(kPanelW + 2 * rand, kopfH + 3 * zeile + rand) * 2,
               QImage::Format_ARGB32);
    img.setDevicePixelRatio(2.0);
    img.fill(QColor(Style::kAppBg));

    QPainter p(&img);
    QFont f = p.font();
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(Style::kTitleText));
    p.drawText(QRect(rand, 4, kPanelW, kopfH), Qt::AlignLeft | Qt::AlignVCenter, kopf);

    for (int i = 0; i < 3; ++i) {
        const int y = kopfH + i * zeile;
        f.setPixelSize(10);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        p.drawText(QRect(rand, y, kPanelW, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   namen.at(i));

        QWidget* w = bauen(faelle[i]);
        w->setAttribute(Qt::WA_DontShowOnScreen);
        w->show();
        QCoreApplication::processEvents();
        w->render(&p, QPoint(rand, y + 14), QRegion(),
                  QWidget::DrawWindowBackground | QWidget::DrawChildren);
        delete w;
    }
    return img;
}

} // namespace

class TstTxEntwurfSheet : public QObject
{
    Q_OBJECT
private slots:
    void blaetter()
    {
        struct { QWidget* (*f)(Fall); const char* kopf; const char* datei; } e[] = {
            {entwurfA, "Entwurf A — Zwei Spalten (nichts faellt weg)",
             "/tmp/tx_entwurf_A.png"},
            {entwurfC, "Entwurf C — Instrumententafel (die Zahl zuerst)",
             "/tmp/tx_entwurf_C.png"},
        };
        for (auto& x : e) {
            const QImage img = blatt(x.f, QString::fromUtf8(x.kopf));
            QVERIFY2(img.save(QString::fromUtf8(x.datei)), x.datei);
        }
    }

    // Das gebaute Feld, mit denselben drei Betriebsfaellen.
    void gebaut()
    {
        RadioModel modell;
        const QImage img = blatt(
            [&modell](Fall f) { return echtesFeld(f, &modell); },
            QStringLiteral("TX-Feld, GEBAUT — Entwurf B, Ton „Messing“"));
        const QString aus = QStringLiteral("/tmp/tx_gebaut.png");
        QVERIFY2(img.save(aus), qPrintable(aus));
        qInfo().noquote() << "Blatt:" << aus;
    }

    // Entwurf B in sieben Toenen. Je Ton ein eigenes Blatt, weil sich
    // Farbe im Nebeneinander nicht beurteilen laesst -- und je Blatt
    // der SENDE-Fall MIT hohem SWR, damit man sieht, ob die Sendetaste
    // der Warnung ins Gehege kommt.
    void toene()
    {
        struct { Ton t; const char* kopf; const char* datei; } e[] = {
            {Ton::Messing,   "Entwurf B · Ton „Messing“ — gedeckte Flaeche, gravierte Schrift",
             "/tmp/tx_B_1_messing.png"},
            {Ton::Bernstein, "Entwurf B · Ton „Bernstein“ — eine Stufe heller und lauter",
             "/tmp/tx_B_2_bernstein.png"},
            {Ton::Kupfer,    "Entwurf B · Ton „Kupfer“ — warm und kraeftig, aber erdig",
             "/tmp/tx_B_3_kupfer.png"},
            {Ton::Glimmen,   "Entwurf B · Ton „Glimmen“ — Licht von innen, heller Ring",
             "/tmp/tx_B_4_glimmen.png"},
            {Ton::Kontur,    "Entwurf B · Ton „Kontur“ — nur Ring und Schrift gehen an",
             "/tmp/tx_B_5_kontur.png"},
            {Ton::Stahl,     "Entwurf B · Ton „Stahl“ — die Akzentfamilie als Flaeche",
             "/tmp/tx_B_6_stahl.png"},
            {Ton::Lampe,     "Entwurf B · Ton „Lampe“ — neutrale Taste, Leuchtstreifen links",
             "/tmp/tx_B_7_lampe.png"},
            {Ton::Rot,       "Entwurf B · Ton „Rot“ — der Hausstil-Stand zum Vergleich",
             "/tmp/tx_B_8_rot.png"},
        };
        for (auto& x : e) {
            const Ton t = x.t;
            const QImage img = blatt([t](Fall f) { return entwurfB(f, t); },
                                     QString::fromUtf8(x.kopf));
            QVERIFY2(img.save(QString::fromUtf8(x.datei)), x.datei);
            qInfo().noquote() << "Blatt:" << x.datei;
        }
    }
};

QTEST_MAIN(TstTxEntwurfSheet)
#include "tst_tx_entwurf_sheet.moc"
