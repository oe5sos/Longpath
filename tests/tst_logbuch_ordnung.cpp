// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbuch_ordnung.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Das Logbuch nach Entwurf A (2026-11-12). Der Betreiber: „ich möchte
// dies überarbeiten und übersichtlicher gestalten", „viele sachen werden
// auch angezeigt, nicht unwichtig sind (export als Beispiel) die wir auch
// in optionen einbinden können", „die anzahl aller modi und bänder sind
// groß und kosten platz". Gewählt: Entwurf A — gleiche drei Spalten, eine
// Leiste statt drei ... dann „bitte doch C": drei Arbeitsweisen Log,
// Browse, Statistics; Datei- und Online-Arbeit hinter dem Zahnrad, die
// Filter links in Browse, keine Band-/Betriebsart-Pillen über der Karte.
//
// Mit LONGPATH_GRAB_DIR legt der Prüfstand Bilder des echten Fensters ab;
// LONGPATH_GRAB_LOG nimmt dafür ein echtes Log (es wird kopiert, nie
// beschrieben).

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/GibsTileLayer.h"
#include "FensterFrist.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>

using namespace Longpath;

namespace {

QString schreibeLog(const QDir& dir)
{
    const QString ziel = dir.filePath(QStringLiteral("log.adi"));
    const QString echt = qEnvironmentVariable("LONGPATH_GRAB_LOG");
    if (!echt.isEmpty() && QFile::exists(echt)) {
        QFile::copy(echt, ziel);
        return ziel;
    }
    QFile f(ziel);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:4>OG3B <QSO_DATE:8>20261031 <TIME_ON:6>180533 <BAND:3>40m <MODE:3>SSB "
            "<COUNTRY:7>Finland <GRIDSQUARE:6>KP10AQ <MY_GRIDSQUARE:6>JN67VV <EOR>\n"
            "<CALL:5>OH3MB <QSO_DATE:8>20261031 <TIME_ON:6>114113 <BAND:3>40m <MODE:3>FT8 "
            "<GRIDSQUARE:4>KP20 <MY_GRIDSQUARE:6>JN67VV <EOR>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260902 <TIME_ON:6>143710 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>FN42 <MY_GRIDSQUARE:6>JN67VV <EOR>\n"
            "<CALL:5>DL1AA <QSO_DATE:8>20260801 <TIME_ON:6>101010 <BAND:3>15m <MODE:3>FT8 "
            "<GRIDSQUARE:4>JO62 <MY_GRIDSQUARE:6>JN67VV <EOR>\n"
            "<CALL:5>G4XYZ <QSO_DATE:8>20260701 <TIME_ON:6>090909 <BAND:3>20m <MODE:3>CW "
            "<GRIDSQUARE:4>IO91 <MY_GRIDSQUARE:6>JN67VV <EOR>\n");
    f.close();
    return ziel;
}

QPushButton* knopf(QWidget& w, const QString& text)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b->text() == text) { return b; }
    }
    return nullptr;
}

// Die Logtabelle -- nicht die Jagdliste der Karte, die auch eine
// QTableWidget ist.
QTableWidget* logTabelle(LogbookWindow& w)
{
    return w.findChild<QTableWidget*>(QStringLiteral("logbookTable"));
}

void zeileWaehlen(LogbookWindow& w, const QString& call)
{
    auto* t = logTabelle(w);
    if (!t) { return; }
    for (int r = 0; r < t->rowCount(); ++r) {
        for (int c = 0; c < t->columnCount(); ++c) {
            if (t->item(r, c) && t->item(r, c)->text() == call) {
                t->setCurrentCell(r, c);
                t->selectRow(r);
                return;
            }
        }
    }
}

void bild(QWidget& w, const QString& name)
{
    const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
    if (dir.isEmpty()) { return; }
    QDir().mkpath(dir);
    QTest::qWait(400);
    w.grab().save(dir + QLatin1Char('/') + name);
}

} // namespace

