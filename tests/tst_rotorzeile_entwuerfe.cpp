// no-port-check: Longpath-original test tool, no Thetis logic.
//
// ── Entwurfsblaetter: die Rotorzeile im Logbuch (2026-11-11) ──────────
//
// Betreiber: „weiters sollte alles sehr übersichtlich gestaltet werden."
// Werkzeug, kein Pruefstand: ohne LONGPATH_GRAB_DIR ueberspringt es.
// Je Entwurf EIN Blatt (Martins Regel), darauf alle Betriebsfaelle:
// Rotor steht, dreht, am Ziel, kein Rotor, kurzer Weg gesperrt.
// Gebaut aus den echten Stilen; das schmale Band in Entwurf C ist
// skizziert (BandSkizze).
//
//   QT_SCALE_FACTOR=2 LONGPATH_GRAB_DIR=<ordner> \
//     ctest -R tst_rotorzeile_entwuerfe -V

#include <QtTest/QtTest>
#include <QDir>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPolygonF>
#include <cmath>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimeZone>
#include <limits>
#include <QVBoxLayout>

#include "core/AdifLog.h"
#include "core/RotorBereich.h"
#include "gui/LogbookWindow.h"
#include "gui/StyleConstants.h"
#include "models/LogEntry.h"

using namespace Longpath;

namespace {

enum class Fall { Steht, Dreht, AmZiel, KeinRotor, Gesperrt };

struct Lage {
    QString call;
    int sp;          // kurzer Weg
    int lp;
    int km;
    QString woher;
    bool grob;
};

const Lage kEa8{QStringLiteral("EA8ABC"), 240, 60, 3349, QStringLiteral("country centre"), true};
const Lage kJa{QStringLiteral("JA1XYZ"), 42, 222, 9046, QStringLiteral("country centre"), true};

QString farbe(const char* hex, const QString& extra = QString())
{
    return QStringLiteral("QLabel { color: %1; font-size: 11px; %2 }").arg(QLatin1String(hex), extra);
}

QLabel* klein(const QString& t, const char* hex = Style::kTextScale)
{
    auto* l = new QLabel(t);
    l->setStyleSheet(farbe(hex));
    return l;
}

QLabel* kopf(const QString& t)
{
    auto* l = new QLabel(t);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: %2px; letter-spacing: 2px; }")
                         .arg(QLatin1String(Style::kTextScale)).arg(Style::kFontCaption));
    return l;
}

QPushButton* knopf(const QString& t, bool an = true)
{
    auto* b = new QPushButton(t);
    b->setStyleSheet(Style::buttonBaseStyle());
    b->setEnabled(an);
    return b;
}

QLineEdit* feld(const QString& t, int breite, bool mono = false)
{
    auto* e = new QLineEdit(t);
    e->setStyleSheet(Style::lineEditStyle());
    if (mono) { e->setFont(Style::monoFont(e->font(), 14, QFont::Bold)); }
    if (breite > 0) { e->setFixedWidth(breite); }
    return e;
}

QLabel* chip(const QString& t)
{
    auto* l = new QLabel(t);
    l->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 2px 8px; font-size: 11px; font-weight: bold; }")
        .arg(QLatin1String(Style::kGreenBg), QLatin1String(Style::kGreenText),
             QLatin1String(Style::kGreenBorder)));
    return l;
}

// Was rechts vom Rufzeichen kommt -- in allen Entwuerfen gleich.
void funkUndRapport(QHBoxLayout* h)
{
    auto* live = new QLabel(QStringLiteral("●"));
    live->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QLatin1String(Style::kGreenText)));
    h->addWidget(live);
    auto* f = new QLabel(QStringLiteral("14.205.000"));
    f->setFont(Style::monoFont(f->font(), 14));
    f->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QLatin1String(Style::kTextPrimary)));
    h->addWidget(f);
    h->addWidget(chip(QStringLiteral("USB")));
    h->addWidget(chip(QStringLiteral("20m")));
    h->addWidget(klein(QStringLiteral("from radio")));
    h->addWidget(klein(QStringLiteral("RST S")));
    h->addWidget(feld(QStringLiteral("59"), 52));
    h->addWidget(klein(QStringLiteral("R")));
    h->addWidget(feld(QStringLiteral("59"), 52));
    auto* c = feld(QString(), 0);
    c->setPlaceholderText(QStringLiteral("comment"));
    c->setMinimumWidth(180);
    h->addWidget(c, 1);
    auto* log = knopf(QStringLiteral("Log QSO"));
    log->setStyleSheet(Style::buttonBaseStyle()
        + QString(Style::blueCheckedStyle()).replace(QStringLiteral("QPushButton:checked"),
                                                     QStringLiteral("QPushButton")));
    h->addWidget(log);
}

