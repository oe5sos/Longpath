// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Einstellungsseite fuer die Spracherkennung ist erreichbar und
// schreibt wirklich dorthin, wo MainWindow_Asr.cpp nachliest.
//
// Anlass: Adresse, Sprache und Modell des Whisper-Dienstes standen
// bisher nur in der XML-Datei. Wer sie aendern wollte, musste die
// Datei von Hand aufmachen.
//
// Gemessen wird am fernen Ende: nicht "die Klasse existiert", sondern
// die Seite haengt im Baum des Einstellungsdialogs UND nach einer
// Eingabe steht der Wert unter genau dem Schluessel, den setAsrEnabled
// liest. Zwei getrennte Fehler -- gebaut-aber-nicht-eingehaengt und
// gespeichert-unter-falschem-Namen -- die heute beide schon vorkamen.

#include <QtTest>

#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "core/AppSettings.h"
#include "gui/SetupDialog.h"
#include "gui/setup/AsrPage.h"

using namespace Longpath;

namespace {

bool treeHasEntry(QTreeWidget* tree, const QString& label)
{
    const QList<QTreeWidgetItem*> hits =
        tree->findItems(label, Qt::MatchExactly | Qt::MatchRecursive, 0);
    return !hits.isEmpty();
}

} // namespace

class TstAsrSettingsPage : public QObject
{
    Q_OBJECT

private slots:
    void seiteStehtImBaum()
    {
        SetupDialog dlg(nullptr);
        QTreeWidget* tree = dlg.findChild<QTreeWidget*>();
        QVERIFY2(tree, "Einstellungsdialog ohne Navigationsbaum");
        QVERIFY2(treeHasEntry(tree, QStringLiteral("Spracherkennung")),
                 "Die Seite ist gebaut, aber nirgends erreichbar.");
    }

    void eingabeLandetUnterDemGelesenenSchluessel()
    {
        auto& s = AppSettings::instance();
        const QString vorher =
            s.value(QStringLiteral("AsrEndpointUrl"), QString()).toString();

        AsrPage page(nullptr);
        const QList<QLineEdit*> edits = page.findChildren<QLineEdit*>();
        QVERIFY2(!edits.isEmpty(), "Seite ohne Eingabefelder");

        // Das Adressfeld ist das einzige, das mit http anfaengt.
        QLineEdit* urlEdit = nullptr;
        for (QLineEdit* e : edits) {
            if (e->text().startsWith(QStringLiteral("http"))) { urlEdit = e; break; }
        }
        QVERIFY2(urlEdit, "kein Adressfeld mit einer Vorgabe gefunden");

        urlEdit->setText(QStringLiteral("  http://127.0.0.1:9099/inference  "));
        emit urlEdit->editingFinished();

        // Genau der Schluessel, den MainWindow::setAsrEnabled liest --
        // und ohne die Leerzeichen, sonst scheitert der Verbindungsaufbau
        // an etwas, das man auf dem Bildschirm nicht sieht.
        QCOMPARE(s.value(QStringLiteral("AsrEndpointUrl")).toString(),
                 QStringLiteral("http://127.0.0.1:9099/inference"));

        s.setValue(QStringLiteral("AsrEndpointUrl"), vorher);
    }

    void spracheUndModellWerdenGetrenntGehalten()
    {
        auto& s = AppSettings::instance();
        const QString sprVorher =
            s.value(QStringLiteral("AsrLanguage"), QString()).toString();
        const QString modVorher =
            s.value(QStringLiteral("AsrModel"), QString()).toString();

        AsrPage page(nullptr);
        QLineEdit* sprache = nullptr;
        QLineEdit* modell  = nullptr;
        for (QLineEdit* e : page.findChildren<QLineEdit*>()) {
            if (e->text() == QStringLiteral("de"))         { sprache = e; }
            if (e->text() == QStringLiteral("whisper-1"))  { modell  = e; }
        }
        QVERIFY2(sprache, "Sprachfeld fehlt oder hat eine andere Vorgabe als de");
        QVERIFY2(modell,  "Modellfeld fehlt oder hat eine andere Vorgabe");

        sprache->setText(QStringLiteral("en"));
        emit sprache->editingFinished();
        modell->setText(QStringLiteral("ggml-large-v3-turbo"));
        emit modell->editingFinished();

        QCOMPARE(s.value(QStringLiteral("AsrLanguage")).toString(),
                 QStringLiteral("en"));
        QCOMPARE(s.value(QStringLiteral("AsrModel")).toString(),
                 QStringLiteral("ggml-large-v3-turbo"));

        s.setValue(QStringLiteral("AsrLanguage"), sprVorher);
        s.setValue(QStringLiteral("AsrModel"), modVorher);
    }
};

QTEST_MAIN(TstAsrSettingsPage)
#include "tst_asr_settings_page.moc"
