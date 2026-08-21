// SPDX-License-Identifier: GPL-3.0-or-later
//
// Erreichbarkeit im ECHTEN Hauptfenster.
//
// Am 2026-08-21 hat der Betreiber an einem Tag viermal etwas gemeldet,
// das ich fuer erledigt hielt: den Rechtsklick auf den Notch, das
// Verschieben der Einblendungen, den Abloese-Knopf, das Zahnrad. Jedes
// Mal war eine Pruefung gruen und die App kaputt — weil die Pruefung
// auf ein FREISTEHENDES Widget klickte statt auf das im Hauptfenster.
//
// Dieser Durchgang prueft eine Fehlerklasse, die sich mechanisch
// finden laesst: ein Widget, das ein Rechtsklick-Menue ANKUENDIGT
// (Qt::CustomContextMenu), ohne dass jemand darauf hoert. Der
// Rechtsklick tut dann nichts — und nichts im Programm meldet das.
//
// ZUM MITTEL: geprueft wird der QUELLTEXT, nicht das laufende
// Programm. Zwei Wege waren versucht und verworfen:
//
//   - QObject::isSignalConnected — geschuetzt, von aussen nicht
//     erreichbar.
//   - Das Signal wirklich ausloesen und sehen, ob ein Menue kommt —
//     gefaehrlich: viele Zuhoerer oeffnen ihr Menue mit exec(), und
//     das haelt die Runde an. Der Durchgang wuerde haengen.
//
// Quelltext lesen ist gröber, aber es findet genau das, worum es
// geht: eine Ankuendigung ohne Einloesung.

#include <QtTest>
#include <QHBoxLayout>
#include <QLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

#include "gui/MainWindow.h"

using namespace Longpath;
#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QRegularExpression>