// Der Rotorzustand als Text und Farbe -- wie LogbookWindow::aktualisiereDrehZeile.
QPair<QString, const char*> zustand(Fall f, const Lage& l)
{
    switch (f) {
    case Fall::Steht:     return {QStringLiteral("rotor at 231°"), Style::kTextScale};
    case Fall::Dreht:     return {QStringLiteral("turning to %1° · now 234°").arg(l.sp), Style::kAmberText};
    case Fall::AmZiel:    return {QStringLiteral("rotor at %1° — on target").arg(l.sp), Style::kGreenText};
    case Fall::KeinRotor: return {QStringLiteral("no rotor connected — the antenna does not turn"), Style::kAmberWarn};
    case Fall::Gesperrt:  return {QStringLiteral("rotor at 231°"), Style::kTextScale};
    }
    return {};
}

const Lage& lageFuer(Fall f) { return f == Fall::Gesperrt ? kJa : kEa8; }

// ── Ist: das echte Logbuchfenster ─────────────────────────────────────
bool landesmitte(const QString& call, double& lat, double& lon)
{
    if (call.startsWith(QStringLiteral("JA"))) { lat = 36.0; lon = 138.0; return true; }
    if (call.startsWith(QStringLiteral("EA8"))) { lat = 28.32; lon = -15.85; return true; }
    return false;
}

QList<QPixmap> istZeilen(int breite)
{
    QList<QPixmap> out;
    QTemporaryDir dir;
    const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
    LogEntry e;
    e.call = QStringLiteral("DL1ABC");
    e.band = QStringLiteral("20m");
    e.mode = QStringLiteral("SSB");
    e.gridSquare = QStringLiteral("JO62");
    e.myGridSquare = QStringLiteral("JN67VV");
    e.timeOn = QDateTime(QDate(2026, 10, 1), QTime(12, 0), QTimeZone::UTC);
    AdifLog::write(pfad, {e});
    RotorBereich b;
    b.an = true; b.vonGrad = 180.0; b.bisGrad = 0.0;
    b.save();
    LogbookWindow w(pfad);
    w.setPositionFallback(&landesmitte);
    w.resize(breite, 900);
    w.show();
    QTest::qWaitForWindowExposed(&w, 20000);
    QWidget* eltern = w.entryCallForTest()->parentWidget();
    const auto greif = [&]() {
        QCoreApplication::processEvents();
        QRect r;
        const QRect call = w.entryCallForTest()->geometry();
        for (QWidget* k : eltern->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            if (!k->isVisible()) { continue; }
            const QRect g = k->geometry();
            if (g.top() < call.top() - 8 || g.top() > call.bottom() + 70) { continue; }
            r = r.united(g);
        }
        // Die Rotorzeile darunter gehoert dazu (seit Entwurf C umgesetzt).
        if (QWidget* rz = w.rotorZeileForTest(); rz && rz->isVisible()) {
            r = r.united(QRect(rz->mapTo(eltern, QPoint(0, 0)), rz->size()));
        }
        r.setLeft(0);
        r.setRight(breite - 1);
        // Aus dem Fenster greifen, nicht aus dem Kind: den dunklen Grund
        // malt das Fenster.
        const QPoint ab = eltern->mapTo(&w, QPoint(0, 0));
        out << w.grab(r.adjusted(0, -8, 0, 8).translated(ab));
    };
    QTest::keyClicks(w.entryCallForTest(), QStringLiteral("EA8ABC"));
    w.entryRotorForTest()->click();
    w.setRotorBearing(231.0);
    greif();                                   // steht
    w.entryRotateForTest()->click();
    w.setRotorBearing(234.0);
    greif();                                   // dreht
    w.setRotorBearing(240.0);
    greif();                                   // am Ziel
    w.setRotorBearing(std::numeric_limits<double>::quiet_NaN());
    w.entryRotateForTest()->click();
    greif();                                   // kein Rotor
    w.setRotorBearing(231.0);
    w.entryCallForTest()->clear();
    QTest::keyClicks(w.entryCallForTest(), QStringLiteral("JA1XYZ"));
    w.entryRotorForTest()->click();
    greif();                                   // gesperrt
    RotorBereich{}.save();
    return out;
}

