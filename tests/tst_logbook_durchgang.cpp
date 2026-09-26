// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Durchgangstest des Logbuchs (Nacht 26./27.09.2026, Betreiber: "nutze
// die Nacht und teste hierzu alles im Logprogramm"). Laeuft ohne
// Bildschirm gegen ein Log:
//   - LONGPATH_LOGBOOK_FIXTURE=<pfad>: eine KOPIE eines echten Logs
//     (das Original wird nie beruehrt, der Test arbeitet im Temp-Ordner)
//   - sonst ein erzeugtes Log mit allen Feldarten (fuer die CI).
// LONGPATH_GRAB_DIR: Bilder der Schritte.
//
// Der wichtigste Teil zuerst: jedes Bearbeiten/Loeschen schreibt das
// GANZE Log neu (LogbookWindow::saveAll -> AdifLog::write). Ein Feld,
// das dabei verloren geht, verschwindet bei der ersten Korrektur aus
// allen Kontakten. Geprueft mit einem eigenen, von AdifLog unabhaengigen
// Zerleger.

#include <QtTest>
#include <memory>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QElapsedTimer>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTimeZone>

#include "core/AdifLog.h"
#include "core/QsoConfirmation.h"
#include "gui/LogbookWindow.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/QsoDetailPane.h"
#include "core/AppSettings.h"

using namespace Longpath;

namespace {

// Ein Datensatz als Liste (FELD, Wert) -- unabhaengig von AdifLog, Laenge
// in Bytes wie ADIF sie meint.
using Record = QList<QPair<QByteArray, QByteArray>>;

QList<Record> rawRecords(const QByteArray& bytes)
{
    QList<Record> out;
    Record cur;
    int i = 0;
    const int eoh = bytes.toLower().indexOf("<eoh>");
    if (eoh >= 0) { i = eoh + 5; }
    while (true) {
        const int lt = bytes.indexOf('<', i);
        if (lt < 0) { break; }
        const int gt = bytes.indexOf('>', lt);
        if (gt < 0) { break; }
        const QByteArray tag = bytes.mid(lt + 1, gt - lt - 1);
        const QList<QByteArray> parts = tag.split(':');
        const QByteArray name = parts.value(0).trimmed().toUpper();
        if (name == "EOR") {
            out << cur; cur.clear(); i = gt + 1; continue;
        }
        const int len = parts.value(1).toInt();
        const QByteArray value = bytes.mid(gt + 1, len);
        cur << qMakePair(name, value);
        i = gt + 1 + len;
    }
    return out;
}

QByteArray recordKey(const Record& r)
{
    QByteArray call, date, time, band, mode;
    for (const auto& kv : r) {
        if (kv.first == "CALL") { call = kv.second.toUpper(); }
        else if (kv.first == "QSO_DATE") { date = kv.second; }
        else if (kv.first == "TIME_ON") { time = kv.second.left(4); }
        else if (kv.first == "BAND") { band = kv.second.toLower(); }
        else if (kv.first == "MODE") { mode = kv.second.toUpper(); }
    }
    return call + '|' + date + '|' + time + '|' + band + '|' + mode;
}

QMap<QByteArray, QByteArray> asMap(const Record& r)
{
    QMap<QByteArray, QByteArray> m;
    for (const auto& kv : r) { m.insert(kv.first, kv.second); }
    return m;
}

// Ein erzeugtes Log mit allem, was ein echtes haben kann -- auch Felder,
// die Longpath nicht kennt (andere Programme, APP_...).
QByteArray syntheticLog(int n)
{
    QByteArray out = "Longpath test\n<ADIF_VER:5>3.1.4 <PROGRAMID:8>Longpath <EOH>\n";
    const char* calls[] = {"OE5VVM", "K1ABC", "JA1XYZ", "VK2AB", "G4ABC", "EA4XX", "UA3AA",
                           "PY2AA", "OH2AA", "OE1XXX", "DL1ABC", "F5LIW", "SP6SMF", "9A8DV"};
    const char* bands[] = {"160m", "80m", "40m", "20m", "17m", "15m", "10m", "6m", "2m", "70cm"};
    const char* modes[] = {"SSB", "FT8", "CW", "FM", "RTTY"};
    auto f = [&out](const QByteArray& name, const QByteArray& v) {
        out += '<' + name + ':' + QByteArray::number(v.size()) + '>' + v + ' ';
    };
    for (int i = 0; i < n; ++i) {
        const QByteArray call = calls[i % 14];
        f("CALL", call);
        f("QSO_DATE", QDate(2024, 4, 10).addDays(i % 900).toString("yyyyMMdd").toUtf8());
        f("TIME_ON", QTime(i % 24, (i * 7) % 60, (i * 13) % 60).toString("HHmmss").toUtf8());
        f("BAND", bands[i % 10]);
        f("MODE", modes[i % 5]);
        if (i % 5 == 0) { f("SUBMODE", "USB"); }
        f("FREQ", QByteArray::number(14.074 + (i % 9) * 0.001, 'f', 6));
        f("RST_SENT", "59"); f("RST_RCVD", "57");
        // Gewohnte Schreibweise mit kleinem Kleinfeld, wie QRZ sie liefert.
        f("GRIDSQUARE", (i % 3) == 0 ? "FN30" : (i % 3) == 1 ? "JN67vx" : "JN67VX");
        f("MY_GRIDSQUARE", "JN67VV");
        f("NAME", i % 4 ? "Thomas Vidra" : "Jürgen Müller");   // Umlaute: Bytes!
        f("QTH", "Laakirchen");
        f("COUNTRY", "Austria");
        f("COMMENT", "Test " + QByteArray::number(i));
        if (i % 7 == 0) { f("LOTW_QSL_RCVD", "Y"); f("LOTW_QSLRDATE", "20250101"); }
        if (i % 11 == 0) { f("QSL_RCVD", "Y"); f("QSL_SENT", "Y"); }
        if (i % 13 == 0) { f("SOTA_REF", "OE/OO-001"); }
        if (i % 17 == 0) { f("MY_POTA_REF", "OE-1234"); }
        f("STATE", "NY"); f("CQZ", "15"); f("ITUZ", "28"); f("DXCC", "206");
        f("MY_RIG", "ANAN 10E"); f("TX_PWR", "100");
        f("APP_N1MM_EXCHANGE1", "599 001");            // fremdes Programm
        f("APP_QRZLOG_LOGID", QByteArray::number(900000 + i));
        f("OPERATOR", "OE5SOS"); f("STATION_CALLSIGN", "OE5SOS");
        out += "<EOR>\n";
    }
    return out;
}

void autoAnswerModals(QObject* ctx, const std::function<void(QWidget*)>& act)
{
    auto* t = new QTimer(ctx);
    t->setInterval(50);
    QObject::connect(t, &QTimer::timeout, ctx, [t, act]() {
        if (QWidget* m = QApplication::activeModalWidget()) { act(m); t->deleteLater(); }
    });
    t->start();
}

// Beantwortet Dialoge, solange er lebt (fuer Ablaeufe mit mehreren
// Fenstern nacheinander: Dateidialog, dann Meldung).
QTimer* answerModals(QObject* ctx, const std::function<void(QWidget*)>& act)
{
    auto* t = new QTimer(ctx);
    t->setInterval(40);
    QObject::connect(t, &QTimer::timeout, ctx, [act]() {
        if (QWidget* m = QApplication::activeModalWidget()) { act(m); }
    });
    t->start();
    return t;
}

void grab(QWidget* w, const QString& name)
{
    const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
    if (dir.isEmpty()) { return; }
    QDir().mkpath(dir);
    w->grab().save(dir + QStringLiteral("/") + name + QStringLiteral(".png"));
}

QComboBox* comboWith(QWidget* w, const QString& item)
{
    for (QComboBox* c : w->findChildren<QComboBox*>()) {
        if (c->findText(item, Qt::MatchFixedString) >= 0) { return c; }
    }
    return nullptr;
}

QLineEdit* editWithPlaceholder(QWidget* w, const QString& part)
{
    for (QLineEdit* e : w->findChildren<QLineEdit*>()) {
        if (e->placeholderText().contains(part, Qt::CaseInsensitive)) { return e; }
    }
    return nullptr;
}

QPushButton* buttonWith(QWidget* w, const QString& text)
{
    for (QPushButton* b : w->findChildren<QPushButton*>()) {
        if (b->text() == text) { return b; }
    }
    return nullptr;
}

QTableWidget* logTable(QWidget* w)
{
    // Die Tabelle mit den meisten Zeilen ist das Log (die Karte hat keine).
    QTableWidget* best = nullptr;
    for (QTableWidget* t : w->findChildren<QTableWidget*>()) {
        if (!best || t->rowCount() > best->rowCount()) { best = t; }
    }
    return best;
}

bool containsCi(const QString& hay, const QString& needle)
{
    return hay.contains(needle, Qt::CaseInsensitive);
}

} // namespace

