#pragma once

// =================================================================
// src/core/FlexApdState.h  (Longpath)
// =================================================================
//
// Der Zustand der Flex-eigenen Vorverzerrung (APD), so wie die
// Maschine ihn meldet. Reines Lesen: diese Klasse schickt nichts und
// kennt keine Verbindung — sie nimmt Statuszeilen entgegen und
// beantwortet Fragen. Die Befehle dazu stehen in
// `FlexCommandWire::apdEnableCommand` / `apdSamplerCommand` /
// `apdResetCommand` (src/core/FlexRadioConnection.h).
//
// ── Warum das NICHT PureSignal ist ───────────────────────────────────
//
// Bei OpenHPSDR rechnet Longpath die Korrektur selbst: WDSPs `calcc`
// bekommt Sende- und Rueckkopplungssignal und legt die Vorverzerrung
// in den eigenen Sendeweg (src/core/PureSignal.h). Die Flex kann das
// nicht annehmen — wir schicken ihr Mikrofonton, sie moduliert. Dafuer
// bringt sie ihre eigene Vorverzerrung mit, und seit SmartSDR 4.2.18
// laesst sie sich auch auf einen EXTERNEN Messpunkt stellen, also auf
// den Koppler hinter einer Endstufe.
//
// Longpath schaltet und zeigt sie also, statt sie zu rechnen.
//
// ── Was die Maschine schickt (am 2026-11-01 an der FLEX-8400 des
//    Betreibers gemessen, Firmware SmartSDR-MB 4.2.20.41343) ─────────
//
//   apd enable=0 configurable=1
//   apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=INVALID
//   apd sampler tx_ant=ANT2 valid_samplers=XVTA,RX_A selected_sampler=INVALID
//   apd sampler tx_ant=XVTA valid_samplers=RX_A selected_sampler=INVALID
//   apd slice=0 mmx=0 client_handle=0x0A0F163D ant=ANT1 freq=14.100000
//       rfpower=5 rx_error_mHz=0.000000 equalizer_active=0 configurable=1
//
// Zwei Fallen stecken darin, und beide kosten Zeit, wenn man sie
// uebersieht:
//
//   1. **Zwei verschiedene Zeilen heiszen „apd".** Die
//      Objektgrenze im Statussatz liegt vor dem ersten `=`
//      (FlexRadioConnection.cpp:138-166), also traegt die
//      Scheibenzeile denselben Objektnamen wie die Schalterzeile.
//      Wer den Zustand aus einer Zeile als Ganzes uebernimmt, loescht
//      mit jeder Scheibenmeldung das `enable` — es steht dort nicht
//      drin. Darum wird je SCHLUESSEL uebernommen, nie je Zeile.
//
//   2. **Die Maschine schickt Teilmeldungen.** Nach einem
//      `apd sampler`-Befehl kam
//      `apd sampler tx_ant=ANT1 selected_sampler=INVALID` — ohne
//      `valid_samplers`. Die Liste der moeglichen Messpunkte darf
//      dadurch nicht verschwinden.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-01 — Neu, von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude (Cowork). Struktur nach AetherSDRs
//                 APD-Feldern in TransmitModel/RadioModel [@d58e2b8a];
//                 die Schluessel und Werte sind am Geraet gemessen.
// =================================================================

#include <QMap>
#include <QString>
#include <QStringList>

namespace Longpath {

/// Ein Messpunkt je Sendeantenne: was die Maschine anbietet und was
/// davon gewaehlt ist.
struct FlexApdSampler {
    QStringList moegliche;   ///< `valid_samplers`, z. B. {"XVTA", "RX_A"}
    QString     gewaehlt;    ///< `selected_sampler`; "INVALID" heisst: keiner
};

class FlexApdState {
public:
    /// Sagt, ob eine Statuszeile zur Vorverzerrung gehoert. Gedacht
    /// fuer Aufrufer, die vor dem Verteilen aussortieren wollen.
    static bool gehoertZuApd(const QString& object);

    /// Nimmt eine Statuszeile an. Gibt true zurueck, wenn sie zur
    /// Vorverzerrung gehoerte UND etwas veraendert hat — so kann der
    /// Aufrufer sein Signal genau dann senden, wenn es etwas zu
    /// melden gibt (die Maschine schiebt denselben Zustand mehrfach).
    bool apply(const QString& object, const QMap<QString, QString>& kvs);

    /// Alles vergessen. Beim Trennen aufzurufen: ein gemerkter
    /// Zustand ohne Maschine dahinter ist eine Behauptung.
    void clear();

    /// Meldet die Maschine ueberhaupt eine Vorverzerrung? Vor der
    /// ersten Statuszeile ist das false — und bleibt false, solange
    /// nichts kommt (eine 6000er-Maschine meldet nie etwas).
    bool bekannt() const { return m_bekannt; }

    /// `enable` — laeuft die Vorverzerrung?
    bool eingeschaltet() const { return m_eingeschaltet; }

    /// `configurable` — darf der Messpunkt gewaehlt werden? Nur die
    /// 8000er-Reihe ab SmartSDR 4.2.18 meldet hier 1.
    bool einstellbar() const { return m_einstellbar; }

    /// `equalizer_active` — traegt die gelernte Kurve gerade?
    bool kurveTraegt() const { return m_kurveTraegt; }

    /// `ant` / `freq` / `rfpower` aus der Scheibenzeile. Sie sagen,
    /// WOFUER die Maschine zuletzt gelernt hat: die Kurve gilt je
    /// Antenne und Band, nicht allgemein.
    QString antenne() const { return m_antenne; }
    double  frequenzMhz() const { return m_frequenzMhz; }
    int     leistungProzent() const { return m_leistungProzent; }

    /// Die Sendeantennen, fuer die die Maschine einen Messpunkt fuehrt.
    QStringList sendeantennen() const;

    /// Was fuer diese Sendeantenne moeglich ist, und was gewaehlt ist.
    QStringList moeglicheMesspunkte(const QString& sendeantenne) const;
    QString     gewaehlterMesspunkt(const QString& sendeantenne) const;

    /// true, wenn fuer diese Sendeantenne ein brauchbarer Messpunkt
    /// steht. „INVALID" ist keiner — die Maschine meldet das wortwoertlich
    /// so, wenn nie einer gewaehlt wurde.
    bool hatMesspunkt(const QString& sendeantenne) const;

private:
    bool    m_bekannt{false};
    bool    m_eingeschaltet{false};
    bool    m_einstellbar{false};
    bool    m_kurveTraegt{false};
    QString m_antenne;
    double  m_frequenzMhz{0.0};
    int     m_leistungProzent{0};

    QMap<QString, FlexApdSampler> m_messpunkte;   ///< Schluessel: tx_ant
};

}  // namespace Longpath
