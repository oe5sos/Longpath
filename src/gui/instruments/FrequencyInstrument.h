#pragma once

// =================================================================
// src/gui/instruments/FrequencyInstrument.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Die Frequenz in der Handschrift der Instrumente ──────────────────
//
// OE5SOS, 2026-08-16, Entwurf ~/Downloads/frequenz-im-zeigerstil.html.
// Drei Varianten, im Widget umschaltbar:
//
//   A Bandstreifen — die Rille gerade gezogen, Verlauf bis zur
//     aktuellen Stelle im Band, Bandkanten als rote Striche, die
//     naechstgelegene Beschriftung hell.
//   B Flacher Bogen — derselbe Bogen wie die Instrumente, sehr weit
//     und flach, Marke auf der Frequenz.
//   C Nur die Zahl — Glut darunter, sonst nichts.
//
// Mulde, Verlauf und Glut kommen aus InstrumentPainter, die Geometrie
// aus LinearSpine bzw. ArcSpine. Dieselben drei Mittel wie beim Zeiger
// und beim Balken; hier steht nichts davon ein zweites Mal.
//
// ── Es ZEIGT nicht nur, es BEDIENT ──────────────────────────────────
//
// Auflage des Betreibers, 2026-08-17: „Das Frequenz-Widget muss Rad
// und Klick-Eingabe tragen, nicht nur anzeigen — sonst nimmt Punkt 3
// das Abstimmen weg."
//
// Also: jede Ziffer eine eigene Trefferflaeche. Mausrad darueber dreht
// DIESE Dekade. Doppelklick auf die Zahl oeffnet ein Eingabefeld.
// Die beiden Hertz-Stellen stehen matt, weil sie beim Abstimmen
// dauernd laufen.
//
// ── Bandkanten ──────────────────────────────────────────────────────
//
// Rot, und zwar kraeftig. CLAUDE.local.md: „Die Bandkante bleibt rot.
// Sie ist keine dezente Markierung und faellt nicht unter die
// Zwei-Prozent-Regel des Hausstils." Die Grenzen kommen aus
// AmateurBands::containing() — dieselbe Tabelle, die auch der
// SWR-Durchlauf benutzt.
//
// Der Guard selbst wird hier NICHT umgangen und auch nicht nachgebaut:
// dieses Widget aendert die Frequenz ueber dasselbe Signal wie jede
// andere Abstimmung. Stellenweises Drehen fuehrt so wenig ueber eine
// Bandkante wie das Rad auf der Flagge.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QList>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QStackedWidget;

namespace NereusSDR {

class FrequencyInstrument : public QWidget {
    Q_OBJECT

public:
    enum class Form {
        BandStrip,   ///< A — Zahl mit Bandstreifen
        FlatArc,     ///< B — Zahl mit flachem Bogen
        NumberOnly,  ///< C — nur die Zahl, mit Glut
    };

    explicit FrequencyInstrument(QWidget* parent = nullptr);

    void setFrequency(double hz);
    double frequency() const { return m_hz; }

    /// Die zweite Scheibe und welche von beiden aktiv ist — damit Split
    /// ohne ein zweites Feld sichtbar ist.
    void setOtherFrequency(double hz);
    void setActiveIsThis(bool active);

    void setForm(Form f);
    Form form() const { return m_form; }

    // ── Eingetippte Frequenz deuten ──────────────────────────────────
    //
    // Am 2026-08-18 aus VfoWidget herausgeloest, wo der Parser als
    // statische Methode stand und mit 24 Faellen geprueft war. Er MUSS
    // die Flagge ueberleben: commitEdit() rief bis dahin ein blosses
    // toDouble() und nahm MHz an — damit waere „7.230.000" still
    // verworfen worden (zwei Punkte) und „7230" als 7230 MHz gelandet.
    // Das ist wortwoertlich Fehlerbericht #73, den dieser Parser
    // seinerzeit geschlossen hat.
    //
    // Deutet europaeische und amerikanische Tausendertrennung,
    // Dezimalkomma und -punkt, ausgeschriebene und kurze Einheiten
    // (MHz/kHz/Hz, M/k), gross wie klein. Liefert Hz, oder -1 wenn
    // sich nichts Sinnvolles lesen laesst.
    //
    // KLEMMT NICHT: der Aufrufer entscheidet, was ausserhalb des
    // Bereichs geschieht. Ein Parser, der klemmt, kann nicht mehr
    // sagen, ob die Eingabe unsinnig oder nur ausserhalb war.
    static double parseUserFrequency(const QString& raw);

    /// Die Ziffernzeile als Text, aus den echten Schildern in
    /// Layoutfolge gelesen. Fuer den Test, der die GRUPPIERUNG
    /// festnagelt und nicht nur den Wert — die beiden liefen am
    /// 2026-08-18 auseinander.
    QString groupedText() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {240, 74}; }

signals:
    /// Neue Frequenz, vom Rad oder aus dem Eingabefeld. Der Empfaenger
    /// leitet sie auf denselben Weg wie jede andere Abstimmung —
    /// dieses Widget schreibt nichts selbst.
    void frequencyEdited(double hz);

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    void buildDigits();
    void refreshDigits();
    void refreshVfoRow();
    void beginEdit();
    void commitEdit();

    /// Anteil der Frequenz im aktuellen Band, oder -1 ausserhalb.
    double bandFraction() const;

    double m_hz{0.0};
    double m_otherHz{0.0};
    bool   m_activeIsThis{true};
    Form   m_form{Form::BandStrip};

    QWidget*        m_digitRow{nullptr};
    QHBoxLayout*    m_digitLayout{nullptr};
    QList<QLabel*>  m_digits;      ///< nur die Ziffern, in Anzeigefolge
    QList<double>   m_decades;     ///< Hz je Ziffer, gleiche Reihenfolge
    QStackedWidget* m_stack{nullptr};
    QLineEdit*      m_edit{nullptr};
    QLabel*         m_vfoRow{nullptr};

    /// Acht Ziffern: 7.139.700 — zehn MHz bis Einer-Hertz.
    static constexpr int kDigitCount = 8;
};

} // namespace NereusSDR
