// =================================================================
// src/gui/widgets/BandwidthFilterPane.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/BandwidthFilterPane.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Rand um die Zeichenflaeche. Oben mehr, weil dort die Beschriftungen
// der Kanten und die Breitenpille sitzen; unten fuer die Skala.
constexpr int kPadX      = 6;
constexpr int kPadTop    = 30;
constexpr int kPadBottom = 20;

// Eine Zahl auf der Achse: Megahertz mit drei Nachkommastellen, wie in
// der Vorlage („13.137"). Kilohertz waeren eine Ziffer mehr und sagen
// dasselbe.
QString axisLabel(double hz)
{
    return QString::number(hz / 1.0e6, 'f', 3);
}

// Eine Kantenangabe. Unter 1 kHz in Hertz, darueber in Kilohertz mit
// zwei Stellen — so macht es die Vorlage, und so bleibt die Zahl kurz
// genug, um ueber der Kante zu stehen.
//
// Betreiber 2026-09-03, mit Nachdruck: "minus darf nie!!!!!" — LSB legt
// beide Kanten unterhalb des Traegers, technisch also negativ, aber das
// gehoert nicht in die Anzeige. std::abs() schuetzte bisher nur die
// KILOHERTZ-JA-ODER-NEIN-Schwelle oben, nicht die ausgegebene Zahl
// selbst — "%1" mit dem rohen (moeglicherweise negativen) hz ergab
// "-150 Hz". Jetzt wird ueberall dieselbe Betragszahl verwendet wie die
// LOW/WIDTH/HIGH-Felder daneben (BandwidthFilterApplet::refreshNumbers).
QString cutLabel(int hz)
{
    const int mag = std::abs(hz);
    if (mag < 1000) {
        return QStringLiteral("%1 Hz").arg(mag);
    }
    return QStringLiteral("%1 kHz").arg(mag / 1000.0, 0, 'f', 2);
}

QString widthLabel(int hz)
{
    if (hz < 1000) { return QStringLiteral("%1 Hz").arg(hz); }
    return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 1);
}

} // namespace

BandwidthFilterPane::BandwidthFilterPane(QWidget* parent)
    : QWidget(parent)
    , m_accent(QColor(Style::role("accent", Style::kAccent)))
{
    // Klein genug fuer den Platz im RxApplet (dort rund 92 px), gross
    // genug, dass Beschriftungen und Achse hineinpassen.
    setMinimumHeight(84);
    setMinimumWidth(180);
    setMouseTracking(true);
}

void BandwidthFilterPane::setLabel(const QString& text)
{
    if (m_label == text) { return; }
    m_label = text;
    update();
}

void BandwidthFilterPane::setAccent(const QColor& c)
{
    if (m_accent == c) { return; }
    m_accent = c;
    update();
}

void BandwidthFilterPane::setVfoFrequency(double hz)
{
    if (qFuzzyCompare(m_vfoHz, hz)) { return; }
    m_vfoHz = hz;
    update();
}

void BandwidthFilterPane::setTrace(const QVector<float>& dbm)
{
    // ── Beruhigen, wie es OpenHPSDR zeigt ───────────────────────────
    //
    // Der Betreiber am 2026-08-23: "extrem unruhig, linie zu dick!
    // baue es genau wie bei openhpsdr."
    //
    // Die Rohwerte kommen mit 20 Bildern je Sekunde und sind
    // Spitzenwerte — jede Stuetzstelle springt bei jedem Bild. Bei
    // OpenHPSDR steht dort eine ruhige, duenne Kurve.
    //
    // Zwei Mittel, in dieser Reihenfolge:
    //
    //   1. ZEITLICH: exponentiell gleiten mit alpha = 0,22. Das ist
    //      dieselbe Familie wie die Glaettung im Panadapter
    //      (LogRecursive), nur ohne dessen Parameterwerk — hier geht
    //      es ums Augenmass beim Kantenziehen, nicht um Messtechnik.
    //      Ein Traeger steigt damit in rund einer Zehntelsekunde auf,
    //      das Zappeln verschwindet.
    //
    // ÖRTLICH wird NICHT geglaettet, und das ist eine Korrektur:
    //
    // Die erste Fassung mittelte ueber drei Stuetzstellen, mit dem
    // Kommentar, das verschlucke keinen echten Traeger — "der ist
    // breiter als drei Punkte". GEMESSEN war das falsch: ein Traeger
    // auf EINER Stuetzstelle, -60 dBm im -120er Rauschen, kam nach dem
    // Fenster bei -100 an. VIERZIG Dezibel weg, durch eine Zeile, die
    // ich fuer harmlos erklaert hatte.
    //
    // Zeitliches Gleiten allein genuegt: Rauschen ist von Bild zu Bild
    // zufaellig und mittelt sich damit ohnehin weg, ein Traeger steht
    // still und bleibt stehen.
    if (dbm.isEmpty()) {
        if (!m_trace.isEmpty()) { m_trace.clear(); update(); }
        return;
    }

    const QVector<float>& in = dbm;

    if (m_trace.size() != in.size()) {
        m_trace = in;                      // Groesse gewechselt: neu setzen
    } else {
        // ── Schnell hoch, gemaechlich runter ────────────────────────
        //
        // Die erste Fassung glich in BEIDE Richtungen gleich schnell
        // (alpha 0,22). Der Betreiber sah daraufhin: "die form bleibt
        // auch immer leicht zu sehen, zeitversetzt" — ein blasses
        // Nachbild, das der Kurve hinterherlaeuft. Genau das macht
        // eine symmetrische Glaettung: sie verzoegert das Steigen
        // ebenso wie das Fallen.
        //
        // Richtig ist ungleich: STEIGEN sofort (ein Traeger, der
        // aufgeht, soll da sein, wenn er da ist), FALLEN gemaechlich
        // (dann beruhigt sich das Rauschen). Dieselbe Bauart wie ein
        // Spitzenwertzeiger mit Ruecklauf — und das ist auch, was
        // OpenHPSDR und Thetis an dieser Stelle zeigen.
        constexpr float aUp   = 0.75f;   // fast sofort
        constexpr float aDown = 0.16f;   // ruhiger Ruecklauf
        for (int i = 0; i < in.size(); ++i) {
            const float a = (in[i] > m_trace[i]) ? aUp : aDown;
            m_trace[i] = m_trace[i] * (1.0f - a) + in[i] * a;
        }
    }
    update();
}

