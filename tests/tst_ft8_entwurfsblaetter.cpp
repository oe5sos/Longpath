// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigene Entwurfsblaetter, kein Port.
//
// ── Wie das FT8-Fenster umfangreicher wird — drei Entwuerfe ────────────
//
// Betreiber 2026-10-24: „und der aufbau-grafik usw. schaut derzeit noch
// sehr einfach aus. sollte danach dann umfangreicher werden."
//
// Der Ist-Zustand (tst_ft8_kopfzeile_blatt, `ft8-ist.png`) ist
// funktionsfaehig und schlicht: zwei nackte Textfelder als Spalten,
// eine Fortschrittsleiste, eine Knopfreihe. Was fehlt, ist nicht Farbe,
// sondern GLIEDERUNG — und seit Stufe 2 ausserdem eine Flaeche fuer die
// Plaetze, die es vorher nicht zu zeigen gab.
//
// Die drei Blaetter bauen aufeinander auf, damit man den Preis sieht:
//
//   A  Spalten        die Textfelder werden Tabellen mit Kopfzeile
//   B  Plaetze        A, dazu das Band der laufenden QSOs
//   C  Arbeitsplatz   B, dazu Wasserfall mit Skala und Marken
//
// Hausstil (docs/design/HAUSSTIL.md): Grau auf Fast-Schwarz, Farbe
// hoechstens zwei Prozent der Flaeche, Blau ist anfassbar, Warm ist
// gemessen. Die Zeilenfarben sind darum sparsam und tragen je eine
// Bedeutung, nicht Schmuck.
//
// Dies ist ein ZEICHENWERKZEUG, kein Prueflauf: ohne LONGPATH_GRAB_DIR
// und LONGPATH_ENTWURF=1 ueberspringt es sich.

#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QStyleFactory>

#include "gui/styles/Theme.h"
#include "gui/styles/AppTheme.h"

using namespace Longpath;

namespace {

// Werte aus src/gui/StyleConstants.h gelesen, hier fuer das Blatt.
const QColor kGrund   ( 18,  19,  21);
const QColor kFeld    ( 24,  26,  29);
const QColor kLinie   ( 44,  47,  52);
const QColor kText    (196, 199, 203);
const QColor kLeise   (122, 126, 132);
const QColor kBlau    ( 90, 150, 220);
const QColor kBernst  (194, 146,  79);
const QColor kGruen   (104, 168, 116);
const QColor kBlatt   ( 12,  13,  15);

constexpr int kB = 880;   // Arbeitsgroesse, wie der Ist-Zustand
constexpr int kH = 560;

QFont schrift(int px, bool fett = false)
{
    QFont f(QStringLiteral("Helvetica Neue"));
    f.setPixelSize(px);
    f.setBold(fett);
    return f;
}

QFont mono(int px, bool fett = false)
{
    QFont f(QStringLiteral("Menlo"));
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px);
    f.setBold(fett);
    return f;
}

// Die Kopfzeile, in allen drei gleich — sie ist heute schon gut.
int kopfzeile(QPainter& p)
{
    p.fillRect(QRect(0, 0, kB, 30), QColor(28, 30, 33));
    p.setPen(kText);
    p.setFont(schrift(11, true));
    p.drawText(QRect(12, 0, 300, 30), Qt::AlignVCenter,
               QStringLiteral("FT8 DECODER"));

    int x = 150;
    auto knopf = [&](const QString& t, bool an, int w) {
        const QRect r(x, 6, w, 18);
        p.fillRect(r, an ? kBlau : QColor(38, 40, 44));
        p.setPen(an ? QColor(16, 18, 20) : kText);
        p.setFont(schrift(10, an));
        p.drawText(r, Qt::AlignCenter, t);
        x += w + 6;
    };
    knopf(QStringLiteral("FT8"), true, 42);
    knopf(QStringLiteral("FT4"), false, 42);
    knopf(QStringLiteral("CQ only"), false, 62);

    p.setPen(kLeise);
    p.setFont(schrift(10));
    p.drawText(QRect(x + 10, 0, 200, 30), Qt::AlignVCenter,
               QStringLiteral("TX 1500 Hz   ·   Passes 2"));
    p.setPen(kText);
    p.setFont(mono(11));
    p.drawText(QRect(kB - 130, 0, 118, 30),
               Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("11.2 / 15.0 s"));
    return 30;
}

