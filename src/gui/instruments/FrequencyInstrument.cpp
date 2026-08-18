// =================================================================
// src/gui/instruments/FrequencyInstrument.cpp  (NereusSDR)
// =================================================================
// Siehe FrequencyInstrument.h.
// =================================================================

#include "gui/instruments/FrequencyInstrument.h"

#include "core/antenna/AmateurBands.h"
#include "gui/StyleConstants.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/styles/ThemeQss.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace NereusSDR {

namespace {

/// Wie im Entwurf: 7.139.700 — acht Ziffern, Punkt nach der ersten
/// und nach der vierten. Die Dekade je Ziffer ergibt sich daraus.
constexpr double kDecades[8] = {
    1e7, 1e6, 1e5, 1e4, 1e3, 1e2, 1e1, 1e0
};

/// Die beiden Hertz-Stellen stehen matt — sie laufen beim Abstimmen
/// dauernd und sollen den Blick nicht halten.
constexpr int kDimFromIndex = 6;

/// Hoehe der Mulde beim Bandstreifen (Entwurf: H = 14 auf 520 Breite).
constexpr double kStripHeight = 14.0;

// ── Zwei Abstaende, nicht einer ──────────────────────────────────────
//
// OE5SOS, 2026-08-18: „Der Trenner steht genauso weit ab wie die
// Ziffern untereinander, damit sieht das Auge acht Einzelzeichen statt
// drei Bloecke — und die Gruppierung, die wir gerade repariert haben,
// wird optisch wieder aufgehoben."
//
// Warum es dazu kam: die Zeile ist in Monospace gesetzt, und dort ist
// der Punkt genau so breit wie eine Ziffer. Das Zeichen ist schmal,
// seine ZELLE nicht — links und rechts davon steht je eine halbe leere
// Zelle. Der Trenner brachte damit von sich aus mehr Luft mit als jede
// Ziffernfuge, und die Zeile zerfiel in acht gleich weit stehende
// Zeichen.
//
// QFont::setLetterSpacing hilft hier nicht: jede Ziffer ist ein eigenes
// Schild mit eigener Trefferflaeche (Rad ueber der Stelle dreht diese
// Dekade), also setzt das Layout die Abstaende und nicht die Schrift.
// Dann sind es zwei Werte, wie der Betreiber gesagt hat — der Abstand
// INNERHALB einer Gruppe und der ZWISCHEN zweien.
//
// Alle drei als Anteil der Zeichenzelle, damit sie ueber die
// Schriftstufen halten.

/// Luft je Ziffer. Nur so viel, dass die Trefferflaeche nicht auf die
/// Glyphe schrumpft — innerhalb einer Gruppe soll es eng sein.
constexpr double kDigitPadOfCell = 0.09;

/// Der Trenner bekommt seine eigene, schmale Breite statt der vollen
/// Monospace-Zelle.
constexpr double kSeparatorOfCell = 0.30;

/// Der Gruppenabstand. Steht NUR hinter dem Trenner: „der Trenner darf
/// schmal bleiben und braucht keine Luft auf beiden Seiten."
constexpr double kGroupGapOfCell = 0.35;

} // namespace

FrequencyInstrument::FrequencyInstrument(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(4);

    // Die Zahl, und darunter Platz fuer Streifen oder Bogen. Der
    // Zwischenraum wird gemalt, nicht ausgelegt: paintEvent kennt die
    // Form und weiss, wieviel Platz sie braucht.
    m_stack = new QStackedWidget(this);
    m_digitRow = new QWidget(m_stack);
    m_digitLayout = new QHBoxLayout(m_digitRow);
    m_digitLayout->setContentsMargins(0, 0, 0, 0);
    m_digitLayout->setSpacing(0);
    buildDigits();
    m_digitLayout->addStretch(1);

    m_edit = new QLineEdit(m_stack);
    m_edit->setPlaceholderText(QStringLiteral("MHz"));
    m_edit->setStyleSheet(Style::themed(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; font-family: Menlo; font-size: %4px; }")
        .arg(Style::kInsetBg, Style::kAmberText, Style::kBorder)
        .arg(Style::kFontReading)));
    connect(m_edit, &QLineEdit::editingFinished,
            this, &FrequencyInstrument::commitEdit);

    m_stack->addWidget(m_digitRow);
    m_stack->addWidget(m_edit);
    root->addWidget(m_stack, 0);

    root->addStretch(1);   // hier malt paintEvent Streifen oder Bogen

    m_vfoRow = new QLabel(this);
    m_vfoRow->setTextFormat(Qt::RichText);
    m_vfoRow->setFont(Style::monoFont(m_vfoRow->font(), Style::kFontBody));
    root->addWidget(m_vfoRow, 0);

    refreshDigits();
    refreshVfoRow();
}