class TstReachabilityAudit : public QObject
{
    Q_OBJECT

private slots:
    void everyPromisedContextMenuHasAListener()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }

        QDir gui(QString::fromLocal8Bit(root) + QStringLiteral("/src"));
        QStringList files;
        QDirIterator it(gui.absolutePath(), QStringList{QStringLiteral("*.cpp")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { files << it.next(); }
        QVERIFY2(!files.isEmpty(), "Keine Quelldateien gefunden");

        static const QRegularExpression promise(
            QStringLiteral("setContextMenuPolicy\\s*\\(\\s*Qt::CustomContextMenu"));
        static const QRegularExpression listener(
            QStringLiteral("customContextMenuRequested"));

        QStringList mute;
        int promised = 0;

        for (const QString& path : files) {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            const QString src = QString::fromUtf8(f.readAll());

            const int promises =
                static_cast<int>(src.count(promise));
            if (promises == 0) { continue; }
            promised += promises;

            // Der Zuhoerer darf in derselben Datei stehen — quer
            // ueber Dateien wird hier NICHT verfolgt. Faende sich
            // spaeter so ein Fall, gehoert er in die Ausnahmeliste
            // mit Begruendung, nicht in eine aufgeweichte Regel.
            if (!src.contains(listener)) {
                mute << QDir(QString::fromLocal8Bit(root)).relativeFilePath(path);
            }
        }

        qInfo() << "Ankuendigungen insgesamt:" << promised;
        qInfo() << "Dateien ohne Zuhoerer:" << mute.size();
        for (const QString& m : mute) { qInfo() << "   stumm:" << m; }

        QVERIFY2(promised > 0,
                 "Keine einzige Ankuendigung gefunden — dann prueft "
                 "dieser Durchgang nichts");
        QVERIFY2(mute.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Diese Dateien kuendigen ein Rechtsklick-Menue an, "
                     "ohne darauf zu hoeren — der Rechtsklick tut dort "
                     "nichts:\n  %1").arg(mute.join(QStringLiteral("\n  ")))));
    }

    /// Wer ein Menue im DRUCKEREIGNIS oeffnet, muss auch das
    /// Kontextmenue-Ereignis beantworten.
    ///
    /// Das ist der Fehler vom 2026-08-21, mechanisch gefasst: der
    /// Panadapter oeffnete das Notch-Menue in mousePressEvent und
    /// nahm den Druck an — aber Qt schickt beim Rechtsklick ZWEI
    /// Ereignisse. Das zweite blieb unbeantwortet, wanderte zum
    /// Elternteil, und dessen Menue ging mit exec() auf und legte
    /// sich obenauf. Der Betreiber sah das falsche Menue.
    ///
    /// Wer also im Druckereignis ein Menue aufmacht, uebernimmt damit
    /// den Rechtsklick — und muss das zweite Ereignis abfangen, sonst
    /// beantwortet es jemand anderes.
    void whoeverOpensAMenuOnPressMustAnswerTheContextEvent()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        QStringList files;
        QDirIterator it(QString::fromLocal8Bit(root) + QStringLiteral("/src"),
                        QStringList{QStringLiteral("*.cpp")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { files << it.next(); }

        QStringList offenders;
        int checked = 0;

        for (const QString& path : files) {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            const QString src = QString::fromUtf8(f.readAll());

            const int press = src.indexOf(QStringLiteral("::mousePressEvent("));
            if (press < 0) { continue; }

            // Nur bis zum Ende dieser Funktion schauen: ein QMenu
            // irgendwo sonst in der Datei geht uns nichts an.
            const int nextFn = src.indexOf(QStringLiteral("\n}\n"), press);
            const QString body =
                src.mid(press, nextFn > press ? nextFn - press : 4000);
            if (!body.contains(QStringLiteral("QMenu"))) { continue; }

            ++checked;
            if (!src.contains(QStringLiteral("::contextMenuEvent("))) {
                offenders << QDir(QString::fromLocal8Bit(root))
                                 .relativeFilePath(path);
            }
        }

        qInfo() << "Dateien, die im Druckereignis ein Menue oeffnen:"
                << checked;
        for (const QString& o : offenders) { qInfo() << "   offen:" << o; }

        QVERIFY2(offenders.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Diese Dateien oeffnen ein Menue im Druckereignis, "
                     "ohne das Kontextmenue-Ereignis zu beantworten. Beim "
                     "Rechtsklick beantwortet es dann der Elternteil und "
                     "legt SEIN Menue obenauf:\n  %1")
                     .arg(offenders.join(QStringLiteral("\n  ")))));
    }

    /// Kein Bedienelement darf angelegt und dann nirgends eingehaengt
    /// werden.
    ///
    /// Diese Fehlerklasse hat diese Woche schon zugeschlagen: der
    /// Abloese-Pfeil war gebaut, verdrahtet und richtig — und lag 622
    /// Pixel neben der Kachel, weil er hinter einem addStretch()
    /// eingehaengt war. Vorhanden, verdrahtet, unerreichbar.
    ///
    /// ZUERST WIRD DAS MESSGERAET GEPRUEFT. Die erste Fassung dieser
    /// Regel benutzte QLayout::indexOf und meldete 105 Treffer,
    /// darunter LSB und USB, die sichtbar in der Leiste stehen —
    /// indexOf sieht nur die OBERSTE Ebene, unsere Knoepfe haengen in
    /// verschachtelten Layouts. Beinahe als Befund gemeldet. Deshalb
    /// baut der erste Teil hier einen echten Verstoss und verlangt,
    /// dass er gefunden wird.
    void noControlIsBuiltAndThenLeftOutOfEveryLayout()
    {
        auto inLayoutTree = [](QWidget* w) {
            QWidget* p = w->parentWidget();
            if (!p || !p->layout()) { return true; }   // kein Layout: nicht dieser Fall
            std::function<bool(QLayout*)> holds = [&](QLayout* l) -> bool {
                if (!l) { return false; }
                for (int i = 0; i < l->count(); ++i) {
                    QLayoutItem* it = l->itemAt(i);
                    if (!it) { continue; }
                    if (it->widget() == w) { return true; }
                    if (it->layout() && holds(it->layout())) { return true; }
                }
                return false;
            };
            return holds(p->layout());
        };

        // ── Erst das Messgeraet ──────────────────────────────────────
        {
            QWidget host;
            auto* col = new QVBoxLayout(&host);
            auto* row = new QHBoxLayout;
            col->addLayout(row);

            auto* proper = new QPushButton(QStringLiteral("drin"), &host);
            row->addWidget(proper);                 // verschachtelt eingehaengt
            auto* orphan = new QPushButton(QStringLiteral("draussen"), &host);
            Q_UNUSED(orphan)                        // absichtlich vergessen

            QVERIFY2(inLayoutTree(proper),
                     "Das Messgeraet findet einen verschachtelt "
                     "eingehaengten Knopf nicht — so hat die erste "
                     "Fassung 105 Fehlalarme erzeugt");
            QVERIFY2(!inLayoutTree(orphan),
                     "Das Messgeraet uebersieht einen Knopf, der in "
                     "keinem Layout haengt — dann prueft es nichts");
        }

        // ── Dann das echte Hauptfenster ──────────────────────────────
        auto* mw = new MainWindow();
        mw->resize(1800, 1100);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 12; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(350);

        QStringList lost;
        int total = 0;
        for (QPushButton* b : mw->findChildren<QPushButton*>()) {
            ++total;
            if (inLayoutTree(b)) { continue; }
            lost << QStringLiteral("\"%1\" in %2")
                        .arg(b->text(),
                             QString::fromLatin1(b->parentWidget()
                                 ->metaObject()->className()));
        }

        qInfo() << "Knoepfe im Hauptfenster:" << total
                << " ohne Layout-Platz:" << lost.size();

        QVERIFY2(total > 50,
                 "Zu wenige Knoepfe gefunden — das Fenster ist wohl nicht "
                 "fertig aufgebaut, dann prueft das hier nichts");
        QVERIFY2(lost.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Diese Bedienelemente sind gebaut, haengen aber in "
                     "keinem Layout — sie koennen nie erscheinen:\n  %1")
                     .arg(lost.join(QStringLiteral("\n  ")))));

        mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
    }
};

QTEST_MAIN(TstReachabilityAudit)
#include "tst_reachability_audit.moc"
