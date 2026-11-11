// no-port-check: Longpath-eigenes Entwurfswerkzeug, keine portierte Logik.
// =================================================================
// tests/tst_bandstapel_entwurf.cpp  (Longpath)
// =================================================================
//
// Entwurfsblaetter fuer den Bandstapel. Kein Pruefstand -- ein
// Zeichenwerkzeug. Je Entwurf ein Blatt, je Blatt zwei Betriebsfaelle:
//
//   · Scheibe A steht auf ihrer Stelle (40m 5/7)
//   · Scheibe A wurde von Hand auf das Band gestimmt und steht neben
//     ihrer Stelle (40m -/7) -- ein Verlassen speichert dann nichts
//
// Entwuerfe:
//   A  Applet in der Applet-Spalte
//   B  Seitenleiste links neben den Panadaptern, aus der Statusleiste
//   C  Nur Klappliste an der Bandtaste (Rechtsklick), dazu der Stand
//      in der Statusleiste
//   D1 Striche im Spektrum an der Frequenzskala
//   D2 Striche im Spektrum als feine Linien ueber die ganze Hoehe
//
// Alles gemalt mit den Werten aus gui/StyleConstants.h und
// docs/design/HAUSSTIL.md: Grau auf Fast-Schwarz, Auswahl ist eine
// Mulde (kGlassSel*), Blau nur als schmaler Strich, Zahlen monospace.
// Nach der Wahl wird mit dem echten Widget nachgerendert.
//
//   LONGPATH_GRAB_DIR=/pfad LONGPATH_ENTWURF=1 \
//       ./build/tests/tst_bandstapel_entwurf
//
// 144 dpi, damit die Vorschau wirkliche Groesse zeigt.
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Angelegt. Longpath, KI-gestuetzt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

#include "gui/StyleConstants.h"

using namespace Longpath;

namespace {

QColor c(const char* hex) { return QColor(QString::fromLatin1(hex)); }

const QColor kBlatt   (18, 19, 21);
const QColor kGrund   (8, 8, 10);
const QColor kPanel   (12, 12, 14);
const QColor kKopfHell(22, 22, 26);
const QColor kKopfDunk(17, 17, 19);

constexpr double kSkalierung = 2.0;  // Retina: 1 Punkt = 2 Bildpunkte

struct Eintrag {
    const char* freq;
    const char* mode;
    const char* breite;
    const char* name;
    bool gesperrt;
};

// Region 1 (Thetis AddRegion1BandStack) plus zwei eigene.
const Eintrag kVierzig[] = {
    {"7.005", "CWL", "500", "", false},
    {"7.010", "CWL", "500", "", false},
    {"7.045", "DIGU", "3.0k", "", false},
    {"7.074", "DIGU", "3.0k", "FT8", true},
    {"7.090", "LSB", "2.7k", "", false},
    {"7.100", "LSB", "2.7k", "", false},
    {"7.155", "LSB", "2.9k", "Runde Sonntag", false},
};
constexpr int kVierzigAnzahl = int(sizeof(kVierzig) / sizeof(kVierzig[0]));
constexpr int kStelle = 4;  // 7.090

struct Band { const char* name; int anzahl; int gesperrt; };
const Band kAndere[] = {
    {"160M", 4, 0}, {"80M", 5, 0}, {"60M", 1, 0}, {"30M", 4, 0},
    {"20M", 6, 1}, {"17M", 4, 0}, {"15M", 5, 0}, {"12M", 4, 0},
    {"10M", 6, 0}, {"6M", 4, 0}, {"GEN", 9, 0}, {"WWV", 6, 6},
};

QFont schrift(int px, bool fett = false)
{
    QFont f = QGuiApplication::font();
    f.setPixelSize(px);
    f.setBold(fett);
    return f;
}

QFont mono(int px, bool fett = false)
{
    // Wie LP_MONO_QSS (StyleConstants.h): Menlo, sonst die feste Schrift.
    QFont f(QStringLiteral("Menlo"));
    if (!QFontInfo(f).fixedPitch()) {
        f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    f.setPixelSize(px);
    f.setBold(fett);
    return f;
}

QFont versal(int px)
{
    QFont f = schrift(px, true);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::PercentageSpacing, 100.0 + Style::kCapsTracking * 100.0);
    return f;
}

void kapsel(QPainter& p, const QRectF& r, const QString& text, bool hell = false)
{
    p.setPen(QPen(hell ? c(Style::kGlassSelBorder) : c(Style::kBorder), 1.0));
    p.setBrush(kGrund);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    p.setFont(mono(10));
    p.setPen(hell ? c(Style::kTextPrimary) : c(Style::kTextSecondary));
    p.drawText(r, Qt::AlignCenter, text);
}

void schloss(QPainter& p, QPointF m, const QColor& farbe)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(farbe, 1.1));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(m.x() - 2.6, m.y() - 6.0, 5.2, 6.0), 0, 180 * 16);
    p.setBrush(farbe);
    p.drawRoundedRect(QRectF(m.x() - 3.6, m.y() - 2.6, 7.2, 5.6), 1.0, 1.0);
    p.restore();
}