// ── A: eigene Rotorzeile unter der Eingabe ────────────────────────────
// Die QSO-Zeile bleibt, wie sie ohne Rotor war. Darunter, nur nach
// ROTOR, eine Zeile mit festen Plaetzen: Peilung gross, dann Entfernung
// und Herkunft, die zwei Knoepfe, der Zustand.
QWidget* entwurfA(Fall f)
{
    const Lage& l = lageFuer(f);
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setSpacing(6);
    auto* h = new QHBoxLayout;
    h->setSpacing(6);
    h->addWidget(kopf(QStringLiteral("NEW QSO")));
    h->addWidget(feld(l.call, 170, true));
    h->addWidget(knopf(QStringLiteral("Rotor")));
    funkUndRapport(h);
    v->addLayout(h);

    auto* r = new QHBoxLayout;
    r->setSpacing(10);
    auto* k = kopf(QStringLiteral("ROTOR"));
    k->setFixedWidth(kopf(QStringLiteral("NEW QSO"))->sizeHint().width());
    r->addWidget(k);
    auto* grad = new QLabel(QStringLiteral("%1°").arg(l.sp));
    grad->setFont(Style::monoFont(grad->font(), 14, QFont::Bold));
    grad->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QLatin1String(Style::kTextPrimary)));
    r->addWidget(grad);
    r->addWidget(klein(QStringLiteral("%1 km").arg(l.km), Style::kTextSecondary));
    r->addWidget(klein(QStringLiteral("%1 · rough").arg(l.woher), l.grob ? Style::kAmberWarn : Style::kTextScale));
    r->addSpacing(8);
    auto* rot = knopf(QStringLiteral("Rotate %1°").arg(l.sp), f != Fall::Gesperrt);
    if (f == Fall::Gesperrt) { rot->setText(QStringLiteral("Rotate %1° — blocked").arg(l.sp)); }
    r->addWidget(rot);
    r->addWidget(knopf(QStringLiteral("LP %1°").arg(l.lp)));
    r->addSpacing(8);
    const auto z = zustand(f, l);
    r->addWidget(klein(QStringLiteral("●  ") + z.first, z.second));
    r->addStretch(1);
    r->addWidget(klein(QStringLiteral("Enter logs · Esc clears")));
    v->addLayout(r);
    return w;
}

// ── B: kompakt -- die Peilung steht auf den Knoepfen ──────────────────
// Nach ROTOR wird aus dem einen Knopf zwei (Rotate 240° / LP 60°) und
// ein kleines Zustandsschild daneben. Entfernung und Herkunft wandern in
// die Hinweiszeile darunter (und stehen ohnehin in der Karteikarte).
QWidget* entwurfB(Fall f)
{
    const Lage& l = lageFuer(f);
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setSpacing(6);
    auto* h = new QHBoxLayout;
    h->setSpacing(6);
    h->addWidget(kopf(QStringLiteral("NEW QSO")));
    h->addWidget(feld(l.call, 170, true));
    h->addWidget(knopf(QStringLiteral("Rotate %1°").arg(l.sp), f != Fall::Gesperrt));
    h->addWidget(knopf(QStringLiteral("LP %1°").arg(l.lp)));
    QString schild;
    const char* sf = Style::kTextScale;
    switch (f) {
    case Fall::Steht:     schild = QStringLiteral("◎ 231°"); break;
    case Fall::Dreht:     schild = QStringLiteral("◎ 234° → %1°").arg(l.sp); sf = Style::kAmberText; break;
    case Fall::AmZiel:    schild = QStringLiteral("◎ %1° ✓").arg(l.sp); sf = Style::kGreenText; break;
    case Fall::KeinRotor: schild = QStringLiteral("◎ no rotor"); sf = Style::kAmberWarn; break;
    case Fall::Gesperrt:  schild = QStringLiteral("◎ 231°"); break;
    }
    auto* s = new QLabel(schild);
    s->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold; border: 1px solid %2;"
        " border-radius: 6px; padding: 2px 8px; }")
        .arg(QLatin1String(sf), QLatin1String(Style::kBorder)));
    h->addWidget(s);
    funkUndRapport(h);
    v->addLayout(h);
    QString unten = QStringLiteral("%1 km · %2 (rough)").arg(l.km).arg(l.woher);
    if (f == Fall::Gesperrt) { unten += QStringLiteral(" · short path blocked by the allowed range"); }
    if (f == Fall::KeinRotor) { unten += QStringLiteral(" · no rotor connected — the antenna does not turn"); }
    auto* u = new QHBoxLayout;
    u->addWidget(klein(unten, f == Fall::KeinRotor || f == Fall::Gesperrt ? Style::kAmberWarn
                                                                         : Style::kTextScale));
    u->addStretch(1);
    u->addWidget(klein(QStringLiteral("Enter logs · Esc clears")));
    v->addLayout(u);
    return w;
}