QSize FrequencyInstrument::sizeHint() const
{
    // Der Bogen braucht mehr Hoehe als der Streifen, und die blosse
    // Zahl am wenigsten — der Entwurf sagt es genauso.
    switch (m_form) {
        case Form::FlatArc:    return {360, 132};
        case Form::BandStrip:  return {360, 116};
        case Form::NumberOnly: return {360,  86};
    }
    return {360, 116};
}

// ── Die Ziffern ──────────────────────────────────────────────────────

void FrequencyInstrument::buildDigits()
{
    // ── Gruppen zu drei, von RECHTS gezaehlt ─────────────────────────
    //
    // Hier stand {0, 3}, und das war um eine Stelle verschoben: die
    // Trennpunkte sassen an festen Stellen von links, waehrend die
    // Gruppierung vom kleinsten Hertz her zaehlt. 7.139.700 Hz kam als
    // „0.713.9700" heraus — die fuehrende Null wurde mitgezaehlt.
    // Befund des Betreibers, 2026-08-18, gegen die VFO-Zeile darunter,
    // die es richtig zeigte.
    //
    // Die Stellen sind 10 MHz .. 1 Hz. Gruppen von rechts:
    //   (1 Hz, 10 Hz, 100 Hz) = 5,6,7
    //   (1 k, 10 k, 100 k)    = 2,3,4
    //   (1 M, 10 M)           = 0,1
    // Also Punkte NACH Stelle 1 und NACH Stelle 4.
    static const int kSepAfter[] = {1, 4};

    // Die Zeichenzelle der Ziffernschrift — Bezugsmass fuer alle drei
    // Abstaende. In Monospace ist sie fuer jedes Zeichen gleich, also
    // genuegt eine Abfrage.
    const QFont digitFont = Style::monoFont(m_digitRow->font(),
                                            Style::kFontDisplay,
                                            QFont::Light);
    const int cell = QFontMetrics(digitFont).horizontalAdvance(
        QStringLiteral("0"));
    const int digitPad = qMax(1, qRound(cell * kDigitPadOfCell));
    const int sepWidth = qMax(2, qRound(cell * kSeparatorOfCell));
    const int groupGap = qMax(2, qRound(cell * kGroupGapOfCell));

    for (int i = 0; i < kDigitCount; ++i) {
        auto* d = new QLabel(QStringLiteral("0"), m_digitRow);
        d->setAlignment(Qt::AlignCenter);
        d->setFont(digitFont);
        // Jede Ziffer ist eine eigene Trefferflaeche. Der Zeiger sagt
        // das an: senkrecht ziehen ist die Geste, die das Rad meint.
        d->setCursor(Qt::SizeVerCursor);
        d->setAttribute(Qt::WA_Hover, true);
        d->installEventFilter(this);
        d->setToolTip(tr("Scroll to tune this digit"));
        // Feste Mindestbreite: die fuehrende Stelle wird unter 10 MHz
        // LEER gezeigt, und ein leeres Schild ohne Breite naehme seine
        // eigene Trefferflaeche mit. Man koennte dann 7 MHz nicht mehr
        // auf 17 MHz drehen.
        // Feste Breite, nicht nur eine Mindestbreite: sonst bestimmte
        // der Schilder-sizeHint die Fuge, und der waere je nach Ziffer
        // verschieden — eine Spalte, die beim Abstimmen atmet.
        d->setFixedWidth(cell + digitPad);
        m_digits.append(d);
        m_decades.append(kDecades[i]);
        m_digitLayout->addWidget(d);

        for (int sep : kSepAfter) {
            if (i == sep) {
                auto* dot = new QLabel(QStringLiteral("."), m_digitRow);
                dot->setFont(digitFont);
                // Auf die schmale Breite geklemmt. Ohne das brachte der
                // Punkt die volle Monospace-Zelle mit und riss die
                // Gruppe auf, die er zusammenhalten soll.
                dot->setFixedWidth(sepWidth);
                dot->setAlignment(Qt::AlignCenter);
                dot->setStyleSheet(Style::themed(
                    QStringLiteral("QLabel { color: %1; }")
                        .arg(Style::kAmberDim)));
                m_digitLayout->addWidget(dot);
                // Die Luft steht HINTER dem Trenner, nicht um ihn
                // herum: er gehoert zur Gruppe links von ihm.
                m_digitLayout->addSpacing(groupGap);
            }
        }
    }

    auto* unit = new QLabel(QStringLiteral("MHz"), m_digitRow);
    // „MHz" ist eine Beschriftung, keine Zahl: Versalzeile auf der
    // kleinen Stufe, mit derselben Laufweite wie jede andere.
    unit->setFont(Style::capsFont(unit->font(), Style::kFontSmall));
    unit->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: %1; }").arg(Style::kTextSecondary)));
    m_digitLayout->addSpacing(10);
    m_digitLayout->addWidget(unit, 0, Qt::AlignBottom);
}