void zahnrad(QPainter& p, QPointF m, const QColor& farbe)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(farbe, 1.2));
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 8; ++i) {
        const double w = i * M_PI / 4.0;
        p.drawLine(QPointF(m.x() + 4.2 * std::cos(w), m.y() + 4.2 * std::sin(w)),
                   QPointF(m.x() + 6.0 * std::cos(w), m.y() + 6.0 * std::sin(w)));
    }
    p.drawEllipse(m, 4.0, 4.0);
    p.drawEllipse(m, 1.6, 1.6);
    p.restore();
}

// Panelkopf nach Hausstil-Regel 3: Akzentbalken, Griff, Titel versal,
// rechts der Zustand als Kapsel (Regel 5) und das Zahnrad (Optionen
// immer rechts oben).
void kopf(QPainter& p, const QRectF& r, const QString& titel, const QString& stand,
          bool zahn = true)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, kKopfHell);
    g.setColorAt(1, kKopfDunk);
    p.fillRect(r, g);
    p.setPen(c(Style::kGlassEdgeLight));
    p.drawLine(r.topLeft() + QPointF(0, 0.5), r.topRight() + QPointF(0, 0.5));
    QLinearGradient a(r.topLeft(), r.bottomLeft());
    a.setColorAt(0, QColor(0xa8, 0x7a, 0x3c));
    a.setColorAt(1, QColor(0x8a, 0x6a, 0x30));
    p.fillRect(QRectF(r.left(), r.top(), 3, r.height()), a);
    p.setFont(schrift(11));
    p.setPen(c(Style::kTextInactive));
    p.drawText(QRectF(r.left() + 9, r.top(), 12, r.height()), Qt::AlignCenter,
               QStringLiteral("⠿"));
    p.setFont(versal(10));
    p.setPen(c(Style::kTitleText));
    p.drawText(QRectF(r.left() + 26, r.top(), r.width() - 120, r.height()),
               Qt::AlignVCenter | Qt::AlignLeft, titel);
    double rechts = r.right() - 8;
    if (zahn) {
        zahnrad(p, QPointF(rechts - 7, r.center().y()), c(Style::kTextSecondary));
        rechts -= 22;
    }
    if (!stand.isEmpty()) {
        const double w = 62;
        kapsel(p, QRectF(rechts - w, r.center().y() - 9, w, 18), stand, true);
    }
}