// Skizze eines schmalen Bandes fuer die Zeile: ±90° um die Antenne,
// Nadel = wo er steht, Dreieck = Ziel, Keil = Strahlbreite. Das echte
// Zifferblatt in Bandform ist fuer Panelgroesse gebaut; in einer
// Zeilenhoehe deckt seine Ablesung die Skala zu.
class BandSkizze : public QWidget {
public:
    double steht{231.0};
    double ziel{-1.0};
    const char* farbe{Style::kAmberText};
    bool gestrichelt{false};
    BandSkizze() { setFixedSize(300, 34); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(QColor(QLatin1String(Style::kBorder)), 1));
        p.setBrush(QColor(QLatin1String(Style::kPanelBg)));
        p.drawRoundedRect(r, 6, 6);
        const double spanne = 180.0;
        const double mitte = r.center().x();
        const double proGrad = (r.width() - 16) / spanne;
        const auto x = [&](double az) {
            double d = std::fmod(az - steht + 540.0, 360.0) - 180.0;
            return mitte + d * proGrad;
        };
        // Strahlkeil
        QColor keil = QColor(QLatin1String(farbe));
        keil.setAlpha(40);
        p.setPen(Qt::NoPen);
        p.setBrush(keil);
        p.drawRect(QRectF(x(steht - 20), r.top() + 3, 40 * proGrad, r.height() - 6));
        // Teilung oben, Beschriftung unten; keine Zahl unter der Nadel
        // (die steht als Text daneben).
        QFont f = font();
        f.setPixelSize(9);
        p.setFont(f);
        for (int a = 0; a < 360; a += 10) {
            double d = std::fmod(a - steht + 540.0, 360.0) - 180.0;
            if (std::abs(d) > spanne / 2) { continue; }
            const double xx = x(a);
            const bool gross = a % 30 == 0;
            p.setPen(QPen(QColor(QLatin1String(gross ? Style::kTextScale : Style::kBorder)), 1));
            p.drawLine(QPointF(xx, r.top() + 9), QPointF(xx, r.top() + (gross ? 16 : 13)));
            if (gross && std::abs(d) > 7.0) {
                const QString t = a == 0 ? QStringLiteral("N") : a == 90 ? QStringLiteral("E")
                                : a == 180 ? QStringLiteral("S") : a == 270 ? QStringLiteral("W")
                                : QString::number(a);
                p.setPen(QColor(QLatin1String(a % 90 == 0 ? Style::kTextPrimary : Style::kTextScale)));
                p.drawText(QRectF(xx - 14, r.top() + 18, 28, 12), Qt::AlignCenter, t);
            }
        }
        // Ziel: Dreieck oben, zeigt nach unten
        if (ziel >= 0.0) {
            const double xz = x(ziel);
            QPolygonF tri;
            tri << QPointF(xz - 5, r.top() + 2) << QPointF(xz + 5, r.top() + 2) << QPointF(xz, r.top() + 9);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(QLatin1String(Style::kAccent)));
            p.drawPolygon(tri);
        }
        // Nadel
        QPen nadel(QColor(QLatin1String(farbe)), 2);
        if (gestrichelt) { nadel.setStyle(Qt::DashLine); }
        p.setPen(nadel);
        p.drawLine(QPointF(mitte, r.top() + 2), QPointF(mitte, r.bottom() - 2));
    }
};

