// =================================================================
// src/gui/applets/Ft8Applet.h  (Longpath)
// =================================================================
//
// Longpath-original. Die Flaeche zum nativen FT8/FT4-Dekoder.
//
// Kein Port: weder Thetis noch AetherSDR dekodieren FT8 selbst (beide
// schicken den Ton an WSJT-X), es gibt also keine fremde Flaeche, von
// der hier etwas uebernommen werden koennte. Was dekodiert, liegt in
// Ft8Decoder/Ft8Stream; die Rechenarbeit macht ft8_lib (MIT, siehe
// third_party/ft8_lib/FT8LIB-PROVENANCE.md).
//
// ── Die Spaltenordnung ist geliehen ─────────────────────────────────
//
// Die Zeile heisst „HHMMSS  dB  DT  Freq  Nachricht" — dieselbe
// Reihenfolge, die WSJT-X in „Band Activity" zeigt. Nicht aus Code
// uebernommen, sondern weil Martin sie kennt und eine zweite
// Spaltenordnung fuer dieselbe Sache nur Suchzeit kostet. Dieselbe
// Begruendung wie bei N1MM/DXLog im Contestprogramm.
//
// ── Was hier NICHT ist ──────────────────────────────────────────────
//
// Senden. Keine Taste, keine Folgeschaltung, kein CQ. Ein Dekoder,
// der von selbst sendet, ist ein anderes Ding als einer, der zuhoert
// — und der Sendeweg gehoert nicht in ein Fenster, das man beim
// Aufraeumen des Layouts aus Versehen anklickt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#pragma once

#include "AppletWidget.h"

#include "core/Ft8Decoder.h"
#include "core/Ft8Ablauf.h"
#include "core/Ft8Tx.h"

#include <QDateTime>
#include <QPointer>
#include <QVector>

#include <memory>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QLineEdit;
class QPushButton;
class QTimer;

namespace Longpath {

class AudioTapRing;
class Ft8Stream;
class RadioModel;
class SliceModel;

/// Nativer FT8/FT4-Dekoder, sichtbar in den Datenmodi (DIGU/DIGL).
///
/// Aufbau:
///   1. Kopfzeile   — FT8 / FT4, Fensteruhr, Fuellstand
///   2. Liste       — eine Zeile je Fund, Spalten wie in WSJT-X
///   3. Fusszeile   — Zahl der Funde im letzten Fenster, Leeren
class Ft8Applet : public AppletWidget {
    Q_OBJECT
public:
    explicit Ft8Applet(RadioModel* model, QWidget* parent = nullptr);
    ~Ft8Applet() override;

    QString appletId()    const override { return QStringLiteral("Ft8Decoder"); }
    QString appletTitle() const override { return QStringLiteral("FT8 Decoder"); }
    void    syncFromModel() override;

    /// Wie beim CW- und RTTY-Dekoder: eine Flaeche, die der jeweils
    /// aktiven Scheibe folgt.
    void setSlice(SliceModel* slice);

    void setProtocol(Ft8Decoder::Protocol p);
    Ft8Decoder::Protocol protocol() const;

    // ── Senden (2026-10-15) ─────────────────────────────────────────
    //
    // Betreiber: „ft4-ft8 fertig machen."
    //
    // Was hier NICHT eingebaut ist: eine Folgeschaltung. Kein
    // automatisches Antworten, kein CQ in Schleife, kein Senden nach
    // einem Fund. Der Betreiber drueckt, das Programm sendet EIN
    // Fenster, dann ist Schluss. Ein Dekoder, der von selbst sendet,
    // ist ein anderes Ding — und einer, der es im Hintergrund tut,
    // ein gefaehrliches.
    //
    // Der Ablauf: Knopf -> die Nachricht wird auf den Ton gelegt ->
    // beim naechsten eigenen Fenster schaltet das Applet den
    // Mikrofonweg auf die FT8-Quelle, tastet, laesst das Fenster
    // laufen und nimmt beides wieder zurueck.
    /// Die Antwort auf eine Zeile der Fundliste, oder leer.
    ///
    /// `zeile` ist eine Zeile im Format der Liste (Spalten wie in
    /// WSJT-X „Band Activity": HHMMSS dB DT Freq Nachricht). Heraus
    /// kommt der Standardanruf „<sein Ruf> <mein Ruf> <mein Locator>"
    /// — das, was man auf ein CQ hin sendet.
    ///
    /// Leer, wenn die Zeile kein Rufzeichen hergibt, wenn das eigene
    /// Rufzeichen fehlt, oder wenn man sich selbst anklickt.
    ///
    /// Statisch und ohne Zustand, damit die Regel ohne Fenster und
    /// ohne Maus zu pruefen ist — der Doppelklick ist nur der Ausloeser.
    static QString antwortAuf(const QString& zeile,
                              const QString& meinRuf,
                              const QString& meinLocator);

