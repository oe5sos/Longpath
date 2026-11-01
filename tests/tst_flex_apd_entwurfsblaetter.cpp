// no-port-check: Longpath-eigenes Entwurfswerkzeug, keine portierte Logik.
// =================================================================
// tests/tst_flex_apd_entwurfsblaetter.cpp  (Longpath)
// =================================================================
//
// Entwurfsblätter zur Bedienung der Flex-Vorverzerrung. Kein
// Prüfstand — ein Zeichenwerkzeug:
//
//   Entwurf A  das ECHTE FlexApdPage, wie es jetzt gebaut ist
//   Entwurf B  Skizze „Eine Karte" — Kopfstreifen mit Schalter,
//              die Erklärung auf eine Zeile eingedampft
//   Entwurf C  Skizze „Signalweg" — zeigt, WO sich die Schleife
//              schliesst (TX → PA → Koppler → Rückweg)
//
// Je Entwurf zwei Betriebsfälle, weil ein Entwurf, der nur im Gutfall
// überzeugt, nichts taugt:
//   · aus, kein externer Messpunkt (der eingebaute Koppler, Martins
//     Fall ohne Abgriff in der Endstufe)
//   · ein, Messpunkt XVTA, gelernte Kurve trägt
//
// Entwurf A wird mit den ECHTEN Statuszeilen der FLEX-8400 gefüttert
// (`flexApdStatusForTest`), geht also durch denselben Parser wie am
// Gerät. B und C sind gemalt und als Skizze gekennzeichnet; nach der
// Wahl wird mit dem echten Widget nachgerendert, wie es die Regel
// verlangt.
//
// Ohne beide Schalter überspringt es sich (die CI setzt
// LONGPATH_GRAB_DIR global, darum der zweite):
//
//   LONGPATH_GRAB_DIR=/pfad LONGPATH_ENTWURF=1 \
//       ./build/tests/tst_flex_apd_entwurfsblaetter
//
// 144 dpi, damit die Vorschau wirkliche Größe zeigt.
// =================================================================
// Modification history (Longpath):
//   2026-11-01 — Angelegt. Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QStyleFactory>

#include "gui/setup/FlexApdPage.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/Theme.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Hausstil, docs/design/HAUSSTIL.md. Farbe bedeckt hoechstens zwei
// Prozent der Flaeche — alles andere ist Grau auf Fast-Schwarz.
const QColor kBlatt    ( 18,  19,  21);
const QColor kGrund    (  8,   8,  10);
const QColor kPanel    ( 12,  12,  14);
const QColor kKopfHell ( 22,  22,  26);
const QColor kKopfDunk ( 17,  17,  19);
const QColor kRahmen   ( 38,  38,  43);
const QColor kRahmenFein(25,  26,  29);
const QColor kWert     (196, 196, 201);
const QColor kBeschrift(142, 142, 147);
const QColor kSkala    ( 92,  92,  96);
const QColor kInaktiv  ( 61,  61,  65);
const QColor kAuswahlA ( 37,  74, 114);
const QColor kAuswahlB ( 30,  61,  95);
const QColor kAuswahlR ( 47,  92, 134);
const QColor kAuswahlT (207, 226, 245);
const QColor kMesswert (194, 146,  79);
const QColor kBestaetigt(111, 163, 132);

void blattSpeichern(const QString& dir, const QString& name,
                    const QImage& inhalt, const QString& titel,
                    const QString& fussnote)
{
    const int rand = 28;
    const int kopf = 54;
    const int fuss = 110;
    QImage blatt(inhalt.width() + 2 * rand,
                 inhalt.height() + kopf + fuss, QImage::Format_RGB32);
    blatt.fill(kBlatt);
    QPainter p(&blatt);
    p.setRenderHint(QPainter::Antialiasing);

    QFont f = p.font();
    f.setPointSizeF(13.0);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kWert);
    p.drawText(QRect(rand, 14, blatt.width() - 2 * rand, 24),
               Qt::AlignLeft | Qt::AlignVCenter, titel);

    p.drawImage(rand, kopf, inhalt);

    f.setBold(false);
    f.setPointSizeF(11.0);
    p.setFont(f);
    p.setPen(kBeschrift);
    p.drawText(QRect(rand, kopf + inhalt.height() + 12,
                     blatt.width() - 2 * rand, fuss - 14),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, fussnote);
    p.end();

    blatt.setDotsPerMeterX(static_cast<int>(144.0 / 0.0254));
    blatt.setDotsPerMeterY(static_cast<int>(144.0 / 0.0254));
    const QString pfad = QDir(dir).filePath(name);
    QVERIFY2(blatt.save(pfad), qPrintable(pfad));
    qInfo() << "Blatt:" << pfad << blatt.size();
}

