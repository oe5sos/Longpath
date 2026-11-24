// SPDX-License-Identifier: GPL-3.0-or-later
//
// Steht „Blau-Gold\" auf der Seite Setup > Appearance > Colors & Theme?
//
// Die Menue-Fassung prueft tst_palette_im_menue. Diese prueft die zweite
// Stelle, an der die Wahl getroffen wird: die Auswahlliste der Seite.
// Gefuellt wird sie aus Style::Theme::available(), also aus derselben
// Quelle wie das Menue -- der Lauf sagt, ob sie dort auch ankommt.
//
// Ein Wechsel auf der Seite muss die Palette setzen UND den Merker
// schreiben, sonst ist sie nach dem Neustart wieder weg.
// =================================================================
// Modification history (Longpath):
//   2026-11-24 -- Original fuer Longpath, KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/SetupDialog.h"
#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"

using namespace Longpath;

class TstPaletteImSetup : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_themeDir;

    /// Die ausgelieferte Blau-Gold-Datei in einen eigenen Ordner kopieren
    /// und ihn als Palettenordner einhaengen. So haengt der Lauf nicht
    /// daran, was gerade im Programmpaket liegt.
    void legeBlauGoldHin()
    {
        QVERIFY(m_themeDir.isValid());
        const QString quelle = QStringLiteral(LONGPATH_SRC_DIR)
                               + QStringLiteral("/resources/themes/blau-gold.json");
        QVERIFY2(QFile::exists(quelle), qPrintable(quelle));
        const QString ziel = m_themeDir.filePath(QStringLiteral("blau-gold.json"));
        QFile::remove(ziel);
        QVERIFY(QFile::copy(quelle, ziel));
        qputenv("LONGPATH_THEME_DIR", m_themeDir.path().toLocal8Bit());
    }

    /// Die Auswahlliste der Seite „Colors & Theme\" suchen. Die Seite
    /// hat genau eine Palettenauswahl; die erkennt man an der eingebauten
    /// Palette als Eintrag.
    static QComboBox* paletteCombo(SetupDialog& dlg)
    {
        for (QComboBox* box : dlg.findChildren<QComboBox*>()) {
            if (box->findText(Style::Theme::builtInName()) >= 0) { return box; }
        }
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        // Nicht in die echten Einstellungen schreiben (siehe
        // tst_theme_selection): vor dem ersten instance()-Aufruf.
        AppSettings::setProfileOverride(QStringLiteral("test-palette-im-setup"));
        legeBlauGoldHin();
        Style::Theme::instance().clear();
    }

    void cleanupTestCase()
    {
        Style::Theme::instance().clear();
        qunsetenv("LONGPATH_THEME_DIR");
    }

    /// Ohne Wahl bleibt die Standardpalette stehen -- und Blau-Gold ist
    /// zwar auswaehlbar, aber nicht aktiv.
    void standardBleibtStandard()
    {
        SetupDialog dlg(nullptr);
        dlg.selectPage(QStringLiteral("Colors & Theme"));
        QComboBox* box = paletteCombo(dlg);
        QVERIFY2(box, "keine Palettenauswahl auf der Seite Colors & Theme");

        QCOMPARE(box->itemText(0), Style::Theme::builtInName());
        QCOMPARE(box->currentText(), Style::Theme::builtInName());
        QVERIFY(!Style::Theme::instance().isActive());
    }

    /// Blau-Gold steht in der Liste der Seite.
    void blauGoldStehtAufDerSeite()
    {
        SetupDialog dlg(nullptr);
        dlg.selectPage(QStringLiteral("Colors & Theme"));
        QComboBox* box = paletteCombo(dlg);
        QVERIFY2(box, "keine Palettenauswahl auf der Seite Colors & Theme");

        QStringList gefuehrt;
        for (int i = 0; i < box->count(); ++i) { gefuehrt << box->itemText(i); }
        qInfo().noquote() << "Auswahl auf der Seite:" << gefuehrt.join(QLatin1String(", "));
        QVERIFY2(gefuehrt.contains(QStringLiteral("Blau-Gold")),
                 qPrintable(QStringLiteral("„Blau-Gold\" fehlt auf der Seite — "
                                           "gefuehrt: %1")
                                .arg(gefuehrt.join(QLatin1String(", ")))));
    }

    /// Die Wahl auf der Seite setzt die Palette und merkt sie.
    void dieWahlAufDerSeiteSetztUndMerkt()
    {
        SetupDialog dlg(nullptr);
        dlg.selectPage(QStringLiteral("Colors & Theme"));
        QComboBox* box = paletteCombo(dlg);
        QVERIFY2(box, "keine Palettenauswahl auf der Seite Colors & Theme");

        const int idx = box->findText(QStringLiteral("Blau-Gold"));
        QVERIFY2(idx >= 0, "Blau-Gold nicht in der Auswahl");
        box->setCurrentIndex(idx);

        QVERIFY2(Style::Theme::instance().isActive(),
                 "Wahl auf der Seite hat die Palette nicht gesetzt");
        QCOMPARE(Style::Theme::instance().name(), QStringLiteral("Blau-Gold"));
        QCOMPARE(AppSettings::instance().value("ActiveTheme", QString()).toString(),
                 QStringLiteral("Blau-Gold"));

        // Zurueck zur Standardpalette, damit der Lauf nichts stehen laesst.
        box->setCurrentIndex(0);
        QVERIFY(!Style::Theme::instance().isActive());
    }

    /// Die Seite in Blau-Gold als Blatt speichern (nur mit LONGPATH_GRAB_DIR).
    void dieSeiteInBlauGoldBlatt()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }

        Style::Theme::instance().activate(QStringLiteral("Blau-Gold"));
        SetupDialog dlg(nullptr);
        dlg.resize(1100, 720);
        dlg.selectPage(QStringLiteral("Colors & Theme"));
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg, 20000));
        QTest::qWait(500);
        QVERIFY(dlg.grab().toImage().save(dir + QStringLiteral("/setup-colors-blau-gold.png")));
        dlg.close();

        Style::Theme::instance().clear();
    }
};

QTEST_MAIN(TstPaletteImSetup)
#include "tst_palette_im_setup.moc"
