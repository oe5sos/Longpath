// SPDX-License-Identifier: GPL-3.0-or-later
//
// Siehe Ft8Wasserfall.h fuer die Begruendung.

#include "gui/widgets/Ft8Wasserfall.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include "gui/StyleConstants.h"

namespace Longpath {

// ── Die Regel ────────────────────────────────────────────────────────
//
// Statisch und ohne Zustand: so faehrt der Prueflauf sie ohne Schirm.

double Ft8Wasserfall::hzZuX(int hz, int breite)
{
    if (breite <= 0) { return 0.0; }
    const double anteil = double(hz - kMinHz) / double(kMaxHz - kMinHz);
    return std::clamp(anteil, 0.0, 1.0) * double(breite);
}

int Ft8Wasserfall::xZuSendeHz(double x, int breite)
{
    if (breite <= 0) { return kSendeMinHz; }
    const double anteil = std::clamp(x / double(breite), 0.0, 1.0);
    const int hz = kMinHz + int(std::lround(anteil * double(kMaxHz - kMinHz)));
    // Auf den sendbaren Bereich holen: ein Klick an den Rand ergibt die
    // kleinste bzw. groesste erlaubte Stelle, nicht 0 oder 3000.
    return std::clamp(hz, kSendeMinHz, kSendeMaxHz);
}

// ── Aufbau ───────────────────────────────────────────────────────────

Ft8Wasserfall::Ft8Wasserfall(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(90);
    setCursor(Qt::CrossCursor);
    setToolTip(tr("The passband, 0-3000 Hz. Click a clear spot to "
                  "transmit there."));

    m_fftEin = fftwf_alloc_real(kFftPunkte);
    m_fftAus = fftwf_alloc_complex(kFftPunkte / 2 + 1);
    m_plan = fftwf_plan_dft_r2c_1d(kFftPunkte, m_fftEin, m_fftAus,
                                   FFTW_ESTIMATE);
    std::fill(m_fftEin, m_fftEin + kFftPunkte, 0.0f);
}

Ft8Wasserfall::~Ft8Wasserfall()
{
    if (m_plan) { fftwf_destroy_plan(m_plan); }
    if (m_fftEin) { fftwf_free(m_fftEin); }
    if (m_fftAus) { fftwf_free(m_fftAus); }
}

void Ft8Wasserfall::setzeSendeTon(int hz)
{
    if (hz == m_sendeTonHz) { return; }
    m_sendeTonHz = hz;
    update();
}

float Ft8Wasserfall::spektrumMaxForTest() const
{
    float m = 0.0f;
    for (float v : m_spektrum) { m = std::max(m, v); }
    return m;
}

// ── Ton hinein ───────────────────────────────────────────────────────

void Ft8Wasserfall::speiseStereo(const float* verschraenkt, int rahmen,
                                 int rateHz)
{
    if (!verschraenkt || rahmen <= 0) { return; }
    if (rateHz > 0) { m_rateHz = rateHz; }

    // Nur der linke Kanal -- der Dekoder hoert denselben.
    m_sammler.reserve(kFftPunkte);
    for (int i = 0; i < rahmen; ++i) {
        m_sammler.append(verschraenkt[i * 2]);
    }
    // Mehr als ein Fenster brauchen wir nie: das Aelteste faellt weg.
    if (m_sammler.size() > kFftPunkte) {
        m_sammler.remove(0, m_sammler.size() - kFftPunkte);
    }
    if (m_sammler.size() == kFftPunkte) {
        rechneSpektrum();
        update();
    }
}

void Ft8Wasserfall::rechneSpektrum()
{
    const int breite = std::max(1, width());
    if (!m_plan) { return; }

    // Hann-Fenster: ohne es schmiert jeder Ton ueber den halben
    // Streifen, und genau die freie Stelle, die man sucht, waere nicht
    // mehr zu erkennen.
    for (int i = 0; i < kFftPunkte; ++i) {
        const double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFftPunkte - 1)));
        m_fftEin[i] = float(m_sammler.at(i) * w);
    }
    fftwf_execute(m_plan);

    const double hzJeBin = double(m_rateHz) / double(kFftPunkte);
    m_spektrum.resize(breite);
    QVector<double> roh(breite);

    for (int x = 0; x < breite; ++x) {
        // Welcher Frequenzbereich faellt auf diese Spalte?
        const double hzVon = kMinHz + double(kMaxHz - kMinHz) * x / breite;
        const double hzBis = kMinHz + double(kMaxHz - kMinHz) * (x + 1) / breite;
        const int binVon = std::max(0, int(hzVon / hzJeBin));
        const int binBis = std::min(kFftPunkte / 2, int(hzBis / hzJeBin) + 1);

        // Das LAUTESTE im Bereich, nicht der Mittelwert: ein schmaler
        // FT8-Ton verschwindet sonst zwischen seinen leeren Nachbarn.
        float spitze = 0.0f;
        for (int b = binVon; b < binBis; ++b) {
            const float re = m_fftAus[b][0];
            const float im = m_fftAus[b][1];
            spitze = std::max(spitze, re * re + im * im);
        }
        roh[x] = 10.0 * std::log10(double(spitze) + 1e-12);
    }

    // Die Skala haengt am tatsaechlichen Rauschboden, nicht an festen
    // Zahlen (2026-10-18, am ersten gezeichneten Bild gesehen): mit
    // einer festen Spanne hob jedes Nebenmaximum den Boden mit an, und
    // der ganze Durchlassbereich stand bernstein da -- er sah voll aus,
    // obwohl nur ein einziger Ton drin war.
    //
    // Das fuenfte Fuenftel von unten ist der Boden: ein Band mit ein
    // paar Signalen ist zum groessten Teil leer, der Mittelwert waere
    // von den Signalen selbst verzogen. Darueber 45 dB Spanne -- so
    // viel liegt zwischen Rauschen und einem kraeftigen FT8-Signal.
    QVector<double> sortiert = roh;
    std::nth_element(sortiert.begin(), sortiert.begin() + sortiert.size() / 5,
                     sortiert.end());
    const double boden = sortiert.at(sortiert.size() / 5);
    constexpr double kSpanneDb = 45.0;

    for (int x = 0; x < breite; ++x) {
        m_spektrum[x] =
            float(std::clamp((roh.at(x) - boden) / kSpanneDb, 0.0, 1.0));
    }

    // Eine neue Zeile in den Wasserfall, die aelteste faellt unten raus.
    if (m_wasserfall.width() != breite) {
        m_wasserfall = QImage(breite, kWasserfallZeilen, QImage::Format_RGB32);
        m_wasserfall.fill(QColor(QString::fromLatin1(Style::kInsetBg)));
    }
    // Nach unten schieben.
    for (int y = kWasserfallZeilen - 1; y > 0; --y) {
        std::memcpy(m_wasserfall.scanLine(y), m_wasserfall.scanLine(y - 1),
                    size_t(breite) * 4);
    }
    auto* oben = reinterpret_cast<QRgb*>(m_wasserfall.scanLine(0));
    for (int x = 0; x < breite; ++x) {
        const float v = m_spektrum.at(x);
        // Dieselbe Richtung wie im grossen Panadapter: dunkel -> bernstein.
        oben[x] = qRgb(int(255 * std::clamp(v * 1.6f, 0.0f, 1.0f)),
                       int(170 * std::clamp(v * 1.2f, 0.0f, 1.0f)),
                       int(70 * v));
    }
}

