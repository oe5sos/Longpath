// SPDX-License-Identifier: GPL-3.0-or-later
//
// Eine Palette darf nichts versprechen, was das Programm nicht malt.
//
// „moos.json" nannte 84 Farben, zwei davon unter Namen, die es im
// Programm nicht gibt: „inset-bg-top" und „tile-active". Das Laden
// meldet keinen Fehler — unbekannte Rollen werden still uebergangen —,
// und so standen sie seit dem ersten Tag wirkungslos in der Datei.
// Geschrieben habe ich sie selbst, in der Nacht zum 2026-10-15.
//
// Schlimmer als wirkungslos: ich habe dem Betreiber daraufhin gemeldet,
// ZWOELF Rollen der Palette seien tot und der Panadapter folge keinem
// Thema. Beides war falsch. Ich hatte nur die Tabelle in ThemeQss
// durchsucht und den ZWEITEN Weg uebersehen — Style::role(), direkt im
// Malcode, dreissig Rollen. Gemessen an den Bildpunkten wechselt der
// Panadapter vollstaendig mit (#08080a -> #1c2025 auf 75 % der
// Flaeche).
//
// Dieser Lauf kennt beide Wege und laesst keine dritte Behauptung zu.

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace {

/// Alle Rollen, die das Programm kennt — aus BEIDEN Wegen.
QSet<QString> bekannteRollen()
{
    QSet<QString> out;
    const QString wurzel = QStringLiteral(LONGPATH_SRC_DIR);

    // Weg 1: die Tabelle in ThemeQss.cpp, fuer Stylesheets.
    {
        QFile f(wurzel + QStringLiteral("/src/gui/styles/ThemeQss.cpp"));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString t = QString::fromUtf8(f.readAll());
            static const QRegularExpression re(
                QStringLiteral("\\{\\s*\"([a-z0-9\\-]+)\"\\s*,"));
            auto m = re.globalMatch(t);
            while (m.hasNext()) { out.insert(m.next().captured(1)); }
        }
    }

    // Weg 2: Style::role(...) im Malcode. DEN hatte ich uebersehen.
    {
        static const QRegularExpression re(
            QStringLiteral("Style::role\\(\\s*\"([a-z0-9\\-]+)\""));
        QDirIterator it(wurzel + QStringLiteral("/src"),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            const QString t = QString::fromUtf8(f.readAll());
            auto m = re.globalMatch(t);
            while (m.hasNext()) { out.insert(m.next().captured(1)); }
        }
    }
    return out;
}

} // namespace

class TstPaletteOhneToteRollen : public QObject
{
    Q_OBJECT

private slots:
    void jedeMitgelieferteFarbeErreichtAuchEtwas()
    {
        const QSet<QString> kennt = bekannteRollen();
        qInfo() << "Rollen, die das Programm kennt:" << kennt.size();
        QVERIFY2(kennt.size() > 40,
                 "zu wenige Rollen gefunden — dann prueft das hier nichts");

        const QString ordner =
            QStringLiteral(LONGPATH_SRC_DIR) + QStringLiteral("/resources/themes");
        QDir d(ordner);
        QVERIFY2(d.exists(), qPrintable(ordner));
        const QStringList dateien =
            d.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        QVERIFY2(!dateien.isEmpty(), "keine mitgelieferte Palette gefunden");

        QStringList tot;
        for (const QString& name : dateien) {
            QFile f(d.filePath(name));
            QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(name));
            QJsonParseError pe{};
            const QJsonDocument doc =
                QJsonDocument::fromJson(f.readAll(), &pe);
            QVERIFY2(pe.error == QJsonParseError::NoError,
                     qPrintable(QStringLiteral("%1: %2")
                                    .arg(name, pe.errorString())));
            const QJsonObject farben =
                doc.object().value(QStringLiteral("colors")).toObject();
            for (auto it = farben.constBegin(); it != farben.constEnd(); ++it) {
                const QString k = it.key();
                // Ein Hexwert als Schluessel ist der Notausgang fuer eine
                // Farbe ohne Rolle (siehe Theme.h) und immer gueltig.
                if (k.startsWith(QLatin1Char('#'))) { continue; }
                if (k.startsWith(QLatin1Char('_'))) { continue; }
                if (kennt.contains(k)) { continue; }
                tot << QStringLiteral("%1: %2").arg(name, k);
            }
            qInfo().noquote() << name << farben.size() << "Farben";
        }

        QVERIFY2(tot.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Diese Rollen stehen in einer mitgelieferten Palette, "
                     "erreichen aber nichts: %1. Entweder sie bekommen eine "
                     "Zeile in ThemeQss::table() oder einen Style::role()-"
                     "Aufruf im Malcode — oder sie gehoeren aus der Datei.")
                                .arg(tot.join(QLatin1String(", ")))));
    }
};

QTEST_MAIN(TstPaletteOhneToteRollen)
#include "tst_palette_ohne_tote_rollen.moc"
