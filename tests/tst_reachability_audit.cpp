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
};

QTEST_MAIN(TstReachabilityAudit)
#include "tst_reachability_audit.moc"
