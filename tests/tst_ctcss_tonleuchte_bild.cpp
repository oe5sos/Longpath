// Macht vom FM-Flag ein Bild, in allen vier Tonstellungen.
//
// Kein Pruefstand im eigentlichen Sinn -- er schreibt PNGs, damit sich
// die Tonleuchte ANSEHEN laesst, statt sie nur zu behaupten. Martins
// laufende Instanz wird dafuer nicht angefasst: der Container wird
// einzeln gebaut und mit `grab()` abgezogen.
//
// Die Bilder landen in LONGPATH_BILD_DIR; ohne die Variable in einem
// temporaeren Ordner, damit der Lauf in der CI nichts ins
// Arbeitsverzeichnis streut.

#include <QDir>
#include <QLabel>
#include <QPixmap>
#include <QtTest>

#include "gui/StyleConstants.h"
#include "gui/widgets/VfoModeContainers.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstCtcssTonleuchteBild : public QObject
{
    Q_OBJECT
private slots:
    void bilderSchreiben();
};

void TstCtcssTonleuchteBild::bilderSchreiben()
{
    const QString dir = qEnvironmentVariable(
        "LONGPATH_BILD_DIR",
        QDir(QDir::tempPath()).filePath(QStringLiteral("longpath-tonleuchte")));
    QDir().mkpath(dir);

    struct Fall { int modus; bool tonDa; const char* name; };
    const Fall faelle[] = {
        {0, false, "1-off"},
        {1, false, "2-encode"},
        {2, false, "3-decode-kein-ton"},
        {2, true,  "4-decode-ton-liegt-an"},
        {3, true,  "5-encdec-ton-liegt-an"},
        {4, false, "6-dcs-kein-code"},
        {4, true,  "7-dcs-code-liegt-an"},
    };

    for (const Fall& f : faelle) {
        SliceModel s;
        s.setFmCtcssValueHz(123.0);
        s.setFmDcsCode(131);
        FmOptContainer c;
        c.setSlice(&s);
        s.setFmCtcssMode(f.modus);
        s.setFmCtcssToneDetected(f.tonDa);
        c.syncFromSlice();
        c.resize(c.sizeHint());
        c.show();
        QVERIFY(QTest::qWaitForWindowExposed(&c));
        c.syncFromSlice();

        const QPixmap bild = c.grab();
        const QString pfad = QStringLiteral("%1/tonleuchte-%2.png").arg(dir, QLatin1String(f.name));
        QVERIFY2(bild.save(pfad), qPrintable(QStringLiteral("konnte %1 nicht schreiben").arg(pfad)));
        qInfo() << "geschrieben:" << pfad << bild.size();
    }
}

QTEST_MAIN(TstCtcssTonleuchteBild)
#include "tst_ctcss_tonleuchte_bild.moc"