/// Die echten Zeilen der FLEX-8400 vom 2026-11-01, zerlegt wie
/// `parseLine` es tut: Objektgrenze vor dem ersten '='.
void speise(RadioModel& rm, const QString& rumpf)
{
    const int firstEq = rumpf.indexOf(QLatin1Char('='));
    const int kvStart = rumpf.lastIndexOf(QLatin1Char(' '), firstEq);
    QMap<QString, QString> kvs;
    const QStringList paare =
        rumpf.mid(kvStart + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& paar : paare) {
        const int gleich = paar.indexOf(QLatin1Char('='));
        if (gleich > 0) { kvs.insert(paar.left(gleich), paar.mid(gleich + 1)); }
    }
    rm.flexApdStatusForTest(rumpf.left(kvStart).trimmed(), kvs);
}

void grundzustand(RadioModel& rm, bool ein, bool externerMesspunkt)
{
    speise(rm, QStringLiteral("apd enable=%1 configurable=1").arg(ein ? 1 : 0));
    speise(rm, QStringLiteral(
        "apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=%1")
        .arg(externerMesspunkt ? QStringLiteral("XVTA") : QStringLiteral("INVALID")));
    speise(rm, QStringLiteral(
        "apd sampler tx_ant=ANT2 valid_samplers=XVTA,RX_A selected_sampler=INVALID"));
    speise(rm, QStringLiteral(
        "apd sampler tx_ant=XVTA valid_samplers=RX_A selected_sampler=INVALID"));
    speise(rm, QStringLiteral(
        "apd slice=0 mmx=0 ant=ANT1 freq=14.100000 rfpower=%1 "
        "rx_error_mHz=0.000000 equalizer_active=%2 configurable=1")
        .arg(ein ? 75 : 5).arg(ein ? 1 : 0));
}

// ── Zeichenhilfen fuer die Skizzen ───────────────────────────────────

void karte(QPainter& p, const QRect& r, bool mitKopf = false)
{
    p.setPen(QPen(kRahmen, 1));
    p.setBrush(kPanel);
    p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 4, 4);
    if (mitKopf) {
        QLinearGradient g(r.topLeft(), QPoint(r.left(), r.top() + 34));
        g.setColorAt(0.0, kKopfHell);
        g.setColorAt(1.0, kKopfDunk);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(QRect(r.left() + 1, r.top() + 1, r.width() - 2, 33), 4, 4);
        p.fillRect(QRect(r.left() + 1, r.top() + 26, r.width() - 2, 8), kKopfDunk);
        p.setPen(QPen(kRahmenFein, 1));
        p.drawLine(r.left() + 1, r.top() + 34, r.right() - 1, r.top() + 34);
    }
}

void schrift(QPainter& p, const QRect& r, const QString& text,
             const QColor& farbe, double pt, bool fett = false,
             int ausrichtung = Qt::AlignLeft | Qt::AlignVCenter)
{
    QFont f = p.font();
    f.setPointSizeF(pt);
    f.setBold(fett);
    p.setFont(f);
    p.setPen(farbe);
    p.drawText(r, ausrichtung | Qt::TextWordWrap, text);
}

/// Ein Schalter im Hausstil: blau heisst anfassbar und an.
void schalter(QPainter& p, const QPoint& linksOben, bool an)
{
    const QRect bahn(linksOben.x(), linksOben.y(), 34, 18);
    p.setPen(QPen(an ? kAuswahlR : kRahmen, 1));
    if (an) {
        QLinearGradient g(bahn.topLeft(), bahn.bottomLeft());
        g.setColorAt(0.0, kAuswahlA);
        g.setColorAt(1.0, kAuswahlB);
        p.setBrush(g);
    } else {
        p.setBrush(QColor(0, 0, 0));
    }
    p.drawRoundedRect(bahn, 9, 9);
    p.setPen(Qt::NoPen);
    p.setBrush(an ? kAuswahlT : kInaktiv);
    p.drawEllipse(QPoint(an ? bahn.right() - 8 : bahn.left() + 9,
                         bahn.center().y() + 1), 6, 6);
}

