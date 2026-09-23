// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ein Hinweis, der unten abgeschnitten ist, ist schlimmer als keiner.
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Betreiber am 2026-09-23, mit Bild: der Hinweis "Das Geraet wurde
// gefunden, liefert aber binnen 6 s keinen Datenstrom ..." stand unten
// abgeschnitten -- der letzte Satz fehlte. Dazu: "das hatten wir schon
// einmal". Hatten wir: am 2026-09-22 ist in StatusToast eigens von
// adjustSize() auf layout->totalHeightForWidth() umgestellt worden,
// weil ein vierzeiliger Text abgeschnitten war.
//
// Dieser Pruefstand nagelt die Eigenschaft fest, statt eine
// Rechenweise: egal welcher Text, der sichtbare Kasten muss so hoch
// sein, dass die Beschriftung bei DIESER Breite hineinpasst. Faellt er
// durch, ist wieder etwas abgeschnitten -- unabhaengig davon, woran es
// diesmal liegt.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-23 — Created for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>

#include "gui/widgets/StatusToast.h"

using namespace Longpath;

namespace {

// Wortgleich der Text aus P1RadioConnection/P2RadioConnection, der am
// 2026-09-23 abgeschnitten dastand.
QString theMessageThatWasCutOff()
{
    return QStringLiteral(
        "Das Gerät wurde gefunden, liefert aber binnen 6 s keinen "
        "Datenstrom.\n\n"
        "Das ist fast immer die Netzwerkstrecke, nicht das Gerät: über "
        "WLAN kommen die Pakete oft nicht durch, auch wenn die Suche es "
        "anzeigt (die läuft per Rundruf). Am zuverlässigsten ist eine "
        "Kabelverbindung.");
}

void assertFits(const QString& message, const char* what)
{
    auto* toast = new StatusToast(message, ToastSeverity::Warning,
                                  60'000, nullptr);
    toast->show();
    QVERIFY(QTest::qWaitForWindowExposed(toast));

    auto* label = toast->findChild<QLabel*>();
    QVERIFY2(label, "der Kasten traegt keine Beschriftung");

    const int needed = label->heightForWidth(label->width());
    QVERIFY2(needed <= label->height(),
             qPrintable(QStringLiteral(
                 "%1: der Text braucht %2 px bei %3 px Breite, die "
                 "Beschriftung ist aber nur %4 px hoch -- unten "
                 "abgeschnitten. Kasten: %5x%6")
                 .arg(QString::fromLatin1(what))
                 .arg(needed).arg(label->width()).arg(label->height())
                 .arg(toast->width()).arg(toast->height())));
    toast->close();
}

} // namespace

class TstStatusToastFitsItsText : public QObject {
    Q_OBJECT

private slots:

    void theWatchdogNoticeIsNotCutOff()
    {
        assertFits(theMessageThatWasCutOff(), "Watchdog-Hinweis");
    }

    void aShortNoticeStillFits()
    {
        assertFits(QStringLiteral("TX > Slice B"), "kurzer Hinweis");
    }

    void anAbsurdlyLongNoticeStillFits()
    {
        // Kein echter Text, sondern die Gegenprobe: waechst der Kasten
        // mit, oder ist irgendwo eine Decke eingebaut?
        QString wall;
        for (int i = 0; i < 40; ++i) {
            wall += QStringLiteral("Zeile %1 mit genug Text, dass sie "
                                   "umbricht und Hoehe kostet. ").arg(i);
        }
        assertFits(wall, "sehr langer Hinweis");
    }
};

QTEST_MAIN(TstStatusToastFitsItsText)
#include "tst_status_toast_fits_its_text.moc"