// ── C: Rotorzeile mit dem Band ────────────────────────────────────────
// Wie A, aber der Zustand ist auch ein Bild: ein schmales Band
// (BandSkizze), Nadel = wo er steht, Dreieck = Ziel. Man sieht ihn
// drehen, statt es zu lesen.
QWidget* entwurfC(Fall f)
{
    const Lage& l = lageFuer(f);
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setSpacing(6);
    auto* h = new QHBoxLayout;
    h->setSpacing(6);
    h->addWidget(kopf(QStringLiteral("NEW QSO")));
    h->addWidget(feld(l.call, 170, true));
    h->addWidget(knopf(QStringLiteral("Rotor")));
    funkUndRapport(h);
    v->addLayout(h);

    auto* r = new QHBoxLayout;
    r->setSpacing(10);
    auto* k = kopf(QStringLiteral("ROTOR"));
    k->setFixedWidth(kopf(QStringLiteral("NEW QSO"))->sizeHint().width());
    r->addWidget(k, 0, Qt::AlignVCenter);
    auto* band = new BandSkizze;
    band->steht = f == Fall::Dreht ? 234.0 : f == Fall::AmZiel ? double(l.sp) : 231.0;
    band->ziel = (f == Fall::Steht || f == Fall::Gesperrt) ? -1.0 : double(l.sp);
    band->farbe = f == Fall::Dreht ? Style::kAmberText
                : f == Fall::AmZiel ? Style::kGreenText
                : f == Fall::KeinRotor ? Style::kTextScale : Style::kAmberText;
    band->gestrichelt = f == Fall::KeinRotor;
    r->addWidget(band);
    auto* txt = new QVBoxLayout;
    txt->setSpacing(2);
    auto* oben = new QHBoxLayout;
    oben->setSpacing(10);
    auto* grad = new QLabel(QStringLiteral("%1°").arg(l.sp));
    grad->setFont(Style::monoFont(grad->font(), 14, QFont::Bold));
    grad->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QLatin1String(Style::kTextPrimary)));
    oben->addWidget(grad);
    oben->addWidget(klein(QStringLiteral("%1 km").arg(l.km), Style::kTextSecondary));
    oben->addWidget(klein(QStringLiteral("%1 · rough").arg(l.woher), Style::kAmberWarn));
    oben->addStretch(1);
    txt->addLayout(oben);
    const auto z = zustand(f, l);
    txt->addWidget(klein(z.first, z.second));
    r->addLayout(txt);
    auto* rot = knopf(QStringLiteral("Rotate %1°").arg(l.sp), f != Fall::Gesperrt);
    if (f == Fall::Gesperrt) { rot->setText(QStringLiteral("Rotate %1° — blocked").arg(l.sp)); }
    r->addWidget(rot);
    r->addWidget(knopf(QStringLiteral("LP %1°").arg(l.lp)));
    r->addStretch(1);
    r->addWidget(klein(QStringLiteral("Enter logs · Esc clears")), 0, Qt::AlignBottom);
    v->addLayout(r);
    return w;
}

QString fallName(Fall f)
{
    switch (f) {
    case Fall::Steht:     return QStringLiteral("1  after ROTOR — rotor stands at 231°");
    case Fall::Dreht:     return QStringLiteral("2  Rotate pressed — turning");
    case Fall::AmZiel:    return QStringLiteral("3  on target");
    case Fall::KeinRotor: return QStringLiteral("4  Rotate pressed — no rotor connected");
    case Fall::Gesperrt:  return QStringLiteral("5  JA1XYZ — short path 42° outside the allowed range 180° → 0°");
    }
    return {};
}

constexpr int kBreite = 1400;
const QList<Fall> kFaelle{Fall::Steht, Fall::Dreht, Fall::AmZiel, Fall::KeinRotor, Fall::Gesperrt};

QList<QPixmap> gebaut(QWidget* (*bau)(Fall))
{
    QList<QPixmap> out;
    for (Fall f : kFaelle) {
        QWidget* w = bau(f);
        w->setAttribute(Qt::WA_StyledBackground, true);
        w->setStyleSheet(w->styleSheet() + QStringLiteral(" QWidget { background: %1; }")
                                               .arg(QLatin1String(Style::kAppBg)));
        w->setFixedWidth(kBreite);
        w->adjustSize();
        w->show();
        QCoreApplication::processEvents();
        out << w->grab();
        delete w;
    }
    return out;
}

