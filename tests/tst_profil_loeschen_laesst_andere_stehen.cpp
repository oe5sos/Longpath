// =================================================================
// tests/tst_profil_loeschen_laesst_andere_stehen.cpp  (Longpath)
// =================================================================
//
// Ein Profil zu loeschen darf ein ANDERES Profil nicht veraendern.
//
// Martin am 2026-11-12, nachdem er „New" und „CONTEST" geloescht hatte
// und Profil N danach verstellt war: „wenn ich was lösche kann sich das
// andere profil nicht ändern" -- und davor „zum hundersten mal!!!!!!!!!".
// Im Protokoll: CONTEST war aktiv, wurde geloescht, LayoutProfiles::
// remove() stellte N her, und beim naechsten Wechsel wurde N mit
// geschrumpftem Kombi-Fenster (125x93), geschrumpftem SWR (157x93) und
// ohne schwebendes Frequency-Fenster gesichert.
//
// Der Prüfstand nimmt Martins Profilsatz (tests/data), stellt N her,
// merkt sich, wie N aufgenommen aussieht, und vergleicht damit:
//   1. Umschalten CONTEST -> N (der normale Weg)
//   2. CONTEST aktiv, CONTEST loeschen (der Weg vom 12.11.)
// Verglichen wird N gegen N, nicht gegen die Zahlen in der Datei: der
// Prüfschirm ist ein anderer als Martins, und das Profil rechnet seine
// Lagen auf den heutigen Hauptrahmen um.
//
// ACHTUNG: offscreen (CI) bleibt dieser Prüfstand auch OHNE die
// Behebung grün. Der Fehler sitzt im nativen Fenster: beim ersten
// show() meldet macOS 164x30 zurueck. Rot war er nur mit
//     QT_QPA_PLATFORM=cocoa LONGPATH_SOURCE_DIR=<quelle> \
//         ./build/tests/tst_profil_loeschen_laesst_andere_stehen
// (am 12.11. gemessen: alle drei Faelle rot vorher, gruen nachher).
// Mit LONGPATH_DIAGNOSE=1 schreibt diagnoseGroessen() jede
// Groessenaenderung von Frequency, SWR und Combined mit.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-12 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <cstdio>
#include <cstdlib>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

constexpr int kToleranz = 2;

QString lageText(const QVariantMap& m)
{
    return QStringLiteral("(%1,%2 %3x%4)")
        .arg(m.value(QStringLiteral("x")).toInt())
        .arg(m.value(QStringLiteral("y")).toInt())
        .arg(m.value(QStringLiteral("w")).toInt())
        .arg(m.value(QStringLiteral("h")).toInt());
}

// Unterschiede der schwebenden Fenster zweier Aufnahmen, lesbar.
QStringList unterschiede(const QVariantMap& soll, const QVariantMap& ist)
{
    QStringList out;
    const QVariantMap a = soll.value(QStringLiteral("floatingApplets")).toMap();
    const QVariantMap b = ist.value(QStringLiteral("floatingApplets")).toMap();
    for (auto it = a.cbegin(); it != a.cend(); ++it) {
        if (!b.contains(it.key())) {
            out << QStringLiteral("%1 schwebt nicht mehr (war %2)")
                       .arg(it.key(), lageText(it.value().toMap()));
            continue;
        }
        const QVariantMap x = it.value().toMap();
        const QVariantMap y = b.value(it.key()).toMap();
        for (const char* k : {"x", "y", "w", "h"}) {
            const QString key = QString::fromLatin1(k);
            if (qAbs(x.value(key).toInt() - y.value(key).toInt()) > kToleranz) {
                out << QStringLiteral("%1 %2 -> %3")
                           .arg(it.key(), lageText(x), lageText(y));
                break;
            }
        }
    }
    for (auto it = b.cbegin(); it != b.cend(); ++it) {
        if (!a.contains(it.key())) {
            out << QStringLiteral("%1 schwebt neu %2")
                       .arg(it.key(), lageText(it.value().toMap()));
        }
    }
    for (const char* k : {"kombiInhalt", "kombiInhalt2"}) {
        const QString key = QString::fromLatin1(k);
        if (soll.value(key).toStringList() != ist.value(key).toStringList()) {
            out << QStringLiteral("%1: %2 -> %3").arg(key,
                       soll.value(key).toStringList().join(QLatin1Char(',')),
                       ist.value(key).toStringList().join(QLatin1Char(',')));
        }
    }
    return out;
}

} // namespace

