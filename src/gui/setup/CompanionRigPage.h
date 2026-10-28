#pragma once

// =================================================================
// src/gui/setup/CompanionRigPage.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Setup -> CAT & Network -> Companion Rig.
//
// ── Wofuer ──────────────────────────────────────────────────────────
//
// Die 2-m-Conteststation vom 2026-10-27: ein klassisches Funkgeraet
// (TS-590S, Elecraft K3) arbeitet als ZF hinter einem Transverter und
// sendet; Longpath haengt mit einem SDR am ZF-Ausgang und zeigt das
// Band. Diese Seite stellt die zwei Dinge ein, die dafuer fehlen:
//
//   1. WO das Geraet steht -- ueber rigctld, das diese Seite auf
//      Wunsch selbst startet.
//   2. WIE die ZF auf die Antenne umzurechnen ist -- der
//      Transverter-Versatz.
//
// ── Was die Seite bewusst nicht tut ─────────────────────────────────
//
// Sie haelt die Dauerverbindung nicht. Eine Setup-Seite wird bei jedem
// Oeffnen des Dialogs neu gebaut und beim Schliessen weggeworfen; was
// den Panadapter nachfuehrt, muss laenger leben und gehoert darum
// nicht hierher. Der Knopf „Test" baut eine Verbindung fuer die Dauer
// des Hinsehens auf und zeigt, was das Geraet meldet -- das ist das,
// was man an einer Einstellseite braucht: die Bestaetigung, dass
// Modell, Anschluss und Baudrate zusammenpassen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "gui/SetupPage.h"

#include "core/TransverterSetup.h"

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace Longpath {

class RigctldClient;
class RigctldProcess;
class CompanionRigController;
class RadioModel;

class CompanionRigPage : public SetupPage {
    Q_OBJECT
public:
    // ── Warum das Modell hier hereinkommt (2026-10-28) ────────────
    //
    // Stufe 1 hatte den Knopf „Test" mit einem EIGENEN RigctldClient
    // bedient. Das war fuer einen Test in Ordnung und fuer alles
    // andere falsch: die Dauerverbindung gehoert dem Modell, und zwei
    // Clients auf demselben rigctld teilen sich die Antworten -- es
    // antwortet in der Reihenfolge der Befehle, nicht nach Absender.
    //
    // Mit einem Modell bedient diese Seite also die EINE Verbindung.
    // Ohne (der Pruefstand baut die Seite nackt) bleibt der alte Weg
    // mit eigenem Client: so laesst sich die Seite ohne halbes
    // Programm pruefen.
    explicit CompanionRigPage(RadioModel* model = nullptr,
                              QWidget* parent = nullptr);
    ~CompanionRigPage() override;

    // ── Fuer den Pruefstand, und weil es ohne Oberflaeche pruefbar
    //    sein soll ──────────────────────────────────────────────────

    /// Die Geraeteliste fuer die Auswahl: die ueblichen ZF-Geraete
    /// zuerst, dann alles, was die installierte Hamlib kennt.
    ///
    /// Die Vorauswahl geschieht ueber den NAMEN, nie ueber die Nummer.
    /// Nummern wandern zwischen Hamlib-Fassungen; wer eine
    /// einprogrammiert, hat irgendwann ein anderes Geraet am Kabel als
    /// im Fenster. Beim Contestprogramm war genau das passiert: 1042
    /// war als FT-710 notiert und ist der FTDX-10.
    static QStringList bevorzugteGeraete();

    /// Wandelt MHz-Eingaben in Hz um, ohne das Rundungsloch von
    /// `mhz * 1e6`: 28.000001 * 1e6 ist nicht 28000001.
    static qint64 mhzZuHz(double mhz);

signals:
    /// Der Transverter-Versatz hat sich geaendert. Wer die Anzeige
    /// fuehrt, haengt sich hier ein.
    void transverterChanged(const TransverterSetup& t);

private:
    void baueCatGruppe();
    void baueTransverterGruppe();
    void ladeGeraeteListe();
    void speichereCat();
    void speichereTransverter();
    void zeigeTransverter();
    void starteTest();
    void beendeTest();
    void zeigeStatus(const QString& text);

    int  gewaehltesModell() const;

    // CAT
    QCheckBox*   m_folgen{nullptr};
    QComboBox*   m_geraet{nullptr};
    QLineEdit*   m_anschluss{nullptr};
    QComboBox*   m_baud{nullptr};
    QCheckBox*   m_selbstStarten{nullptr};
    QLineEdit*   m_host{nullptr};
    QSpinBox*    m_port{nullptr};
    QPushButton* m_test{nullptr};
    QLabel*      m_status{nullptr};

    // Transverter
    QCheckBox*      m_tvEin{nullptr};
    QComboBox*      m_tvVorgabe{nullptr};
    QDoubleSpinBox* m_tvVon{nullptr};
    QDoubleSpinBox* m_tvBis{nullptr};
    QDoubleSpinBox* m_tvVersatz{nullptr};
    QLabel*         m_tvText{nullptr};

    RadioModel* m_model{nullptr};

    /// Die Dauerverbindung, wenn es eine gibt. Nicht besessen.
    CompanionRigController* regler() const;

    TransverterSetup m_tv;
    bool m_fuellt{false};   ///< Felder werden gerade aus den Werten gefuellt

    std::unique_ptr<RigctldProcess> m_proc;
    std::unique_ptr<RigctldClient>  m_client;
};

} // namespace Longpath
