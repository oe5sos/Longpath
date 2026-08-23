// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Kachelreihe ueber der Frequenz zeigt jeden Empfaenger, markiert
// den aktiven — und wechselt ihn auf Klick.
//
// Anlass, 2026-08-23: "weiters sollte die frequenz auch ein eigenes
// widget sein, wie hier am foto" (Bildschirmfoto Zeus Link, Kacheln
// ueber der Ziffernanzeige).
//
// Gemessen wird der TEXT der Kacheln und der Zustand des Modells
// danach — nicht, ob eine Methode zurueckkehrt.

#include <QtTest>
#include <QLabel>
#include <QPainter>

#include "gui/StyleConstants.h"
#include "gui/widgets/VfoTileRow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

QString allText(QWidget* w)
{
    QStringList out;
    for (QLabel* l : w->findChildren<QLabel*>()) {
        if (!l->text().isEmpty()) { out << l->text(); }
    }
    return out.join(QStringLiteral(" | "));
}

} // namespace

class TstVfoTiles : public QObject
{
    Q_OBJECT

private slots:
    void jedeScheibeBekommtEineKachel()
    {
        // Ein nacktes RadioModel hat keine Scheiben — die entstehen
        // erst beim Verbinden. Ohne dieses Anlegen prueft der Rest
        // dieser Pruefung nur die KiwiSDR-Kachel, und der erste Lauf
        // hat genau das gezeigt.
        RadioModel model;
        model.addSlice();
        model.addSlice();
        VfoTileRow row(&model);
        row.resize(600, 40);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));

        const QString txt = allText(&row);
        qInfo().noquote() << "Kacheln:" << txt;

        // Mindestens die erste Scheibe und die KiwiSDR-Kachel.
        QVERIFY2(txt.contains(QStringLiteral("KIWI")), qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("AUS")), qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("1")), qPrintable(txt));
    }

    void dieFrequenzStehtMitBandDabei()
    {
        RadioModel model;
        model.addSlice();
        VfoTileRow row(&model);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));

        const QList<SliceModel*> slices = model.slices();
        QVERIFY2(!slices.isEmpty(), "addSlice hat keine Scheibe angelegt");
        slices.first()->setFrequency(7'131'300.0);
        row.refresh();

        const QString txt = allText(&row);
        qInfo().noquote() << "nach dem Abstimmen:" << txt;
        QVERIFY2(txt.contains(QStringLiteral("7.131")), qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("40")), qPrintable(txt));
    }

    void dieKiwiKachelMeldetSich()
    {
        RadioModel model;
        VfoTileRow row(&model);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));

        QSignalSpy spy(&row, &VfoTileRow::kiwiToggleRequested);

        // Die Kiwi-Kachel ist die letzte vor dem Dehnfeld. Gesucht wird
        // ueber ihren TEXT, nicht ueber ihre Lage — eine Pruefung, die
        // auf der Reihenfolge sitzt, bricht beim naechsten Umbau.
        QWidget* kiwi = nullptr;
        for (QLabel* l : row.findChildren<QLabel*>()) {
            if (l->text() == QStringLiteral("KIWI")) {
                kiwi = l->parentWidget();
                break;
            }
        }
        QVERIFY2(kiwi, "Keine KiwiSDR-Kachel gefunden");
        QTest::mouseClick(kiwi, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        qInfo() << "KiwiSDR-Kachel hat gemeldet";

        row.setKiwiOn(true);
        QVERIFY2(allText(&row).contains(QStringLiteral("AN")),
                 qPrintable(allText(&row)));
    }

    void einBlattZumAnsehen()
    {
        // WERKZEUG: zeigt die Reihe, wie sie im Betrieb aussieht.
        RadioModel model;
        model.addSlice();
        model.addSlice();
        VfoTileRow row(&model);
        row.resize(520, 34);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        const QList<SliceModel*> slices = model.slices();
        if (!slices.isEmpty()) { slices.first()->setFrequency(7'131'300.0); }
        row.refresh();

        QImage img(row.size() * 3, QImage::Format_ARGB32);
        img.fill(QColor(Style::kAppBg));
        QImage one(row.size(), QImage::Format_ARGB32);
        one.fill(QColor(Style::kAppBg));
        row.render(&one);
        QPainter p(&img);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRect(QPoint(0, 0), row.size() * 3), one);
        p.end();

        const QString out = QStringLiteral("/tmp/vfo_kacheln.png");
        QVERIFY2(img.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out;
    }
};

QTEST_MAIN(TstVfoTiles)
#include "tst_vfo_tiles.moc"
