// =================================================================
// tests/tst_hausstil_farben.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. docs/design/HAUSSTIL.md nennt es selbst „das
// groessere Hindernis": Farben, die als nackte Hexwerte im Quelltext
// stehen statt als Rolle. Eine Palette sagt „wo der Quelltext Akzent
// malt, male Messing" — eine Farbe ohne Rolle kann sie nicht
// ansprechen. Genau deshalb blieben am 2026-10-14 bei der hellen
// Palette Panadapter, Kommandoleiste und Fussleiste schwarz, waehrend
// die rechte Spalte hell wurde.
//
// Am 2026-10-14 gezaehlt: 216 verschiedene Farben im Quelltext, 88
// davon in StyleConstants.h benannt, 128 ohne Namen. Die fuenf
// haeufigsten ohne Namen stehen in 26 bis 47 Dateien: #c8d8e8,
// #205070, #1a2a3a, #203040, #0f0f1a — die grossen Flaechen also.
//
// Dieser Prueflauf raeumt nichts auf. Er ist eine SPERRKLINKE: die
// Zahl darf sinken, nie steigen. Ohne ihn waechst der Rueckstand
// schneller, als er abgetragen wird — jede neue Datei bringt ihre
// eigenen Hexwerte mit, und niemand merkt es.
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
#include <QSet>

namespace {

// Stand 2026-10-14. Diese Zahl darf SINKEN. Wer sie erhoeht, hat eine
// neue namenlose Farbe eingefuehrt und soll stattdessen eine Rolle in
// StyleConstants.h anlegen.
constexpr int kNamenloseFarbenHoechstens = 128;

QSet<QString> hexIn(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("#[0-9a-fA-F]{6}\\b"));
    QSet<QString> aus;
    auto it = re.globalMatch(text);
    while (it.hasNext()) { aus.insert(it.next().captured(0).toLower()); }
    return aus;
}

// Benannt ist eine Farbe nur, wenn sie in StyleConstants.h einem Namen
// ZUGEWIESEN ist — also in Anfuehrungszeichen steht. Ein Hexwert in
// einem Kommentar erklaert etwas, er benennt nichts; wer ihn mitzaehlt,
// baut eine Sperrklinke, die nicht greift (beim ersten Entwurf dieses
// Prueflaufs genau so passiert: die Gegenprobe mit drei neuen Farben
// fiel nicht).
QSet<QString> zugewieseneHexIn(const QString& text)
{
    static const QRegularExpression re(
        QStringLiteral("\"(#[0-9a-fA-F]{6})\""));
    QSet<QString> aus;
    auto it = re.globalMatch(text);
    while (it.hasNext()) { aus.insert(it.next().captured(1).toLower()); }
    return aus;
}

} // namespace

class TstHausstilFarben : public QObject {
    Q_OBJECT

private slots:
    void namenloseFarbenWerdenNichtMehr()
    {
        const QString wurzel = QStringLiteral(LONGPATH_SRC_DIR) + QStringLiteral("/src");
        QVERIFY2(QDir(wurzel).exists(), qPrintable(wurzel));

        QFile tokenDatei(wurzel + QStringLiteral("/gui/StyleConstants.h"));
        QVERIFY(tokenDatei.open(QIODevice::ReadOnly | QIODevice::Text));
        const QSet<QString> benannt =
            zugewieseneHexIn(QString::fromUtf8(tokenDatei.readAll()));
        QVERIFY2(benannt.size() > 50,
                 "StyleConstants.h liefert kaum Farben — stimmt der Pfad?");

        QSet<QString> alle;
        int dateien = 0;
        QDirIterator it(wurzel, {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            ++dateien;
            alle.unite(hexIn(QString::fromUtf8(f.readAll())));
        }
        QVERIFY2(dateien > 100, "zu wenige Dateien gelesen");

        const QSet<QString> ohneNamen = alle - benannt;
        QVERIFY2(ohneNamen.size() <= kNamenloseFarbenHoechstens,
                 qPrintable(QStringLiteral(
                     "Namenlose Farben: %1, erlaubt sind hoechstens %2. "
                     "Eine neue Farbe gehoert als Rolle nach "
                     "StyleConstants.h — sonst kann keine Palette sie "
                     "ansprechen (HAUSSTIL.md).")
                                .arg(ohneNamen.size())
                                .arg(kNamenloseFarbenHoechstens)));
    }
};

QTEST_MAIN(TstHausstilFarben)
#include "tst_hausstil_farben.moc"
