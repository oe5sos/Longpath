// =================================================================
// tests/tst_hausstil_radius.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. docs/design/HAUSSTIL.md, Abschnitt „Maße",
// sagt einen Satz ohne Hintertuer:
//
//   „Nie Radius 3. Das ist der Qt-Standardwert und laesst alles nach
//    Qt aussehen."
//
// Am 2026-10-14 standen trotzdem elf davon im Quelltext. Sieben waren
// echte Verstoesse (Beschriftungen, Knoepfe, Listen); vier sitzen in
// QSlider::groove, wo die Rille nur 4 bis 6 px hoch ist -- dort ist
// Radius 3 eine Pille, kein Qt-Look, und bleibt erlaubt.
//
// Eine Regel, die niemand nachhaelt, ist eine Bitte. Dieser Prueflauf
// macht sie nachhaltbar: er liest den Quelltext und faellt, sobald ein
// neuer Radius 3 ausserhalb einer Schieberrille dazukommt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

class TstHausstilRadius : public QObject {
    Q_OBJECT

private slots:
    void keinRadiusDreiAusserhalbVonSchieberrillen()
    {
        const QString wurzel = QStringLiteral(LONGPATH_SRC_DIR) + QStringLiteral("/src");
        QVERIFY2(QDir(wurzel).exists(), qPrintable(wurzel));

        static const QRegularExpression dreier(
            QStringLiteral("border-radius:\\s*3px"));

        QStringList verstoesse;
        int geprueft = 0;
        QDirIterator it(wurzel, {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            ++geprueft;
            const QStringList zeilen =
                QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
            for (int i = 0; i < zeilen.size(); ++i) {
                if (!dreier.match(zeilen.at(i)).hasMatch()) { continue; }
                // Die Ausnahme: eine Schieberrille. Der Selektor steht in
                // denselben oder den vier Zeilen darueber.
                bool rille = false;
                for (int j = qMax(0, i - 4); j <= i; ++j) {
                    if (zeilen.at(j).contains(QLatin1String("QSlider::groove"))) {
                        rille = true;
                        break;
                    }
                }
                if (!rille) {
                    verstoesse << QStringLiteral("%1:%2")
                                      .arg(QFileInfo(pfad).fileName())
                                      .arg(i + 1);
                }
            }
        }
        QVERIFY2(geprueft > 100, "zu wenige Dateien gelesen — stimmt der Pfad?");
        QVERIFY2(verstoesse.isEmpty(),
                 qPrintable(QStringLiteral(
                     "HAUSSTIL.md: „Nie Radius 3\". Gefunden an: %1")
                                .arg(verstoesse.join(QStringLiteral(", ")))));
    }
};

QTEST_MAIN(TstHausstilRadius)
#include "tst_hausstil_radius.moc"
