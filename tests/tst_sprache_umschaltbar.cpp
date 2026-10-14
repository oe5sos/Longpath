// =================================================================
// tests/tst_sprache_umschaltbar.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. Betreiber am 2026-10-14: "englisch
// durchgehend, aber deutsch als option". Dieser Prueflauf haelt die
// Kette fest, an der diese Zusage haengt:
//
//   translations/longpath_de.ts  --lrelease-->  longpath_de.qm
//        --qt_add_translations-->  :/i18n/  --QTranslator-->  Oberflaeche
//
// Es sind drei Stellen, an denen sie reissen kann, und keine davon
// faellt im Betrieb auf: eine Uebersetzung, die nicht uebersetzt wurde,
// eine .qm, die nicht in die Binaerdatei kam, und ein Uebersetzer, der
// zu spaet eingehaengt wird. Die ersten beiden stehen hier; die dritte
// (Reihenfolge in main.cpp) zeigt die Protokollzeile "UI language".
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QTranslator>
#include <QCoreApplication>
#include <QFileInfo>

class TstSpracheUmschaltbar : public QObject {
    Q_OBJECT

private slots:
    // Die .qm muss gebaut worden sein. Fehlt sie, waere die
    // Sprachwahl im Setup eine Zusage ohne Deckung.
    void dieUebersetzungWirdGebaut()
    {
        const QString qm = QStringLiteral(LONGPATH_QM_FILE);
        QVERIFY2(QFileInfo::exists(qm), qPrintable(QStringLiteral("nicht gebaut: %1").arg(qm)));
        QVERIFY(QFileInfo(qm).size() > 0);
    }

    // Und sie muss wirklich etwas uebersetzen. Eine leere .qm laedt
    // ohne Fehler und laesst alles englisch -- genau der Fall, den
    // niemand bemerkt.
    void deutschUebersetztWirklich()
    {
        QTranslator t;
        QVERIFY2(t.load(QStringLiteral(LONGPATH_QM_FILE)), "QTranslator nahm die .qm nicht an");
        // Aus Setup > General > Options, der Seite, auf der die
        // Sprachwahl selbst sitzt.
        QCOMPARE(t.translate("Longpath::GeneralOptionsPage", "Options"),
                 QStringLiteral("Optionen"));
        QCOMPARE(t.translate("Longpath::GeneralOptionsPage", "Language"),
                 QStringLiteral("Sprache"));
    }

    // Ein unbekannter Text darf nicht erfunden werden: QTranslator gibt
    // dann leer zurueck, und Qt faellt auf die englische Quelle zurueck.
    void unuebersetztesBleibtEnglisch()
    {
        QTranslator t;
        QVERIFY(t.load(QStringLiteral(LONGPATH_QM_FILE)));
        QVERIFY(t.translate("Longpath::GeneralOptionsPage",
                            "Diesen Text gibt es nirgends").isEmpty());
    }
};

QTEST_MAIN(TstSpracheUmschaltbar)
#include "tst_sprache_umschaltbar.moc"