// ── Malen ────────────────────────────────────────────────────────────

void Ft8Wasserfall::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const int breite = width();
    const int spektrumHoehe = std::max(20, height() / 3);

    p.fillRect(rect(), QColor(QString::fromLatin1(Style::kInsetBg)));

    // Der Wasserfall unten.
    if (!m_wasserfall.isNull()) {
        p.drawImage(QRect(0, spektrumHoehe, breite,
                          height() - spektrumHoehe - kSkalaHoehe),
                    m_wasserfall);
    }

    // Das laufende Spektrum oben.
    if (!m_spektrum.isEmpty()) {
        p.setPen(QColor(QString::fromLatin1(Style::kAmberText)));
        const int bis = std::min<int>(breite, int(m_spektrum.size()));
        for (int x = 0; x < bis; ++x) {
            const int h = int(m_spektrum.at(x) * spektrumHoehe);
            p.drawLine(x, spektrumHoehe, x, spektrumHoehe - h);
        }
    }

    // Die Marke der Sendestelle -- ueber alles, damit sie nie verdeckt
    // ist.
    const double x = hzZuX(m_sendeTonHz, breite);
    p.setPen(QPen(QColor(QString::fromLatin1(Style::kGreenText)), 1));
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
    p.drawText(QPointF(std::min(double(breite) - 44.0, x + 3), 11),
               QStringLiteral("%1").arg(m_sendeTonHz));

    // Tausenderstriche, damit die Stelle ablesbar ist.
    // ── Die weiteren Sendeplaetze (2026-10-24) ───────────────────────
    //
    // Bei Multi Answering liegen sie je 60 Hz hoeher (MSHV
    // gen_ft8.cpp:262). Sie werden leiser gezeichnet als die eigene
    // Sendestelle: man soll sehen, WO man noch sendet, ohne dass es
    // mit der Hauptmarke um Aufmerksamkeit streitet.
    if (m_plaetze > 1) {
        QColor leise(QString::fromLatin1(Style::kGreenText));
        leise.setAlpha(110);
        p.setPen(QPen(leise, 1, Qt::DotLine));
        for (int i = 1; i < m_plaetze; ++i) {
            const int hz = m_sendeTonHz + i * kPlatzAbstandHz;
            if (hz > kSendeMaxHz) { break; }
            const double px = hzZuX(hz, breite);
            p.drawLine(QPointF(px, 0), QPointF(px, height() - kSkalaHoehe));
        }
    }

    // ── Frequenzskala (2026-10-24) ───────────────────────────────────
    //
    // Vorher standen hier nur vier Striche von 4 px Hoehe, ohne eine
    // einzige Zahl -- man sah, DASS eine Teilung da ist, aber nicht
    // welche. Der Betreiber am 2026-10-24: „uebersichtlich,
    // professionell, einfach, high end". Eine Skala ohne Zahlen ist
    // keins davon.
    const int skalaY = height() - kSkalaHoehe;
    p.fillRect(QRect(0, skalaY, breite, kSkalaHoehe),
               QColor(QString::fromLatin1(Style::kPanelBg)));
    p.setPen(QColor(QString::fromLatin1(Style::kTextTertiary)));
    QFont beschriftung = p.font();
    beschriftung.setPixelSize(Style::kFontMicro);
    p.setFont(beschriftung);
    for (int hz = 400; hz < kMaxHz; hz += 400) {
        const double gx = hzZuX(hz, breite);
        p.drawLine(QPointF(gx, skalaY), QPointF(gx, skalaY + 3));
        p.drawText(QRectF(gx - 22, skalaY + 3, 44, kSkalaHoehe - 3),
                   Qt::AlignHCenter | Qt::AlignTop, QString::number(hz));
    }
}