    /// Das eigene Rufzeichen aus den Einstellungen. `User/Callsign`
    /// ist der kanonische Schluessel (SpotHubDialog schreibt ihn);
    /// `StationCallsign` ist der Rueckfall.
    static QString eigenesRufzeichen();
    static QString eigenerLocator();

    void sendeNachricht(const QString& text);
    void abbrechen();
    bool sendetGerade() const { return m_sendet; }

    // Fuer den Pruefstand.
    QPushButton* cqButtonForTest()  const { return m_cqBtn; }
    QPushButton* stopButtonForTest() const { return m_stopBtn; }
    QPushButton* autoButtonForTest() const { return m_autoBtn; }
    const Ft8Ablauf& ablaufForTest() const { return m_ablauf; }
    QLineEdit*   textFeldForTest()  const { return m_textFeld; }

    // Fuer den Pruefstand.
    Ft8Stream*      streamForTest() const { return m_stream; }
    QPlainTextEdit* listForTest()   const { return m_list; }
    QLabel*         statusForTest() const { return m_status; }
    QLabel*         lastLabelForTest() const { return m_lastLabel; }
    QPushButton*    ft8ButtonForTest() const { return m_ft8Btn; }
    QPushButton*    ft4ButtonForTest() const { return m_ft4Btn; }
    int             lastPeriodCountForTest() const { return m_lastPeriodCount; }

    /// Der Rufzeichen-Absender aus einer FT8-Nachricht, oder leer.
    /// Oeffentlich, weil er sonst nicht einzeln pruefbar waere.
    static QString senderCallsign(const QString& message);

protected:
    /// Doppelklick in die Fundliste: die Zeile wird zur Antwort im
    /// Eingabefeld. Gesendet wird dadurch NICHTS — der Druck auf den
    /// Knopf bleibt beim Betreiber.
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onDecoded(const QVector<Longpath::Ft8Decode>& funde,
                   const QDateTime& periodStart);

private:
    void buildUI();
    void updateAudioTap();
    void updateStatus();

    QPointer<SliceModel> m_slice;
    Ft8Stream* m_stream{nullptr};
    std::unique_ptr<AudioTapRing> m_tapRing;
    QTimer* m_pumpTimer{nullptr};
    int     m_lastPeriodCount{-1};
    int     m_lineCount{0};

    QLabel*         m_status{nullptr};
    QLabel*         m_lastLabel{nullptr};
    QProgressBar*   m_fill{nullptr};
    QPlainTextEdit* m_list{nullptr};
    QPushButton*    m_ft8Btn{nullptr};
    QPushButton*    m_ft4Btn{nullptr};
    QPushButton*    m_clearBtn{nullptr};

    // Senden
    QLineEdit*   m_textFeld{nullptr};
    QPushButton* m_cqBtn{nullptr};
    QPushButton* m_stopBtn{nullptr};
    QTimer*      m_sendeTimer{nullptr};
    /// In welcher Haelfte der Minute diese Station sendet. „Gerade"
    /// ist die Vorgabe und entspricht WSJT-X' „Tx even/1st".
    Ft8Periode   m_periode{Ft8Periode::Gerade};
    bool         m_sendet{false};

    // ── Der QSO-Ablauf (2026-10-16) ──────────────────────────────────
    //
    // Ft8Ablauf rechnet nur: er sagt, was im naechsten Fenster zu
    // senden WAERE. Gesendet wird hier — mit Bandplanpruefung und
    // Tasten, wie bei jedem anderen Druck auf „Send" auch.
    //
    // m_autoBtn ist der Schalter dafuer und steht aus. Ohne ihn legt
    // ein Doppelklick die Antwort nur ins Feld (siehe antwortAuf);
    // mit ihm faengt der Ablauf an und sendet, bis er fertig ist,
    // aufgibt oder angehalten wird.
    Ft8Ablauf    m_ablauf;
    QPushButton* m_autoBtn{nullptr};
    void fensterZuEnde();
    void ablaufSchritt();
    void zeigeAblauf();
    int          m_quelleVorher{-1};   ///< MicSource, zum Zuruecknehmen
};

} // namespace Longpath
