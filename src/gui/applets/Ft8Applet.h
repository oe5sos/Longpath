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
#include "core/DigiFrequenzen.h"

#include "core/DigiFrequenzen.h"
#include "core/Ft8Decoder.h"
#include "core/Ft8Ablauf.h"
#include "models/LogEntry.h"
#include "core/Ft8Tx.h"

#include <QDateTime>
#include <QPointer>
#include <QHash>
#include <QColor>
#include <QVector>

#include <memory>

class QAction;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QSpinBox;

namespace Longpath { class Ft8Wasserfall; }
namespace Longpath { class Ft8Fensterbalken; }
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

    /// Die Nachricht aus einer Anzeigezeile — ohne die Landspalte.
    ///
    /// Seit der Landspalte (#311) steht hinter der Nachricht der
    /// Landesname. `simplified()` plus `section(' ', 4)` nimmt ihn
    /// mit, und `section(' ', -1)` liefert dann „Hungary" statt
    /// „JN86". Das traf den Doppelklick an zwei Stellen.
    ///
    /// Die Spalten stehen fest (siehe zeilentext()): vier Zahlenfelder,
    /// dann die Nachricht, dann — durch ZWEI Leerzeichen getrennt —
    /// das Land. Innerhalb einer FT8-Nachricht stehen nie zwei
    /// Leerzeichen hintereinander, also trennt das erste Doppelte
    /// sauber. Ohne Landspalte bleibt die Zeile unveraendert.
    static QString nachrichtAusZeile(const QString& zeile);

    /// Der Locator der Gegenstation, oder leer. Geprueft wird die Form
    /// eines Maidenhead-Feldes (zwei Buchstaben, zwei Ziffern) —
    /// „JN86" ist einer, „R-09" und „+02" sind keine.
    static QString locatorAusNachricht(const QString& nachricht);

    /// Was ich gerade sende, in die rechte Spalte („Rx Frequency").
    /// Wie in WSJT-X: links steht das Band, rechts das eigene
    /// Gespraech — die Antworten an mich standen dort schon.
    void zeigeEigeneSendung(const QString& nachricht);

    /// Welcher Sorte ist eine Zeile? Die REIHENFOLGE ist die Aussage:
    ///
    ///   1. AnMich      -- jemand ruft DICH. Schlaegt alles.
    ///   2. NeuesLand   -- ein Land, das noch nie im Logbuch stand.
    ///   3. Cq          -- jemand ist rufbar.
    ///   4. Gewoehnlich -- ein fremdes QSO.
    ///
    /// „Neues Land" steht ueber „CQ", weil ein neues Land auch dann
    /// zaehlt, wenn die Station gerade kein CQ ruft -- man kann sie
    /// trotzdem im Auge behalten. Es steht UNTER „an mich", weil ein
    /// Ruf an die eigene Station immer das Dringendere ist.
    ///
    /// Statisch, damit die Reihenfolge ohne Fenster, ohne Logbuch und
    /// ohne cty.dat pruefbar ist.
    enum class Zeilenart { AnMich, NeuesLand, Cq, Gewoehnlich };
    static Zeilenart zeilenart(bool anMich, bool neuesLand, bool istCq);

    /// Eine Zeile der Liste, zusammengesetzt. Spalten wie in WSJT-X
    /// „Band Activity", dahinter das Land.
    ///
    /// Betreiber am 2026-10-21: „vielleicht kann man auch das land
    /// hinzufuegen." Der Auflaeser kannte es die ganze Zeit -- das
    /// Fenster benutzte den Namen nur, um eine Farbe zu waehlen, und
    /// warf ihn dann weg.
    ///
    /// Die Nachricht wird auf feste Breite gebracht, sonst stuende das
    /// Land bei jeder Zeile woanders und die Spalte waere keine. Ist
    /// sie laenger, gewinnt die Nachricht: lieber eine verrutschte
    /// Landspalte als eine abgeschnittene Nachricht.
    ///
    /// Statisch und ohne Zustand, damit die Spalten ohne Fenster,
    /// ohne Logbuch und ohne cty.dat pruefbar sind.
    static QString zeilentext(const QString& uhrzeit, int snrDb,
                              double dtSekunden, int hz,
                              const QString& nachricht,
                              const QString& land);

    /// Das Land zu einem Rufzeichen, oder leer. Leer heisst „nicht
    /// aufloesbar" und nicht „kein Land" -- dann bleibt die Spalte
    /// frei, statt etwas zu behaupten.
    QString landFuer(const QString& rufzeichen) const;

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
    /// Der „CQ"-Knopf, der den eigenen Ruf einsetzt. Nicht zu
    /// verwechseln mit cqButtonForTest() — das ist seit #244 der
    /// „Send"-Knopf (m_cqBtn), der Name ist dort eine Altlast.
    QPushButton* cqRufenButtonForTest() const { return m_cqRufenBtn; }
    const Ft8Ablauf& ablaufForTest() const { return m_ablauf; }
    /// Nur fuer Werkzeuge und Prueflaeufe: den Ablauf von aussen in
    /// einen Zustand bringen, um die Arbeitsleiste abzubilden.
    Ft8Ablauf& ablaufForTestSchreibend() { return m_ablauf; }
    void zeigeAblaufForTest() { zeigeAblauf(); }
    /// Der Zaehler fuer die CQ-Versuche, fuer den Pruefstand.
    class QSpinBox* cqVersucheSpinForTest() const { return m_cqVersucheSpin; }
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
    /// Der Zeitgeber, der auf den Beginn des eigenen Fensters wartet.
    /// Ein Prueflauf stellt ihn auf 0 und laesst ihn ablaufen — nur so
    /// wird der Sendeweg wirklich gefahren und nicht nur die Methode
    /// am Ende davon gerufen.
    QTimer* sendeTimerForTest() const { return m_sendeTimer; }
    QLabel*         statusForTest() const { return m_status; }
    QLabel*         lastLabelForTest() const { return m_lastLabel; }
    /// Hiesz bis zum 2026-10-30 „CQ only" und war ein Haken in der
    /// Kopfzeile; jetzt ist es die Pille „CQ". Derselbe Zweck,
    /// derselbe Zugang.
    QPushButton*    nurCqButtonForTest() const { return m_sichtCq; }
    QPushButton*    nurNeueLaenderButtonForTest() const { return m_sichtNeuesLand; }
    QPushButton*    sichtAllesButtonForTest() const { return m_sichtAlles; }
    QPushButton*    sichtAnMichButtonForTest() const { return m_sichtAnMich; }
    QPushButton*    sichtFreqButtonForTest() const { return m_sichtFreq; }
    QSpinBox*       sendeTonFeldForTest() const { return m_tonFeld; }
    QSpinBox*       rundenFeldForTest() const { return m_rundenFeld; }
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
    /// Die Knoepfe FT8/FT4 IM Fenster gedrueckt.
    ///
    /// Betreiber am 2026-10-21: „bei ft4 und ft8 muss er glaube ich auch
    /// die frequenz anpassen und aendern." Es gab zwei Stellen, an denen
    /// man FT8 und FT4 waehlen konnte, und nur eine davon stellte das
    /// Geraet. Das Fenster kennt die Anrufstellen nicht und soll sie
    /// nicht kennen -- es meldet den Wunsch, MainWindow richtet her.
    void digiGewuenscht(Longpath::DigiFrequenzen::Digimodus modus);

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

    /// Das Blatt hinter dem ⚙ in der Kopfzeile: eigenes Rufzeichen und
    /// Locator. Betreiber 2026-10-21: „sollt in der taksleiste im
    /// decoder eine optionsmöglichkeit geben" -- und die Optionen
    /// gehoeren bei ihm immer rechts oben in den Panelkopf.
    void zeigeOptionen();
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
    // ── Der Zeilenspeicher (2026-10-30) ───────────────────
    //
    // Bis dahin ging jeder Fund direkt in die Textflaeche, und die
    // Filter wirkten nur auf NEUE Zeilen: wer „CQ only" einschaltete,
    // sah das Band unveraendert, bis die naechste Periode durch war.
    // Mit den Pillen aus Entwurf C faellt das sofort auf — also wird
    // behalten, was gefunden wurde, und die Liste bei jedem
    // Filterwechsel neu gezeichnet.
    //
    // Gespeichert wird das ERGEBNIS der Einordnung, nicht die
    // Nachricht allein: „neues Land" kostet eine Abfrage im
    // DXCC-Aufloeser, und die soll nicht bei jedem Pillendruck
    // hundertmal neu laufen.
    struct Fundzeile {
        QString text;        ///< die fertige Zeile, Spalten schon gesetzt
        QString nachricht;   ///< der reine FT8-Satz
        int     tonHz{0};
        bool    anMich{false};
        bool    istCq{false};
        bool    neuesLand{false};
        QColor  landFarbe;
        bool    eigeneSendung{false};
    };
    QVector<Fundzeile> m_zeilen;
    static constexpr int kMaxZeilen = 400;

    /// Welche Zeilen zu sehen sind. Eine Pille, nicht vier Haken:
    /// zwei Filter gleichzeitig ergeben Faelle, die niemand im Kopf
    /// behaelt (und „CQ only" + „New DXCC only" zeigte frueher die
    /// Schnittmenge, ohne dass das irgendwo stand).
    enum class Sicht { Alles, AnMich, Cq, NeuesLand, MeineFrequenz };
    Sicht m_sicht{Sicht::Alles};
    /// Die Liste aus dem Speicher neu zeichnen.
    void zeichneListe();
    /// Gehoert diese Zeile in die aktuelle Sicht?
    bool inSicht(const Fundzeile& z) const;
    /// Eine Zeile in den Speicher und, wenn sie passt, in die Liste.
    void merkeZeile(const Fundzeile& z);
    /// Eine Zeile unten an die Textflaeche haengen, richtig gefaerbt.
    void haengeAn(const Fundzeile& z);
    /// „10 Funde" bzw. „3 von 10" rechts neben den Pillen.
    void zeigeFundZahl();
    /// Die Arbeitsleiste nachziehen: mit wem, welcher Zug, was danach.
    void zeigeArbeit();

    QPlainTextEdit* m_list{nullptr};
    // Zweite Spalte wie in WSJT-X: links alles („Band Activity"),
    // rechts nur, was auf der eigenen Stelle liegt oder an mich geht
    // („Rx Frequency"). Ohne sie geht die eigene Stelle in einem
    // vollen Band unter -- genau dort, wo man hinsehen muss.

    // Die Signalbreite von FT8 und FT4 (50 Hz). Siehe zeigeZeile().
    static constexpr int kRxFensterHz = 50;
    /// „CQ only": ein reiner ANSICHTS-Filter. Der Ablauf und die Funde
    /// fuer Panadapter/Logbuch laufen weiter ueber ALLE Zeilen -- wer
    /// dich ruft, darf nicht verschwinden, nur weil die Liste gerade
    /// aufgeraeumt ist. Bewusst nicht gesichert: er gehoert zum Blick
    /// auf das Band, nicht zur Einrichtung der Station.
    /// Haengt eine fertig gesetzte Zeile an die Liste -- gefiltert
    /// (siehe m_nurCq) und nach Dringlichkeit gefaerbt. `nachricht` ist
    /// der reine FT8-Text ohne die Spalten davor.
    // `tonHz` ist die Audiostelle des Fundes: sie entscheidet, ob die
    // Zeile auch rechts erscheint.
    void zeigeZeile(const QString& zeile, const QString& nachricht, int tonHz);

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
    /// Wie viele Runden der Dekoder ueber ein Fenster geht (1..3).
    /// Vorgabe 2; siehe core/Ft8Subtract.h.
    QSpinBox*    m_rundenFeld{nullptr};
    /// Der Streifen ueber der Fundliste. Zeigt den Durchlassbereich und
    /// nimmt per Klick die Sendestelle entgegen -- ohne ihn waere die
    /// waehlbare Sendestelle eine Zahl ohne Blick aufs Band.
    Longpath::Ft8Wasserfall* m_wasserfall{nullptr};
    int m_sendeTonHz{1500};

    QPushButton* m_sichtAlles{nullptr};
    QPushButton* m_sichtAnMich{nullptr};
    QPushButton* m_sichtCq{nullptr};
    QPushButton* m_sichtNeuesLand{nullptr};
    QPushButton* m_sichtFreq{nullptr};
    QLabel*      m_fundZahl{nullptr};

    // ── Die Arbeitsleiste (Entwurf C, 2026-10-30) ────────────
    //
    // Was beim automatischen Durchlauf zaehlt, steht unten, wo die
    // Hand ist: mit wem, welcher Zug, was danach, wie weit das
    // Fenster. Betreiber am 2026-10-30 zu Entwurf C: „c bitte".
    QWidget* m_arbeit{nullptr};
    QLabel*  m_arbeitPartner{nullptr};
    QLabel*  m_arbeitLand{nullptr};
    QLabel*  m_arbeitZustand{nullptr};
    QLabel*  m_arbeitZug{nullptr};
    QLabel*  m_arbeitDanach{nullptr};
    QLabel*  m_arbeitUhr{nullptr};
    Longpath::Ft8Fensterbalken* m_arbeitBalken{nullptr};

    /// Der Wasserfall nahm ein Viertel der Hoehe und lebt erst, wenn
    /// Ton hereinkommt. Jetzt abschaltbar; die Wahl bleibt erhalten.
    /// Fox-Betrieb: ein Schalter, keine Erkennung. Ob eine Station im
    /// Fox-Betrieb laeuft, steht erst in ihrem 0.1-Satz, und den hoert
    /// man erst, wenn sie jemanden aufruft.
    QPushButton* m_houndBtn{nullptr};
    bool         m_houndBetrieb{false};

    QPushButton* m_wasserfallBtn{nullptr};
    bool         m_wasserfallAn{true};

    /// Die sechs Standardsaetze, jetzt hinter einem Knopf statt als
    /// sechs gleich aussehende Knoepfe in einer Reihe.
    QPushButton* m_handBtn{nullptr};
    QAction*     m_txAktionen[6]{};

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
    // Ein Doppelklick auf einen Fund nimmt das QSO an, und ab da
    // laeuft es bis 73 von selbst. Betreiber am 2026-10-22: „wichtig
    // ist auch, dass nach der annahme des qso das qso automatisch
    // anlaeuft bis 73."
    //
    // Bis dahin sass davor der Schalter „Auto", und er stand aus:
    // ein Doppelklick legte die Antwort nur ins Feld. Das war
    // derselbe Fehlgriff wie beim CQ-Knopf (#315) -- ein Knopf, der
    // nicht tut, was er verspricht. Der Schalter ist weg; angehalten
    // wird mit „Stop", und der haelt jetzt auch den Ablauf an.
    Ft8Ablauf    m_ablauf;
    /// Ein QSO wird genau EINMAL gemeldet. zeigeAblauf() laeuft bei
    /// jedem Schritt, und der Zustand bleibt auf Fertig stehen — ohne
    /// diese Wache stuende dasselbe QSO mehrfach im Logbuch.
    bool         m_qsoGemeldet{false};
    /// Laeuft der Ruf vom CQ-Knopf? Dann geht es nach einem fertigen
    /// QSO von selbst weiter. Ein Doppelklick in die Fundliste ist
    /// eine Einzelentscheidung und setzt das zurueck, „Stop“ ebenso.
    bool         m_cqAutomatik{false};
    /// „CQ": ruft CQ und faengt damit den Ablauf als ANRUFER an — bis zum 2026-10-16 war diese Haelfte
    /// von Ft8Ablauf gebaut und geprueft, aber von nirgends
    /// erreichbar: starteAlsAnrufer() hatte keinen einzigen Aufrufer.
    QPushButton* m_cqRufenBtn{nullptr};

    /// Wie oft ein CQ-Ruf von selbst wiederholt wird, und wie viele
    /// Versuche noch offen sind.
    ///
    /// Betreiber am 2026-10-22: „der cq ruf funktioniert nicht" (er
    /// fuellte nur das Textfeld) und „man soll auch entscheiden
    /// koennen, wie oft er das automatisch probiert".
    class QSpinBox* m_cqVersucheSpin{nullptr};
    int m_cqVersucheOffen{0};
    void fensterZuEnde();

    /// CQ und Send zeigen, was gerade laeuft — nicht, was zuletzt
    /// geklickt wurde.
    void zeigeKnopfzustand();
    /// Einen CQ-Ruf anfangen. Eine Stelle fuer den Knopf UND fuer den
    /// Wiederanlauf nach einem fertigen QSO.
    void starteCqRuf();
    void meldeFertigesQso();
    void ablaufSchritt();
    void zeigeAblauf();
    int          m_quelleVorher{-1};   ///< MicSource, zum Zuruecknehmen
};

} // namespace Longpath