void blatt(const QString& datei, const QString& titel, const QString& unter,
           const QList<QPixmap>& bilder)
{
    QList<QPair<QString, QPixmap>> zeilen;
    for (int i = 0; i < bilder.size() && i < kFaelle.size(); ++i) {
        zeilen << qMakePair(fallName(kFaelle.at(i)), bilder.at(i));
    }
    const qreal dpr = zeilen.first().second.devicePixelRatio();
    int hoehe = 70;
    for (const auto& z : zeilen) { hoehe += 24 + int(z.second.height() / dpr) + 14; }
    QImage bild(int(kBreite * dpr), int(hoehe * dpr), QImage::Format_ARGB32_Premultiplied);
    bild.setDevicePixelRatio(dpr);
    bild.fill(QColor(QLatin1String(Style::kAppBg)));
    QPainter p(&bild);
    p.setRenderHint(QPainter::Antialiasing);
    QFont tf = p.font();
    tf.setPixelSize(16);
    tf.setBold(true);
    p.setFont(tf);
    p.setPen(QColor(QLatin1String(Style::kTextPrimary)));
    p.drawText(QPointF(16, 28), titel);
    QFont uf = p.font();
    uf.setPixelSize(12);
    uf.setBold(false);
    p.setFont(uf);
    p.setPen(QColor(QLatin1String(Style::kTextSecondary)));
    p.drawText(QPointF(16, 50), unter);
    int y = 70;
    for (const auto& z : zeilen) {
        QFont lf = p.font();
        lf.setPixelSize(11);
        p.setFont(lf);
        p.setPen(QColor(QLatin1String(Style::kTextScale)));
        p.drawText(QPointF(16, y + 16), z.first);
        y += 24;
        p.drawPixmap(QPointF(0, y), z.second);
        y += int(z.second.height() / dpr) + 14;
        p.setPen(QPen(QColor(QLatin1String(Style::kBorder)), 1));
        p.drawLine(QPointF(16, y - 7), QPointF(kBreite - 16, y - 7));
    }
    p.end();
    // 144 dpi: in der Vorschau am Retina-Schirm wirkliche Groesse.
    if (dpr > 1.5) {
        bild.setDotsPerMeterX(5669);
        bild.setDotsPerMeterY(5669);
    }
    QVERIFY2(bild.save(datei), qPrintable(datei));
}

} // namespace

class TstRotorzeileEntwuerfe : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    void blaetter()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (ziel.isEmpty()) { QSKIP("nur mit LONGPATH_GRAB_DIR"); }
        QDir().mkpath(ziel);
        if (qEnvironmentVariableIsSet("LONGPATH_NUR_ECHT")) {
            blatt(ziel + QStringLiteral("/C-umgesetzt.png"),
                  QStringLiteral("Entwurf C — umgesetzt (echtes Logbuchfenster)"),
                  QStringLiteral("QSO-Zeile unverändert; darunter, nur nach ROTOR: Band (Nadel = Rotor, "
                                 "Dreieck = Station), Peilung, Entfernung, Herkunft, Zustand, Rotate / LP."),
                  istZeilen(kBreite));
            return;
        }
        blatt(ziel + QStringLiteral("/0-ist.png"),
              QStringLiteral("Ist — so steht es heute (echtes Logbuchfenster)"),
              QStringLiteral("Rotor, Peilung, Rotate, LP und Zustand sitzen mitten in der QSO-Zeile; "
                             "der Rest der Zeile bricht um und rutscht nach unten."),
              istZeilen(kBreite));
        blatt(ziel + QStringLiteral("/A-eigene-zeile.png"),
              QStringLiteral("Entwurf A — eigene Rotorzeile unter der Eingabe"),
              QStringLiteral("Die QSO-Zeile bleibt unverändert. Darunter, nur nach ROTOR: Peilung groß, "
                             "Entfernung, Herkunft, Rotate / LP, Zustand — jedes an festem Platz."),
              gebaut(&entwurfA));
        blatt(ziel + QStringLiteral("/B-kompakt.png"),
              QStringLiteral("Entwurf B — kompakt, die Peilung steht auf den Knöpfen"),
              QStringLiteral("Aus ROTOR werden zwei Knöpfe plus ein kleines Zustandsschild; "
                             "Entfernung und Herkunft stehen klein in der Hinweiszeile."),
              gebaut(&entwurfB));
        blatt(ziel + QStringLiteral("/C-band.png"),
              QStringLiteral("Entwurf C — eigene Rotorzeile mit dem Band"),
              QStringLiteral("Wie A, dazu ein schmales Band (±90° um die Antenne): Nadel = wo er steht, "
                             "Dreieck = Ziel. Man sieht ihn drehen. (Band hier skizziert, wäre ein neues Bauteil.)"),
              gebaut(&entwurfC));
    }
};

QTEST_MAIN(TstRotorzeileEntwuerfe)
#include "tst_rotorzeile_entwuerfe.moc"
