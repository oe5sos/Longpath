// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_both_paint_paths.cpp  (Longpath)
// =================================================================
// Zwei Zeichenwege sind zwei Orte, an denen etwas fehlen kann.
//
// SpectrumWidget malt zweimal dasselbe Bild: einmal im QPainter-Weg
// (paintEvent, Rueckfallebene ohne GPU) und einmal beim Aufbau der
// Ueberlagerungsflaeche fuer die GPU. Wer den einen aendert und den
// anderen vergisst, baut etwas, das im Test funktioniert und beim
// Betreiber nicht — oder umgekehrt.
//
// Genau das ist am 2026-08-21 passiert: die Einblendungen (Kompass,
// Stehwelle) standen nur im QPainter-Weg. Die Bilder waren richtig und
// durchsichtig — sie wurden nur nie in die Flaeche gemalt, die im
// normalen Betrieb zu sehen ist. Der Betreiber: „einblenden kann ich
// noch immer nichts im pandapter."
//
// Diese Pruefung liest den QUELLTEXT und vergleicht, was beide Wege
// zeichnen. Das ist ungewoehnlich und hier richtig: der Fehler ist
// eine AUSLASSUNG, und eine Auslassung sieht man nur, wenn man die
// beiden Listen nebeneinander legt.
//
// Modification history (Longpath):
//   2026-08-21 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QFile>
#include <QRegularExpression>

namespace {

QString source()
{
    const QString root = QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR"));
    QFile f(root + QStringLiteral("/src/gui/SpectrumWidget.cpp"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
    return QString::fromUtf8(f.readAll());
}

int countCalls(const QString& src, const QString& fn)
{
    // Nur Aufrufe, keine Definition und keine Kommentarzeilen.
    const QRegularExpression re(
        QStringLiteral("^\\s*%1\\s*\\(").arg(QRegularExpression::escape(fn)),
        QRegularExpression::MultilineOption);
    int n = 0;
    auto it = re.globalMatch(src);
    while (it.hasNext()) { it.next(); ++n; }
    return n;
}

} // namespace

class TestBothPaintPaths : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR")).isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }
    }

    void everyChromeLayerIsPaintedInBothPaths()
    {
        const QString src = source();
        QVERIFY2(!src.isEmpty(), "SpectrumWidget.cpp muss lesbar sein");

        // Schichten, die BEIDE Wege zeichnen muessen. Wer hier eine
        // hinzufuegt, hat sie in beiden Wegen zu verdrahten — und
        // genau daran erinnert diese Pruefung.
        const QStringList layers = {
            QStringLiteral("paintBackgroundLayer"),
            QStringLiteral("drawGrid"),
            QStringLiteral("drawBandPlan"),
            QStringLiteral("drawCompassOverlay"),
        };
        for (const QString& fn : layers) {
            const int n = countCalls(src, fn);
            QVERIFY2(n >= 2,
                     qPrintable(QStringLiteral(
                         "%1 wird nur %2-mal gerufen. Beide Zeichenwege "
                         "brauchen sie: der QPainter-Weg (paintEvent) UND "
                         "der Aufbau der GPU-Ueberlagerung. Fehlt sie im "
                         "GPU-Weg, sieht der Betreiber nichts, waehrend "
                         "jeder Test gruen bleibt.").arg(fn).arg(n)));
        }
    }
};
QTEST_MAIN(TestBothPaintPaths)
#include "tst_both_paint_paths.moc"
