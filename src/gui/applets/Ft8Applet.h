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
#include "models/LogEntry.h"
#include "core/Ft8Tx.h"

#include <QDateTime>
#include <QPointer>
#include <QHash>
#include <QVector>

#include <memory>

class QLabel;
class QPlainTextEdit;
class QSpinBox;

namespace Longpath { class Ft8Wasserfall; }
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
    /// Der Logbucheintrag zu einem fertigen Ablauf. Statisch und ohne
    /// Zustand, damit die Felder ohne Fenster zu pruefen sind — das
    /// Signal ist nur die Zustellung.
    /// Welcher Satz gehoert zu Tx1..Tx6? Die Zuordnung ist WSJT-X
    /// nachgebildet und steht als statische Regel da, damit sie ohne
    /// Fenster pruefbar ist. Gebaut wird der Satz selbst nicht hier,
    /// sondern von Ft8Ablauf::nachrichtFuer() -- eine Wahrheit.
    ///
    /// `partner` leer heisst: nur Tx6 (CQ) ergibt einen Satz, die
    /// uebrigen fuenf sprechen jemanden an, den es noch nicht gibt.
    /// Wer ist gerade das Gegenueber? Der laufende Ablauf, sonst das
    /// erste Wort im Textfeld (dort legt der Doppelklick es hin).
    QString aktuellerPartner() const;

    static QString txKnopfNachricht(int nummer,
                                    const QString& partner,
                                    const QString& meinRuf,
                                    const QString& meinLocator,
                                    int reportDb);

    /// `sendeTonHz` ist die Tonlage, auf der gesendet wurde -- seit
    /// 2026-10-18 waehlbar (siehe m_sendeTonHz). Sie geht in die
    /// gemeldete Frequenz ein, darum ein PARAMETER und keine
    /// Konstante: statisch bleibt die Funktion, damit die Rechnung
    /// ohne Fenster pruefbar ist.
    static Longpath::LogEntry logEintrag(const Ft8Ablauf& ablauf,
                                         double wahlHz,
                                         bool unteresSeitenband,
                                         Ft8Decoder::Protocol p,
                                         const QString& meinLocator,
                                         int sendeTonHz);

    /// Der eigene CQ-Ruf: „CQ <mein Ruf> <mein Locator>". Leer ohne
    /// Rufzeichen. Statisch, damit die Regel ohne Fenster zu pruefen
    /// ist — wie antwortAuf().
    static QString cqRuf(const QString& meinRuf, const QString& meinLocator);

    static QString eigenesRufzeichen();
    static QString eigenerLocator();

    void sendeNachricht(const QString& text);
    void abbrechen();
    bool sendetGerade() const { return m_sendet; }

    // Fuer den Pruefstand.
    QPushButton* cqButtonForTest()  const { return m_cqBtn; }
    QPushButton* stopButtonForTest() const { return m_stopBtn; }
    QPushButton* autoButtonForTest() const { return m_autoBtn; }
    /// Der „CQ"-Knopf, der den eigenen Ruf einsetzt. Nicht zu
    /// verwechseln mit cqButtonForTest() — das ist seit #244 der
    /// „Send"-Knopf (m_cqBtn), der Name ist dort eine Altlast.
    QPushButton* cqRufenButtonForTest() const { return m_cqRufenBtn; }
    const Ft8Ablauf& ablaufForTest() const { return m_ablauf; }
    /// Einen Fund einspeisen, als waere er dekodiert worden — der
    /// einzige Weg, auf dem der Ablauf vorankommt, also auch der
    /// einzige, auf dem er sich pruefen laesst.
    void fundForTest(const Longpath::Ft8Decode& d);
    /// Ein gesendetes Fenster ist um. Im Betrieb kommt das von einem
    /// Zeitgeber nach bis zu 15 s — im Prueflauf waere das eine
    /// Viertelminute Warten je Zug.
    void fensterZuEndeForTest() { fensterZuEnde(); }
    QLineEdit*   textFeldForTest()  const { return m_textFeld; }

    // Fuer den Pruefstand.
    Ft8Stream*      streamForTest() const { return m_stream; }
    QPlainTextEdit* listForTest()   const { return m_list; }
    QLabel*         statusForTest() const { return m_status; }
    QLabel*         lastLabelForTest() const { return m_lastLabel; }
    QPushButton*    nurCqButtonForTest() const { return m_nurCqBtn; }
    QSpinBox*       sendeTonFeldForTest() const { return m_tonFeld; }
    Longpath::Ft8Wasserfall* wasserfallForTest() const { return m_wasserfall; }
    int             sendeTonHzForTest() const { return m_sendeTonHz; }
    QPushButton*    ft8ButtonForTest() const { return m_ft8Btn; }
    QPushButton*    ft4ButtonForTest() const { return m_ft4Btn; }
    int             lastPeriodCountForTest() const { return m_lastPeriodCount; }

    /// Der Rufzeichen-Absender aus einer FT8-Nachricht, oder leer.
    /// Oeffentlich, weil er sonst nicht einzeln pruefbar waere.
    static QString senderCallsign(const QString& message);