/// Eine Station im Signalweg. `aktiv` bekommt als einzige Farbe.
void station(QPainter& p, const QRect& r, const QString& oben,
             const QString& unten, bool aktiv)
{
    p.setPen(QPen(aktiv ? kAuswahlR : kRahmenFein, 1));
    p.setBrush(aktiv ? QColor(20, 30, 42) : QColor(14, 14, 16));
    p.drawRoundedRect(r, 3, 3);
    schrift(p, r.adjusted(0, 5, 0, -22), oben,
            aktiv ? kAuswahlT : kBeschrift, 10.5, true, Qt::AlignCenter);
    schrift(p, r.adjusted(0, 25, 0, -4), unten,
            aktiv ? kBeschrift : kInaktiv, 9.5, false, Qt::AlignHCenter | Qt::AlignTop);
}

void pfeil(QPainter& p, int x1, int x2, int y, bool aktiv)
{
    p.setPen(QPen(aktiv ? kAuswahlR : kInaktiv, 1));
    p.drawLine(x1, y, x2 - 5, y);
    const QPoint spitze(x2, y);
    QPolygon dreieck;
    dreieck << spitze << QPoint(x2 - 6, y - 3) << QPoint(x2 - 6, y + 3);
    p.setBrush(aktiv ? kAuswahlR : kInaktiv);
    p.setPen(Qt::NoPen);
    p.drawPolygon(dreieck);
}

// ── Entwurf B: „Eine Karte" ──────────────────────────────────────────
QImage skizzeKarte(bool ein, bool externerMesspunkt)
{
    QImage bild(620, 252, QImage::Format_RGB32);
    bild.fill(kGrund);
    QPainter p(&bild);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r(12, 12, bild.width() - 24, bild.height() - 24);
    karte(p, r, true);

    schrift(p, QRect(r.left() + 14, r.top() + 2, 300, 32),
            QStringLiteral("PREDISTORTION"), kWert, 11.0, true);
    schalter(p, QPoint(r.right() - 52, r.top() + 9), ein);

    // Eine Zeile Zustand statt eines Absatzes Erklaerung.
    const QString zustand = ein
        ? QStringLiteral("Running · learned curve in circuit · ANT1 · 14.100 MHz · 75 %")
        : QStringLiteral("Off · nothing learned · ANT1 · 14.100 MHz");
    schrift(p, QRect(r.left() + 14, r.top() + 44, r.width() - 28, 20),
            zustand, ein ? kBestaetigt : kBeschrift, 10.5);

    // Messpunkt je Sendeantenne, als ruhige Zeilen statt Formular.
    const QStringList antennen{ QStringLiteral("ANT1"), QStringLiteral("ANT2"),
                                QStringLiteral("XVTA") };
    int y = r.top() + 76;
    for (int i = 0; i < antennen.size(); ++i) {
        const bool gewaehlt = (i == 0) && externerMesspunkt;
        schrift(p, QRect(r.left() + 14, y, 70, 22), antennen.at(i), kBeschrift, 10.5);
        const QRect feld(r.left() + 92, y, 230, 22);
        p.setPen(QPen(gewaehlt ? kAuswahlR : kRahmenFein, 1));
        p.setBrush(QColor(0, 0, 0));
        p.drawRoundedRect(feld, 3, 3);
        schrift(p, feld.adjusted(9, 0, -9, 0),
                gewaehlt ? QStringLiteral("XVTA")
                         : QStringLiteral("Internal coupler (own PA)"),
                gewaehlt ? kAuswahlT : kWert, 10.5);
        schrift(p, QRect(feld.right() + 12, y, 190, 22),
                i == 0 ? QStringLiteral("XVTA, RX_A possible")
                       : (i == 2 ? QStringLiteral("RX_A possible")
                                 : QStringLiteral("XVTA, RX_A possible")),
                kSkala, 9.5);
        y += 28;
    }

    schrift(p, QRect(r.left() + 14, r.bottom() - 44, r.width() - 160, 34),
            QStringLiteral("The radio computes the correction itself. "
                           "Without an external sampler it corrects its own PA."),
            kSkala, 9.5);
    const QRect knopf(r.right() - 132, r.bottom() - 40, 118, 26);
    p.setPen(QPen(kRahmen, 1));
    p.setBrush(QColor(22, 22, 26));
    p.drawRoundedRect(knopf, 3, 3);
    schrift(p, knopf, QStringLiteral("Discard curve"), kWert, 10.0, false, Qt::AlignCenter);

    p.end();
    return bild;
}