// Eine Zeile des Stapels.
void zeile(QPainter& p, const QRectF& r, const Eintrag& e, bool stelle, bool scheibeDrauf,
           bool breit)
{
    if (stelle) {
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, c(Style::kGlassSelTop));
        g.setColorAt(1, c(Style::kGlassSelBot));
        p.setPen(QPen(c(Style::kGlassSelBorder), 1.0));
        p.setBrush(g);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        // Blau nur als schmaler Strich: anfassbar, gewaehlt.
        p.fillRect(QRectF(r.left() + 1, r.top() + 5, 2, r.height() - 10), c(Style::kAccent));
    }
    const QColor wert = stelle ? c(Style::kGlassSelText) : c(Style::kTextPrimary);
    double x = r.left() + 10;
    if (e.gesperrt) {
        schloss(p, QPointF(x + 4, r.center().y() + 1), c(Style::kTextScale));
    }
    x += 14;
    p.setFont(mono(13, stelle));
    p.setPen(wert);
    p.drawText(QRectF(x, r.top(), 52, r.height()), Qt::AlignVCenter | Qt::AlignRight,
               QString::fromLatin1(e.freq));
    x += 60;
    p.setFont(versal(9));
    p.setPen(stelle ? c(Style::kTextPrimary) : c(Style::kTextSecondary));
    p.drawText(QRectF(x, r.top(), 40, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
               QString::fromLatin1(e.mode));
    x += 42;
    p.setFont(mono(10));
    p.setPen(c(Style::kTextScale));
    p.drawText(QRectF(x, r.top(), 34, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
               QString::fromLatin1(e.breite));
    x += 38;
    const double flagge = scheibeDrauf ? 24 : 0;
    p.setFont(schrift(11));
    p.setPen(c(Style::kTextTertiary));
    const double nameBreite = r.right() - 8 - flagge - x;
    if (nameBreite > 20) {
        p.drawText(QRectF(x, r.top(), nameBreite, r.height()),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(QString::fromUtf8(e.name), Qt::ElideRight,
                                              int(nameBreite)));
    }
    Q_UNUSED(breit);
    if (scheibeDrauf) {
        const QRectF f(r.right() - 26, r.center().y() - 8, 18, 16);
        p.setPen(QPen(c(Style::kAccent), 1.0));
        p.setBrush(kGrund);
        p.drawRoundedRect(f, 4, 4);
        p.setFont(schrift(10, true));
        p.setPen(c(Style::kTextPrimary));
        p.drawText(f, Qt::AlignCenter, QStringLiteral("A"));
    }
}

void gruppenkopf(QPainter& p, const QRectF& r, const QString& name, int anzahl, int gesperrt,
                 bool offen, const QString& rueckkehr = {})
{
    p.setFont(versal(9));
    p.setPen(offen ? c(Style::kTextSecondary) : c(Style::kTextScale));
    p.drawText(QRectF(r.left() + 10, r.top(), 60, r.height()), Qt::AlignVCenter, name);
    p.setFont(mono(10));
    p.setPen(c(Style::kTextScale));
    p.drawText(QRectF(r.left() + 60, r.top(), 30, r.height()), Qt::AlignVCenter,
               QString::number(anzahl));
    if (gesperrt > 0) {
        schloss(p, QPointF(r.left() + 96, r.center().y() + 1), c(Style::kTextInactive));
        p.drawText(QRectF(r.left() + 103, r.top(), 20, r.height()), Qt::AlignVCenter,
                   QString::number(gesperrt));
    }
    if (!rueckkehr.isEmpty()) {
        p.setFont(schrift(10));
        p.setPen(c(Style::kTextScale));
        p.drawText(QRectF(r.left(), r.top(), r.width() - 26, r.height()),
                   Qt::AlignVCenter | Qt::AlignRight, rueckkehr);
    }
    p.setFont(schrift(10));
    p.setPen(c(Style::kTextScale));
    p.drawText(QRectF(r.right() - 20, r.top(), 14, r.height()), Qt::AlignCenter,
               offen ? QStringLiteral("▾") : QStringLiteral("▸"));
}

void erhabenerKnopf(QPainter& p, const QRectF& r, const QString& text)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, QColor(0x22, 0x22, 0x27));
    g.setColorAt(1, QColor(0x14, 0x14, 0x17));
    p.setPen(QPen(c(Style::kBorder), 1.0));
    p.setBrush(g);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);
    p.setPen(c(Style::kGlassEdgeLight));
    p.drawLine(QPointF(r.left() + 6, r.top() + 1.5), QPointF(r.right() - 6, r.top() + 1.5));
    p.setFont(schrift(11));
    p.setPen(c(Style::kTextPrimary));
    p.drawText(r, Qt::AlignCenter, text);
}