class TstProfilLoeschenLaesstAndereStehen : public QObject {
    Q_OBJECT

private:
    MainWindow*     m_mw{nullptr};
    LayoutProfiles* m_profile{nullptr};
    QVariantMap     m_nNachStart;

    // Bis sich alles gesetzt hat: Raster (180 ms), Profilrahmen-
    // Nachlauf (bis 3000 ms), Kombi-Fuellung.
    static void warten() { QTest::qWait(3500); }

    QVariantMap nimmNAuf()
    {
        m_profile->captureIntoCurrent();
        return m_profile->snapshot(QStringLiteral("Neu"));
    }

private slots:
    void initTestCase()
    {
        QFile f(QStringLiteral(LONGPATH_TEST_DATA_DIR)
                + QStringLiteral("/layoutprofile_neu_contest_2026-11-12.json"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "Profilsatz fehlt in tests/data");
        const QByteArray json = f.readAll();
        QVERIFY(QJsonDocument::fromJson(json).isObject());
        AppSettings::instance().setValue(LayoutProfiles::settingsKey(),
                                         QString::fromUtf8(json));

        // Das Contestprogramm fragt im leeren Pruefordner nach seinen
        // Einstellungen (modaler Dialog, exec) -- hier niemand, der
        // antwortet. Jeden modalen Dialog sofort wieder zumachen.
        auto* zu = new QTimer(this);
        connect(zu, &QTimer::timeout, this, []() {
            if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                d->reject();
            }
        });
        zu->start(100);

        m_mw = new MainWindow();
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        warten();

        m_profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(m_profile);
        QCOMPARE(m_profile->current(), QStringLiteral("Neu"));
        QVERIFY(m_profile->exists(QStringLiteral("CONTEST")));

        m_nNachStart = nimmNAuf();
        const QVariantMap fa =
            m_nNachStart.value(QStringLiteral("floatingApplets")).toMap();
        QVERIFY2(fa.size() >= 5,
                 qPrintable(QStringLiteral("Voraussetzung: N hat seine schwebenden "
                                           "Fenster, gefunden: %1")
                                .arg(fa.keys().join(QLatin1Char(',')))));
    }

    // 1. Der normale Weg: CONTEST und zurueck.
    void umschaltenUeberContestLaesstNStehen()
    {
        QVERIFY(m_profile->activate(QStringLiteral("CONTEST")));
        warten();
        QVERIFY(m_profile->activate(QStringLiteral("Neu")));
        warten();
        const QStringList u = unterschiede(m_nNachStart, nimmNAuf());
        QVERIFY2(u.isEmpty(), qPrintable(u.join(QStringLiteral("; "))));
    }

    // 2. Der Weg vom 12.11.: CONTEST ist aktiv und wird geloescht.
    void contestLoeschenLaesstNStehen()
    {
        QVERIFY(m_profile->activate(QStringLiteral("CONTEST")));
        warten();
        QVERIFY(m_profile->remove(QStringLiteral("CONTEST")));
        QCOMPARE(m_profile->current(), QStringLiteral("Neu"));
        warten();
        const QStringList u = unterschiede(m_nNachStart, nimmNAuf());
        QVERIFY2(u.isEmpty(), qPrintable(u.join(QStringLiteral("; "))));
    }

    // Nur zur Diagnose (QT_QPA_PLATFORM=cocoa): wer verkleinert?
    void diagnoseGroessen()
    {
        if (qEnvironmentVariableIsEmpty("LONGPATH_DIAGNOSE")) { QSKIP("nur Diagnose"); }
        QVERIFY(m_profile->activate(QStringLiteral("CONTEST")));
        warten();
        struct Rec : QObject {
            QElapsedTimer uhr; QStringList log;
            bool eventFilter(QObject* o, QEvent* e) override {
                auto* w = qobject_cast<QWidget*>(o);
                if (w && w->isWindow() && (e->type() == QEvent::Resize || e->type() == QEvent::Show)) {
                    const QString t = w->windowTitle();
                    if (t.contains(QStringLiteral("Freq")) || t.contains(QStringLiteral("SWR"))
                        || t.contains(QStringLiteral("Combined")) || t.contains(QStringLiteral("Kombi"))) {
                        log << QStringLiteral("%1 ms %2 %3 %4x%5 spontan=%6")
                                   .arg(uhr.elapsed()).arg(t)
                                   .arg(e->type() == QEvent::Show ? QStringLiteral("SHOW") : QStringLiteral("RESIZE"))
                                   .arg(w->width()).arg(w->height()).arg(e->spontaneous());
                    }
                }
                return false;
            }
        } rec;
        rec.uhr.start();
        qApp->installEventFilter(&rec);
        QVERIFY(m_profile->activate(QStringLiteral("Neu")));
        rec.log << QStringLiteral("%1 ms -- activate fertig").arg(rec.uhr.elapsed());
        warten();
        qApp->removeEventFilter(&rec);
        for (const QString& l : rec.log) { qWarning().noquote() << "[DIAG]" << l; }
    }