signals:
    /// Ein QSO ist vollstaendig. Es geht denselben Weg wie eines aus
    /// WSJT-X (MainWindow -> RotorLogbookPanel::logExternalQso): eine
    /// Datei, eine Doppelt-Regel, ein Hochladen.
    void qsoFertig(const Longpath::LogEntry& eintrag);

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
    /// „CQ only": ein reiner ANSICHTS-Filter. Der Ablauf und die Funde
    /// fuer Panadapter/Logbuch laufen weiter ueber ALLE Zeilen -- wer
    /// dich ruft, darf nicht verschwinden, nur weil die Liste gerade
    /// aufgeraeumt ist. Bewusst nicht gesichert: er gehoert zum Blick
    /// auf das Band, nicht zur Einrichtung der Station.
    /// Haengt eine fertig gesetzte Zeile an die Liste -- gefiltert
    /// (siehe m_nurCq) und nach Dringlichkeit gefaerbt. `nachricht` ist
    /// der reine FT8-Text ohne die Spalten davor.
    void zeigeZeile(const QString& zeile, const QString& nachricht);

    /// Der zuletzt gehoerte Rauschabstand je Rufzeichen -- daraus
    /// bauen die Knoepfe Tx2/Tx3 ihren Rapport. Ohne ihn muesste man
    /// eine Zahl erfinden, und ein erfundener Rapport ist schlimmer
    /// als keiner.
    QHash<QString, int> m_letzterRapport;
    /// Die Tonlage im Durchlassbereich, auf der GESENDET wird.
    ///
    /// Betreiber 2026-10-18: „die sendestelle steht fest auf 1500 Hz"
    /// war der einzige Punkt der Luecken-Liste mit Folgen auf dem BAND
    /// statt am Schirm. Wer mit „Auto" auf einem vollen Band ruft,
    /// sendet sonst immer dorthin, wo schon jemand ist.
    ///
    /// Gesichert unter `Ft8/SendeTonHz` -- anders als „CQ only" gehoert
    /// das zur Einrichtung der Station und nicht zum Blick aufs Band:
    /// wer sich eine freie Stelle gesucht hat, will sie beim naechsten
    /// Start wiederhaben.
    QSpinBox* m_tonFeld{nullptr};
    /// Der Streifen ueber der Fundliste. Zeigt den Durchlassbereich und
    /// nimmt per Klick die Sendestelle entgegen -- ohne ihn waere die
    /// waehlbare Sendestelle eine Zahl ohne Blick aufs Band.
    Longpath::Ft8Wasserfall* m_wasserfall{nullptr};
    int m_sendeTonHz{1500};

    QPushButton* m_nurCqBtn{nullptr};
    bool m_nurCq{false};
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
    /// Ein QSO wird genau EINMAL gemeldet. zeigeAblauf() laeuft bei
    /// jedem Schritt, und der Zustand bleibt auf Fertig stehen — ohne
    /// diese Wache stuende dasselbe QSO mehrfach im Logbuch.
    bool         m_qsoGemeldet{false};
    QPushButton* m_autoBtn{nullptr};
    /// „CQ": legt den eigenen Ruf ins Feld. Mit Auto faengt damit der
    /// Ablauf als ANRUFER an — bis zum 2026-10-16 war diese Haelfte
    /// von Ft8Ablauf gebaut und geprueft, aber von nirgends
    /// erreichbar: starteAlsAnrufer() hatte keinen einzigen Aufrufer.
    QPushButton* m_cqRufenBtn{nullptr};
    void fensterZuEnde();
    void meldeFertigesQso();
    void ablaufSchritt();
    void zeigeAblauf();
    int          m_quelleVorher{-1};   ///< MicSource, zum Zuruecknehmen
};

} // namespace Longpath