class TstLogbuchOrdnung : public QObject { Q_OBJECT
private slots:
    void init()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("LogbookMode"), QStringLiteral("Log"));
        for (const char* k : {"LogbookLogSplit", "LogbookLogMitte", "LogbookBrowseSplit",
                              "LogbookBrowseRight"}) {
            s.setValue(QString::fromLatin1(k), QByteArray());
        }
        // Fuer die Bilder: das echte Weltbild und die Kartenansicht des
        // Betreibers -- ohne Weltbild zeichnet Longpath die schematische
        // Notkugel, und die sah im Bild aus wie ein Fehler (2026-11-12).
        const QString welt = qEnvironmentVariable("LONGPATH_GRAB_WELT");
        if (!welt.isEmpty()) {
            s.setValue(QStringLiteral("GlobeWorldImagePath"), welt);
            s.setValue(QStringLiteral("GlobeWorldImageStyle"), QStringLiteral("Crisp"));
        }
        const QString ansicht = qEnvironmentVariable("LONGPATH_GRAB_ANSICHT");
        if (!ansicht.isEmpty()) { s.setValue(QStringLiteral("QsoMapView"), ansicht); }
    }

    // Datei- und Online-Arbeit steht nicht mehr in der Leiste, sondern
    // hinter dem Zahnrad -- und ist dort vollstaendig.
    void dateiarbeit_hinter_dem_zahnrad()
    {
        QTemporaryDir dir;
        LogbookWindow w(schreibeLog(QDir(dir.path())));
        w.resize(1468, 790);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        for (const QString& alt : {QStringLiteral("Import…"), QStringLiteral("Export ADIF…"),
                                   QStringLiteral("Export CSV…"), QStringLiteral("Cabrillo…"),
                                   QStringLiteral("SOTA…")}) {
            QPushButton* b = knopf(w, alt);
            QVERIFY2(!b || !b->isVisible(), qPrintable(alt + QStringLiteral(" steht noch in der Leiste")));
        }
        for (const char* name : {"logbookImportAction", "logbookExportAdifAction",
                                 "logbookExportCsvAction", "logbookCabrilloAction",
                                 "logbookUploadAction", "logbookSyncAction", "logbookSotaAction"}) {
            QVERIFY2(w.findChild<QAction*>(QString::fromLatin1(name)),
                     qPrintable(QStringLiteral("fehlt im Zahnrad: %1").arg(QLatin1String(name))));
        }
        // Das Zahnrad ist in der Titelleiste sichtbar.
        QPushButton* zahnrad = knopf(w, QStringLiteral("⚙"));
        QVERIFY(zahnrad && zahnrad->isVisible());
    }

    // Drei Arbeitsweisen: Log, Browse, Statistics -- je eine Seite.
    void drei_arbeitsweisen()
    {
        QTemporaryDir dir;
        LogbookWindow w(schreibeLog(QDir(dir.path())));
        w.resize(1468, 790);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        auto* log = w.findChild<QPushButton*>(QStringLiteral("logbookModeLog"));
        auto* browse = w.findChild<QPushButton*>(QStringLiteral("logbookModeBrowse"));
        auto* stats = w.findChild<QPushButton*>(QStringLiteral("logbookModeStats"));
        auto* eingabe = w.findChild<QWidget*>(QStringLiteral("logbookEntryArea"));
        auto* filter = w.findChild<QWidget*>(QStringLiteral("logbookFilterBar"));
        QVERIFY(log && browse && stats && eingabe && filter);

        log->click();
        QTRY_VERIFY(eingabe->isVisible());
        QVERIFY(!filter->isVisible());
        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        karte->imagery()->setNetworkEnabled(false);
        QTRY_VERIFY(karte->isVisible());
        QVERIFY(!karte->isKompakt());
        QTRY_VERIFY(logTabelle(w)->isVisible());   // die letzten QSOs

        browse->click();
        QTRY_VERIFY(filter->isVisible());
        QVERIFY(!eingabe->isVisible());
        QTRY_VERIFY(karte->isVisible());
        QVERIFY(karte->isKompakt());
        QTRY_VERIFY(logTabelle(w)->isVisible());

        stats->click();
        QTRY_VERIFY(w.statsSectionForTest()->isVisible());
        QVERIFY(!karte->isVisible());
        QVERIFY(!filter->isVisible());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("LogbookMode")).toString(),
                 QStringLiteral("Statistics"));
    }

    // In Browse: aktive Filter als Kapseln, ein Klick nimmt einen weg.
    void filterspalte_und_kapseln()
    {
        QTemporaryDir dir;
        LogbookWindow w(schreibeLog(QDir(dir.path())));
        w.resize(1468, 790);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        w.findChild<QPushButton*>(QStringLiteral("logbookModeBrowse"))->click();
        auto* kapseln = w.findChild<QWidget*>(QStringLiteral("logbookFilterChips"));
        QVERIFY(kapseln);
        QTRY_VERIFY(!kapseln->isVisible());

        auto* modeWahl = w.findChild<QWidget*>(QStringLiteral("logbookModeChoice"));
        QVERIFY(modeWahl);
        QPushButton* ft8 = nullptr;
        for (QPushButton* b : modeWahl->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("FT8")) { ft8 = b; }
        }
        QVERIFY2(ft8, "FT8 nicht unter den drei haeufigsten");
        auto* t = logTabelle(w);
        const int alle = t->rowCount();
        ft8->click();
        QTRY_VERIFY(kapseln->isVisible());
        QVERIFY2(t->rowCount() > 0 && t->rowCount() < alle,
                 qPrintable(QStringLiteral("%1 von %2").arg(t->rowCount()).arg(alle)));
        zeileWaehlen(w, QStringLiteral("OH3MB"));
        bild(w, QStringLiteral("C2-browse.png"));

        QList<QPushButton*> k = kapseln->findChildren<QPushButton*>();
        QCOMPARE(k.size(), 1);
        k.first()->click();
        QTRY_COMPARE(t->rowCount(), alle);
        QTRY_VERIFY(!kapseln->isVisible());
    }

    // Ueber der Karte: keine Band-/Betriebsart-Pillen, die Haken im
    // Ebenen-Menue, Zoom auf dem Bild, 3D in der Ansichtsgruppe.
    void karte_ohne_pillen()
    {
        QTemporaryDir dir;
        LogbookWindow w(schreibeLog(QDir(dir.path())));
        w.resize(1468, 790);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        w.findChild<QPushButton*>(QStringLiteral("logbookModeLog"))->click();
        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte && karte->isEmbedded());
        karte->imagery()->setNetworkEnabled(false);
        QTRY_VERIFY(karte->isVisible());

        for (const QString& band : {QStringLiteral("40m"), QStringLiteral("20m"), QStringLiteral("15m")}) {
            QPushButton* pille = knopf(*karte, band);
            QVERIFY2(!pille || !pille->isVisible(), qPrintable(band + QStringLiteral("-Pille ueber der Karte")));
        }
        auto* ebenen = karte->findChild<QPushButton*>(QStringLiteral("qsoMapLayersButton"));
        QVERIFY(ebenen && ebenen->isVisible());
        for (QCheckBox* c : karte->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Paths") || c->text() == QStringLiteral("Imagery")) {
                QVERIFY2(!c->isVisible(), qPrintable(c->text() + QStringLiteral(" steht noch in der Leiste")));
            }
        }
        auto* zoom = karte->findChild<QWidget*>(QStringLiteral("qsoMapZoomOverlay"));
        QVERIFY(zoom && zoom->isVisible());
        QVERIFY(karte->threeDButtonForTest() && karte->threeDButtonForTest()->isVisible());

        // Wege an/aus und nach Betriebsart direkt an der Karte (Betreiber
        // 2026-11-12: „die blauen linien solle man ein und ausblenden
        // können, aber auch ft8, ft4 bzw. ssb ändern können").
        auto* pfade = karte->findChild<QPushButton*>(QStringLiteral("qsoMapPathsToggle"));
        QVERIFY(pfade && pfade->isVisible() && pfade->isChecked());
        auto* gruppe = karte->findChild<QWidget*>(QStringLiteral("qsoMapPathGroup"));
        QPushButton* ft8 = nullptr;
        QPushButton* alle = nullptr;
        for (QPushButton* b : gruppe->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("FT8")) { ft8 = b; }
            if (b->text() == QStringLiteral("All")) { alle = b; }
        }
        QVERIFY2(ft8 && alle, "keine Betriebsart-Knoepfe an der Karte");
        QTRY_VERIFY(karte->shownCountForTest() > 0);
        const int gesamt = karte->shownCountForTest();
        ft8->click();
        QCOMPARE(karte->pfadModusForTest(), QStringLiteral("FT8"));
        QTRY_VERIFY(karte->shownCountForTest() < gesamt);
        for (QPushButton* b : gruppe->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("All")) { alle = b; }
        }
        alle->click();
        QTRY_COMPARE(karte->shownCountForTest(), gesamt);
        pfade->click();
        QVERIFY(!pfade->isChecked());
        pfade->click();
        QVERIFY(pfade->isChecked());

        // Fall 1 aus dem Entwurf: ein Rufzeichen in der Eingabe.
        for (QLineEdit* e : w.findChildren<QLineEdit*>()) {
            if (e->placeholderText() == QStringLiteral("callsign")) { e->setText(QStringLiteral("OG3B")); }
        }
        zeileWaehlen(w, QStringLiteral("OG3B"));
        bild(w, QStringLiteral("C1-log.png"));

        w.findChild<QPushButton*>(QStringLiteral("logbookModeStats"))->click();
        bild(w, QStringLiteral("C3-statistics.png"));
    }
};

QTEST_MAIN(TstLogbuchOrdnung)
#include "tst_logbuch_ordnung.moc"
