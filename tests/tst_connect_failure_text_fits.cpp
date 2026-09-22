// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_connect_failure_text_fits.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Betreiber am 2026-09-22 mit Bild: der Watchdog-Text („Das Geraet
// wurde gefunden, liefert aber binnen 6 s keinen Datenstrom. Das ist
// fast immer die Netzwerkstrecke …", vier Zeilen) stand oben und unten
// abgeschnitten in der 40-px-Leiste des Verbindungsfensters — und
// nochmal abgeschnitten im Hinweis rechts unten. Jetzt: erster Satz in
// die Leiste, der Rest umgebrochen darunter; der Hinweis misst seine
// Hoehe fuer seine Breite.

#include <QtTest>

#include "core/RadioConnection.h"
#include "gui/ConnectionPanel.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"

#include <QLabel>
#include <QLayout>

using namespace Longpath;

namespace {
const QString kDetail = QStringLiteral(
    "Das Gerät wurde gefunden, liefert aber binnen 6 s keinen Datenstrom.\n\n"
    "Das ist fast immer die Netzwerkstrecke, nicht das Gerät: über WLAN kommen "
    "die Pakete oft nicht durch, auch wenn die Suche es anzeigt (die läuft per "
    "Rundruf). Am zuverlässigsten ist eine Kabelverbindung.");
}

class TstConnectFailureTextFits : public QObject { Q_OBJECT
private slots:
    void theStripKeepsOneLineAndTheRestGoesBelow()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        panel.resize(1000, 700);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        QLabel* strip  = panel.stripInfoLabelForTest();
        QLabel* detail = panel.failureDetailLabelForTest();
        QVERIFY(strip && detail);
        QVERIFY(!detail->isVisibleTo(&panel));

        emit model.connectAttemptFailed(ConnectFailure::Timeout, kDetail);
        QCoreApplication::processEvents();

        QCOMPARE(strip->text(),
                 QStringLiteral("Disconnected — Das Gerät wurde gefunden, liefert aber binnen 6 s keinen Datenstrom."));
        QVERIFY(!strip->text().contains(QLatin1Char('\n')));
        QVERIFY2(strip->sizeHint().height() <= 40, qPrintable(QString::number(strip->sizeHint().height())));
        QCOMPARE(strip->toolTip(), kDetail);

        QVERIFY(detail->isVisibleTo(&panel));
        QVERIFY(detail->text().startsWith(QLatin1String("Das ist fast immer")));
        QVERIFY(detail->wordWrap());
        // Umgebrochen braucht der Rest mehr als eine Zeile — und bekommt sie.
        QVERIFY2(detail->heightForWidth(600) > detail->fontMetrics().height() * 2,
                 qPrintable(QString::number(detail->heightForWidth(600))));
    }

    void theToastIsAsTallAsItsText()
    {
        StatusToast toast(kDetail, ToastSeverity::Warning, 5000);
        toast.show();
        QVERIFY(QTest::qWaitForWindowExposed(&toast));
        QLabel* lbl = toast.findChild<QLabel*>();
        QVERIFY(lbl);
        const int need = toast.layout()->totalHeightForWidth(toast.width());
        QVERIFY2(toast.height() >= need,
                 qPrintable(QStringLiteral("%1 px hoch, braucht %2").arg(toast.height()).arg(need)));
        // Vier Zeilen Text: deutlich mehr als ein Einzeiler.
        QVERIFY2(toast.height() > 3 * lbl->fontMetrics().height(),
                 qPrintable(QString::number(toast.height())));
        // Und die letzte Zeile liegt innerhalb des Hinweises.
        QVERIFY(lbl->geometry().bottom() <= toast.height());
        QVERIFY(lbl->height() >= lbl->heightForWidth(lbl->width()));
    }
};

QTEST_MAIN(TstConnectFailureTextFits)
#include "tst_connect_failure_text_fits.moc"