void BandwidthFilterPane::setSpan(int hz)
{
    // Unter 2 kHz wird die Achse unlesbar, ueber 40 kHz verschwindet
    // der Durchlass zu einem Strich.
    const int clamped = std::clamp(hz, 2000, 40000);
    if (m_spanHz == clamped) { return; }
    m_spanHz = clamped;
    update();
}

void BandwidthFilterPane::setFilter(int low, int high)
{
    if (m_low == low && m_high == high) { return; }
    m_low  = low;
    m_high = high;
    update();
}

void BandwidthFilterPane::setHasFrequency(bool on)
{
    if (m_hasFrequency == on) { return; }
    m_hasFrequency = on;
    update();
}

QRect BandwidthFilterPane::plotRect() const
{
    return QRect(kPadX, kPadTop,
                 std::max(1, width() - 2 * kPadX),
                 std::max(1, height() - kPadTop - kPadBottom));
}

int BandwidthFilterPane::hzToX(int hz) const
{
    const QRect r = plotRect();
    const double frac = (static_cast<double>(hz) + m_spanHz / 2.0) / m_spanHz;
    return r.left() + static_cast<int>(std::lround(frac * r.width()));
}

int BandwidthFilterPane::xToHz(int x) const
{
    const QRect r = plotRect();
    const double frac = static_cast<double>(x - r.left())
                      / std::max(1, r.width());
    return static_cast<int>(std::lround(frac * m_spanHz - m_spanHz / 2.0));
}

BandwidthFilterPane::Zone BandwidthFilterPane::zoneAt(int x) const
{
    const int xl = hzToX(m_low);
    const int xh = hzToX(m_high);

    // Bei beiden in Reichweite gewinnt die naehere — sonst laesst sich
    // ein sehr schmaler Durchlass nur an einer Seite anfassen.
    const bool nearLow  = std::abs(x - xl) <= kGrabPx;
    const bool nearHigh = std::abs(x - xh) <= kGrabPx;
    if (nearLow && nearHigh) {
        return (std::abs(x - xl) <= std::abs(x - xh)) ? Zone::LowEdge
                                                      : Zone::HighEdge;
    }
    if (nearLow)  { return Zone::LowEdge; }
    if (nearHigh) { return Zone::HighEdge; }
    if (x > xl && x < xh) { return Zone::Body; }
    return Zone::None;
}

// ── Zeichnen ─────────────────────────────────────────────────────────