// Der Inhalt einer Stapel-Liste: offene Gruppe 40 m, die anderen zu.
double liste(QPainter& p, const QRectF& r, bool aufStelle, bool alleGruppen, bool breit)
{
    double y = r.top();
    gruppenkopf(p, QRectF(r.left(), y, r.width(), 24), QStringLiteral("40M"), kVierzigAnzahl,
                1, true, QStringLiteral("Return: current spot"));
    y += 26;
    for (int i = 0; i < kVierzigAnzahl; ++i) {
        const bool stelle = aufStelle && i == kStelle;
        zeile(p, QRectF(r.left() + 4, y, r.width() - 8, 26), kVierzig[i], stelle, stelle, breit);
        y += 27;
    }
    y += 4;
    erhabenerKnopf(p, QRectF(r.left() + 8, y, r.width() - 16, 26),
                   QStringLiteral("+  Add current spot"));
    y += 36;
    if (alleGruppen) {
        p.setPen(c(Style::kBorderSubtle));
        p.drawLine(QPointF(r.left() + 8, y - 4), QPointF(r.right() - 8, y - 4));
        for (const Band& b : kAndere) {
            gruppenkopf(p, QRectF(r.left(), y, r.width(), 22), QString::fromLatin1(b.name),
                        b.anzahl, b.gesperrt, false);
            y += 22;
        }
    }
    return y;
}

void blattSpeichern(const QString& dir, const QString& name, const QImage& inhalt,
                    const QString& titel, const QString& fussnote)
{
    const int rand = 28 * kSkalierung;
    const int kopfH = 54 * kSkalierung;
    const int fussH = 120 * kSkalierung;
    QImage blatt(inhalt.width() + 2 * rand, inhalt.height() + kopfH + fussH,
                 QImage::Format_RGB32);
    blatt.fill(kBlatt);
    QPainter p(&blatt);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(kSkalierung, kSkalierung);
    const double b = blatt.width() / kSkalierung;
    p.setFont(schrift(17, true));
    p.setPen(c(Style::kTextPrimary));
    p.drawText(QRectF(28, 14, b - 56, 26), Qt::AlignLeft | Qt::AlignVCenter, titel);
    p.resetTransform();
    p.drawImage(rand, kopfH, inhalt);
    p.scale(kSkalierung, kSkalierung);
    p.setFont(schrift(13));
    p.setPen(c(Style::kTextSecondary));
    p.drawText(QRectF(28, (kopfH + inhalt.height()) / kSkalierung + 12, b - 56, 110),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, fussnote);
    p.end();
    blatt.setDotsPerMeterX(int(144.0 / 0.0254));
    blatt.setDotsPerMeterY(int(144.0 / 0.0254));
    const QString pfad = QDir(dir).filePath(name);
    QVERIFY2(blatt.save(pfad), qPrintable(pfad));
    qInfo() << "Blatt:" << pfad << blatt.size();
}

QImage leinwand(double w, double h)
{
    QImage img(int(w * kSkalierung), int(h * kSkalierung), QImage::Format_RGB32);
    img.fill(kBlatt);
    return img;
}

void fallTitel(QPainter& p, QPointF at, const QString& text)
{
    p.setFont(schrift(12, true));
    p.setPen(c(Style::kTextSecondary));
    p.drawText(at, text);
}

// ── Entwurf A: Applet in der Spalte ─────────────────────────────────
void appletZeichnen(QPainter& p, const QRectF& r, bool aufStelle)
{
    p.setPen(QPen(c(Style::kBorder), 1.0));
    p.setBrush(kPanel);
    p.drawRoundedRect(r, 8, 8);
    kopf(p, QRectF(r.left() + 1, r.top() + 1, r.width() - 2, 32), QStringLiteral("Band Stack"),
         aufStelle ? QStringLiteral("40m 5/7") : QStringLiteral("40m –/7"));
    p.setFont(schrift(11));
    p.setPen(c(Style::kTextScale));
    p.drawText(QRectF(r.left() + 12, r.top() + 38, r.width() - 24, 30),
               Qt::AlignLeft | Qt::TextWordWrap,
               aufStelle ? QStringLiteral("Saved spots per band. Press the band again for the next.")
                         : QStringLiteral("Tuned here by hand: leaving 40m saves nothing."));
    liste(p, QRectF(r.left(), r.top() + 72, r.width(), r.height() - 80), aufStelle, true, false);
}

