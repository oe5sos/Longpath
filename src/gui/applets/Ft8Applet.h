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

#include <QDateTime>
#include <QPointer>
#include <QVector>

#include <memory>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
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
};

} // namespace Longpath