    // 3. Martins Folge vom 12.11., 15:07-15:12, mit seinen Pausen
    //    (Protokoll longpath-20261112-145557.log). N wird nach jedem
    //    Schritt aufgenommen, in dem es aktiv ist.
    void martinsFolgeVom12November()
    {
        // Frischer Satz: Fall 2 hat CONTEST schon geloescht.
        QFile f(QStringLiteral(LONGPATH_TEST_DATA_DIR)
                + QStringLiteral("/layoutprofile_neu_contest_2026-11-12.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue& v : o.value(QStringLiteral("profiles")).toArray()) {
            const QJsonObject p = v.toObject();
            const QString name = p.value(QStringLiteral("name")).toString();
            if (name == QStringLiteral("Neu") || m_profile->exists(name)) { continue; }
            QVERIFY(m_profile->createWith(name,
                        p.value(QStringLiteral("state")).toObject().toVariantMap()));
            QVERIFY(m_profile->activate(QStringLiteral("Neu")));
            QTest::qWait(1500);
        }
        QCOMPARE(m_profile->current(), QStringLiteral("Neu"));
        warten();
        const QVariantMap start = nimmNAuf();

        QStringList befund;
        auto pruefe = [&](const QString& schritt) {
            const QStringList u = unterschiede(start, nimmNAuf());
            if (!u.isEmpty()) {
                befund << schritt + QStringLiteral(": ") + u.join(QStringLiteral("; "));
            }
        };

        QVERIFY(m_profile->activate(QStringLiteral("CONTEST")));   // 15:07:54
        QTest::qWait(1500);
        QVERIFY(m_profile->activate(QStringLiteral("TEST")));      // 15:09:22
        QTest::qWait(1500);
        QVERIFY(m_profile->activate(QStringLiteral("New")));       // 15:11:01
        QTest::qWait(1500);
        QVERIFY(m_profile->remove(QStringLiteral("New")));         // 15:11:07 -> N
        QTest::qWait(2000);
        pruefe(QStringLiteral("nach Loeschen von New"));
        QVERIFY(m_profile->activate(QStringLiteral("FT8")));       // 15:11:09
        QTest::qWait(1500);
        QVERIFY(m_profile->activate(QStringLiteral("CONTEST")));   // 15:11:25
        QTest::qWait(1500);
        QVERIFY(m_profile->remove(QStringLiteral("CONTEST")));     // 15:11:59 -> N
        warten();
        pruefe(QStringLiteral("nach Loeschen von CONTEST"));
        QVERIFY2(befund.isEmpty(), qPrintable(befund.join(QStringLiteral(" || "))));
    }
};

// Kein QTEST_MAIN: das Abraeumen stuerzt ab, und jeder Absturz legt
// Martin eine Meldung auf den Schirm (am 12.11. sechs Stueck). Offscreen
// faellt ~QApplication an einem Contest-Fenster um, das keinem Elternteil
// gehoert (QOffscreenBackingStore::clearHash); ein delete des
// Hauptfensters laeuft offscreen in endlose createWinId-Rekursion und
// unter Cocoa in SIGSEGV. Darum nach dem Lauf sofort hinaus, mit dem
// Ergebnis des Laufs als Rueckgabe.
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstProfilLoeschenLaesstAndereStehen t;
    QTEST_SET_MAIN_SOURCE_PATH
    const int rc = QTest::qExec(&t, argc, argv);
    std::fflush(nullptr);
    std::_Exit(rc);
}
#include "tst_profil_loeschen_laesst_andere_stehen.moc"
