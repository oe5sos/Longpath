// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prüfstand. Kein Port.
//
// =================================================================
// Ein Zoom in CTUN ist kein Zug am Fenster.
// =================================================================
//
// Am 2026-10-30 wurde für den herkömmlichen Betrieb festgehalten:
// *„Nur ein Zug ist ein Befehl"* — das Nachscrollen des Fensters an
// den Bildrändern darf nicht als Abstimmbefehl zurückgeschrieben
// werden, sonst zieht das Fenster den VFO um 80 kHz hinter sich her
// (`core/FensterRandScroll.h`, an der FLEX-8400 viermal gemessen).
//
// Dieselbe Frage stellt sich in CTUN, nur für ein anderes Ziel. Dort
// schreibt MainWindow jede Mittenänderung als **DDC-Verstellung** ins
// Gerät:
//
//     forceHardwareFrequency(stream, centerHz);
//     sw->setDdcCenterFrequency(centerHz);
//     m_radioModel->reshiftSlicesOnStream(stream, centerHz);
//
// Das ist für einen Zug am Panadapter genau richtig — der Zug IST die
// Bitte, die DDC zu verschieben. Nur ist der Zug nicht der einzige
// Weg, auf dem sich `m_centerHz` bewegt. `PanadapterApplet.cpp` zählt
// sie seit dem 2026-10-27 auf: vier Stellen schreiben ihn direkt und
// melden `centerChanged`. Drei davon sind **keine Züge**, sondern das
// Fenster, das sich von selbst auf den VFO stellt:
//
//   * Zoomziehen am Frequenzmaßstab (SpectrumWidget.cpp, m_draggingBandwidth)
//   * Zoomen mit Strg/Cmd + Mausrad
//   * `recenterOnVfo()`
//
// Die vierte — das Randscrollen — läuft in CTUN gar nicht
// (`setVfoFrequency` prüft `!m_ctunEnabled`), und genau deshalb fiel
// die Lücke bisher nicht auf: die eine Stelle, die eine Fahne trägt,
// ist die eine, die in CTUN nie feuert.
//
// Für den Betreiber heißt das: in CTUN hineinzoomen verstellt die DDC
// auf den VFO. CTUN verspricht das Gegenteil — die DDC steht fest,
// und der VFO wandert darin. Ein zweiter Slice im selben Fenster wird
// dabei mitgezogen.
//
// Seit dem 2026-11-02 wiegt es schwerer: `reshiftSlicesOnStream` führt
// seitdem den Zuteiler nach (PR #375). Eine Mitte, die aus einem Zoom
// stammt, landet damit nicht mehr nur im Gerät, sondern auch in der
// Buchführung, aus der jede spätere Platzierung schöpft.
//
// Dieser Lauf hält fest, was das Fenster über seine eigene Bewegung
// sagen muss, damit der Empfänger beides unterscheiden kann.
//
// Modification history (Longpath):
//   2026-11-02 — Original für Longpath von Martin Fischer (OE5SOS),
//                 KI-gestützt über Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QWheelEvent>

#include "core/FensterRandScroll.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;
using namespace Longpath::FensterRandScroll;

namespace {

constexpr double kDdcHz    = 7150000.0;   // wo die DDC steht
constexpr double kVfoHz    = 7180000.0;   // 30 kHz darüber
constexpr double kBreiteHz = 192000.0;

} // namespace

class TstZoomInCtunIstKeinZug : public QObject
{
    Q_OBJECT

private slots:
    void dasUrteilKenntDenZoom();
    void dasRadMeldetDieEigeneBewegung();
    void einEchterZugBleibtEinZug();

private:
    // Eine Fläche in CTUN, DDC auf kDdcHz, VFO 30 kHz darüber.
    static void richteEin(SpectrumWidget& sw)
    {
        sw.resize(800, 400);
        sw.setSampleRate(kBreiteHz);
        sw.setDdcCenterFrequency(kDdcHz);
        sw.setFrequencyRange(kDdcHz, kBreiteHz);
        sw.setCtunEnabled(true);
        sw.setVfoFrequency(kVfoHz);
    }
};

// ── Das Urteil kennt den Zoom ───────────────────────────────────────
//
// `mitteIstAbstimmbefehl` beantwortet die Frage des herkömmlichen
// Betriebs: darf die Mitte als FREQUENZ zurückgeschrieben werden? In
// CTUN ist die Antwort immer nein, weil die Mitte dort nicht die
// Frequenz ist.
//
// Die Frage in CTUN ist eine andere: darf die Mitte die DDC
// verstellen? Darauf antwortet `mitteIstDdcBefehl` — und zwar nur
// dann ja, wenn die Bewegung wirklich vom Bedienenden kam.
void TstZoomInCtunIstKeinZug::dasUrteilKenntDenZoom()
{
    // Ein Zug: ja.
    QVERIFY(mitteIstDdcBefehl(/*folgtDemVfo=*/false));
    // Das Fenster stellt sich selbst auf den VFO: nein.
    QVERIFY(!mitteIstDdcBefehl(/*folgtDemVfo=*/true));

    // Die alte Frage bleibt, wie sie war -- in CTUN ist eine Mitte nie
    // ein Abstimmbefehl, egal woher sie kommt.
    QVERIFY(!mitteIstAbstimmbefehl(/*ctun=*/true,  /*folgtDemVfo=*/false));
    QVERIFY(!mitteIstAbstimmbefehl(/*ctun=*/true,  /*folgtDemVfo=*/true));
    QVERIFY(mitteIstAbstimmbefehl(/*ctun=*/false, /*folgtDemVfo=*/false));
    QVERIFY(!mitteIstAbstimmbefehl(/*ctun=*/false, /*folgtDemVfo=*/true));
}