// Eine Spaltentabelle mit Kopf — der Kern von Variante A.
void tabelle(QPainter& p, const QRect& r, const QString& titel,
             bool mitFarben, int nurErste = 0)
{
    // Ueberschrift: klein, leise, gesperrt. Keine Rahmen.
    p.setPen(QColor(96, 100, 106));
    QFont ue = schrift(9, true);
    ue.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(ue);
    p.drawText(QRect(r.left(), r.top() - 16, r.width(), 12),
               Qt::AlignLeft, titel);

    // Kein Kasten, kein Fuellfeld -- nur eine Haarlinie unter dem Kopf.
    const int zh = 22;          // Luft: 22 statt 17
    const int sp[5] = {0, 62, 108, 152, 210};
    const char* kn[5] = {"UTC", "dB", "DT", "Hz", "NACHRICHT"};

    p.setPen(QColor(96, 100, 106));
    QFont kf = mono(9);
    kf.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    p.setFont(kf);
    for (int i = 0; i < 5; ++i) {
        p.drawText(QRect(r.left() + sp[i], r.top(), r.width(), 14),
                   Qt::AlignVCenter | Qt::AlignLeft, QString::fromUtf8(kn[i]));
    }
    p.setPen(QColor(48, 51, 56));
    p.drawLine(r.left(), r.top() + 17, r.right(), r.top() + 17);

    struct Zeile { const char* z; const char* db; const char* dt;
                   const char* hz; const char* msg; int art; };
    static const Zeile zeilen[] = {
        {"1142", " -7", "0.2", "1130", "CQ DL1ABC JO31",     1},
        {"1142", "-12", "0.1", " 920", "OE5SOS G0XYZ IO91",  2},
        {"1142", "-18", "0.3", "1560", "CQ DX F5QRP JN18",   1},
        {"1142", " -3", "0.0", "1340", "SP9ABC OK1XX -05",   0},
        {"1142", "-21", "0.4", " 760", "CQ VK3AAA QF22",     1},
        {"1142", " -9", "0.2", "1810", "OE5SOS DL1ABC R-12", 2},
        {"1142", "-15", "0.1", "1020", "JA1XYZ I2ABC 73",    0},
        {"1142", "-11", "0.2", "1420", "CQ EA7ZZ IM76",      1},
        {"1142", " -6", "0.1", "1240", "HB9ZZ SM5AA RR73",   0},
        {"1142", "-19", "0.3", " 880", "CQ LA2XX JP50",      1},
    };
    int y = r.top() + 17;
    int gezeigt = 0;
    for (const Zeile& z : zeilen) {
        if (y + zh > r.bottom()) { break; }
        if (nurErste > 0) {
            // Die RX-Spalte zeigt nur, was auf der eigenen Frequenz
            // laeuft -- sonst stuenden zweimal dieselben Zeilen da.
            if (z.art != 2) { continue; }
            if (++gezeigt > nurErste) { break; }
        }
        const QRect zr(r.left(), y, r.width(), zh);

        // EIN Akzent, nicht fuenf: ein Ruf an die eigene Station wird
        // hinterlegt. Alles andere ist Grau in zwei Helligkeiten --
        // Rufzeichen hell, Zahlen leise. Hausstil: Farbe unter zwei
        // Prozent der Flaeche.
        const bool anMich = mitFarben && z.art == 2;
        if (anMich) {
            p.fillRect(zr.adjusted(-6, 1, 6, -1), QColor(30, 40, 34));
            p.fillRect(QRect(zr.left() - 6, zr.top() + 1, 2, zh - 2), kGruen);
        }
        p.setFont(mono(11));
        p.setPen(QColor(104, 108, 114));                 // Zahlen leise
        p.drawText(zr.adjusted(sp[0], 0, 0, 0), Qt::AlignVCenter, QString::fromUtf8(z.z));
        p.drawText(zr.adjusted(sp[1], 0, 0, 0), Qt::AlignVCenter, QString::fromUtf8(z.db));
        p.drawText(zr.adjusted(sp[2], 0, 0, 0), Qt::AlignVCenter, QString::fromUtf8(z.dt));
        p.drawText(zr.adjusted(sp[3], 0, 0, 0), Qt::AlignVCenter, QString::fromUtf8(z.hz));
        p.setPen(anMich ? QColor(214, 230, 218) : kText);  // Text hell
        p.setFont(mono(11, anMich));
        p.drawText(zr.adjusted(sp[4], 0, 0, 0), Qt::AlignVCenter, QString::fromUtf8(z.msg));
        y += zh;
    }
}