// ── Entwurf C: „Signalweg" ───────────────────────────────────────────
QImage skizzeSignalweg(bool ein, bool externerMesspunkt)
{
    QImage bild(620, 288, QImage::Format_RGB32);
    bild.fill(kGrund);
    QPainter p(&bild);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r(12, 12, bild.width() - 24, bild.height() - 24);
    karte(p, r, true);

    schrift(p, QRect(r.left() + 14, r.top() + 2, 300, 32),
            QStringLiteral("PREDISTORTION"), kWert, 11.0, true);
    schalter(p, QPoint(r.right() - 52, r.top() + 9), ein);

    // Der Weg. Nur die Stationen, die wirklich im Spiel sind, tragen
    // Farbe — der Rest bleibt grau, Zwei-Prozent-Regel.
    const int y = r.top() + 56;
    const int h = 46;
    const int w = 108;
    const int lueck = 26;
    int x = r.left() + 18;

    station(p, QRect(x, y, w, h), QStringLiteral("TX"),
            QStringLiteral("mic → radio"), ein);
    pfeil(p, x + w, x + w + lueck, y + h / 2, ein);
    x += w + lueck;

    station(p, QRect(x, y, w, h), QStringLiteral("PA"),
            externerMesspunkt ? QStringLiteral("external amp")
                              : QStringLiteral("own 100 W"), ein);
    pfeil(p, x + w, x + w + lueck, y + h / 2, ein);
    x += w + lueck;

    station(p, QRect(x, y, w, h), QStringLiteral("COUPLER"),
            externerMesspunkt ? QStringLiteral("amp sampler")
                              : QStringLiteral("built-in"), ein);
    pfeil(p, x + w, x + w + lueck, y + h / 2, ein);
    x += w + lueck;

    station(p, QRect(x, y, w, h), QStringLiteral("FEEDBACK"),
            externerMesspunkt ? QStringLiteral("XVTA") : QStringLiteral("internal"), ein);

    // Die Rueckfuehrung als Bogen unter den Stationen — sie ist der
    // Grund, warum das Ganze ueberhaupt funktioniert.
    p.setPen(QPen(ein ? kAuswahlR : kInaktiv, 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    const int bogenY = y + h + 16;
    p.drawLine(x + w / 2, y + h, x + w / 2, bogenY);
    p.drawLine(x + w / 2, bogenY, r.left() + 18 + w / 2, bogenY);
    p.drawLine(r.left() + 18 + w / 2, bogenY, r.left() + 18 + w / 2, y + h);
    schrift(p, QRect(r.left() + 18, bogenY + 2, r.width() - 36, 18),
            ein ? QStringLiteral("correction applied in the radio")
                : QStringLiteral("loop open — predistortion off"),
            ein ? kBestaetigt : kSkala, 9.5, false, Qt::AlignHCenter | Qt::AlignTop);

    // Darunter die Bedienung, knapp.
    int zy = bogenY + 30;
    schrift(p, QRect(r.left() + 14, zy, 110, 22),
            QStringLiteral("Sampler ANT1"), kBeschrift, 10.5);
    const QRect feld(r.left() + 132, zy, 220, 22);
    p.setPen(QPen(externerMesspunkt ? kAuswahlR : kRahmenFein, 1));
    p.setBrush(QColor(0, 0, 0));
    p.drawRoundedRect(feld, 3, 3);
    schrift(p, feld.adjusted(9, 0, -9, 0),
            externerMesspunkt ? QStringLiteral("XVTA")
                              : QStringLiteral("Internal coupler (own PA)"),
            externerMesspunkt ? kAuswahlT : kWert, 10.5);
    const QRect knopf(r.right() - 132, zy, 118, 22);
    p.setPen(QPen(kRahmen, 1));
    p.setBrush(QColor(22, 22, 26));
    p.drawRoundedRect(knopf, 3, 3);
    schrift(p, knopf, QStringLiteral("Discard curve"), kWert, 10.0, false, Qt::AlignCenter);

    zy += 30;
    schrift(p, QRect(r.left() + 14, zy, r.width() - 28, 34),
            ein ? QStringLiteral("ANT1 · 14.100 MHz · 75 % · learned curve in circuit")
                : QStringLiteral("ANT1 · 14.100 MHz · nothing learned yet"),
            ein ? kMesswert : kSkala, 10.0);

    p.end();
    return bild;
}

}  // namespace

class TestFlexApdEntwurfsblaetter : public QObject {
    Q_OBJECT

private slots:
    void blaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        if (qEnvironmentVariableIntValue("LONGPATH_ENTWURF") != 1) {
            QSKIP("kein LONGPATH_ENTWURF=1 — Zeichenwerkzeug, kein Prüflauf");
        }
        QDir().mkpath(dir);

        // Dieselben Zeilen wie main.cpp: ohne sie rendert Qt im hellen
        // Systemstil, und das Blatt zeigt nicht, was der Betreiber hat.
        qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        Longpath::applyDarkPalette(*qApp);
        Longpath::applyAppBaselineQss(*qApp);
        qApp->installEventFilter(new Longpath::Style::ThemeFilter(qApp));
        Longpath::Style::Theme::instance().applyStoredChoice();

        // ── Entwurf A: das echte Fenster ─────────────────────────────
        {
            RadioModel rm;
            grundzustand(rm, /*ein=*/false, /*externerMesspunkt=*/false);
            FlexApdPage seite(&rm);
            seite.resize(620, seite.sizeHint().height());
            seite.show();
            QVERIFY(QTest::qWaitForWindowExposed(&seite, 5000));
            blattSpeichern(dir, "apd-C-gebaut-aus.png", seite.grab().toImage(),
                QStringLiteral("Entwurf C — GEBAUT, das echte Fenster, aus"),
                QStringLiteral("Das ECHTE Widget nach dem Umbau auf Entwurf C, "
                               "gefüttert mit den gemessenen Zeilen der 8400. "
                               "Aus heisst: der Bogen ist offen, die Stationen "
                               "sind grau, der Koppler ist der eingebaute."));
        }
        {
            RadioModel rm;
            grundzustand(rm, /*ein=*/true, /*externerMesspunkt=*/true);
            FlexApdPage seite(&rm);
            seite.resize(620, seite.sizeHint().height());
            seite.show();
            QVERIFY(QTest::qWaitForWindowExposed(&seite, 5000));
            blattSpeichern(dir, "apd-C-gebaut-ein.png", seite.grab().toImage(),
                QStringLiteral("Entwurf C — GEBAUT, das echte Fenster, ein, Messpunkt XVTA"),
                QStringLiteral("Derselbe Aufbau im laufenden Fall: der Weg trägt "
                               "Farbe, die Beschriftung wandert auf „external "
                               "amp“ und „amp sampler“, und die Messwerte stehen "
                               "unten in der Messwert-Farbe."));
        }

        // ── Entwurf B: eine Karte ────────────────────────────────────
        blattSpeichern(dir, "apd-B-karte-aus.png",
            skizzeKarte(/*ein=*/false, /*externerMesspunkt=*/false),
            QStringLiteral("Entwurf B — eine Karte (Skizze), aus"),
            QStringLiteral("Kopfstreifen mit dem Schalter rechts, darunter EINE "
                           "Zustandszeile statt des Absatzes. Die drei Messpunkte "
                           "stehen als ruhige Zeilen, rechts daneben leise, was "
                           "die Maschine jeweils zulässt."));
        blattSpeichern(dir, "apd-B-karte-ein.png",
            skizzeKarte(/*ein=*/true, /*externerMesspunkt=*/true),
            QStringLiteral("Entwurf B — eine Karte (Skizze), ein, Messpunkt XVTA"),
            QStringLiteral("Im laufenden Fall trägt die Zustandszeile die Farbe "
                           "„bestätigt“, und das gewählte Feld ist das einzige "
                           "blaue Element. Zwei-Prozent-Regel eingehalten."));

        // ── Entwurf C: Signalweg ─────────────────────────────────────
        blattSpeichern(dir, "apd-C-signalweg-aus.png",
            skizzeSignalweg(/*ein=*/false, /*externerMesspunkt=*/false),
            QStringLiteral("Entwurf C — Signalweg (Skizze), aus"),
            QStringLiteral("Zeigt, WO sich die Schleife schliesst: TX → PA → "
                           "Koppler → Rückweg, mit der Rückführung als Bogen. "
                           "Aus heisst: der Bogen ist offen und grau. Das "
                           "beantwortet die Frage, die bei der Vorverzerrung "
                           "wirklich schwer ist — woher die Maschine hört."));
        blattSpeichern(dir, "apd-C-signalweg-ein.png",
            skizzeSignalweg(/*ein=*/true, /*externerMesspunkt=*/true),
            QStringLiteral("Entwurf C — Signalweg (Skizze), ein, Messpunkt XVTA"),
            QStringLiteral("Mit externem Abgriff heisst die PA „external amp“ und "
                           "der Koppler „amp sampler“ — derselbe Weg, andere "
                           "Beschriftung, ohne zweite Seite. Unten die Messwerte "
                           "in der Messwert-Farbe."));
    }
};

QTEST_MAIN(TestFlexApdEntwurfsblaetter)
#include "tst_flex_apd_entwurfsblaetter.moc"