void FrequencyInstrument::refreshDigits()
{
    const qint64 hz = static_cast<qint64>(std::llround(m_hz));
    for (int i = 0; i < m_digits.size(); ++i) {
        const qint64 dec = static_cast<qint64>(m_decades.at(i));
        const int digit = static_cast<int>((hz / dec) % 10);
        // Unter 10 MHz bleibt die vorderste Stelle leer statt „0". Die
        // Zeile darunter macht es genauso (0 wird dort nicht gepolstert),
        // und eine fuehrende Null liest sich als Teil der Zahl.
        const bool blank = (i == 0 && hz < 10000000);
        m_digits[i]->setText(blank ? QString()
                                   : QString::number(digit));
        const bool dim = (i >= kDimFromIndex);
        m_digits[i]->setStyleSheet(Style::themed(
            QStringLiteral("QLabel { color: %1; padding: 2px 1px;"
                           " border-radius: 6px; }"
                           "QLabel:hover { background: %2; }")
                .arg(dim ? Style::kAmberDim : Style::kAmberText,
                     Style::kBadgeWarnBg)));
    }
}

QString FrequencyInstrument::groupedText() const
{
    // Liest die WIRKLICHEN Schilder in der WIRKLICHEN Reihenfolge des
    // Layouts — keine zweite Fassung der Formatierung, die gruen bleiben
    // koennte, waehrend auf dem Schirm etwas anderes steht.
    QString out;
    QLayout* lay = m_digitRow ? m_digitRow->layout() : nullptr;
    if (!lay) { return out; }
    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem* it = lay->itemAt(i);
        auto* l = it ? qobject_cast<QLabel*>(it->widget()) : nullptr;
        if (!l) { continue; }
        // Die Einheit gehoert nicht zur Zahl.
        if (l->text() == QStringLiteral("MHz")) { continue; }
        out += l->text();
    }
    return out;
}

void FrequencyInstrument::refreshVfoRow()
{
    // Beide Scheiben, die aktive hervorgehoben — Split ist damit ohne
    // ein zweites Feld zu sehen.
    auto fmt = [](double hz) {
        const qint64 v = static_cast<qint64>(std::llround(hz));
        return QStringLiteral("%1.%2.%3")
            .arg(v / 1000000)
            .arg((v / 1000) % 1000, 3, 10, QLatin1Char('0'))
            .arg(v % 1000, 3, 10, QLatin1Char('0'));
    };
    const QString on  = QString::fromLatin1(Style::kAmberText);
    const QString off = QString::fromLatin1(Style::kAmberDim);
    m_vfoRow->setText(
        QStringLiteral("<font color='%1'>A %2</font>"
                       "&nbsp;&nbsp;<font color='%3'>B %4</font>")
            .arg(m_activeIsThis ? on : off, fmt(m_hz),
                 m_activeIsThis ? off : on, fmt(m_otherHz)));
}

// ── Bedienen ─────────────────────────────────────────────────────────

bool FrequencyInstrument::eventFilter(QObject* watched, QEvent* ev)
{
    const int idx = m_digits.indexOf(qobject_cast<QLabel*>(watched));
    if (idx < 0) { return QWidget::eventFilter(watched, ev); }

    if (ev->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(ev);
        const int steps = we->angleDelta().y() > 0 ? 1 : -1;
        // Diese Dekade, nicht die eingestellte Schrittweite. Das ist der
        // Sinn der eigenen Trefferflaeche: man greift die Stelle an, die
        // man drehen will.
        const double next = m_hz + steps * m_decades.at(idx);
        if (next >= 0.0) {
            emit frequencyEdited(next);
        }
        return true;
    }
    if (ev->type() == QEvent::MouseButtonDblClick) {
        beginEdit();
        return true;
    }
    return QWidget::eventFilter(watched, ev);
}

void FrequencyInstrument::beginEdit()
{
    m_edit->setText(QString::number(m_hz / 1e6, 'f', 6));
    m_stack->setCurrentWidget(m_edit);
    m_edit->selectAll();
    m_edit->setFocus();
}

