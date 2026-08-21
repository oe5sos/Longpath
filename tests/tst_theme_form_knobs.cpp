// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Themendatei kann jetzt FORM, nicht nur Farbe.
//
// Der Betreiber, 2026-08-21, zu Zeus: „das design hat teilweise mehr
// stil ... die uebergaenge im hintergrund bei den widget wirken sehr
// gut." Auf die Frage, wie das waehlbar werden soll, wählte er „b":
// die Themendatei erweitern statt nur weitere Farbschemata anbieten.
//
// Der Grund steckt in seiner Beobachtung: was ihm gefaellt, sind keine
// Farben. Es sind Verlaufstiefe, Polsterung, Eckenradius. Ein Thema,
// das nur Farben tauscht, kann das nicht ausdruecken.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"

using namespace Longpath;

class TstThemeFormKnobs : public QObject
{
    Q_OBJECT

private:
    static QString writeTheme(const QDir& dir, const QByteArray& json)
    {
        const QString path = dir.filePath(QStringLiteral("form.json"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) { return {}; }
        f.write(json);
        return path;
    }

private slots:
    void cleanup() { Style::Theme::instance().clear(); }

    /// Ohne Thema gelten die Vorgaben — jede Datei ohne „formen"
    /// bleibt gueltig.
    void withoutAThemeTheDefaultsApply()
    {
        Style::Theme::instance().clear();
        QCOMPARE(Style::formInt("radius", 7), 7);
        QCOMPARE(Style::formInt("mulde", 10), 10);

        const QString btn = Style::buttonBaseStyle();
        QVERIFY2(btn.contains(QStringLiteral("border-radius: 7px")),
                 qPrintable(QStringLiteral("Vorgabe-Radius fehlt: %1").arg(btn)));
        QVERIFY2(btn.contains(QStringLiteral("padding: 4px 10px")),
                 "Vorgabe-Polsterung fehlt");
    }

    /// Mit Thema gelten dessen Zahlen — und man sieht es am Stil.
    void aThemeCanChangeTheShape()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = writeTheme(QDir(tmp.path()), QByteArray(
            "{\n"
            "  \"name\": \"Pruefform\",\n"
            "  \"formen\": { \"radius\": 12, \"luft-v\": 8, \"luft-h\": 20,\n"
            "                \"relief\": 30, \"mulde\": 24 }\n"
            "}\n"));
        QVERIFY(!path.isEmpty());

        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(path, &err), qPrintable(err));

        QCOMPARE(Style::formInt("radius", 7), 12);
        QCOMPARE(Style::formInt("relief", 16), 30);

        const QString btn = Style::buttonBaseStyle();
        QVERIFY2(btn.contains(QStringLiteral("border-radius: 12px")),
                 qPrintable(QStringLiteral("Radius folgt dem Thema nicht: %1")
                                .arg(btn)));
        QVERIFY2(btn.contains(QStringLiteral("padding: 8px 20px")),
                 "Polsterung folgt dem Thema nicht");
    }

    /// Unsinnige Werte werden still uebergangen.
    ///
    /// Eine Themendatei ist Zubehoer. Ein Vertipper darf die
    /// Oberflaeche nicht entstellen — aber er darf auch nicht die ganze
    /// Palette durchfallen lassen, sonst sucht man den Fehler im
    /// Programm statt in der Datei.
    void nonsenseIsIgnoredQuietly()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = writeTheme(QDir(tmp.path()), QByteArray(
            "{\n"
            "  \"name\": \"Unfug\",\n"
            "  \"colors\": { \"button\": \"#334455\" },\n"
            "  \"formen\": { \"radius\": 900, \"mulde\": -5,\n"
            "                \"quatsch\": 3 }\n"
            "}\n"));

        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(path, &err),
                 qPrintable(QStringLiteral(
                     "Die Palette faellt wegen eines Formfehlers durch: %1")
                     .arg(err)));

        QCOMPARE(Style::formInt("radius", 7), 7);    // 900 verworfen
        QCOMPARE(Style::formInt("mulde", 10), 10);   // -5 verworfen

        // Die Farbe daneben gilt trotzdem.
        QCOMPARE(Style::Theme::instance().forRole(QStringLiteral("button")),
                 QStringLiteral("#334455"));
    }

    /// Eingabefelder sind versenkt und folgen dem Thema.
    ///
    /// Vorher stand dort „background: #08080a", fest eingetippt — ein
    /// Wert, den KEINE Themen-Tabelle kennt. Im hellen Thema blieben
    /// die Felder schwarz.
    void inputFieldsAreSunkenAndFollowTheTheme()
    {
        Style::Theme::instance().clear();
        const QString dark = Style::lineEditStyle();

        QVERIFY2(dark.contains(QStringLiteral("qlineargradient")),
                 qPrintable(QStringLiteral(
                     "Das Eingabefeld ist flach: %1").arg(dark)));
        QVERIFY2(!dark.contains(QStringLiteral("#08080a")),
                 "Der alte fest eingetippte Wert steht noch drin");

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = writeTheme(QDir(tmp.path()), QByteArray(
            "{\n  \"name\": \"Hell\",\n"
            "  \"colors\": { \"inset\": \"#eeeef2\" }\n}\n"));
        QString err;
        QVERIFY(Style::Theme::instance().loadFile(path, &err));

        QVERIFY2(Style::lineEditStyle() != dark,
                 "Das Eingabefeld folgt dem Thema nicht");
    }
};

QTEST_MAIN(TstThemeFormKnobs)
#include "tst_theme_form_knobs.moc"