class TstLogbookDurchgang : public QObject { Q_OBJECT
    QTemporaryDir m_dir;
    QString m_log;
    QByteArray m_original;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Die Rotor/Log-Schritte loeschen und schreiben
        // RotorLogbookPanel::logbookPath(). Eine gesetzte Sandbox-Variable
        // lenkte das auf einen echten Ordner -- hier nie.
        qunsetenv("LONGPATH_CONFIG_DIR");
        QVERIFY(m_dir.isValid());
        m_log = m_dir.filePath(QStringLiteral("logbook.adi"));
        const QString fixture = qEnvironmentVariable("LONGPATH_LOGBOOK_FIXTURE");
        if (!fixture.isEmpty()) {
            QFile in(fixture);
            QVERIFY2(in.open(QIODevice::ReadOnly), qPrintable(fixture));
            m_original = in.readAll();
        } else {
            m_original = syntheticLog(1500);
        }
        QFile out(m_log);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(m_original);
        out.close();
        qInfo().noquote() << "Log:" << (fixture.isEmpty() ? QStringLiteral("erzeugt") : fixture)
                          << m_original.size() << "Bytes";
    }

    // Einlesen + neu schreiben (wie saveAll) verliert kein Feld, keinen Wert.
    void rewritingTheLogKeepsEveryField()
    {
        QElapsedTimer t; t.start();
        const QVector<LogEntry> all = AdifLog::read(m_log);
        const qint64 readMs = t.elapsed();
        const QString copy = m_dir.filePath(QStringLiteral("rewritten.adi"));
        t.restart();
        QVERIFY(AdifLog::write(copy, all));
        const qint64 writeMs = t.elapsed();
        QFile f(copy);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray rewritten = f.readAll();

        const QList<Record> before = rawRecords(m_original);
        const QList<Record> after = rawRecords(rewritten);
        qInfo().noquote() << QStringLiteral("%1 Datensaetze vorher, %2 nachher, gelesen %3 ms, geschrieben %4 ms")
                                 .arg(before.size()).arg(after.size()).arg(readMs).arg(writeMs);
        QCOMPARE(after.size(), before.size());

        QMultiMap<QByteArray, QMap<QByteArray, QByteArray>> afterByKey;
        for (const Record& r : after) { afterByKey.insert(recordKey(r), asMap(r)); }

        int lostFields = 0, changedValues = 0, unmatched = 0;
        QMap<QByteArray, int> lostByField, changedByField;
        QStringList examples;
        for (const Record& r : before) {
            const QByteArray key = recordKey(r);
            const QList<QMap<QByteArray, QByteArray>> cands = afterByKey.values(key);
            if (cands.isEmpty()) {
                ++unmatched;
                if (unmatched <= 5) {
                    const QByteArray pre = key.left(key.indexOf('|', key.indexOf('|') + 1));
                    QStringList near;
                    for (auto it = afterByKey.cbegin(); it != afterByKey.cend() && near.size() < 3; ++it) {
                        if (it.key().startsWith(pre)) { near << QString::fromUtf8(it.key()); }
                    }
                    qInfo().noquote() << "OHNE GEGENSTUECK" << key << "naechste:" << near.join(QStringLiteral(", "));
                }
                continue;
            }
            const QMap<QByteArray, QByteArray> a = asMap(r);
            // Der passendste Kandidat (bei doppelten Schluesseln).
            const QMap<QByteArray, QByteArray>* best = nullptr;
            int bestMiss = INT_MAX;
            for (const auto& c : cands) {
                int miss = 0;
                for (auto it = a.cbegin(); it != a.cend(); ++it) {
                    if (c.value(it.key()) != it.value()) { ++miss; }
                }
                if (miss < bestMiss) { bestMiss = miss; best = &c; }
            }
            for (auto it = a.cbegin(); it != a.cend(); ++it) {
                if (!best->contains(it.key())) {
                    if (it.value().trimmed().isEmpty()) { continue; }   // leeres Feld: kein Verlust
                    ++lostFields; ++lostByField[it.key()];
                    if (examples.size() < 12) {
                        examples << QStringLiteral("fehlt %1=%2 bei %3").arg(QString::fromUtf8(it.key()),
                                     QString::fromUtf8(it.value().left(40)), QString::fromUtf8(key));
                    }
                } else if (best->value(it.key()) != it.value()) {
                    ++changedValues; ++changedByField[it.key()];
                    if (examples.size() < 12) {
                        examples << QStringLiteral("anders %1: '%2' -> '%3' bei %4").arg(QString::fromUtf8(it.key()),
                                     QString::fromUtf8(it.value().left(40)),
                                     QString::fromUtf8(best->value(it.key()).left(40)), QString::fromUtf8(key));
                    }
                }
            }
        }
        for (auto it = lostByField.cbegin(); it != lostByField.cend(); ++it) {
            qInfo().noquote() << "VERLOREN" << it.key() << it.value();
        }
        for (auto it = changedByField.cbegin(); it != changedByField.cend(); ++it) {
            qInfo().noquote() << "VERAENDERT" << it.key() << it.value();
        }
        for (const QString& e : examples) { qInfo().noquote() << "  " << e; }
        qInfo().noquote() << QStringLiteral("Ohne Gegenstueck: %1, Felder verloren: %2, Werte veraendert: %3")
                                 .arg(unmatched).arg(lostFields).arg(changedValues);
        QCOMPARE(unmatched, 0);
        QCOMPARE(lostFields, 0);
        QCOMPARE(changedValues, 0);
    }

    // Oeffnen: jede Zeile des Logs steht in der Tabelle.
    void openShowsTheWholeLog()
    {
        const QVector<LogEntry> all = AdifLog::read(m_log);
        QElapsedTimer t; t.start();
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        qInfo().noquote() << "Oeffnen + Zeigen:" << t.elapsed() << "ms";
        QTableWidget* table = logTable(&w);
        QVERIFY(table);
        QCOMPARE(table->rowCount(), all.size());
        QCOMPARE(w.entryCountForTesting(), all.size());
        grab(&w, QStringLiteral("01-geoeffnet"));
    }

    // Suche und Filter: jede Anzahl gegen eine eigene Zaehlung.
    void everyFilterShowsWhatItSays()
    {
        const QVector<LogEntry> all = AdifLog::read(m_log);
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* table = logTable(&w);

        // Suchbegriff: ein Rufzeichen, das im Log vorkommt.
        const QString term = all.isEmpty() ? QString() : all.at(all.size() / 2).call;
        int expect = 0;
        for (const LogEntry& e : all) {
            if (containsCi(e.call, term) || containsCi(e.name, term) || containsCi(e.qth, term)
                || containsCi(e.country, term) || containsCi(e.gridSquare, term)
                || containsCi(e.band, term) || containsCi(e.mode, term)
                || containsCi(e.submode, term) || containsCi(e.comment, term)) { ++expect; }
        }
        w.searchForTest()->setText(term);
        QCOMPARE(table->rowCount(), expect);
        w.searchForTest()->clear();
        QCOMPARE(table->rowCount(), all.size());

        // Band.
        QComboBox* band = comboWith(&w, QStringLiteral("20m"));
        QVERIFY(band);
        int n20 = 0;
        for (const LogEntry& e : all) { if (e.band.compare(QStringLiteral("20m"), Qt::CaseInsensitive) == 0) { ++n20; } }
        band->setCurrentIndex(band->findText(QStringLiteral("20m"), Qt::MatchFixedString));
        QCOMPARE(table->rowCount(), n20);

        // Band + Betriebsart: die haeufigste Betriebsart auf 20 m.
        QMap<QString, int> modes20;
        for (const LogEntry& e : all) {
            if (e.band.compare(QStringLiteral("20m"), Qt::CaseInsensitive) == 0) { ++modes20[e.mode.toUpper()]; }
        }
        QString m20;
        for (auto it = modes20.cbegin(); it != modes20.cend(); ++it) {
            if (m20.isEmpty() || it.value() > modes20.value(m20)) { m20 = it.key(); }
        }
        QComboBox* mode = comboWith(&w, m20);
        QVERIFY(mode && mode != band);
        int n20ft8 = 0;
        for (const LogEntry& e : all) {
            if (e.band.compare(QStringLiteral("20m"), Qt::CaseInsensitive) == 0
                && (e.mode.compare(m20, Qt::CaseInsensitive) == 0
                    || e.submode.compare(m20, Qt::CaseInsensitive) == 0)) { ++n20ft8; }
        }
        mode->setCurrentIndex(mode->findText(m20, Qt::MatchFixedString));
        QCOMPARE(table->rowCount(), n20ft8);
        grab(&w, QStringLiteral("02-filter-20m-ft8"));

        // Alles weg.
        QPushButton* clear = buttonWith(&w, QStringLiteral("Clear"));
        QVERIFY(clear);
        clear->click();
        QCOMPARE(table->rowCount(), all.size());

        // Locator als Anfang.
        QLineEdit* grid = editWithPlaceholder(&w, QStringLiteral("JN67"));
        QVERIFY(grid);
        int nJn67 = 0;
        for (const LogEntry& e : all) { if (e.gridSquare.startsWith(QStringLiteral("JN67"), Qt::CaseInsensitive)) { ++nJn67; } }
        grid->setText(QStringLiteral("JN67"));
        QCOMPARE(table->rowCount(), nJn67);
        clear->click();

        // Land als Teilwort.
        QLineEdit* country = editWithPlaceholder(&w, QStringLiteral("part of"));
        QVERIFY(country);
        int nAt = 0;
        for (const LogEntry& e : all) { if (containsCi(e.country, QStringLiteral("austria"))) { ++nAt; } }
        country->setText(QStringLiteral("austria"));
        QCOMPARE(table->rowCount(), nAt);
        clear->click();

        // Nur unbestaetigte: weniger als alles, mehr als nichts (bei einem echten Log).
        QCheckBox* unconf = nullptr;
        for (QCheckBox* c : w.findChildren<QCheckBox*>()) {
            if (c->text().contains(QStringLiteral("Unconfirmed"))) { unconf = c; }
        }
        QVERIFY(unconf);
        int nUnconf = 0;
        for (const LogEntry& e : all) { if (!QsoConfirmation::isConfirmed(e)) { ++nUnconf; } }
        unconf->setChecked(true);
        QCOMPARE(table->rowCount(), nUnconf);
        qInfo().noquote() << QStringLiteral("Filter: '%1' %2, 20m %3, 20m+%9 %4, JN67 %5, austria %6, unbestaetigt %7 von %8")
                                 .arg(term).arg(expect).arg(n20).arg(n20ft8).arg(nJn67).arg(nAt).arg(nUnconf).arg(all.size()).arg(m20);

        // Datum: ein Jahr, eigene Zaehlung.
        QCheckBox* dates = nullptr;
        for (QCheckBox* c : w.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Dates")) { dates = c; }
        }
        QVERIFY(dates);
        // Die beiden Datumsfelder neben dem Haken (die Eingabezeile hat eigene).
        const QList<QDateEdit*> de = dates->parentWidget()->findChildren<QDateEdit*>(Qt::FindDirectChildrenOnly);
        QCOMPARE(de.size(), 2);
        unconf->setChecked(false);
        de.at(0)->setDate(QDate(2025, 1, 1));
        de.at(1)->setDate(QDate(2025, 12, 31));
        dates->setChecked(true);
        int n2025 = 0;
        for (const LogEntry& e : all) {
            const QDate d = e.timeOn.toUTC().date();
            if (d.isValid() && d >= QDate(2025, 1, 1) && d <= QDate(2025, 12, 31)) { ++n2025; }
        }
        QCOMPARE(table->rowCount(), n2025);
        unconf->setChecked(true);
        clear->click();
        QVERIFY(!unconf->isChecked());
        QCOMPARE(table->rowCount(), all.size());
    }

    // Tippen in die Suche (und in die Eingabezeile, die sie mitfuehrt):
    // jede Taste filtert das ganze Log. Darf nicht ruckeln.
    void typingStaysFast()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const QString call = QStringLiteral("OE5VVM");
        qint64 worst = 0, sum = 0;
        for (int i = 1; i <= call.size(); ++i) {
            QElapsedTimer t; t.start();
            w.searchForTest()->setText(call.left(i));
            QCoreApplication::processEvents();
            const qint64 ms = t.elapsed();
            worst = qMax(worst, ms); sum += ms;
        }
        w.searchForTest()->clear();
        QElapsedTimer t; t.start();
        QCoreApplication::processEvents();
        qInfo().noquote() << QStringLiteral("Tippen: schlechteste Taste %1 ms, Mittel %2 ms (%3 QSOs)")
                                 .arg(worst).arg(sum / call.size()).arg(w.entryCountForTesting());
        // Grenze fuer den Test: spuerbar trage ab ~150 ms je Taste.
        QVERIFY2(worst < 400, qPrintable(QStringLiteral("%1 ms je Taste").arg(worst)));
    }

    // Sortieren nach jeder Spalte: nichts faellt aus, die Zeilenzahl bleibt.
    void sortingByEveryColumn()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* table = logTable(&w);
        const int rows = table->rowCount();
        for (int c = 0; c < table->columnCount(); ++c) {
            for (int twice = 0; twice < 2; ++twice) {
                QElapsedTimer t; t.start();
                emit table->horizontalHeader()->sectionClicked(c);
                QCoreApplication::processEvents();
                if (t.elapsed() > 300) {
                    qInfo().noquote() << "Sortieren Spalte" << c << t.elapsed() << "ms";
                }
                QCOMPARE(table->rowCount(), rows);
            }
        }
    }

    // Bearbeiten: nur das geaenderte Feld aendert sich, im Speicher und in der Datei.
    void editingChangesOnlyThatField()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* table = logTable(&w);
        QVERIFY(table->rowCount() > 10);
        auto readFile = [this]() { QFile f(m_log); f.open(QIODevice::ReadOnly); return f.readAll(); };
        auto flat = [](QMap<QByteArray, QByteArray> m, bool dropComment) {
            if (dropComment) { m.remove("COMMENT"); }
            QByteArray out;
            for (auto it = m.cbegin(); it != m.cend(); ++it) { out += it.key() + '=' + it.value() + '\x1f'; }
            return out;
        };

        // Oben, Mitte, unten: im echten Log sehr verschiedene Datensaetze
        // (eigene, importierte, aus QRZ geholte).
        QList<int> rows = {2, table->rowCount() / 2, table->rowCount() - 3};
        // Dazu eine Zeile mit Locator in gewohnter Schreibweise (JN67vv) --
        // im echten Log 1495 Kontakte, der Dialog darf sie nicht umschreiben.
        QSet<QString> mixedCalls;
        for (const LogEntry& e : AdifLog::read(m_log)) {
            if (e.gridSquare != e.gridSquare.toUpper()) { mixedCalls.insert(e.call); }
        }
        for (int r = 3; r < table->rowCount(); ++r) {
            QTableWidgetItem* call = table->item(r, 2);
            if (call && mixedCalls.contains(call->text())) { rows << r; break; }
        }
        QVERIFY2(rows.size() == 4, "kein Kontakt mit JN67vv-Schreibweise gefunden");
        int n = 0;
        for (int row : rows) {
            const QByteArray comment = "Nachttest " + QByteArray::number(++n);
            table->setCurrentCell(row, 2);
            const QByteArray before = readFile();
            autoAnswerModals(this, [comment](QWidget* m) {
                auto* dlg = qobject_cast<QDialog*>(m);
                if (!dlg) { return; }
                auto* form = qobject_cast<QFormLayout*>(dlg->layout());
                for (QLineEdit* e : dlg->findChildren<QLineEdit*>()) {
                    // Das Kommentarfeld steht in der Formzeile "Comment".
                    auto* l = form ? qobject_cast<QLabel*>(form->labelForField(e)) : nullptr;
                    if (l && l->text() == QStringLiteral("Comment")) { e->setText(QString::fromUtf8(comment)); }
                }
                for (QDialogButtonBox* box : dlg->findChildren<QDialogButtonBox*>()) {
                    if (QPushButton* save = box->button(QDialogButtonBox::Save)) { save->click(); return; }
                }
            });
            buttonWith(&w, QStringLiteral("Edit…"))->click();
            const QList<Record> rb = rawRecords(before);
            const QList<Record> ra = rawRecords(readFile());
            QCOMPARE(ra.size(), rb.size());

            QMap<QByteArray, int> pool;              // vorher, als ganze Datensaetze
            for (const Record& r : rb) { ++pool[flat(asMap(r), false)]; }
            QMap<QByteArray, int> poolNoComment;     // vorher, ohne Kommentar
            for (const Record& r : rb) { ++poolNoComment[flat(asMap(r), true)]; }
            int edited = 0, others = 0;
            QStringList diff;
            for (const Record& r : ra) {
                const auto m = asMap(r);
                if (m.value("COMMENT") == comment) {
                    ++edited;
                    const QByteArray key = flat(m, true);
                    if (!poolNoComment.contains(key)) {
                        // Welches Feld hat sich mitveraendert? Gegen den
                        // aehnlichsten Datensatz von vorher (derselbe Kontakt).
                        QStringList best;
                        for (const Record& o : rb) {
                            const auto om = asMap(o);
                            if (om.value("CALL") != m.value("CALL")) { continue; }
                            QStringList d;
                            for (auto it = m.cbegin(); it != m.cend(); ++it) {
                                if (it.key() != "COMMENT" && om.value(it.key()) != it.value()) {
                                    d << QStringLiteral("%1: '%2' -> '%3'").arg(QString::fromUtf8(it.key()),
                                             QString::fromUtf8(om.value(it.key())), QString::fromUtf8(it.value()));
                                }
                            }
                            for (auto it = om.cbegin(); it != om.cend(); ++it) {
                                if (it.key() != "COMMENT" && !m.contains(it.key())) {
                                    d << QStringLiteral("%1 verloren").arg(QString::fromUtf8(it.key()));
                                }
                            }
                            if (best.isEmpty() || d.size() < best.size()) { best = d; }
                        }
                        diff << (best.isEmpty() ? QStringList{QStringLiteral("unbekannt")} : best);
                    }
                } else if (pool.value(flat(m, false)) > 0) {
                    --pool[flat(m, false)];
                } else {
                    ++others;
                }
            }
            qInfo().noquote() << QStringLiteral("Zeile %1 bearbeitet: Kommentar %2x, andere veraendert %3, am Kontakt mitveraendert: %4")
                                     .arg(row).arg(edited).arg(others).arg(diff.isEmpty() ? QStringLiteral("nichts") : diff.join(QStringLiteral("; ")));
            QCOMPARE(edited, 1);
            QCOMPARE(others, 0);
            QVERIFY2(diff.isEmpty(), qPrintable(diff.join(QStringLiteral("; "))));
        }
    }

    // Gegenprobe: ein Feld, das man aendert, wird weiterhin in Form
    // gebracht (Locator gross, Rufzeichen gross).
    void editingAFieldStillTidiesIt()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* table = logTable(&w);
        table->setCurrentCell(4, 2);
        const QString call = table->item(4, 2)->text();
        autoAnswerModals(this, [](QWidget* m) {
            auto* dlg = qobject_cast<QDialog*>(m);
            if (!dlg) { return; }
            auto* form = qobject_cast<QFormLayout*>(dlg->layout());
            for (QLineEdit* e : dlg->findChildren<QLineEdit*>()) {
                auto* l = form ? qobject_cast<QLabel*>(form->labelForField(e)) : nullptr;
                if (l && l->text() == QStringLiteral("Their grid")) { e->setText(QStringLiteral(" jn48ab ")); }
            }
            for (QDialogButtonBox* box : dlg->findChildren<QDialogButtonBox*>()) {
                if (QPushButton* save = box->button(QDialogButtonBox::Save)) { save->click(); return; }
            }
        });
        buttonWith(&w, QStringLiteral("Edit…"))->click();
        int hits = 0;
        for (const LogEntry& e : AdifLog::read(m_log)) {
            if (e.call == call && e.gridSquare == QStringLiteral("JN48AB")) { ++hits; }
        }
        QCOMPARE(hits, 1);
    }

    // Loeschen: genau einer weniger, nach Rueckfrage; die Datei auch.
    void deletingRemovesExactlyOne()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* table = logTable(&w);
        const int before = w.entryCountForTesting();
        table->setCurrentCell(5, 2);
        table->selectRow(5);
        autoAnswerModals(this, [](QWidget* m) {
            if (auto* box = qobject_cast<QMessageBox*>(m)) {
                box->button(QMessageBox::Yes)->click();
            }
        });
        buttonWith(&w, QStringLiteral("Delete"))->click();
        QCOMPARE(w.entryCountForTesting(), before - 1);
        QCOMPARE(AdifLog::read(m_log).size(), before - 1);
    }

    // Import: Neues kommt dazu, schon Vorhandenes nicht doppelt.
    void importAddsOnlyWhatIsNew()
    {
        const QVector<LogEntry> all = AdifLog::read(m_log);
        QVERIFY(!all.isEmpty());
        const QString file = m_dir.filePath(QStringLiteral("import.adi"));
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("import\n<EOH>\n");
        f.write("<CALL:6>ZZ9NT1 <QSO_DATE:8>20260926 <TIME_ON:6>221500 <BAND:3>20m <MODE:3>SSB <EOR>\n");
        f.write("<CALL:6>ZZ9NT2 <QSO_DATE:8>20260926 <TIME_ON:6>221600 <BAND:3>40m <MODE:2>CW <EOR>\n");
        f.write(all.first().toAdifRecord().toUtf8() + "\n");   // schon da
        f.close();
        LogbookWindow w(m_log);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const int before = w.entryCountForTesting();
        QString told;
        w.setOperatorHooks([](const QString&) { return true; },
                                    [&told](const QString& m) { told = m; });
        w.importAdifFile(file);
        QCoreApplication::processEvents();
        qInfo().noquote() << "Import-Meldung:" << told.simplified();
        QCOMPARE(w.entryCountForTesting(), before + 2);
        QCOMPARE(AdifLog::read(m_log).size(), before + 2);
        // Ein zweites Mal: nichts Neues mehr.
        w.importAdifFile(file);
        QCOMPARE(w.entryCountForTesting(), before + 2);
    }

    // Karte und Kennzahlen ein und aus, das Rotor-Radar dabei.
    void mapAndStatsToggle()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("True"));
        AppSettings::instance().setValue(QStringLiteral("LogbookShowStats"), QStringLiteral("True"));
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.mapPanelForTest());
        QVERIFY(w.mapPanelForTest()->isVisible());
        QVERIFY(w.statsSectionForTest()->isVisible());
        w.setRotorBearing(120.0);
        QTest::qWait(100);
        grab(&w, QStringLiteral("03-karte-statistik-rotor"));
        QTableWidget* table = logTable(&w);
        QTest::qWait(100);
        const QSize withBoth = table->size();
        qInfo().noquote() << "Karte: Mindestbreite" << w.mapPanelForTest()->minimumSizeHint().width()
                          << "px, Fenster" << w.width() << "px";
        w.mapToggleForTest()->click();
        QVERIFY(!w.mapPanelForTest()->isVisible());
        w.statsToggleForTest()->click();
        QVERIFY(!w.statsSectionForTest()->isVisible());
        QTest::qWait(100);
        const QSize without = table->size();
        qInfo().noquote() << "Tabelle mit Karte+Statistik" << withBoth << "ohne" << without;
        grab(&w, QStringLiteral("04-ohne-karte-statistik"));
        // Was Karte und Kennzahlen freigeben, bekommt die Tabelle.
        QVERIFY2(without.width() > withBoth.width(), "Tabelle wird ohne Karte nicht breiter");
        QVERIFY2(without.height() > withBoth.height(), "Tabelle wird ohne Kennzahlen nicht hoeher");
        w.mapToggleForTest()->click();
        w.statsToggleForTest()->click();
        QVERIFY(w.mapPanelForTest()->isVisible());
        QVERIFY(w.statsSectionForTest()->isVisible());
    }

    // Exporte: was gefiltert zu sehen ist, geht hinaus -- beim ADIF jedes
    // Feld jedes Kontakts, auch was Longpath nicht kennt.
    void exportsCarryWhatIsShown()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* table = logTable(&w);
        QComboBox* band = comboWith(&w, QStringLiteral("20m"));
        QVERIFY(band);
        band->setCurrentIndex(band->findText(QStringLiteral("20m"), Qt::MatchFixedString));
        const int shown = table->rowCount();
        QVERIFY(shown > 0);

        QStringList told;
        auto exportTo = [&](const QString& button, const QString& file) {
            const QString wanted = m_dir.filePath(file);
            auto chosen = std::make_shared<QString>();
            QTimer* t = answerModals(this, [&told, wanted, chosen](QWidget* m) {
                if (auto* fd = qobject_cast<QFileDialog*>(m)) {
                    fd->selectFile(wanted);
                    // Der Qt-Dialog setzt damit den Ordner; den Namen
                    // behaelt er vom Vorschlag -- also gilt, was er meldet.
                    *chosen = fd->selectedFiles().value(0);
                    QMetaObject::invokeMethod(fd, "done", Qt::DirectConnection,
                                              Q_ARG(int, QDialog::Accepted));
                } else if (auto* box = qobject_cast<QMessageBox*>(m)) {
                    told << box->text();
                    box->accept();
                }
            });
            QPushButton* b = buttonWith(&w, button);
            if (!b) { qWarning() << "kein Knopf" << button; delete t; return QString(); }
            b->click();
            delete t;
            return *chosen;
        };

        // ADIF
        const QString adi = exportTo(QStringLiteral("Export ADIF…"), QStringLiteral("export.adi"));
        QVERIFY2(QFile::exists(adi), "keine ADIF-Datei");
        const QList<Record> out = rawRecords([&]() { QFile f(adi); f.open(QIODevice::ReadOnly); return f.readAll(); }());
        QCOMPARE(out.size(), shown);
        QMap<QByteArray, int> pool;
        auto flat = [](const Record& r) {
            QByteArray o; const auto m = asMap(r);
            for (auto it = m.cbegin(); it != m.cend(); ++it) { o += it.key() + '=' + it.value() + '\x1f'; }
            return o;
        };
        // Gegen den Stand der Datei jetzt (fruehere Schritte haben
        // bearbeitet, geloescht, eingelesen).
        const QByteArray now = [this]() { QFile f(m_log); f.open(QIODevice::ReadOnly); return f.readAll(); }();
        for (const Record& r : rawRecords(now)) { ++pool[flat(r)]; }
        int notVerbatim = 0;
        for (const Record& r : out) {
            if (pool.value(flat(r)) > 0) { --pool[flat(r)]; } else { ++notVerbatim; }
            if (asMap(r).value("BAND").toLower() != "20m") { ++notVerbatim; }
        }
        QCOMPARE(notVerbatim, 0);

        // CSV: Kopfzeile + eine Zeile je Kontakt.
        const QString csv = exportTo(QStringLiteral("Export CSV…"), QStringLiteral("export.csv"));
        QFile cf(csv);
        QVERIFY(cf.open(QIODevice::ReadOnly));
        const QByteArray csvBytes = cf.readAll();
        // Datensaetze nach RFC 4180 zaehlen: ein Zeilenende zaehlt nur
        // ausserhalb von Anfuehrungszeichen (ein Kommentar darf einen
        // Umbruch enthalten, gequotet ist das gueltig).
        int csvRecords = 0, rawLines = 0;
        bool inQuotes = false, lineHasText = false;
        for (const char ch : csvBytes) {
            if (ch == '"') { inQuotes = !inQuotes; }
            if (ch == '\n') {
                ++rawLines;
                if (!inQuotes && lineHasText) { ++csvRecords; lineHasText = false; }
                continue;
            }
            if (ch != '\r') { lineHasText = true; }
        }
        if (lineHasText) { ++csvRecords; }
        QVERIFY2(!inQuotes, "CSV endet in einem offenen Anfuehrungszeichen");
        qInfo().noquote() << QStringLiteral("CSV %1 Datensaetze (%2 Zeilen) fuer %3 Kontakte")
                                 .arg(csvRecords).arg(rawLines).arg(shown);
        QCOMPARE(csvRecords, shown + 1);

        // Cabrillo: eine QSO:-Zeile je Kontakt, zwischen START- und END-OF-LOG.
        const QString cbr = exportTo(QStringLiteral("Cabrillo…"), QStringLiteral("export.cbr"));
        QFile bf(cbr);
        QVERIFY(bf.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(bf.readAll());
        QCOMPARE(text.count(QStringLiteral("\nQSO:")), shown);
        QVERIFY(text.startsWith(QStringLiteral("START-OF-LOG")));
        QVERIFY(text.trimmed().endsWith(QStringLiteral("END-OF-LOG:")));
        qInfo().noquote() << "Meldungen:" << told.join(QStringLiteral(" | ")).simplified();
    }

    // Sortieren: die Reihenfolge stimmt wirklich (Datum+Zeit, Rufzeichen,
    // Frequenz als Zahl), in beide Richtungen.
    void sortingPutsRowsInOrder()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* t = logTable(&w);
        QHeaderView* h = t->horizontalHeader();
        auto colText = [t](int row, int col) { QTableWidgetItem* it = t->item(row, col); return it ? it->text() : QString(); };
        auto checkOrder = [&](int col, bool numeric, const char* what) {
            for (int pass = 0; pass < 2; ++pass) {
                emit h->sectionClicked(col);
                const bool asc = h->sortIndicatorOrder() == Qt::AscendingOrder;
                int bad = 0;
                QString prev;
                for (int r = 0; r < t->rowCount(); ++r) {
                    QString cur = colText(r, col);
                    if (col == 0) { cur += QLatin1Char(' ') + colText(r, 1); }   // Datum + Zeit
                    if (cur.isEmpty() || r == 0) { if (!cur.isEmpty()) { prev = cur; } continue; }
                    int cmp = numeric ? (cur.toDouble() < prev.toDouble() ? -1 : cur.toDouble() > prev.toDouble() ? 1 : 0)
                                      : QString::compare(cur, prev, Qt::CaseInsensitive);
                    if ((asc && cmp < 0) || (!asc && cmp > 0)) { ++bad; }
                    prev = cur;
                }
                qInfo().noquote() << QStringLiteral("Sortieren %1 %2: %3 Zeilen ausser der Reihe")
                                         .arg(QLatin1String(what), asc ? QStringLiteral("auf") : QStringLiteral("ab")).arg(bad);
                QCOMPARE(bad, 0);
            }
        };
        checkOrder(0, false, "Datum");
        checkOrder(2, false, "Rufzeichen");
        checkOrder(3, true, "Frequenz");
    }

    // Mehrere auf einmal loeschen: genau diese, sonst nichts.
    void deletingSeveralRemovesExactlyThose()
    {
        LogbookWindow w(m_log);
        w.resize(1600, 950);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTableWidget* t = logTable(&w);
        const int before = w.entryCountForTesting();
        QSet<QString> gone;
        t->clearSelection();
        for (int r : {1, 3, 7}) {
            t->selectionModel()->select(t->model()->index(r, 0),
                                        QItemSelectionModel::Select | QItemSelectionModel::Rows);
            gone.insert(t->item(r, 0)->text() + t->item(r, 1)->text() + t->item(r, 2)->text());
        }
        QString asked;
        QTimer* timer = answerModals(this, [&asked](QWidget* m) {
            if (auto* box = qobject_cast<QMessageBox*>(m)) { asked = box->text(); box->button(QMessageBox::Yes)->click(); }
        });
        buttonWith(&w, QStringLiteral("Delete"))->click();
        delete timer;
        qInfo().noquote() << "Rueckfrage:" << asked;
        QCOMPARE(asked, QStringLiteral("Delete 3 contacts?"));
        QCOMPARE(w.entryCountForTesting(), before - 3);
        QCOMPARE(AdifLog::read(m_log).size(), before - 3);
        int stillThere = 0;
        for (int r = 0; r < t->rowCount(); ++r) {
            if (gone.contains(t->item(r, 0)->text() + t->item(r, 1)->text() + t->item(r, 2)->text())) { ++stillThere; }
        }
        QCOMPARE(stillThere, 0);
    }

    // Ein QSO aus WSJT-X, waehrend das Logbuch offen ist, danach eine
    // Korrektur im Logbuch: das WSJT-X-QSO muss in der Datei bleiben.
    // saveAll() schreibt den Stand des Fensters -- war der alt, loeschte
    // die Korrektur das neue QSO (Nachttest 2026-09-26).
    void aWsjtxContactSurvivesAnEditInTheOpenLogbook()
    {
        const QString path = RotorLogbookPanel::logbookPath();
        QVERIFY2(!path.startsWith(QDir::homePath() + QStringLiteral("/Library/Preferences/Longpath/"))
                 && !path.startsWith(QDir::homePath() + QStringLiteral("/.config/Longpath/")),
                 qPrintable(QStringLiteral("Testpfad zeigt auf echte Daten: ") + path));
        QFile::remove(path);
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("t\n<EOH>\n"
                    "<CALL:5>K1ABC <QSO_DATE:8>20260920 <TIME_ON:6>101500 <BAND:3>20m <MODE:3>SSB <EOR>\n"
                    "<CALL:5>G4ABC <QSO_DATE:8>20260921 <TIME_ON:6>111500 <BAND:3>40m <MODE:2>CW <EOR>\n");
        }
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        panel.showLogbook();
        LogbookWindow* w = panel.findChild<LogbookWindow*>();
        QVERIFY(w);
        QVERIFY(QTest::qWaitForWindowExposed(w));
        QCOMPARE(w->entryCountForTesting(), 2);

        LogEntry ft8;
        ft8.call = QStringLiteral("JA1FT8");
        ft8.timeOn = QDateTime(QDate(2026, 9, 26), QTime(22, 40, 15), QTimeZone::UTC);
        ft8.band = QStringLiteral("20m");
        ft8.mode = QStringLiteral("FT8");
        ft8.freqMHz = 14.074;
        panel.logExternalQso(ft8);
        QCoreApplication::processEvents();
        qInfo() << "Logbuch nach WSJT-X-QSO:" << w->entryCountForTesting() << "Kontakte";

        // Korrektur an einem alten Kontakt.
        QTableWidget* table = logTable(w);
        int row = -1;
        for (int r = 0; r < table->rowCount(); ++r) {
            if (table->item(r, 2) && table->item(r, 2)->text() == QStringLiteral("G4ABC")) { row = r; }
        }
        QVERIFY(row >= 0);
        table->setCurrentCell(row, 2);
        autoAnswerModals(this, [](QWidget* m) {
            auto* dlg = qobject_cast<QDialog*>(m);
            if (!dlg) { return; }
            auto* form = qobject_cast<QFormLayout*>(dlg->layout());
            for (QLineEdit* e : dlg->findChildren<QLineEdit*>()) {
                auto* l = form ? qobject_cast<QLabel*>(form->labelForField(e)) : nullptr;
                if (l && l->text() == QStringLiteral("Comment")) { e->setText(QStringLiteral("korrigiert")); }
            }
            for (QDialogButtonBox* box : dlg->findChildren<QDialogButtonBox*>()) {
                if (QPushButton* save = box->button(QDialogButtonBox::Save)) { save->click(); return; }
            }
        });
        buttonWith(w, QStringLiteral("Edit…"))->click();

        QStringList calls;
        for (const LogEntry& e : AdifLog::read(path)) { calls << e.call; }
        qInfo() << "Datei nach der Korrektur:" << calls;
        QVERIFY2(calls.contains(QStringLiteral("JA1FT8")), "WSJT-X-QSO durch die Korrektur geloescht");
        QCOMPARE(calls.size(), 3);
        w->hide();
        QFile::remove(path);
    }

    // Dasselbe, aber das WSJT-X-QSO kommt, WAEHREND der Bearbeiten-Dialog
    // bzw. die Loesch-Rueckfrage offen ist: das Logbuch liest neu ein und
    // sortiert um -- bearbeitet/geloescht werden muss trotzdem der
    // Kontakt, der gewaehlt war.
    void aWsjtxContactArrivingDuringADialogChangesNothingElse()
    {
        const QString path = RotorLogbookPanel::logbookPath();
        QVERIFY2(!path.startsWith(QDir::homePath() + QStringLiteral("/Library/Preferences/Longpath/"))
                 && !path.startsWith(QDir::homePath() + QStringLiteral("/.config/Longpath/")),
                 qPrintable(QStringLiteral("Testpfad zeigt auf echte Daten: ") + path));
        auto writeLog = [&path]() {
            QFile::remove(path);
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write("t\n<EOH>\n"
                    "<CALL:5>K1ABC <QSO_DATE:8>20260920 <TIME_ON:6>101500 <BAND:3>20m <MODE:3>SSB <EOR>\n"
                    "<CALL:5>G4ABC <QSO_DATE:8>20260921 <TIME_ON:6>111500 <BAND:3>40m <MODE:2>CW <EOR>\n");
        };
        auto ft8 = [](const QString& call, int minute) {
            LogEntry e;
            e.call = call;
            e.timeOn = QDateTime(QDate(2026, 9, 26), QTime(22, minute, 15), QTimeZone::UTC);
            e.band = QStringLiteral("20m");
            e.mode = QStringLiteral("FT8");
            return e;
        };
        auto rowOf = [](QTableWidget* t, const QString& call) {
            for (int r = 0; r < t->rowCount(); ++r) {
                if (t->item(r, 2) && t->item(r, 2)->text() == call) { return r; }
            }
            return -1;
        };

        // Bearbeiten
        writeLog();
        {
            RotorLogbookPanel panel(nullptr, nullptr, nullptr);
            panel.showLogbook();
            LogbookWindow* w = panel.findChild<LogbookWindow*>();
            QVERIFY(QTest::qWaitForWindowExposed(w));
            QTableWidget* t = logTable(w);
            t->setCurrentCell(rowOf(t, QStringLiteral("G4ABC")), 2);
            RotorLogbookPanel* pp = &panel;
            autoAnswerModals(this, [pp, ft8](QWidget* m) {
                auto* dlg = qobject_cast<QDialog*>(m);
                if (!dlg) { return; }
                pp->logExternalQso(ft8(QStringLiteral("JA1FT8"), 40));   // mitten im Dialog
                auto* form = qobject_cast<QFormLayout*>(dlg->layout());
                for (QLineEdit* e : dlg->findChildren<QLineEdit*>()) {
                    auto* l = form ? qobject_cast<QLabel*>(form->labelForField(e)) : nullptr;
                    if (l && l->text() == QStringLiteral("Comment")) { e->setText(QStringLiteral("korrigiert")); }
                }
                for (QDialogButtonBox* box : dlg->findChildren<QDialogButtonBox*>()) {
                    if (QPushButton* save = box->button(QDialogButtonBox::Save)) { save->click(); return; }
                }
            });
            buttonWith(w, QStringLiteral("Edit…"))->click();
            QMultiMap<QString, QString> byCall;
            for (const LogEntry& e : AdifLog::read(path)) { byCall.insert(e.call, e.comment); }
            qInfo() << "Bearbeiten mit WSJT-X dazwischen:" << byCall;
            QCOMPARE(byCall.size(), 3);
            QCOMPARE(byCall.value(QStringLiteral("G4ABC")), QStringLiteral("korrigiert"));
            QVERIFY(byCall.contains(QStringLiteral("JA1FT8")));
            QCOMPARE(byCall.value(QStringLiteral("JA1FT8")), QString());
            w->hide();
        }

        // Loeschen
        writeLog();
        {
            RotorLogbookPanel panel(nullptr, nullptr, nullptr);
            panel.showLogbook();
            LogbookWindow* w = panel.findChild<LogbookWindow*>();
            QVERIFY(QTest::qWaitForWindowExposed(w));
            QTableWidget* t = logTable(w);
            t->clearSelection();
            t->selectRow(rowOf(t, QStringLiteral("G4ABC")));
            RotorLogbookPanel* pp = &panel;
            autoAnswerModals(this, [pp, ft8](QWidget* m) {
                auto* box = qobject_cast<QMessageBox*>(m);
                if (!box) { return; }
                pp->logExternalQso(ft8(QStringLiteral("VK2FT8"), 41));   // mitten in der Rueckfrage
                box->button(QMessageBox::Yes)->click();
            });
            buttonWith(w, QStringLiteral("Delete"))->click();
            QStringList calls;
            for (const LogEntry& e : AdifLog::read(path)) { calls << e.call; }
            qInfo() << "Loeschen mit WSJT-X dazwischen:" << calls;
            QCOMPARE(calls.size(), 2);
            QVERIFY(calls.contains(QStringLiteral("K1ABC")));
            QVERIFY(calls.contains(QStringLiteral("VK2FT8")));
            QVERIFY(!calls.contains(QStringLiteral("G4ABC")));
            w->hide();
        }
        QFile::remove(path);
    }
};

QTEST_MAIN(TstLogbookDurchgang)
#include "tst_logbook_durchgang.moc"
