// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die drei Fassungen der Oberflaeche: Flach · Weich · Tief.
//
// Der Betreiber hat am 2026-08-22 aus einem Vergleichsblatt „weich"
// gewaehlt. Weich IST die eingebaute Vorgabe — es war also nichts
// umzustellen, aber genau deshalb gehoert es festgehalten: eine
// Vorgabe, die niemand bewacht, wandert beim naechsten Handgriff.
//
// Flach und Tief liegen als mitgelieferte Themendateien daneben und
// tragen ABSICHTLICH KEINE FARBEN. Sie sollen die Palette in Ruhe
// lassen und nur Ecken, Luft und Verlaufstiefe umstellen.

#include <QtTest>
#include <QDir>

#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"

using namespace Longpath;

class TstFormVariants : public QObject
{
    Q_OBJECT

private:
    static QString themeFile(const QString& name)
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { return {}; }
        return QDir(QString::fromLocal8Bit(root))
            .filePath(QStringLiteral("resources/themes/%1.json").arg(name));
    }

private slots:
    void cleanup() { Style::Theme::instance().clear(); }

    /// Die Vorgabe ist WEICH — gemessen am erzeugten Stil, nicht am
    /// Rueckgabewert von formInt.
    ///
    /// Der erste Anlauf verglich formInt("radius", 7) gegen 7. Das ist
    /// eine Tautologie: ohne Thema liefert formInt genau den
    /// mitgegebenen Vorgabewert zurueck, egal was der Malcode
    /// tatsaechlich benutzt. Der Test waere gruen geblieben, wenn
    /// jemand in buttonBaseStyle() eine andere Zahl eingetragen haette.
    ///
    /// Geprueft wird deshalb der STIL, den der Knopf wirklich bekommt.
    void theBuiltInDefaultIsWeich()
    {
        Style::Theme::instance().clear();

        const QString btn = Style::buttonBaseStyle();
        QVERIFY2(btn.contains(QStringLiteral("border-radius: 7px")),
                 qPrintable(QStringLiteral("Ecken nicht 7: %1").arg(btn)));
        QVERIFY2(btn.contains(QStringLiteral("padding: 4px 10px")),
                 qPrintable(QStringLiteral("Luft nicht 4/10: %1").arg(btn)));

        // Relief und Mulde stecken in den Verlaufsstufen. Sie sind
        // nicht als Zahl im Stil zu finden — also ueber den Vergleich:
        // die Vorgabe muss dieselbe Flaeche erzeugen wie ein Thema,
        // das ausdruecklich 16 und 10 setzt.
        const QString vorgabe = Style::raisedFill(Style::kButtonBg);
        QCOMPARE(vorgabe, Style::raisedFill(Style::kButtonBg, 16, 12));

        const QString mulde = Style::sunkenFill(Style::kInsetBg);
        QCOMPARE(mulde, Style::sunkenFill(Style::kInsetBg, 10, 12));
    }

    /// Beide Nachbarn sind ladbar und stellen die Form wirklich um.
    void bothNeighboursLoadAndChangeTheShape()
    {
        const QString flach = themeFile(QStringLiteral("flach"));
        const QString tief  = themeFile(QStringLiteral("tief"));
        if (flach.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(flach, &err), qPrintable(err));
        QCOMPARE(Style::formInt("relief", 16), 6);
        QCOMPARE(Style::formInt("mulde", 10),  4);
        const QString flachKnopf = Style::buttonBaseStyle();

        QVERIFY2(Style::Theme::instance().loadFile(tief, &err), qPrintable(err));
        QCOMPARE(Style::formInt("relief", 16), 26);
        QCOMPARE(Style::formInt("mulde", 10),  18);

        QVERIFY2(Style::buttonBaseStyle() != flachKnopf,
                 "Flach und Tief erzeugen denselben Knopf");
    }

    /// Ein Thema, das NUR die Form aendert, gilt als aktiv.
    ///
    /// Bis zum 2026-08-22 zaehlten dafuer nur Farben — die beiden
    /// Fassungen haetten sich als „kein Thema aktiv" gemeldet, obwohl
    /// sichtbar etwas anders aussieht.
    void aFormOnlyThemeCountsAsActive()
    {
        const QString flach = themeFile(QStringLiteral("flach"));
        if (flach.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        QString err;
        QVERIFY(Style::Theme::instance().loadFile(flach, &err));
        QVERIFY2(Style::Theme::instance().isActive(),
                 "Eine Form-Fassung meldet sich als nicht aktiv");
    }

    /// Und sie lassen die FARBEN in Ruhe.
    ///
    /// Das ist ihr ganzer Zweck: wer die Form umstellt, will nicht
    /// nebenbei seine Palette verlieren.
    void theyLeaveTheColoursAlone()
    {
        const QString tief = themeFile(QStringLiteral("tief"));
        if (tief.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        Style::Theme::instance().clear();
        const QString vorher = Style::hexRole(Style::kButtonBg);

        QString err;
        QVERIFY(Style::Theme::instance().loadFile(tief, &err));
        QCOMPARE(Style::hexRole(Style::kButtonBg), vorher);
    }
};

QTEST_MAIN(TstFormVariants)
#include "tst_form_variants.moc"