void Ft8Wasserfall::mousePressEvent(QMouseEvent* e)
{
    if (!e || e->button() != Qt::LeftButton) { return; }
    const int hz = xZuSendeHz(e->position().x(), width());
    setzeSendeTon(hz);
    emit sendeTonGewaehlt(hz);
}

// resizeEvent gibt es NICHT -- und das ist Absicht.
//
// Zuerst stand hier ein Wegwerfen von Spektrum und Wasserfall bei
// jeder Groessenaenderung. Das erste gezeichnete Bild (2026-10-18) war
// daraufhin leer, obwohl alle Zahlen stimmten: Breite 600, Sammler
// 4096, 600 Spalten, Spitze 1,0. Qt stellt das ERSTE resizeEvent eines
// nie gezeigten Widgets bis zum Zeichnen zurueck und gibt als alte
// Groesse -1 an -- die Breite galt also als „geaendert", und geleert
// wurde genau in dem Augenblick, in dem gemalt werden sollte.
//
// Gebraucht wird das Wegwerfen ohnehin nicht: rechneSpektrum() legt
// den Wasserfall neu an, sobald seine Breite nicht mehr stimmt, und
// paintEvent zeichnet nur so viele Spalten, wie es hat.

} // namespace Longpath

// Siehe Ft8Wasserfall.h.
void Longpath::Ft8Wasserfall::setzePlaetze(int n)
{
    const int neu = std::max(1, n);
    if (neu == m_plaetze) { return; }
    m_plaetze = neu;
    update();
}