double FrequencyInstrument::parseUserFrequency(const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) { return -1.0; }

    // Detect and strip unit suffix (longest match first so "MHz" wins over "Hz").
    double mult = 0.0;
    bool hasUnit = false;
    const auto tryStripSuffix = [&](const char* suffix, double m) {
        if (s.endsWith(QLatin1String(suffix), Qt::CaseInsensitive)) {
            s.chop(qstrlen(suffix));
            mult = m;
            hasUnit = true;
            return true;
        }
        return false;
    };
    tryStripSuffix("MHz", 1e6)
        || tryStripSuffix("kHz", 1e3)
        || tryStripSuffix("Hz",  1.0)
        || tryStripSuffix("M",   1e6)
        || tryStripSuffix("K",   1e3);
    s = s.trimmed();
    if (s.isEmpty()) { return -1.0; }

    const int nDots   = s.count(QLatin1Char('.'));
    const int nCommas = s.count(QLatin1Char(','));

    // Normalize separators. The goal: end up with at most one '.' as the
    // decimal separator, with any grouping separators removed.
    if (nDots >= 2 && nCommas == 0) {
        // "7.230.000" (or with unit: "7.230.000 Hz") — dots are thousand
        // separators. When no unit was given, default to Hz since that's
        // the only sensible interpretation of a multi-dot number.
        s.remove(QLatin1Char('.'));
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas >= 2 && nDots == 0) {
        // "7,230,000" — US thousand-separated Hz value.
        s.remove(QLatin1Char(','));
        if (!hasUnit) { mult = 1.0; }
    } else if (nDots > 0 && nCommas > 0) {
        // Mixed: the last occurrence is the decimal, the rest are thousands.
        // The presence of thousand separators means the user is writing a
        // Hz value (e.g. "7,230,000.50"); no unit makes no other sense.
        if (s.lastIndexOf(QLatin1Char('.')) > s.lastIndexOf(QLatin1Char(','))) {
            s.remove(QLatin1Char(','));
        } else {
            s.remove(QLatin1Char('.'));
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas == 1 && nDots == 0) {
        // Single comma — ambiguous. If a unit suffix was already parsed, a
        // three-digit tail is a US-style thousands separator
        // (e.g. "7,230 kHz" → 7,230 kHz), anything else is EU decimal
        // ("7,23 MHz" → 7.23 MHz). Without a unit, fall through to EU
        // decimal — the historical behavior — because a bare "7,23" with
        // no grouping context reads as a decimal in every locale that
        // writes it that way.
        const int commaIdx  = s.indexOf(QLatin1Char(','));
        const int tailCount = s.size() - commaIdx - 1;
        if (hasUnit && tailCount == 3) {
            s.remove(QLatin1Char(','));
        } else {
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
    }
    // else: at most a single '.' (C-locale ready) or a plain integer.

    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || v < 0.0) { return -1.0; }

    if (mult != 0.0) {
        return v * mult;
    }

    // Plain number, no unit, no grouping separators. Matches the Thetis
    // MHz-decimal convention when a decimal point is present. For bare
    // integers, pick the first unit (MHz → kHz → Hz) whose interpretation
    // lies in the Red Pitaya tuning range — this rescues users who typed
    // "7230" (intending kHz) or "7230000" (intending Hz) without guessing
    // wrong like the prior heuristic did (issue #73).
    constexpr double kMinHz = 100000.0;     // 100 kHz floor
    constexpr double kMaxHz = 61440000.0;   // 61.44 MHz ceiling
    const bool isDecimal = (nDots == 1);
    if (isDecimal) {
        return v * 1e6;  // Thetis convention: decimal number is MHz
    }
    const double asMHz = v * 1e6;
    const double asKHz = v * 1e3;
    const double asHz  = v;
    if (asMHz >= kMinHz && asMHz <= kMaxHz) { return asMHz; }
    if (asKHz >= kMinHz && asKHz <= kMaxHz) { return asKHz; }
    if (asHz  >= kMinHz && asHz  <= kMaxHz) { return asHz;  }
    // No interpretation in range: fall back to MHz; caller will clamp.
    return asMHz;
}