void BandwidthFilterPane::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = plotRect();

    // Grund: eine Mulde, oben minimal heller. Dieselbe Richtung wie bei
    // den Eingabefeldern — versenkt, nicht aufgelegt.
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(0x10, 0x10, 0x14));
    bg.setColorAt(1.0, QColor(Style::role("inset-bg", Style::kInsetBg)));
    p.fillRect(rect(), bg);

    // Kein Rahmenkasten mehr.
    //
    // OpenHPSDR zeichnet die Flaeche randlos — das Bild steht fuer
    // sich, statt in einem Kaestchen zu sitzen. Auf dem Foto des
    // Betreibers (2026-08-23) ist das einer der auffaelligsten
    // Unterschiede: bei uns rahmte eine Linie jede Flaeche ein, dort
    // trennen nur die Zwischenraeume.

    // Senkrechte Hilfslinien alle 2 kHz — dieselbe Teilung wie die
    // Beschriftungen darunter, damit die Zahlen etwas zum Festhalten
    // haben.
    const QColor grid(0x14, 0x14, 0x18);
    p.setPen(grid);
    for (int hz = -m_spanHz / 2; hz <= m_spanHz / 2; hz += 2000) {
        const int x = hzToX(hz);
        p.drawLine(x, r.top(), x, r.bottom());
    }

    // ── Das Signal ──────────────────────────────────────────────────
    //
    // Vorbild Zeus Link, vorgefuehrt am 2026-08-22: der Bandfilter
    // zeigt dort das ECHTE Spektrum, und erst dadurch sieht man, ob
    // die Kante an der richtigen Stelle sitzt. Ohne Kurve ist das
    // Fenster ein Zahlenformular.
    //
    // Der Ausschnitt kommt vom Panadapter (SpectrumWidget::
    // dbmOverRange) — dieselbe Abbildung, dieselbe Kalibrierung. Eine
    // zweite eigene waere ein zweiter Ort, an dem sie falsch sein
    // kann.
    //
    // Massstab: der Kopf des Fensters gehoert den Beschriftungen, also
    // beginnt die Kurve darunter. Der Pegelbereich richtet sich nach
    // dem, was da ist (mit Mindestspanne), sonst klebt eine leise
    // Band-Mitte am Boden und man sieht nichts.
    if (m_trace.size() >= 2) {
        // ── FESTER Massstab ueber dem Rauschflur ────────────────────
        //
        // Der Betreiber am 2026-08-23, mit drei Bildern von OpenHPSDR
        // Zeus: "wo kein signal ist, ist die linie am boden" und
        // "wenn kein signal ist, linie bei 0, auch bei den
        // sprechpausen".
        //
        // Genau daran lag der Unterschied — nicht an Farbe oder
        // Strichstaerke. Wir dehnten bisher IMMER auf Minimum bis
        // Maximum der sichtbaren Werte. Ist nur Rauschen da, wird
        // dessen Zappeln von ein paar Dezibel auf die volle Hoehe
        // gezogen: die Flaeche sieht belebt aus, wo nichts ist. In den
        // Sprechpausen sprang die Kurve deshalb jedes Mal auf.
        //
        // Die Vorlage haelt einen FESTEN Bereich: der Rauschflur liegt
        // unten am Boden, und nur was wirklich darueber ist, ragt
        // heraus. Sechzig Dezibel — dieselbe Groessenordnung, die auch
        // der Panadapter zeigt, und genug fuer den staerksten Traeger.
        //
        // Der Boden ist ein UNTERES PERZENTIL, kein Minimum: ein
        // einzelner Ausreisser nach unten (eine Luecke, ein
        // Nulldurchgang) wuerde sonst die ganze Skala verschieben.
        QVector<float> sorted = m_trace;
        std::sort(sorted.begin(), sorted.end());
        const float lo = sorted.at(sorted.size() / 10);   // 10. Perzentil
        // ── 40 dB, nicht 60 ─────────────────────────────────────────
        //
        // Der Betreiber am 2026-08-23: "der eigentliche filterbereich
        // sollte natürlich stark nach oben gehen wie bei open."
        //
        // Auf seinen Bildern reichen die Signale fast bis an den
        // oberen Rand, waehrend der Rauschflur unten klebt. Mit 60 dB
        // Spanne blieb bei uns ein Traeger von 30 dB ueber dem Flur
        // auf HALBER Hoehe stehen — richtig gerechnet, aber kraftlos.
        //
        // Vierzig Dezibel: ein starkes Signal fuellt die Flaeche, ein
        // sehr starkes stoesst oben an. Genau das tut die Vorlage
        // auch — auf einem seiner Bilder laeuft die rechte Spitze in
        // den Rand.
        const float hi = lo + 40.0f;
        const int top = r.top() + 30;          // Platz fuer die Marken
        const int bot = r.bottom() - 2;
        const double yScale = (bot - top) / static_cast<double>(hi - lo);

        QPolygonF poly;
        poly.reserve(m_trace.size() + 2);
        poly << QPointF(r.left(), bot);
        for (int i = 0; i < m_trace.size(); ++i) {
            const double x = r.left()
                + (r.width() - 1.0) * i / (m_trace.size() - 1.0);
            const double y = bot - (m_trace[i] - lo) * yScale;
            poly << QPointF(x, qBound<double>(top, y, bot));
        }
        poly << QPointF(r.right(), bot);

        // ── GERADE Striche, so wie die Vorlage ──────────────
        //
        // Hier lag einen Tag lang ein Catmull-Rom-Zug, weil ich den
        // Satz "das sind 2 welten" als "die Vorlage rundet" gelesen
        // habe. Die Bilder vom 2026-08-23 sagen das Gegenteil: auf
        // dem Ausschnitt 14.127–14.135 hat der Zug SCHARFE Ecken,
        // Ecke an Ecke, ohne jede Rundung.
        //
        // Der ruhige Eindruck kommt nicht vom Weg zwischen den
        // Punkten, sondern von ihrer Zahl — die Vorlage setzt sie
        // rund zwoelf Bildpunkte auseinander. Das ist jetzt in
        // BandwidthFilterApplet geregelt; hier wird wieder schlicht
        // verbunden.
        QPainterPath tracePath;
        tracePath.addPolygon(poly);

        // ── Pegelraster ─────────────────────────────────────────────
        //
        // Drei waagrechte Linien mit dBm-Marke. Ohne sie ist die Kurve
        // eine Form ohne Massstab: man sieht, DASS da etwas ist, aber
        // nicht, wie stark. Zeus zeigt an derselben Stelle einen
        // Pegelwert im Durchlass.
        {
            QFont tiny = font();
            tiny.setPointSizeF(std::max(6.0, tiny.pointSizeF() - 3.5));
            p.setFont(tiny);
            const QColor gridLine(0x1c, 0x1c, 0x22);
            const QColor gridText(Style::role("text-scale", Style::kTextScale));
            for (int k = 1; k <= 3; ++k) {
                const double frac = k / 4.0;
                const int y = static_cast<int>(bot - (bot - top) * frac);
                p.setPen(gridLine);
                p.drawLine(r.left() + 1, y, r.right() - 1, y);
                const int dbm = static_cast<int>(std::lround(lo + (hi - lo) * frac));
                p.setPen(gridText);
                p.drawText(QRect(r.right() - 46, y - 7, 42, 12),
                           Qt::AlignRight | Qt::AlignVCenter,
                           QStringLiteral("%1").arg(dbm));
            }
        }

        // ── Nur der Durchlass ist GEFUELLT ──────────────────────────
        //
        // Vorbild OpenHPSDR Zeus 2.0, vom Betreiber am 2026-08-22
        // gezeigt: dort ist die Kurve INNERHALB des Filters flaechig
        // gefuellt, ausserhalb nur eine duenne Linie. Das ist der
        // Griff, der das Fenster lesbar macht — man sieht auf einen
        // Blick, was durchkommt und was die Kante abschneidet.
        //
        // Er hatte es so beschrieben: "beim bandfilter geht es darum,
        // dass eigentlich alles gleich aussieht ... gut gefallen hat
        // mir openhpsdr."
        const int xlF = qBound(r.left(), hzToX(m_low),  r.right());
        const int xhF = qBound(r.left(), hzToX(m_high), r.right());

        QColor traceLine(Style::role("trace", "#c8a06a"));
        // Die Vorlage traegt ein deutlich kraeftigeres Bernstein als
        // unser gedaempftes #c8a06a. Ohne diesen Schritt bleibt die
        // Kurve neben dem tuerkisen Durchlass blass.
        traceLine = traceLine.lighter(118);
        traceLine.setHsv(traceLine.hue(),
                         qMin(255, int(traceLine.saturation() * 1.35)),
                         traceLine.value());
        // ── Hof, Verlauf, Linie — in dieser Reihenfolge ─────────────
        //
        // Der Betreiber hat das OpenHPSDR-Bild vergroessern lassen,
        // damit ich den Filterbereich genau sehe (2026-08-23). Der
        // auffaelligste Unterschied ist nicht Farbe und nicht
        // Strichstaerke, sondern ein HOF um die Kurve: ein breiter,
        // weicher Schein, der nach aussen ausblendet. Daraus entsteht
        // der Eindruck, den er mit "wirkt eher 3D" beschrieben hat.
        //
        // EIN breiter Strich waere falsch — der gibt einen Balken.
        // Drei Durchgaenge von breit und blass nach schmal und
        // kraeftig geben den Verlauf.
        {
            struct GlowPass { double width; int alpha; };
            // Zwei Durchgaenge, nicht drei: mit den grob gesetzten
            // Stuetzstellen traegt die Linie selbst wieder, und der
            // dritte Durchgang machte aus jeder Zacke einen Klumpen.
            static const GlowPass kGlow[] = {
                {7.0, 12}, {3.0, 22},
            };
            p.setBrush(Qt::NoBrush);
            for (const GlowPass& g : kGlow) {
                QColor glow(traceLine);
                glow.setAlpha(g.alpha);
                QPen glowPen(glow, g.width);
                glowPen.setJoinStyle(Qt::RoundJoin);
                glowPen.setCapStyle(Qt::RoundCap);
                p.setPen(glowPen);
                p.drawPath(tracePath);
            }
        }

        // ── Weicher Verlauf unter der GANZEN Kurve ──────────────────
        //
        // So macht es die Vorlage: eine duenne Linie, darunter ein
        // Verlauf, der nach unten ausblendet — ueber die ganze Breite,
        // nicht nur im Durchlass. Der Durchlass wird durch seinen
        // Umriss kenntlich, nicht durch eine zweite Fuellung.
        //
        // Hier stand eine DECKENDE Fuellung, auf den Durchlass
        // beschnitten. Sie sollte am 2026-08-23 schon zweimal weichen
        // — beide Ersetzungen sind stillschweigend danebengegangen,
        // und die Festschreibungen behaupteten die Aenderung trotzdem.
        // Aufgefallen ist es erst, als der Schein im Bild fehlte und
        // ein grep nach dem neuen Namen NULL Treffer lieferte.
        {
            QLinearGradient grad(0, top, 0, bot);
            QColor c0(traceLine); c0.setAlpha(130);
            QColor c1(traceLine); c1.setAlpha(10);
            grad.setColorAt(0.0, c0);
            grad.setColorAt(1.0, c1);
            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawPath(tracePath);
        }

        // Zarte Linie, wie in der Vorlage. 1,2 war fuer eine Flaeche
        // dieser Groesse zu fett.
        p.setPen(QPen(traceLine, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tracePath);

        // ── Die blasse Bezugslinie ──────────────────────────────────
        //
        // Auf den Bildern von OpenHPSDR laeuft quer durchs Bild eine
        // zweite, viel blassere Linie — deutlich ueber dem Rauschflur
        // und ruhig. Das ist der GEGLAETTETE Mittelwert: er sagt, wo
        // der Empfaenger im Mittel steht, und macht damit sichtbar, ob
        // eine Spitze wirklich heraussticht oder nur der Flur atmet.
        //
        // Traeges Gleiten (alpha 0,02), damit sie sich nicht mit der
        // Kurve mitbewegt — eine Bezugslinie, die zappelt, ist keine.
        {
            if (m_avgLine.size() != m_trace.size()) {
                m_avgLine = m_trace;
            } else {
                constexpr float aa = 0.02f;
                for (int i = 0; i < m_trace.size(); ++i) {
                    m_avgLine[i] = m_avgLine[i] * (1.0f - aa) + m_trace[i] * aa;
                }
            }
            QColor avg(Style::role("text-scale", Style::kTextScale));
            avg.setAlpha(70);
            p.setPen(QPen(avg, 1.0));
            QPolygonF ap;
            ap.reserve(m_avgLine.size());
            for (int i = 0; i < m_avgLine.size(); ++i) {
                const double x = r.left()
                    + (r.width() - 1.0) * i / (m_avgLine.size() - 1.0);
                const double y = bot - (m_avgLine[i] - lo) * yScale;
                ap << QPointF(x, qBound<double>(top, y, bot));
            }
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(ap);
        }

        // ── Feinskala oben im Durchlass ─────────────────────────────
        //
        // In der Vorlage laeuft am oberen Rand des Durchlasses eine
        // duenne Linie mit drei Teilstrichen. Sie sagt nichts Neues —
        // sie gibt dem Rechteck einen Boden und macht auf einen Blick
        // klar, dass es ein MASS ist und keine Markierung.
        if (xhF - xlF > 40) {
            QColor tick(m_accent);
            tick.setAlpha(150);
            p.setPen(tick);
            const int yT = top + 8;
            p.drawLine(xlF + 6, yT, xhF - 6, yT);
            for (int k = 1; k <= 3; ++k) {
                const int x = xlF + (xhF - xlF) * k / 4;
                p.drawLine(x, yT - 3, x, yT + 3);
            }
        }

        // ── Was liegt DRIN? Ablage und Pegel ────────────────────────
        //
        // OpenHPSDR Zeus zeigt unter dem Durchlass eine Reihe Zellen:
        // "+324 / 14dB", "+777 / 11dB", "+1.5k / 12dB". Das ist die
        // Antwort auf die Frage, fuer die man dieses Fenster aufmacht —
        // welche Anteile kommen durch, und wie stark.
        //
        // Wir nehmen die vier staerksten Spitzen im Durchlass, die
        // mindestens 6 dB ueber dem leisesten Punkt darin liegen (sonst
        // benennt man Rauschen), und mit Mindestabstand, damit nicht
        // viermal derselbe Buckel gezaehlt wird.
        if (xhF - xlF > 60 && bot - top > 40) {
            const int n = m_trace.size();
            auto hzAt = [&](int i) {
                return -m_spanHz / 2.0 + m_spanHz * double(i) / (n - 1);
            };
            const int i0 = qBound(0, int((m_low  + m_spanHz / 2.0)
                                         / m_spanHz * (n - 1)), n - 1);
            const int i1 = qBound(0, int((m_high + m_spanHz / 2.0)
                                         / m_spanHz * (n - 1)), n - 1);
            float floorDb = m_trace[qMin(i0, i1)];
            for (int i = qMin(i0, i1); i <= qMax(i0, i1); ++i) {
                floorDb = qMin(floorDb, m_trace[i]);
            }

            // ── FESTE Eimer, nicht die fuenf hoechsten Spitzen ──
            //
            // Hier stand eine Spitzensuche: sie fand die fuenf
            // staerksten Buckel und beschriftete nur die. Stand ein
            // einziger Traeger im Durchlass, blieb genau EINE Zelle
            // uebrig — das Blatt vom 2026-08-23 zeigt das.
            //
            // Die Vorlage macht es anders. Auf dem Bild mit dem
            // ruhigen Durchlass stehen ACHT Zellen nebeneinander
            // (+377, +755, +939, +1.4k, +1.8k, +1.9k, +2.3k, +2.4k)
            // und fast alle melden 0 dB. Die Abstaende sind
            // unregelmaessig, die Zahl der Zellen nicht: der
            // Durchlass wird in gleich breite Eimer geteilt, und jede
            // Zelle nennt die Lage IHRES groessten Wertes. Die Reihe
            // steht damit immer, auch wenn nichts zu hoeren ist.
            struct Peak { int i; float db; };
            QVector<Peak> peaks;
            {
                const int lo = qMin(i0, i1);
                const int hi = qMax(i0, i1);
                const int pxWide = qAbs(hzToX(m_high) - hzToX(m_low));
                // Rund 46 px je Zelle — schmaler, und "+1.4k" passt
                // nicht mehr hinein.
                const int cells = qBound(1, qMin(pxWide / 46, hi - lo), 8);
                for (int k = 0; k < cells; ++k) {
                    const int s0 = lo + (hi - lo) * k / cells;
                    const int s1 = lo + (hi - lo) * (k + 1) / cells;
                    if (s1 <= s0) { continue; }
                    int best = s0;
                    for (int i = s0; i < s1; ++i) {
                        if (m_trace[i] > m_trace[best]) { best = i; }
                    }
                    peaks.append({best, m_trace[best]});
                }
            }

            if (!peaks.isEmpty()) {
                QFont cell = font();
                cell.setPointSizeF(std::max(6.0, cell.pointSizeF() - 3.5));
                p.setFont(cell);
                const int cw = (xhF - xlF) / peaks.size();
                const int cy = bot - 32;
                for (int k = 0; k < peaks.size(); ++k) {
                    const QRect box(xlF + k * cw, cy, cw - 1, 28);
                    p.setPen(Qt::NoPen);
                    // Leiser als vorher (110 -> 150 auf dunklem Grund
                    // heisst: weniger Kontrast zur Flaeche). Die Zellen
                    // sind Beiwerk, nicht die Hauptsache.
                    p.setBrush(QColor(0, 0, 0, 150));
                    p.drawRect(box);

                    const double offHz = hzAt(peaks[k].i);
                    const QString offTxt = (qAbs(offHz) >= 1000.0)
                        ? QStringLiteral("%1%2k")
                              .arg(offHz < 0 ? QStringLiteral("-")
                                             : QStringLiteral("+"))
                              .arg(qAbs(offHz) / 1000.0, 0, 'f', 1)
                        : QStringLiteral("%1%2")
                              .arg(offHz < 0 ? QStringLiteral("-")
                                             : QStringLiteral("+"))
                              .arg(int(qAbs(offHz)));
                    QColor cellInk(Style::role("text", Style::kTextPrimary));
                    cellInk.setAlpha(190);
                    p.setPen(cellInk);
                    p.drawText(QRect(box.x(), box.y() + 1, box.width(), 13),
                               Qt::AlignCenter, offTxt);
                    p.setPen(QColor(Style::role("text-scale",
                                                Style::kTextScale)));
                    p.drawText(QRect(box.x(), box.y() + 14, box.width(), 13),
                               Qt::AlignCenter,
                               QStringLiteral("%1dB")
                                   .arg(int(std::lround(peaks[k].db - floorDb))));
                }
            }
        }
    }

    // Die Nulllinie ist die VFO-Frequenz. Sie traegt einen eigenen Ton,
    // sonst ist sie eine Hilfslinie unter vielen.
    {
        const int x0 = hzToX(0);
        p.setPen(QColor(Style::role("border", Style::kBorder)));
        p.drawLine(x0, r.top(), x0, r.bottom());
    }

    // ── Der Durchlass ────────────────────────────────────────────────
    const int xl = hzToX(m_low);
    const int xh = hzToX(m_high);

    QColor fill = m_accent;
    // 0,10 war auf dem Vergleichsblatt kaum zu sehen; die Vorlage
    // legt eine deutlich erkennbare Tuenche ueber den Durchlass.
    fill.setAlphaF(0.14f);
    p.fillRect(QRect(xl, r.top(), std::max(1, xh - xl), r.height()), fill);

    QColor topEdge = m_accent;
    topEdge.setAlphaF(0.55f);
    p.setPen(topEdge);
    p.drawLine(xl, r.top(), xh, r.top());

    p.setPen(QPen(m_accent, 2));
    p.drawLine(xl, r.top(), xl, r.bottom());
    p.drawLine(xh, r.top(), xh, r.bottom());

    // Griffe als Pillen in halber Hoehe. Die ganze Kante als Griff
    // waere zwar groesser, sagt aber nicht, WO man fassen soll.
    auto drawHandle = [&](int x, bool active) {
        const QRect h(x - 4, r.center().y() - 11, 9, 22);
        p.setBrush(QColor(active ? 0x1e : 0x16, active ? 0x2a : 0x20,
                          active ? 0x36 : 0x2a));
        p.setPen(QPen(m_accent, active ? 2 : 1));
        p.drawRoundedRect(h, 3, 3);
        QColor tick = m_accent;
        tick.setAlphaF(0.8f);
        p.setPen(tick);
        p.drawLine(x, h.top() + 5, x, h.bottom() - 5);
        p.setBrush(Qt::NoBrush);
    };
    drawHandle(xl, m_hover == Zone::LowEdge || m_drag == Zone::LowEdge);
    drawHandle(xh, m_hover == Zone::HighEdge || m_drag == Zone::HighEdge);

    // ── Beschriftungen oben ──────────────────────────────────────────
    QFont small = font();
    small.setPointSizeF(std::max(6.0, small.pointSizeF() - 3.0));
    QFont value = font();
    value.setPointSizeF(std::max(7.0, value.pointSizeF() - 2.0));

    const QColor faint(Style::role("text-scale", Style::kTextScale));
    const QColor ink(Style::role("text", Style::kTextPrimary));

    // ── Eng: die Wortmarken weichen, die Zahlen bleiben ─────────────
    //
    // Der Betreiber hat es am 2026-08-22 fotografiert: beim
    // Verkleinern schoben sich "LOW CUT" und "HIGH CUT" ineinander und
    // ueber das Kaestchen mit der Breite. Drei Beschriftungen wollen
    // Platz, den es nicht mehr gibt.
    //
    // Rangfolge, wie ueberall sonst in diesem Fenster: zuerst faellt
    // das WORT, dann die Zahl. Die Zahl traegt die Information; das
    // Wort sagt nur, was ohnehin an der Kante steht, an der es klebt.
    //
    // Gemessen wird gegen den tatsaechlichen Platz zwischen den beiden
    // Griffen, nicht gegen die Fensterbreite: bei schmalem Durchlass
    // ist es auch in einem breiten Fenster eng.
    const int labelRoom = xh - xl;
    const bool wordMarks = labelRoom >= 190;
    const bool numbers   = labelRoom >= 110;

    // ── Wortmarken wieder da, ueber den Zahlen ──────────────────────
    //
    // Sie waren am 2026-08-22 entfallen, weil sie oben mit dem
    // Breitenkaestchen kollidierten. Auf den Bildern des Betreibers
    // vom 2026-08-23 stehen sie aber sehr wohl — als kleine,
    // gesperrte Grossbuchstaben UEBER dem Wert, nicht daneben. So
    // gedraengt wird nichts, und die Zahl bekommt ihre Ueberschrift
    // zurueck.
    if (wordMarks) {
        QFont mark = font();
        mark.setPointSizeF(std::max(5.5, mark.pointSizeF() - 4.0));
        mark.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(mark);
        p.setPen(faint);
        p.drawText(QRect(xl + 4, 1, 74, 10),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("LOW CUT"));
        p.drawText(QRect(xh - 78, 1, 74, 10),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("HIGH CUT"));
    }

    // Frueherer Vermerk (2026-08-22), zur Geschichte:
    //
    // Sie sagten, was ohnehin an der Stelle steht, an der sie klebten,
    // und waren die Haelfte des Gedraenges oben. Seit die Zahlen unten
    // an ihren Kanten stehen, braucht es sie nicht mehr. (wordMarks
    // bleibt als Groessenmass fuer die Zahlen erhalten.)
    Q_UNUSED(small);
    Q_UNUSED(faint);

    if (numbers) {
        // ── Die Kantenwerte AN DIE KANTEN, unten ────────────────────
        //
        // Sie standen oben und stiessen dort mit dem Breitenkaestchen
        // zusammen — auf dem Bild des Betreibers vom 2026-08-22 lagen
        // "LOW CUT", "HIGH CUT" und "2.9 kHz" uebereinander.
        //
        // Unten an der jeweiligen Kante ist ohnehin der bessere Platz:
        // die Zahl steht dort, wo sie gilt, und muss nicht sagen,
        // wozu sie gehoert.
        // Oben AN DEN KANTEN, wie bei OpenHPSDR Zeus — dort steht
        // "LOW CUT +100 Hz" links und "HIGH CUT +2.44 kHz" rechts vom
        // Breitenkaestchen. Unten ist kein Platz mehr: dort stehen
        // jetzt die Anteilszellen.
        p.setFont(value);
        p.setPen(ink);
        const int yVal = wordMarks ? 11 : 3;
        p.drawText(QRect(xl + 4, yVal, 74, 13),
                   Qt::AlignLeft | Qt::AlignVCenter, cutLabel(m_low));
        p.drawText(QRect(xh - 78, yVal, 74, 13),
                   Qt::AlignRight | Qt::AlignVCenter, cutLabel(m_high));
    }

    // Die Breite in der Mitte, in einem eigenen Kaestchen: sie ist die
    // Zahl, nach der man den Filter benennt.
    {
        const QString t = widthLabel(m_high - m_low);
        const QFontMetrics fm(value);
        const int tw = fm.horizontalAdvance(t) + 14;
        const int cx = (xl + xh) / 2;
        const QRect box(cx - tw / 2, 8, tw, 17);
        p.setBrush(QColor(0x0b, 0x0b, 0x0d));
        p.setPen(QColor(Style::role("border", Style::kBorder)));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(ink);
        p.drawText(box, Qt::AlignCenter, t);
        p.setBrush(Qt::NoBrush);
    }

    // ── Die Beschriftung des Empfaengers ─────────────────────────────
    {
        p.setFont(small);
        const QFontMetrics fm(small);
        const int tw = fm.horizontalAdvance(m_label) + 10;
        const QRect box(4, 4, tw, 13);
        QColor bgc = m_accent;
        bgc.setAlphaF(0.22f);
        p.setBrush(bgc);
        p.setPen(m_accent);
        p.drawRoundedRect(box, 2, 2);
        p.setPen(ink);
        p.drawText(box, Qt::AlignCenter, m_label);
        p.setBrush(Qt::NoBrush);
    }

    // ── Die Achse ────────────────────────────────────────────────────
    //
    // Ohne Verbindung steht hier NICHTS. Eine erfundene Frequenz waere
    // eine Behauptung — dieselbe Regel wie beim Panadapter-Kopf.
    p.setFont(small);
    if (!m_hasFrequency) {
        p.setPen(faint);
        p.drawText(QRect(0, height() - kPadBottom, width(), kPadBottom),
                   Qt::AlignCenter, QStringLiteral("no radio"));
        return;
    }

    // ── Abstand statt Abschnitt am Rand ─────────────────────────────
    //
    // Die Zahlen standen mittig unter ihrer Linie, auch wenn die Linie
    // am Bildrand lag — die aeusseren wurden dadurch angeschnitten
    // (".111" statt "7.111"). Eine halb gelesene Frequenz ist
    // schlimmer als keine.
    //
    // Schrittweite richtet sich nach der Spanne: bei 2 kHz Fenster
    // waeren 2-kHz-Schritte eine einzige Marke.
    const int stepHz = (m_spanHz <= 6000) ? 1000
                     : (m_spanHz <= 24000) ? 2000 : 5000;
    for (int hz = -(m_spanHz / 2 / stepHz) * stepHz;
         hz <= m_spanHz / 2; hz += stepHz) {
        const int x = hzToX(hz);
        const bool isCentre = (hz == 0);
        const QString t = axisLabel(m_vfoHz + hz);
        const int tw = p.fontMetrics().horizontalAdvance(t) + 8;
        int left = x - tw / 2;
        if (left < 1) { left = 1; }
        if (left + tw > width() - 1) { left = width() - 1 - tw; }
        p.setPen(isCentre ? QColor(Style::role("text-secondary",
                                               Style::kTextSecondary))
                          : faint);
        p.drawText(QRect(left, height() - kPadBottom + 2, tw, 13),
                   Qt::AlignCenter, t);
    }
}