// ── Panadapter-Skizze ───────────────────────────────────────────────
struct Pan {
    QRectF r;
    double lo{7.000};
    double hi{7.200};
    double xVon(double mhz) const { return r.left() + (mhz - lo) / (hi - lo) * r.width(); }
};

void panZeichnen(QPainter& p, const Pan& pan, double vfo)
{
    p.fillRect(pan.r, QColor(6, 6, 8));
    p.setPen(QColor(24, 24, 28));
    for (int i = 1; i < 8; ++i) {
        const double y = pan.r.top() + i * pan.r.height() / 8;
        p.drawLine(QPointF(pan.r.left(), y), QPointF(pan.r.right(), y));
    }
    for (double f = 7.000; f <= 7.2001; f += 0.025) {
        const double x = pan.xVon(f);
        p.drawLine(QPointF(x, pan.r.top()), QPointF(x, pan.r.bottom() - 18));
        p.setFont(mono(9));
        p.setPen(c(Style::kTextScale));
        p.drawText(QRectF(x - 24, pan.r.bottom() - 16, 48, 14), Qt::AlignCenter,
                   QString::number(f, 'f', 3));
        p.setPen(QColor(24, 24, 28));
    }
    // Rauschen und ein paar Signale, grau -- die Kurve ist warm, aber
    // hier nicht die Frage.
    QPainterPath kurve;
    const double basis = pan.r.top() + pan.r.height() * 0.70;
    for (int i = 0; i <= 400; ++i) {
        const double x = pan.r.left() + i * pan.r.width() / 400;
        const double f = pan.lo + (pan.hi - pan.lo) * i / 400;
        double y = basis + 6 * std::sin(i * 1.7) * std::cos(i * 0.31);
        for (double s : {7.0125, 7.0308, 7.074, 7.093, 7.131, 7.155, 7.178}) {
            const double d = (f - s) * 1000.0;
            y -= 46 * std::exp(-d * d / 1.2);
        }
        if (i == 0) { kurve.moveTo(x, y); } else { kurve.lineTo(x, y); }
    }
    p.setPen(QPen(QColor(0xd8, 0xa5, 0x5f, 200), 1.4));
    p.setBrush(Qt::NoBrush);
    p.drawPath(kurve);
    // Filter und VFO-Strich der Scheibe.
    const double xv = pan.xVon(vfo);
    p.fillRect(QRectF(xv - 27, pan.r.top(), 27, pan.r.height() - 18), QColor(255, 255, 255, 14));
    p.setPen(QPen(c(Style::kAccent), 1.4));
    p.drawLine(QPointF(xv, pan.r.top()), QPointF(xv, pan.r.bottom() - 18));
}

// D1: Marken an der Frequenzskala. D2: feine Linien ueber die Hoehe.
void markenZeichnen(QPainter& p, const Pan& pan, bool ganzeHoehe, bool aufStelle)
{
    const double fs[] = {7.005, 7.010, 7.045, 7.074, 7.090, 7.100, 7.155};
    double letzteRechts = -1e9;
    int reihe = 0;
    for (int i = 0; i < kVierzigAnzahl; ++i) {
        const double x = pan.xVon(fs[i]);
        const bool stelle = aufStelle && i == kStelle;
        const QColor farbe = stelle ? c(Style::kTextPrimary) : c(Style::kTextScale);
        if (ganzeHoehe) {
            QPen pen(stelle ? c(Style::kTextSecondary) : QColor(0x7e, 0x7e, 0x85, 90), 1.0,
                     Qt::DashLine);
            pen.setDashPattern({3, 4});
            p.setPen(pen);
            p.drawLine(QPointF(x, pan.r.top() + 20), QPointF(x, pan.r.bottom() - 18));
            const QString t = QString::fromLatin1(kVierzig[i].name[0] ? kVierzig[i].name
                                                                       : kVierzig[i].mode);
            p.setFont(versal(8));
            const double w = p.fontMetrics().horizontalAdvance(t) + 12;
            // Ueberlappt das Schild das vorige, eine Reihe tiefer.
            reihe = (x - w / 2 < letzteRechts + 4) ? (reihe + 1) % 2 : 0;
            const QRectF chip(x - w / 2, pan.r.top() + 4 + reihe * 17, w, 14);
            letzteRechts = chip.right() + (kVierzig[i].gesperrt ? 14 : 0);
            p.setPen(QPen(stelle ? c(Style::kGlassSelBorder) : c(Style::kBorder), 1.0));
            p.setBrush(QColor(8, 8, 10, 230));
            p.drawRoundedRect(chip, 7, 7);
            p.setPen(farbe);
            p.drawText(chip, Qt::AlignCenter, t);
            if (kVierzig[i].gesperrt) {
                schloss(p, QPointF(chip.right() + 7, chip.center().y() + 1), farbe);
            }
        } else {
            const double y = pan.r.bottom() - 18;
            QPainterPath dreieck;
            dreieck.moveTo(x, y - 9);
            dreieck.lineTo(x - 4.5, y - 1);
            dreieck.lineTo(x + 4.5, y - 1);
            dreieck.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(farbe);
            p.drawPath(dreieck);
            if (kVierzig[i].gesperrt) {
                schloss(p, QPointF(x, y - 15), c(Style::kTextScale));
            }
        }
    }
}