void FrequencyInstrument::commitEdit()
{
    m_stack->setCurrentWidget(m_digitRow);
    // Unlesbares verwerfen und die alte Zahl stehen lassen. Eine
    // Frequenz, die aus einem Tippfehler entsteht, ist schlimmer als
    // eine, die sich nicht geaendert hat.
    //
    // Hier stand bis 2026-08-18 ein blosses toDouble() mit der Annahme
    // MHz. Das haette „7.230.000" still verworfen und „7230" als
    // 7230 MHz gelesen — Fehlerbericht #73 noch einmal.
    const double hz = parseUserFrequency(m_edit->text());
    if (hz > 0.0) {
        emit frequencyEdited(hz);
    }
}

void FrequencyInstrument::setFrequency(double hz)
{
    if (qFuzzyCompare(1.0 + m_hz, 1.0 + hz)) { return; }
    m_hz = hz;
    refreshDigits();
    refreshVfoRow();
    update();
}

void FrequencyInstrument::setOtherFrequency(double hz)
{
    m_otherHz = hz;
    refreshVfoRow();
}

void FrequencyInstrument::setActiveIsThis(bool active)
{
    m_activeIsThis = active;
    refreshVfoRow();
}

void FrequencyInstrument::setForm(Form f)
{
    if (m_form == f) { return; }
    m_form = f;
    updateGeometry();
    update();
}

// ── Zeichnen ─────────────────────────────────────────────────────────

double FrequencyInstrument::bandFraction() const
{
    const AmateurBands::Band b = AmateurBands::containing(m_hz);
    if (!b.isValid()) { return -1.0; }
    return (m_hz - b.lowHz) / b.widthHz();
}

void FrequencyInstrument::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor col = Instrument::measured();
    const double f = bandFraction();

    // Der Bereich unter der Zahl und ueber der VFO-Zeile.
    const int top = m_stack ? m_stack->geometry().bottom() + 6 : 40;
    const int bot = m_vfoRow ? m_vfoRow->geometry().top() - 6 : height() - 20;
    const QRectF area(14, top, qMax(0, width() - 28), qMax(0, bot - top));

    if (m_form == Form::NumberOnly) {
        // Nur die Glut, und zwar unter der ZAHL — nicht in der leeren
        // Flaeche darunter, die es in dieser Form gar nicht gibt.
        if (m_stack) {
            const QRectF r = m_stack->geometry();
            const QRectF glow(r.center().x() - r.width() * 0.39,
                              r.center().y() - r.height() * 0.75,
                              r.width() * 0.78, r.height() * 1.5);
            Instrument::paintGlow(p, glow, QPainterPath(), col);
        }
        return;
    }

    if (area.height() < 16.0 || area.width() < 60.0) { return; }

    // Ausserhalb jedes Bandes gibt es keine Stelle im Band, auf die ein
    // Verlauf zeigen koennte. Dann nur die leere Mulde: sie sagt, dass
    // hier eine Bandlage stuende, wenn es eine gaebe.
    const bool inBand = (f >= 0.0);

    if (m_form == Form::BandStrip) {
        LinearSpine spine(QRectF(area.left(), area.top(),
                                 area.width(), kStripHeight));
        Instrument::paintTrough(p, spine);
        if (inBand) {
            Instrument::paintFade(p, spine, f, col);
            Instrument::paintValueEdge(p, spine, f, col);
        }
        // Die Bandkanten. Rot, kraeftig, nicht entsaettigt —
        // CLAUDE.local.md nennt sie ausdruecklich als Ausnahme vom
        // Hausstil.
        if (inBand) {
            const QColor edge(Style::kGaugeDanger);
            p.setPen(QPen(edge, 1.4));
            p.drawLine(spine.crossAt(0.0, 4.0, 4.0));
            p.drawLine(spine.crossAt(1.0, 4.0, 4.0));
        }
        return;
    }

    // Form::FlatArc — derselbe Bogen wie die Instrumente, nur sehr weit
    // und flach. Der Entwurf rechnet mit R = 700 auf 520 Breite; das
    // Verhaeltnis waechst mit.
    const qreal radius = area.width() * (700.0 / 520.0);
    const QPointF pivot(area.center().x(), area.top() + radius);
    ArcSpine spine(pivot, radius, 12.0, 100.6, 79.4);
    spine.setSectorInnerRadius(radius * 0.93);
    Instrument::paintTrough(p, spine);
    if (inBand) {
        Instrument::paintFade(p, spine, f, col);
        Instrument::paintValueEdge(p, spine, f, col);
        const QColor edge(Style::kGaugeDanger);
        p.setPen(QPen(edge, 1.6));
        p.drawLine(spine.crossAt(0.0, 3.0, 3.0));
        p.drawLine(spine.crossAt(1.0, 3.0, 3.0));
    }
}

} // namespace NereusSDR