// Das Band der laufenden QSOs — Variante B und C.
int plaetzeBand(QPainter& p, int y)
{
    // Eine Zeile je Platz, keine Kaesten. Die Ruhe kommt aus der
    // Ausrichtung: Rufzeichen links buendig, Zustand rechts buendig,
    // dazwischen Luft. Ein Kasten je Platz waere fuenf Rahmen mehr auf
    // einer Flaeche, die ohnehin schon Rahmen hat.
    const int h = 70;
    p.setPen(QColor(48, 51, 56));
    p.drawLine(12, y, kB - 12, y);

    p.setPen(QColor(96, 100, 106));
    QFont ue = schrift(9, true);
    ue.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(ue);
    p.drawText(QRect(12, y + 8, 300, 12), Qt::AlignLeft,
               QStringLiteral("IM GESPRÄCH"));
    p.setPen(QColor(78, 82, 88));
    p.setFont(mono(9));
    p.drawText(QRect(kB - 212, y + 8, 200, 12), Qt::AlignRight,
               QStringLiteral("3 von 6 Plätzen"));

    struct P { const char* ruf; const char* zu; const char* hz; int schritt; };
    static const P pl[] = {
        {"DL1ABC", "R-12",  "1500", 3},
        {"G0XYZ",  "RR73",  "1560", 4},
        {"F5QRP",  "wartet", "1620", 2},
    };
    int x = 12;
    const int breite = (kB - 24) / 3;
    for (const P& e : pl) {
        const int zy = y + 28;
        p.setPen(kText);
        p.setFont(mono(13));
        p.drawText(QRect(x, zy, breite - 20, 18),
                   Qt::AlignVCenter | Qt::AlignLeft, QString::fromUtf8(e.ruf));
        p.setPen(QColor(104, 108, 114));
        p.setFont(mono(10));
        p.drawText(QRect(x, zy, breite - 20, 18),
                   Qt::AlignVCenter | Qt::AlignRight, QString::fromUtf8(e.zu));
        p.setFont(mono(9));
        p.setPen(QColor(84, 88, 94));
        p.drawText(QRect(x, zy + 17, breite - 20, 14),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromUtf8(e.hz) + QStringLiteral(" Hz"));

        // Der Fortschritt als Haarlinie, nicht als Balken: vier
        // Schritte bis 73, die zurueckgelegten hell.
        const int ly = zy + 32;
        p.setPen(QColor(44, 47, 52));
        p.drawLine(x, ly, x + breite - 20, ly);
        p.setPen(QPen(kBlau, 2));
        p.drawLine(x, ly, x + (breite - 20) * e.schritt / 5, ly);
        x += breite;
    }
    return h;
}

