// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_design_consistency.cpp  (Longpath)
// =================================================================
// Ist die Handschrift ueberall dieselbe?
//
// Der Betreiber, 2026-08-20: „ueberpruefe, ob sich die grafik und das
// design auf alles bezieht und ueberall gleich ist."
//
// Die Antwort war: fast. Zwei Ambertoene fuer dieselbe Anfassmarke
// (#d8a13a in meinen neuen Leisten, Style::kAmberText #c2924f in den
// Containern), eine flache Kopfleiste neben zwei plastischen, und ein
// Zifferblatt in Neon-Cyan neben lauter bernsteinfarbenen
// Instrumenten.
//
// Diese Pruefung liest den QUELLTEXT. Das ist ungewoehnlich fuer einen
// Test und hier richtig: es geht nicht um Verhalten zur Laufzeit,
// sondern darum, dass niemand eine zweite Zahl fuer eine Farbe
// einfuehrt, die die Palette schon fuehrt. Genau so ist der zweite
// Amberton entstanden — nicht durch eine falsche Entscheidung,
// sondern durch eine, die an der Palette vorbeiging.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QFile>
#include <QRegularExpression>

namespace {

QString sourceRoot()
{
    // Vom Testbinaer aus zurueck zum Quellbaum. LONGPATH_SOURCE_DIR
    // setzt CMake; ohne die Angabe wird die Pruefung uebersprungen,
    // statt falsch gruen zu sein.
    return QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR"));
}

QString readFile(const QString& rel)
{
    QFile f(sourceRoot() + QLatin1Char('/') + rel);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
    return QString::fromUtf8(f.readAll());
}

// Ohne Kommentarzeilen.
//
// Ein Kommentar, der erklaert, WARUM eine Farbe nicht mehr benutzt
// wird, nennt sie zwangslaeufig — und wuerde die Pruefung sonst
// ausloesen. Der Fehler soll im Code stecken, nicht in seiner
// Begruendung.
QString codeOnly(const QString& src)
{
    QStringList out;
    const QStringList lines = src.split(QLatin1Char('\n'));
    for (const QString& l : lines) {
        const QString t = l.trimmed();
        if (t.startsWith(QLatin1String("//"))) { continue; }
        out << l;
    }
    return out.join(QLatin1Char('\n'));
}

} // namespace

class TestDesignConsistency : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (sourceRoot().isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }
    }

    // Alle Kopfleisten tragen dieselbe Anfassmarke — aus der Palette,
    // nicht als eigene Zahl.
    void everyGrabMarkUsesTheSameAmber()
    {
        const QStringList files = {
            QStringLiteral("src/gui/WindowChrome.cpp"),
            QStringLiteral("src/gui/applets/GridCellWidget.cpp"),
            QStringLiteral("src/gui/containers/ContainerWidget.cpp"),
        };
        for (const QString& rel : files) {
            const QString src = codeOnly(readFile(rel));
            QVERIFY2(!src.isEmpty(), qPrintable(rel + QStringLiteral(" fehlt")));
            QVERIFY2(src.contains(QStringLiteral("kAmberText")),
                     qPrintable(rel + QStringLiteral(
                         ": die Anfassmarke muss aus der Palette kommen "
                         "(Style::kAmberText)")));
            QVERIFY2(!src.contains(QStringLiteral("#d8a13a")),
                     qPrintable(rel + QStringLiteral(
                         ": #d8a13a ist ein zweiter Amberton fuer dieselbe "
                         "Marke — die Palette fuehrt sie schon")));
        }
    }

    // Alle Kopfleisten benutzen denselben Verlauf.
    void everyTitleBarUsesTheSharedGradient()
    {
        const QStringList files = {
            QStringLiteral("src/gui/WindowChrome.cpp"),
            QStringLiteral("src/gui/applets/GridCellWidget.cpp"),
            QStringLiteral("src/gui/containers/ContainerWidget.cpp"),
        };
        for (const QString& rel : files) {
            const QString src = codeOnly(readFile(rel));
            QVERIFY2(src.contains(QStringLiteral("titleBarStyle()")),
                     qPrintable(rel + QStringLiteral(
                         ": die Kopfleiste muss Style::titleBarStyle() "
                         "benutzen — sonst steht eine flache Leiste neben "
                         "zwei plastischen")));
        }
    }

    // Kein Zifferblatt malt in einer Farbfamilie, die es sonst
    // nirgends gibt.
    void noDialPaintsInNeon()
    {
        const QStringList files = {
            QStringLiteral("src/gui/widgets/DiversityRadarWidget.cpp"),
            QStringLiteral("src/gui/widgets/RotorDialWidget.cpp"),
        };
        // Neon-Cyan und reines Weiss: beides kommt in der Palette nicht
        // vor und faellt neben den bernsteinfarbenen Instrumenten auf.
        const QRegularExpression neon(
            QStringLiteral("QColor *\\( *(0 *, *(2[0-9][0-9]|1[5-9][0-9])"
                           "|255 *, *255 *, *255)"));
        for (const QString& rel : files) {
            const QString src = codeOnly(readFile(rel));
            QVERIFY2(!src.isEmpty(), qPrintable(rel + QStringLiteral(" fehlt")));
            const auto m = neon.match(src);
            QVERIFY2(!m.hasMatch(),
                     qPrintable(rel + QStringLiteral(
                         ": malt in einer Farbfamilie ausserhalb der "
                         "Palette (%1)").arg(m.captured(0))));
        }
    }
};

QTEST_MAIN(TestDesignConsistency)
#include "tst_design_consistency.moc"
