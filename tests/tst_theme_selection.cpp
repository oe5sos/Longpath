// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_theme_selection.cpp  (Longpath)
// =================================================================
// Paletten auswaehlen, und die Wahl ueberlebt den Neustart.
//
// Der Betreiber, 2026-08-20: zwei helle Paletten „welche ich bei viel
// licht nutzen kann", und man solle „bei themes wechseln" koennen.
//
// Bis dahin nahm Theme::loadUserTheme() die ERSTE JSON-Datei in
// alphabetischer Reihenfolge. Mit einer Datei war das eine Wahl, mit
// dreien ist es Zufall: „tageslicht.json" gewaenne gegen
// „werkbank.json", weil t vor w kommt.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"
#include "core/AppSettings.h"

using namespace Longpath;

namespace {

void writeTheme(const QDir& d, const QString& file, const QString& name,
                const QString& measured)
{
    QFile f(d.filePath(file));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(QStringLiteral(
        "{\"name\": \"%1\", \"colors\": {\"measured\": \"%2\"}}")
            .arg(name, measured).toUtf8());
}

} // namespace

class TestThemeSelection : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // ── Nicht in die echten Einstellungen schreiben ──────────────
        //
        // activate() merkt die Wahl unter „ActiveTheme" in
        // AppSettings::instance() — und das ist ohne Profil die ECHTE
        // Datei des Betreibers. Am 2026-08-19 hat genau so ein Griff
        // seine Sichtbarkeitsschluessel ueberschrieben und sein
        // Programm leer starten lassen.
        //
        // setProfileOverride VOR dem ersten instance()-Aufruf: der Pfad
        // wird im Baukasten aufgeloest, danach hilft es nicht mehr.
        AppSettings::setProfileOverride(
            QStringLiteral("test-theme-selection"));

        QVERIFY(m_dir.isValid());
        QDir d(m_dir.path());
        writeTheme(d, QStringLiteral("aaa-hell.json"),
                   QStringLiteral("Tageslicht"), QStringLiteral("#8a5a12"));
        writeTheme(d, QStringLiteral("zzz-warm.json"),
                   QStringLiteral("Werkbank"), QStringLiteral("#7a4c06"));
        qputenv("NEREUS_THEME_DIR", m_dir.path().toLocal8Bit());
    }

    void cleanupTestCase()
    {
        qunsetenv("NEREUS_THEME_DIR");
        Style::Theme::instance().clear();
    }

    void bothThemesAreListedByName()
    {
        const QVector<Style::Theme::Entry> all = Style::Theme::available();
        QStringList names;
        for (const auto& e : all) { names << e.name; }
        QVERIFY2(names.contains(QStringLiteral("Tageslicht")),
                 qPrintable(QStringLiteral("gefunden: %1").arg(names.join(", "))));
        QVERIFY2(names.contains(QStringLiteral("Werkbank")),
                 qPrintable(QStringLiteral("gefunden: %1").arg(names.join(", "))));
    }

    // DIE Eigenschaft: die zweite Datei ist erreichbar, obwohl die
    // erste alphabetisch gewinnt.
    void theSecondFileCanBeChosen()
    {
        QString err;
        QVERIFY2(Style::Theme::instance().activate(
                     QStringLiteral("Werkbank"), &err), qPrintable(err));
        QCOMPARE(Style::Theme::instance().name(), QStringLiteral("Werkbank"));
        QCOMPARE(Style::role("measured", "#000000"), QStringLiteral("#7a4c06"));
    }

    // Und die Wahl ueberlebt: applyStoredChoice muss sie wiederfinden.
    void theChoiceSurvivesARestart()
    {
        QVERIFY(Style::Theme::instance().activate(QStringLiteral("Tageslicht")));
        Style::Theme::instance().clear();          // „Neustart"
        QVERIFY(!Style::Theme::instance().isActive());

        QVERIFY(Style::Theme::instance().applyStoredChoice());
        QCOMPARE(Style::Theme::instance().name(), QStringLiteral("Tageslicht"));
    }

    // Eingebaut ist eine Wahl wie jede andere — und bleibt eine.
    void builtInIsAChoiceThatSticks()
    {
        QVERIFY(Style::Theme::instance().activate(Style::Theme::builtInName()));
        QVERIFY(!Style::Theme::instance().isActive());

        QVERIFY(Style::Theme::instance().activate(QStringLiteral("Werkbank")));
        QVERIFY(Style::Theme::instance().isActive());

        QVERIFY(Style::Theme::instance().activate(Style::Theme::builtInName()));
        QVERIFY(Style::Theme::instance().applyStoredChoice());
        QVERIFY2(!Style::Theme::instance().isActive(),
                 "eine bewusst gewaehlte eingebaute Palette darf nach dem "
                 "Neustart nicht wieder von loadUserTheme ueberschrieben "
                 "werden — sonst kaeme die Datei zurueck, die man gerade "
                 "abgewaehlt hat");
    }

    // Eine kaputte Datei darf das Programm nicht farblos machen.
    void abrokenFileLeavesTheRunningPaletteAlone()
    {
        QVERIFY(Style::Theme::instance().activate(QStringLiteral("Werkbank")));

        QDir d(m_dir.path());
        QFile f(d.filePath(QStringLiteral("kaputt.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ das ist kein JSON ");
        f.close();

        QString err;
        const bool ok = Style::Theme::instance().activate(
            QStringLiteral("kaputt"), &err);
        QVERIFY2(!ok, "eine kaputte Datei darf nicht als Erfolg gelten");
        QCOMPARE(Style::Theme::instance().name(), QStringLiteral("Werkbank"));

        f.remove();
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(TestThemeSelection)
#include "tst_theme_selection.moc"