// Wasserfallstreifen mit Skala und Marken — Variante C.
int wasserfall(QPainter& p, int y, bool mitSkala)
{
    const int h = mitSkala ? 96 : 76;
    p.fillRect(QRect(0, y, kB, h), QColor(14, 15, 17));

    // Rauschen. Ein Modulo-Muster liest sich als Diagonale und sieht
    // nach Fehler aus, nicht nach Band -- darum ein echter Zufall mit
    // festem Keim, damit das Blatt reproduzierbar bleibt.
    quint32 keim = 20261024u;
    auto wuerfel = [&keim]() {
        keim = keim * 1664525u + 1013904223u;     // Park-Miller-artig
        return (keim >> 16) & 0xFF;
    };
    const int boden = h - (mitSkala ? 24 : 10);
    for (int sy = y + 4; sy < y + boden; ++sy) {
        for (int sx = 10; sx < kB - 10; sx += 2) {
            const int v = 14 + (wuerfel() % 26);
            p.fillRect(QRect(sx, sy, 2, 1), QColor(v, v + 3, v + 7));
        }
    }
    // Zwei kraeftige Traeger.
    p.fillRect(QRect(300, y + 4, 2, h - (mitSkala ? 24 : 10)), QColor(150, 190, 230));
    p.fillRect(QRect(520, y + 4, 2, h - (mitSkala ? 24 : 10)), QColor(120, 160, 200));

    // Die Sendestelle, und bei C auch die Plaetze daneben.
    p.fillRect(QRect(430, y, 2, h - (mitSkala ? 20 : 0)), kBernst);
    p.setPen(kBernst);
    p.setFont(mono(9, true));
    p.drawText(QRect(438, y + 3, 80, 12), Qt::AlignLeft,
               QStringLiteral("TX 1500"));
    if (mitSkala) {
        for (int i = 1; i <= 2; ++i) {
            const int px = 430 + i * 17;
            p.fillRect(QRect(px, y + 14, 1, h - 34), QColor(150, 116, 66));
        }
        // Frequenzskala unten.
        p.fillRect(QRect(0, y + h - 20, kB, 20), QColor(20, 21, 24));
        p.setPen(kLeise);
        p.setFont(mono(9));
        for (int hz = 200; hz <= 2800; hz += 400) {
            const int px = 10 + (hz - 200) * (kB - 20) / 2600;
            p.drawLine(px, y + h - 20, px, y + h - 15);
            p.drawText(QRect(px - 24, y + h - 15, 48, 14), Qt::AlignCenter,
                       QString::number(hz));
        }
    }
    return h;
}

void fussleiste(QPainter& p, int y)
{
    p.fillRect(QRect(0, y, kB, kH - y), QColor(22, 24, 27));
    int x = 12;
    for (const char* t : {"Tx1", "Tx2", "Tx3", "Tx4", "Tx5", "Tx6"}) {
        const QRect r(x, y + 8, 40, 20);
        p.fillRect(r, QColor(36, 38, 42));
        p.setPen(kText);
        p.setFont(schrift(10));
        p.drawText(r, Qt::AlignCenter, QString::fromUtf8(t));
        x += 46;
    }
    const QRect eingabe(12, y + 34, kB - 230, 22);
    p.fillRect(eingabe, QColor(30, 32, 36));
    p.setPen(kLinie);
    p.drawRect(eingabe.adjusted(0, 0, -1, -1));
    p.setPen(kLeise);
    p.setFont(mono(10));
    p.drawText(eingabe.adjusted(8, 0, 0, 0), Qt::AlignVCenter,
               QStringLiteral("CQ OE5SOS JN67"));

    int bx = kB - 210;
    auto taste = [&](const QString& t, const QColor& f, int w) {
        const QRect r(bx, y + 34, w, 22);
        p.fillRect(r, f);
        p.setPen(f == kBlau ? QColor(16, 18, 20) : kText);
        p.setFont(schrift(10, f == kBlau));
        p.drawText(r, Qt::AlignCenter, t);
        bx += w + 6;
    };
    taste(QStringLiteral("CQ"), QColor(38, 40, 44), 44);
    taste(QStringLiteral("Send"), kBlau, 58);
    taste(QStringLiteral("Stop"), QColor(38, 40, 44), 52);
}

void blattSpeichern(const QString& dir, const QString& name,
                    const QImage& inhalt, const QString& titel,
                    const QString& fuss)
{
    const int rand = 24, kopf = 46, fussH = 52;
    QImage blatt(inhalt.width() + 2 * rand,
                 inhalt.height() + kopf + fussH, QImage::Format_RGB32);
    blatt.fill(kBlatt);
    QPainter p(&blatt);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(kText);
    p.setFont(schrift(15, true));
    p.drawText(QRect(rand, 14, blatt.width() - 2 * rand, 24),
               Qt::AlignLeft, titel);
    p.drawImage(rand, kopf, inhalt);
    p.setPen(kLinie);
    p.drawRect(QRect(rand - 1, kopf - 1, inhalt.width() + 1, inhalt.height() + 1));
    p.setPen(kLeise);
    p.setFont(schrift(11));
    p.drawText(QRect(rand, kopf + inhalt.height() + 12,
                     blatt.width() - 2 * rand, fussH - 14),
               Qt::AlignLeft | Qt::TextWordWrap, fuss);
    p.end();
    blatt.setDotsPerMeterX(static_cast<int>(144.0 / 0.0254));
    blatt.setDotsPerMeterY(static_cast<int>(144.0 / 0.0254));
    const QString pfad = dir + QStringLiteral("/") + name;
    if (!blatt.save(pfad)) { qWarning() << "nicht gespeichert:" << pfad; }
    qInfo() << "Blatt:" << pfad << blatt.size();
}

} // namespace