bool zeichnen()
{
    return !qEnvironmentVariableIsEmpty("LONGPATH_GRAB_DIR")
           && qEnvironmentVariable("LONGPATH_ENTWURF") == QLatin1String("1");
}

} // namespace

class TstBandstapelEntwurf : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!zeichnen()) {
            QSKIP("Zeichenwerkzeug: LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1 setzen");
        }
    }

    void entwurfA_applet()
    {
        const double w = 2 * 300 + 3 * 24;
        const double h = 636;
        QImage img = leinwand(w, h);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(kSkalierung, kSkalierung);
        fallTitel(p, QPointF(24, 18), QStringLiteral("Auf der Stelle"));
        appletZeichnen(p, QRectF(24, 28, 300, 596), true);
        fallTitel(p, QPointF(348, 18), QStringLiteral("Von Hand hineingestimmt"));
        appletZeichnen(p, QRectF(348, 28, 300, 596), false);
        p.end();
        blattSpeichern(qEnvironmentVariable("LONGPATH_GRAB_DIR"),
                       QStringLiteral("bandstapel-A-applet.png"), img,
                       QStringLiteral("Entwurf A — Bandstapel als Applet in der Spalte"),
                       QStringLiteral(
                           "Ein Applet wie TCI oder FT8: im Kopf der Stand als Kapsel "
                           "(„40m 5/7“) und das Zahnrad für die Optionen (Rückkehrart, "
                           "Verfall, Auto-Speichern, Region neu laden). Das aktuelle Band "
                           "ist offen, die anderen zu, mit Anzahl und gesperrten Einträgen. "
                           "Die Stelle der Scheibe ist eine Mulde mit schmalem blauem "
                           "Strich und der Marke „A“. Klick = hin, Doppelklick auf den "
                           "Namen = umbenennen, Rechtsklick = Menü."));
    }

    void entwurfB_seitenleiste()
    {
        const double w = 1100;
        const double h = 720;
        QImage img = leinwand(w, h);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(kSkalierung, kSkalierung);
        // Fenster-Skizze: links die Leiste, rechts der Panadapter.
        p.fillRect(QRectF(0, 0, w, h), kGrund);
        const QRectF leiste(8, 8, 320, h - 44);
        p.setPen(QPen(c(Style::kBorder), 1.0));
        p.setBrush(kPanel);
        p.drawRoundedRect(leiste, 8, 8);
        kopf(p, QRectF(leiste.left() + 1, leiste.top() + 1, leiste.width() - 2, 32),
             QStringLiteral("Band Stack · Slice A"), QStringLiteral("40m 5/7"));
        liste(p, QRectF(leiste.left(), leiste.top() + 44, leiste.width(), leiste.height() - 50),
              true, true, true);
        Pan pan{QRectF(336, 8, w - 344, (h - 44) * 0.55), 7.000, 7.200};
        panZeichnen(p, pan, 7.090);
        p.fillRect(QRectF(336, pan.r.bottom() + 2, w - 344, h - 44 - pan.r.height() - 10),
                   QColor(10, 12, 16));
        // Statusleiste mit dem Stand als Zustand (Regel 8).
        const QRectF fuss(0, h - 30, w, 30);
        p.fillRect(fuss, QColor(14, 14, 16));
        p.setPen(c(Style::kBorderSubtle));
        p.drawLine(fuss.topLeft(), fuss.topRight());
        kapsel(p, QRectF(12, fuss.top() + 6, 92, 18), QStringLiteral("▤  40m 5/7"), true);
        p.setFont(schrift(10));
        p.setPen(c(Style::kTextScale));
        p.drawText(QRectF(112, fuss.top(), 400, 30), Qt::AlignVCenter,
                   QStringLiteral("← Klick öffnet und schließt die Leiste"));
        p.end();
        blattSpeichern(qEnvironmentVariable("LONGPATH_GRAB_DIR"),
                       QStringLiteral("bandstapel-B-seitenleiste.png"), img,
                       QStringLiteral("Entwurf B — Seitenleiste links neben dem Panadapter"),
                       QStringLiteral(
                           "Wie bei Nereus und AetherSDR: die Leiste schiebt sich links "
                           "neben die Panadapter, geöffnet über den Stand in der "
                           "Statusleiste (oder Band → Band Stacking…). Mehr Platz für "
                           "Namen; der Panadapter wird um die Breite der Leiste schmäler, "
                           "solange sie offen ist. Inhalt und Bedienung wie Entwurf A."));
    }

    void entwurfC_klappliste()
    {
        const double w = 760;
        const double h = 380;
        QImage img = leinwand(w, h);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(kSkalierung, kSkalierung);
        fallTitel(p, QPointF(24, 18), QStringLiteral("Rechtsklick auf „40“"));
        // Bandtasten-Raster (erhaben, gewaehlt = Mulde).
        const char* baender[] = {"160", "80", "60", "40", "30", "20", "17", "15",
                                 "12", "10", "6", "WWV"};
        for (int i = 0; i < 12; ++i) {
            const QRectF k(24 + (i % 4) * 53, 32 + (i / 4) * 31, 48, 26);
            if (i == 3) {
                QLinearGradient g(k.topLeft(), k.bottomLeft());
                g.setColorAt(0, c(Style::kGlassSelTop));
                g.setColorAt(1, c(Style::kGlassSelBot));
                p.setPen(QPen(c(Style::kGlassSelBorder), 1.0));
                p.setBrush(g);
                p.drawRoundedRect(k, 7, 7);
                p.setFont(schrift(11, true));
                p.setPen(c(Style::kGlassSelText));
                p.drawText(k, Qt::AlignCenter, QString::fromLatin1(baender[i]));
            } else {
                erhabenerKnopf(p, k, QString::fromLatin1(baender[i]));
            }
        }
        // Die Klappliste am Zeiger.
        const QRectF klapp(140, 70, 330, 276);
        p.setPen(QPen(c(Style::kBorder), 1.0));
        QLinearGradient g(klapp.topLeft(), klapp.bottomLeft());
        g.setColorAt(0, QColor(0x17, 0x17, 0x1b));
        g.setColorAt(1, QColor(0x0e, 0x0e, 0x10));
        p.setBrush(g);
        p.drawRoundedRect(klapp, 10, 10);
        p.setFont(versal(9));
        p.setPen(c(Style::kTextSecondary));
        p.drawText(QRectF(klapp.left() + 12, klapp.top() + 6, 200, 20), Qt::AlignVCenter,
                   QStringLiteral("40M · Band stack"));
        kapsel(p, QRectF(klapp.right() - 72, klapp.top() + 7, 62, 18), QStringLiteral("5/7"), true);
        double y = klapp.top() + 32;
        for (int i = 0; i < kVierzigAnzahl; ++i) {
            zeile(p, QRectF(klapp.left() + 6, y, klapp.width() - 12, 26), kVierzig[i],
                  i == kStelle, i == kStelle, false);
            y += 27;
        }
        erhabenerKnopf(p, QRectF(klapp.left() + 10, y + 6, klapp.width() - 20, 26),
                       QStringLiteral("+  Add current spot"));
        // Zeiger.
        QPainterPath zeiger;
        zeiger.moveTo(176, 52);
        zeiger.lineTo(176, 68);
        zeiger.lineTo(180, 64);
        zeiger.lineTo(183, 71);
        zeiger.lineTo(185, 70);
        zeiger.lineTo(182, 63);
        zeiger.lineTo(187, 63);
        zeiger.closeSubpath();
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(Qt::white);
        p.drawPath(zeiger);
        // Statusleiste rechts unten als Hinweis.
        fallTitel(p, QPointF(500, 90), QStringLiteral("Statusleiste"));
        kapsel(p, QRectF(500, 102, 92, 18), QStringLiteral("▤  40m 5/7"), true);
        kapsel(p, QRectF(500, 128, 92, 18), QStringLiteral("▤  40m –/7"));
        p.setFont(schrift(11));
        p.setPen(c(Style::kTextScale));
        p.drawText(QRectF(500, 152, 240, 120), Qt::AlignLeft | Qt::TextWordWrap,
                   QStringLiteral("Oben: auf der Stelle. Unten: von Hand "
                                  "hineingestimmt, Verlassen speichert nichts."));
        p.end();
        blattSpeichern(qEnvironmentVariable("LONGPATH_GRAB_DIR"),
                       QStringLiteral("bandstapel-C-klappliste.png"), img,
                       QStringLiteral("Entwurf C — nur Klappliste an der Bandtaste"),
                       QStringLiteral(
                           "Kein stehendes Fenster: Rechtsklick auf eine Bandtaste (Leiste, "
                           "Flyout, Behälterknöpfe) öffnet die Einträge dieses Bandes am "
                           "Zeiger. Linksklick bleibt die Bandtaste (zweimal = nächster "
                           "Eintrag, ⇧ = voriger, ⌘ = Stelle ablegen). Der Stand steht in "
                           "der Statusleiste. Am wenigsten Fläche; dafür sieht man den "
                           "Stapel nur auf Abruf."));
    }

    void entwurfD_striche()
    {
        for (const bool ganz : {false, true}) {
            const double w = 1000;
            const double h = 2 * 250 + 3 * 24;
            QImage img = leinwand(w, h);
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            p.scale(kSkalierung, kSkalierung);
            for (int fall = 0; fall < 2; ++fall) {
                const double top = 24 + fall * (250 + 24);
                fallTitel(p, QPointF(24, top - 6),
                          fall == 0 ? QStringLiteral("Auf der Stelle 7.090")
                                    : QStringLiteral("Von Hand auf 7.131 gestimmt"));
                Pan pan{QRectF(24, top, w - 48, 240), 7.000, 7.200};
                panZeichnen(p, pan, fall == 0 ? 7.090 : 7.131);
                markenZeichnen(p, pan, ganz, fall == 0);
            }
            p.end();
            blattSpeichern(
                qEnvironmentVariable("LONGPATH_GRAB_DIR"),
                ganz ? QStringLiteral("bandstapel-D2-linien.png")
                     : QStringLiteral("bandstapel-D1-skala.png"),
                img,
                ganz ? QStringLiteral("Entwurf D2 — Einträge als feine Linien im Spektrum")
                     : QStringLiteral("Entwurf D1 — Einträge als Marken an der Skala"),
                ganz ? QStringLiteral(
                           "Jeder Eintrag eine gestrichelte Linie über die ganze Höhe, oben "
                           "ein Schild mit Name oder Betriebsart. Klick auf das Schild = "
                           "hin. Gut zu lesen, aber es liegt über dem Signal. Abschaltbar "
                           "im Zahnrad-Menü; beim Senden ausgeblendet.")
                     : QStringLiteral(
                           "Jeder Eintrag ein kleines Dreieck auf der Frequenzskala, die "
                           "eigene Stelle heller, gesperrte mit Schloss. Klick auf ein "
                           "Dreieck = hin; Überfahren zeigt Frequenz, Betriebsart und Name. "
                           "Nichts liegt über dem Signal. Abschaltbar; beim Senden "
                           "ausgeblendet."));
        }
    }
};

QTEST_MAIN(TstBandstapelEntwurf)
#include "tst_bandstapel_entwurf.moc"
