// tests/tst_chart_trace_roles.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Der Notbehelf darf nie zum Einsatz kommen ────────────────────────
//
// Die Spurfarben des SWR-Diagramms kommen aus Theme-Rollen. Im Quelltext
// steht nur eine Rueckfallreihe fuer den Fall, dass GAR KEINE
// Theme-Datei da ist.
//
// Eine frische Installation bekommt aber die Vorlage
// docs/design/oe5sos.example.json ausgeliefert -- und die muss die
// Rollen kennen, sonst faellt der Betreiber auf den Notbehelf, ohne dass
// es jemand merkt. Genau das prueft dieser Test.
//
// OE5SOS, 2026-08-16: "Wenn es dafuer einen Test gibt, der prueft, dass
// jede chart-trace-Rolle in der Beispieldatei vorkommt, ist der Fall
// dauerhaft erledigt."
//
// Die Rollennamen werden aus SwrChartWidget.cpp GELESEN, nicht hier
// abgeschrieben. Sonst haetten sie zwei Besitzer, und der Test wuerde
// gruen bleiben, waehrend eine siebte Spur dazukommt.

#include <QtTest/QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

QString repoRoot()
{
    // Vom Testprogramm aus nach oben, bis docs/design auftaucht.
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFile::exists(d.filePath(QStringLiteral("docs/design/oe5sos.example.json")))) {
            return d.absolutePath();
        }
        d.cdUp();
    }
    return QString();
}

} // namespace

class TestChartTraceRoles : public QObject
{
    Q_OBJECT

private slots:

    void everyChartTraceRoleIsInTheTemplate()
    {
        const QString root = repoRoot();
        QVERIFY2(!root.isEmpty(), "Repo-Wurzel nicht gefunden");

        // 1. Die Rollennamen aus der Quelle lesen.
        QFile src(root + QStringLiteral("/src/gui/widgets/SwrChartWidget.cpp"));
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(src.readAll());

        QStringList roles;
        QRegularExpression re(QStringLiteral("\"(chart-trace-[0-9]+)\""));
        auto it = re.globalMatch(code);
        while (it.hasNext()) {
            const QString r = it.next().captured(1);
            if (!roles.contains(r)) { roles << r; }
        }
        QVERIFY2(!roles.isEmpty(),
                 "keine chart-trace-Rolle in SwrChartWidget.cpp gefunden -- "
                 "entweder umbenannt oder der Test liest die falsche Datei");

        // 2. Gegen die Vorlage halten.
        QFile tpl(root + QStringLiteral("/docs/design/oe5sos.example.json"));
        QVERIFY(tpl.open(QIODevice::ReadOnly));
        const QJsonObject colors =
            QJsonDocument::fromJson(tpl.readAll()).object()
                .value(QStringLiteral("colors")).toObject();

        for (const QString& r : roles) {
            QVERIFY2(colors.contains(r),
                     qPrintable(QStringLiteral(
                         "Rolle %1 fehlt in oe5sos.example.json -- eine "
                         "frische Installation fiele auf den Notbehelf")
                             .arg(r)));
            QVERIFY2(!colors.value(r).toString().isEmpty(),
                     qPrintable(QStringLiteral("Rolle %1 ist leer").arg(r)));
        }
    }

    void theSeriesKeepsItsTonesApart()
    {
        // Gedeckt duerfen sie sein, gleich nicht: mehrere Sweeps liegen
        // uebereinander und muessen unterscheidbar bleiben. Zwei Spuren
        // mit demselben Wert waeren ein huebsches, unlesbares Diagramm.
        const QString root = repoRoot();
        QFile tpl(root + QStringLiteral("/docs/design/oe5sos.example.json"));
        QVERIFY(tpl.open(QIODevice::ReadOnly));
        const QJsonObject colors =
            QJsonDocument::fromJson(tpl.readAll()).object()
                .value(QStringLiteral("colors")).toObject();

        QStringList seen;
        for (const QString& k : colors.keys()) {
            if (!k.startsWith(QStringLiteral("chart-trace-"))) { continue; }
            const QString v = colors.value(k).toString().toLower();
            QVERIFY2(!seen.contains(v),
                     qPrintable(QStringLiteral(
                         "zwei Spurfarben tragen denselben Wert %1").arg(v)));
            seen << v;
        }
        QVERIFY(seen.size() >= 2);
    }
};

QTEST_MAIN(TestChartTraceRoles)
#include "tst_chart_trace_roles.moc"
