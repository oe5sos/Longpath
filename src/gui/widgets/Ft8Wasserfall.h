#pragma once

// =================================================================
// src/gui/widgets/Ft8Wasserfall.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-10-18, nach dem ersten Blick in das FT8-Fenster:
// „dachte das sind dann mehr optionen usw. wie ein richtiges ft8
// programm." Aus der Liste, die daraufhin entstand, hing dieser Punkt
// mit der waehlbaren Sendestelle zusammen: eine Sendestelle, die man
// waehlen kann, ohne zu SEHEN wo frei ist, ist die halbe Sache.
//
// Ein schmaler Streifen ueber der Fundliste: oben das laufende
// Spektrum des Durchlassbereichs, darunter der Wasserfall der letzten
// Durchgaenge. Zwei Marken darin -- die gewaehlte SENDESTELLE und die
// Grenzen, innerhalb derer sie liegen darf.
//
// Und der Griff, der beides verbindet: ein KLICK in den Streifen setzt
// die Sendestelle. Genau so macht man es in WSJT-X, und es ist der
// einzige Weg, auf dem „such dir eine freie Stelle" eine Handlung
// statt einer Zahl wird.
//
// Die Umrechnung Hz <-> Bildpunkt steht als STATISCHE Funktion da,
// nicht im Malcode: so laesst sich die Regel ohne Schirm pruefen (ein
// Klick an den linken Rand muss die kleinste erlaubte Tonlage ergeben,
// nicht null), und der Prueflauf braucht kein Fenster.

#include <QImage>
#include <QVector>
#include <QWidget>

#include <fftw3.h>

namespace Longpath {

class Ft8Wasserfall : public QWidget
{
    Q_OBJECT

public:
    /// Der dargestellte Ausschnitt. 3000 Hz, weil darueber im
    /// SSB-Durchlass nichts mehr steht, was FT8 angeht.
    static constexpr int kMinHz = 0;
    static constexpr int kMaxHz = 3000;

    /// Die Grenzen, in denen eine Sendestelle liegen darf -- dieselben
    /// wie im Feld „TX Hz" des Fensters.
    static constexpr int kSendeMinHz = 200;
    static constexpr int kSendeMaxHz = 2800;

    /// Hoehe der Frequenzskala unten. Platz fuer Strich und Zahl in
    /// kFontMicro, nicht mehr.
    static constexpr int kSkalaHoehe = 14;

    /// Abstand zweier Sendeplaetze bei Multi Answering, aus MSHV
    /// (gen_ft8.cpp:262). Dieselbe Zahl wie Ft8TxSource::
    /// kPlatzAbstandHz -- hier noch einmal, damit das Bild nicht von
    /// der Sendeseite abhaengt.
    static constexpr int kPlatzAbstandHz = 60;

    /// Wie viele Sendeplaetze gerade belegt sind (1 = der gewoehnliche
    /// Fall). Zeichnet die weiteren Marken leiser daneben.
    void setzePlaetze(int n);
    int  plaetze() const { return m_plaetze; }

    explicit Ft8Wasserfall(QWidget* parent = nullptr);
    ~Ft8Wasserfall() override;

    /// Toene aus dem Abgriff: verschraenktes Stereo, wie es die
    /// Tonmaschine liefert. Der Streifen nimmt den linken Kanal.
    void speiseStereo(const float* verschraenkt, int rahmen, int rateHz);

    /// Die Marke der Sendestelle.
    void setzeSendeTon(int hz);
    int sendeTon() const { return m_sendeTonHz; }

    // ── Die Regel, ohne Schirm pruefbar ─────────────────────────────

    /// Wo liegt `hz` im Streifen der Breite `breite`?
    static double hzZuX(int hz, int breite);

    /// Welche Tonlage meint ein Klick bei `x`? Auf den ERLAUBTEN
    /// Sendebereich begrenzt -- ein Klick an den linken Rand soll die
    /// kleinste sendbare Stelle ergeben und nicht null, denn eine
    /// Sendestelle bei 0 Hz gibt es nicht.
    static int xZuSendeHz(double x, int breite);

    // ── Haken fuer den Prueflauf ────────────────────────────────────
    int   spektrumSpaltenForTest() const { return m_spektrum.size(); }
    float spektrumMaxForTest() const;
    int   sammlerForTest() const { return m_sammler.size(); }
    int   plaetzeForTest() const { return m_plaetze; }

signals:
    /// Ein Klick in den Streifen. Das Fenster zieht sein Feld nach.
    void sendeTonGewaehlt(int hz);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void rechneSpektrum();

    static constexpr int kFftPunkte = 4096;
    static constexpr int kWasserfallZeilen = 120;

    QVector<float> m_sammler;        ///< Mono, Abtastrate wie geliefert
    int            m_rateHz{48000};
    QVector<float> m_spektrum;       ///< dB je Bildspalte, 0..1 normiert
    QImage         m_wasserfall;
    int            m_sendeTonHz{1500};
    int            m_plaetze{1};

    float*         m_fftEin{nullptr};
    fftwf_complex* m_fftAus{nullptr};
    fftwf_plan     m_plan{nullptr};
};

} // namespace Longpath