class TestFt8Entwurfsblaetter : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
    }

    void blaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()
            || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1");
        }

        // ── A: Spalten ───────────────────────────────────────────────
        {
            QImage b(kB, kH, QImage::Format_RGB32);
            b.fill(kGrund);
            QPainter p(&b);
            p.setRenderHint(QPainter::Antialiasing);
            int y = kopfzeile(p);
            y += wasserfall(p, y, false);
            y += 18;
            const int th = kH - y - 76;
            tabelle(p, QRect(10, y, 520, th), QStringLiteral("BAND ACTIVITY"), true);
            tabelle(p, QRect(542, y, kB - 552, th), QStringLiteral("RX FREQUENCY"), true, 3);
            fussleiste(p, kH - 70);
            p.end();
            blattSpeichern(dir, QStringLiteral("ft8-A-spalten.png"), b,
                QStringLiteral("A — Spalten"),
                QStringLiteral("Die zwei Textfelder werden Tabellen: Kopfzeile, feste Spalten, 22 px Zeilenhöhe. Keine Rahmen, "
                               "nur eine Haarlinie unter dem Kopf. Grau in zwei Helligkeiten — Zahlen leise, Text hell. "
                               "EINE Farbe im ganzen Fenster: ein Ruf an die eigene Station. Sonst ändert sich nichts."));
        }

        // ── B: Plaetze ───────────────────────────────────────────────
        {
            QImage b(kB, kH, QImage::Format_RGB32);
            b.fill(kGrund);
            QPainter p(&b);
            p.setRenderHint(QPainter::Antialiasing);
            int y = kopfzeile(p);
            y += wasserfall(p, y, false);
            y += plaetzeBand(p, y);
            y += 18;
            const int th = kH - y - 76;
            tabelle(p, QRect(10, y, 520, th), QStringLiteral("BAND ACTIVITY"), true);
            tabelle(p, QRect(542, y, kB - 552, th), QStringLiteral("RX FREQUENCY"), true, 3);
            fussleiste(p, kH - 70);
            p.end();
            blattSpeichern(dir, QStringLiteral("ft8-B-plaetze.png"), b,
                QStringLiteral("B — Plätze"),
                QStringLiteral("A, dazu das Band der laufenden QSOs — eine Zeile je Platz, keine Kästen: Rufzeichen links, "
                               "was als nächstes hinausgeht rechts, darunter die Tonlage und der Fortschritt bis 73 als Haarlinie. "
                               "Das ist die Fläche, die Stufe 2 braucht und die es vorher nicht gab."));
        }

        // ── C: Arbeitsplatz ──────────────────────────────────────────
        {
            QImage b(kB, kH, QImage::Format_RGB32);
            b.fill(kGrund);
            QPainter p(&b);
            p.setRenderHint(QPainter::Antialiasing);
            int y = kopfzeile(p);
            y += wasserfall(p, y, true);
            y += plaetzeBand(p, y);
            y += 18;
            const int th = kH - y - 76;
            tabelle(p, QRect(10, y, kB - 20, th), QStringLiteral("DEKODIERT"), true);
            fussleiste(p, kH - 70);
            p.end();
            blattSpeichern(dir, QStringLiteral("ft8-C-arbeitsplatz.png"), b,
                QStringLiteral("C — Arbeitsplatz"),
                QStringLiteral("B, dazu Frequenzskala am Wasserfall und Marken für die eigenen Sendeplätze (60 Hz Abstand, "
                               "wie wirklich gesendet wird). Eine breite Tabelle statt zweier schmaler — die Trennung "
                               "Band/RX wird ein Filter statt einer Spalte. Der größte Eingriff, die ruhigste Fläche."));
        }
    }
};

QTEST_MAIN(TestFt8Entwurfsblaetter)
#include "tst_ft8_entwurfsblaetter.moc"