// ── Das Rad meldet die eigene Bewegung ──────────────────────────────
//
// Der gemessene Fall. Strg+Rad in CTUN: das Fenster zoomt und stellt
// sich dabei auf den VFO. Es meldet eine neue Mitte -- und muss dazu
// sagen, dass es die selbst gesetzt hat.
//
// Vor der Behebung stand die Fahne auf false, und MainWindow verstellte
// daraufhin die DDC um 30 kHz auf den VFO.
void TstZoomInCtunIstKeinZug::dasRadMeldetDieEigeneBewegung()
{
    SpectrumWidget sw;
    richteEin(sw);

    bool folgte = false;
    int meldungen = 0;
    connect(&sw, &SpectrumWidget::centerChanged, &sw,
            [&](double) { ++meldungen; folgte = sw.mitteFolgtDemVfo(); });

    // Strg+Rad: hineinzoomen. Die Fläche stellt sich dabei auf den VFO
    // (SpectrumWidget.cpp, "Recenter on VFO when zooming").
    QWheelEvent rad(QPointF(400, 200), sw.mapToGlobal(QPoint(400, 200)),
                    QPoint(0, 0), QPoint(0, 120), Qt::NoButton,
                    Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&sw, &rad);

    QVERIFY2(meldungen > 0, "das Rad hat gar keine neue Mitte gemeldet -- "
                            "dann prueft dieser Lauf nichts");
    QVERIFY2(folgte,
             "die Flaeche hat sich selbst auf den VFO gestellt und das NICHT "
             "gemeldet. MainWindow haelt das fuer einen Zug und verstellt in "
             "CTUN die DDC auf den VFO -- samt Zuteiler (PR #375).");

    // Und die Mitte sitzt wirklich auf dem VFO, nicht mehr auf der DDC:
    // das ist die Bewegung, um die es geht.
    QVERIFY(qAbs(sw.centerFrequency() - kVfoHz) < 1.0);

    // Die Fahne ist ein Zustand FUER DIE DAUER DER MELDUNG, kein
    // Dauerzustand -- danach steht sie wieder auf "Zug".
    QVERIFY2(!sw.mitteFolgtDemVfo(),
             "die Fahne blieb stehen; der naechste echte Zug wuerde "
             "faelschlich als eigene Bewegung durchgehen");
}

// ── Ein echter Zug bleibt ein Zug ───────────────────────────────────
//
// Die Gegenprobe zur Gegenprobe: die Behebung darf den Fall nicht
// miterschlagen, fuer den der ganze Zweig da ist. Zieht der Bedienende
// das Panadapter, MUSS die DDC folgen -- sonst sieht er ein Band, das
// das Geraet nicht empfaengt.
void TstZoomInCtunIstKeinZug::einEchterZugBleibtEinZug()
{
    SpectrumWidget sw;
    richteEin(sw);

    // Der Zug am Panadapter laeuft ueber mouseMoveEvent und meldet die
    // Mitte OHNE die Fahne zu setzen -- sie steht dann auf ihrem
    // Ruhewert. Genau das ist hier die Aussage: ausserhalb einer
    // eigenen Bewegung ist die Fahne false, also bleibt ein Zug ein
    // Zug und die DDC folgt ihm.
    QVERIFY2(!sw.mitteFolgtDemVfo(),
             "die Fahne steht im Ruhezustand auf 'eigene Bewegung' -- dann "
             "wuerde jeder Zug des Bedienenden verschluckt");
    QVERIFY(mitteIstDdcBefehl(sw.mitteFolgtDemVfo()));

    // Und sie bleibt es ueber einen VFO-Lauf hinweg, der in CTUN gar
    // nicht nachscrollt (setVfoFrequency prueft !m_ctunEnabled).
    sw.setVfoFrequency(kVfoHz + 50000.0);
    QVERIFY(!sw.mitteFolgtDemVfo());
    QVERIFY(mitteIstDdcBefehl(sw.mitteFolgtDemVfo()));
}

QTEST_MAIN(TstZoomInCtunIstKeinZug)
#include "tst_zoom_in_ctun_ist_kein_zug.moc"