// ── Ziehen ───────────────────────────────────────────────────────────

void BandwidthFilterPane::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }

    m_drag = zoneAt(ev->pos().x());
    if (m_drag == Zone::None) { return; }

    m_dragStartX    = ev->pos().x();
    m_dragStartLow  = m_low;
    m_dragStartHigh = m_high;
    update();
}

void BandwidthFilterPane::mouseMoveEvent(QMouseEvent* ev)
{
    const int x = ev->pos().x();

    if (m_drag == Zone::None) {
        // Der Zeiger sagt VORHER, was ein Ziehen tun wird. Nur setzen,
        // wenn er sich aendert — jedes setCursor kostet auf macOS ein
        // CGImageCreate, und bei jeder Mausbewegung ist das zu viel.
        const Zone z = zoneAt(x);
        if (z != m_hover) {
            m_hover = z;
            const Qt::CursorShape want =
                (z == Zone::LowEdge || z == Zone::HighEdge)
                    ? Qt::SizeHorCursor
                    : (z == Zone::Body ? Qt::SizeAllCursor : Qt::ArrowCursor);
            if (cursor().shape() != want) { setCursor(want); }
            update();
        }
        return;
    }

    const int deltaHz = ((xToHz(x) - xToHz(m_dragStartX)) / kStepHz) * kStepHz;

    if (m_drag == Zone::Body) {
        // Die Breite bleibt. Deshalb die MITTE melden und nicht zwei
        // Kanten: nur so kann das Modell mit filterShift begrenzen und
        // die Breite am Rand erhalten.
        const int centre = (m_dragStartLow + m_dragStartHigh) / 2 + deltaHz;
        emit filterCentreChanged(centre);
        return;
    }

    int low  = m_dragStartLow;
    int high = m_dragStartHigh;
    if (m_drag == Zone::LowEdge)  { low  = m_dragStartLow  + deltaHz; }
    if (m_drag == Zone::HighEdge) { high = m_dragStartHigh + deltaHz; }

    // Die Kanten duerfen einander nicht ueberholen. Alles Weitere —
    // Seitenband, Deckel — entscheidet das Modell.
    if (high - low < kStepHz) {
        if (m_drag == Zone::LowEdge) { low  = high - kStepHz; }
        else                          { high = low  + kStepHz; }
    }
    emit filterChanged(low, high);
}

void BandwidthFilterPane::mouseReleaseEvent(QMouseEvent* ev)
{
    Q_UNUSED(ev)
    if (m_drag == Zone::None) { return; }
    m_drag = Zone::None;
    update();
}

void BandwidthFilterPane::leaveEvent(QEvent*)
{
    if (m_hover == Zone::None) { return; }
    m_hover = Zone::None;
    unsetCursor();
    update();
}

} // namespace Longpath
