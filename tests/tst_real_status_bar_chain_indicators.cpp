// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Kettenanzeige (CH 0 / CH 1) in der Fussleiste, am ECHTEN
// MainWindow und in der Fensterbreite des Betreibers (1470 Punkte).
//
// Anlass, 2026-09-17: auf seinem Foto stand links unten "CH 1 /
// 20m (idle)" — und KEIN "CH 0", obwohl beide Ketten auf derselben
// Faltsprosse (4) liegen und ChromeBarController sie nur gemeinsam
// zeigen oder verbergen kann. Ohne Funkgeraet war es nicht
// nachzustellen (dann faltet die Leiste bei 1470 beide weg). Hier
// wird das Geraet nachgestellt: ein OrionMkII (ANVELINA PRO 3) hat
// zwei Filterketten, also wurde CH 1 verfuegbar.
//
// Befund: kein Faltfehler. CH 0 war seit dem Betreiber-Wunsch "bitte
// weg" bewusst nicht verfuegbar (Pille im Panadapter-Kopf ersetzt es),
// CH 1 hing aber weiter am Zwei-Ketten-Gatter und kam allein zurueck.
// Seit der Behebung folgt CH 1 der Entscheidung fuer CH 0.

#include <QtTest>
#include <QLabel>
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "core/BoardCapabilities.h"
#include "gui/chrome/ChromeBarController.h"

using namespace Longpath;

namespace {
QWidget* chainWidget(MainWindow* mw, int adc)
{
    auto* body = mw->findChild<QLabel*>(QStringLiteral("chainIndicator%1").arg(adc));
    return body ? body->parentWidget() : nullptr;
}
QString chainTitle(QWidget* w)
{
    if (!w) { return QString(); }
    const auto labels = w->findChildren<QLabel*>();
    return labels.isEmpty() ? QString() : labels.first()->text();
}
}

class TestRealStatusBarChainIndicators : public QObject
{
    Q_OBJECT
private slots:
    void bothChainsShowOrHideTogether_data()
    {
        QTest::addColumn<int>("width");
        QTest::newRow("Notebook 1470") << 1470;
        QTest::newRow("breit 1900")    << 1900;
    }

    void bothChainsShowOrHideTogether()
    {
        QFETCH(int, width);
        auto* mw = new MainWindow();      // bewusst nicht abgeraeumt
        mw->resize(width, 859);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        QTest::qWait(400);

        QWidget* c0 = chainWidget(mw, 0);
        QWidget* c1 = chainWidget(mw, 1);
        QVERIFY2(c0 && c1, "beide Kettenanzeigen muessen gebaut sein");
        QCOMPARE(chainTitle(c0), QStringLiteral("CH 0"));
        QCOMPARE(chainTitle(c1), QStringLiteral("CH 1"));

        // Ohne Geraet: CH 1 ist nicht verfuegbar, also nie sichtbar.
        qInfo() << "ohne Geraet, Breite" << width
                << "CH0 sichtbar" << c0->isVisible()
                << "CH1 sichtbar" << c1->isVisible();
        QVERIFY2(!c1->isVisible(), "ohne Geraet darf CH 1 nicht stehen");

        // Ein OrionMkII (ANVELINA PRO 3): zwei Filterketten.
        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm);
        rm->setCapsHwForTest(HPSDRHW::OrionMKII);
        QCOMPARE(rm->boardCapabilities().rxFilterChainCount, 2);
        rm->emitCurrentRadioChangedForTest();
        QTest::qWait(300);

        if (auto* bar = mw->findChild<ChromeBarController*>()) {
            qInfo() << "gefaltet bis Sprosse" << bar->foldedThroughRung()
                    << "gefaltet:" << bar->foldedLabels();
        }
        qInfo() << "OrionMkII, Breite" << width
                << "CH0 sichtbar" << c0->isVisible() << c0->geometry()
                << "CH1 sichtbar" << c1->isVisible() << c1->geometry();
        // Befund beim ersten Lauf (vor der Behebung): CH 0 blieb — wie
        // vom Betreiber gewuenscht — unsichtbar, CH 1 kam mit dem
        // Zwei-Ketten-Geraet zurueck (bei 1470 UND bei 1900). Genau das
        // Foto. Seitdem folgt CH 1 der Entscheidung fuer CH 0
        // (MainWindow: kChainIndicatorsInBottomBar).
        QVERIFY2(!c0->isVisible(),
                 "CH 0 ist aus der Fussleiste genommen (Betreiber: 'bitte "
                 "weg'), die Pille im Panadapter-Kopf zeigt die Kette");
        QVERIFY2(!c1->isVisible(),
                 "CH 1 darf nicht allein zurueckkommen, wenn ein Geraet mit "
                 "zwei Filterketten verbunden wird — auf dem Foto vom "
                 "2026-09-17 stand links unten 'CH 1 / 20m (idle)'");
    }
};

QTEST_MAIN(TestRealStatusBarChainIndicators)
#include "tst_real_status_bar_chain_indicators.moc"
