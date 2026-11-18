// tests/tst_werksprofile.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
//
// ── Die Werksvorgabe der Layoutprofile ───────────────────────────────
//
// Betreiber, 2026-11-18: „ich finde, dass mein standard profil auch als
// ausgangsbasis herangezogen werden soll, kann das automatisch bei einem
// download gespeichert sein?"
//
// Wer frisch installiert, bekommt Martins Profile N und F aus
// resources/layout/werksprofile.json (gezogen mit
// tools/werksprofile_ziehen.py). Geprueft wird, was dabei schiefgehen
// kann, und zwar in beide Richtungen:
//
//   · Die frische Sandkiste bekommt N und F, N ist aktiv, und keine
//     Lage liegt ausserhalb eines 1440x900-Schirms.
//   · Wer schon Profile hat -- auch einen beschaedigten Eintrag --
//     bekommt sie NIE; sein Eintrag bleibt Byte fuer Byte stehen.
//   · In der Ressource steht nichts Privates: das Repo ist oeffentlich.
//
// Modification history (Longpath)
//   2026-11-18  angelegt (Martin Fischer, OE5SOS; mit KI-Hilfe: Claude Code)

#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"

using namespace Longpath;

namespace {

QString quellordner()
{
    const QByteArray d = qgetenv("LONGPATH_SOURCE_DIR");
    return d.isEmpty() ? QString() : QString::fromLocal8Bit(d);
}

QByteArray vorgabe()
{
    QFile f(quellordner() + QStringLiteral("/resources/layout/werksprofile.json"));
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    return f.readAll();
}

// Ein 1440x900-Schirm mit 25 Punkten Menueleiste, maximiertes Hauptfenster:
// dx/dy zaehlen ab der Ecke der verfuegbaren Flaeche (bezugspunktFuerProfil).
const QRect kSchirm(0, 0, 1440, 900);
const QPoint kVerfuegbarEcke(0, 25);

} // namespace

class TestWerksprofile : public QObject
{
    Q_OBJECT

private:
    QVariantMap m_live;
    void hook(LayoutProfiles& lp)
    {
        lp.setHooks([this]() { return m_live; },
                    [this](const QVariantMap& s) { m_live = s; });
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(!quellordner().isEmpty(),
                 "LONGPATH_SOURCE_DIR fehlt -- ueber ctest fahren, nicht die Binaerdatei");
        QVERIFY2(!vorgabe().isEmpty(), "resources/layout/werksprofile.json nicht lesbar");
    }

    void init()
    {
        m_live.clear();
        AppSettings::instance().remove(LayoutProfiles::settingsKey());
    }

    // ── Frische Sandkiste ────────────────────────────────────────────
    void frischeSandkisteBekommtNundF()
    {
        LayoutProfiles lp;
        hook(lp);
        lp.load();
        QVERIFY(lp.names().isEmpty());

        // Betreiber 2026-11-18: „nur N und F".
        QVERIFY(lp.ladeWerksvorgabe(vorgabe()));
        QCOMPARE(lp.names(), QStringList({QStringLiteral("Neu"), QStringLiteral("FT8")}));
        QCOMPARE(lp.current(), QStringLiteral("Neu"));
        // FT8 bleibt an den Digimodus FT8 gebunden.
        QVERIFY(lp.isBoundToDigi(QStringLiteral("FT8"), DigiFrequenzen::Digimodus(0)));
        // N traegt seine schwebenden Fenster mit.
        QVERIFY(lp.snapshot(QStringLiteral("Neu"))
                    .value(QStringLiteral("floatingApplets")).toMap()
                    .contains(QStringLiteral("Kombi")));

        // Gesichert: ein zweiter Start liest dieselben zwei, N vorn.
        const QString gesichert = AppSettings::instance()
                                      .value(LayoutProfiles::settingsKey()).toString();
        QVERIFY(!gesichert.isEmpty());
        LayoutProfiles zweiter;
        zweiter.load();
        zweiter.startWithFirst();
        QCOMPARE(zweiter.names(), lp.names());
        QCOMPARE(zweiter.current(), QStringLiteral("Neu"));
        // Und ein zweiter Lauf der Vorgabe tut nichts mehr.
        QVERIFY(!zweiter.ladeWerksvorgabe(vorgabe()));
    }

    void keinFensterAusserhalbVon1440x900()
    {
        const QJsonObject wurzel = QJsonDocument::fromJson(vorgabe()).object();
        int gezaehlt = 0;
        for (const QJsonValue& pv : wurzel.value(QStringLiteral("profiles")).toArray()) {
            const QJsonObject p = pv.toObject();
            const QJsonObject s = p.value(QStringLiteral("state")).toObject();
            for (const char* art : {"floatingApplets", "floatingPans", "containerGeometry"}) {
                const QJsonObject fenster = s.value(QLatin1String(art)).toObject();
                for (auto it = fenster.constBegin(); it != fenster.constEnd(); ++it) {
                    const QJsonObject l = it.value().toObject();
                    const QSize groesse(l.value(QStringLiteral("w")).toInt(),
                                        l.value(QStringLiteral("h")).toInt());
                    QVERIFY(groesse.width() > 0 && groesse.height() > 0);
                    const QRect relativ(kVerfuegbarEcke
                                            + QPoint(l.value(QStringLiteral("dx")).toInt(),
                                                     l.value(QStringLiteral("dy")).toInt()),
                                        groesse);
                    const QRect absolut(l.value(QStringLiteral("x")).toInt(),
                                        l.value(QStringLiteral("y")).toInt(),
                                        groesse.width(), groesse.height());
                    const QString wer = p.value(QStringLiteral("name")).toString()
                                        + QLatin1Char('/') + it.key();
                    QVERIFY2(kSchirm.contains(relativ),
                             qPrintable(wer + QStringLiteral(" relativ ausserhalb")));
                    QVERIFY2(kSchirm.contains(absolut),
                             qPrintable(wer + QStringLiteral(" absolut ausserhalb")));
                    ++gezaehlt;
                }
            }
            // Der Profilrahmen, auf den sich alles bezieht, passt selbst.
            const QJsonArray r = s.value(QStringLiteral("mainWindow")).toObject()
                                     .value(QStringLiteral("rahmen")).toArray();
            QCOMPARE(r.size(), 4);
            QVERIFY(kSchirm.contains(QRect(r.at(0).toInt(), r.at(1).toInt(),
                                           r.at(2).toInt(), r.at(3).toInt())));
        }
        QVERIFY2(gezaehlt >= 8, "zu wenige schwebende Fenster -- falsche Datei?");
    }

    void nichtsPrivatesInDerRessource()
    {
        const QString text = QString::fromUtf8(vorgabe());
        const struct { const char* muster; const char* was; } verboten[] = {
            {"/Users/|/home/|[A-Za-z]:\\\\\\\\", "Dateipfad"},
            {"\"screen\"", "Bildschirmschluessel"},
            {"(?i)retina|display|monitor", "Bildschirmname"},
            {"\"worldImage\"", "Weltbild-Pfad"},
            {"\\b\\d{1,3}(\\.\\d{1,3}){3}\\b", "IPv4-Adresse"},
            {"\\b[0-9A-Fa-f]{2}([:-][0-9A-Fa-f]{2}){5}\\b", "MAC-Adresse"},
            {"@", "E-Mail"},
            {"(?i)password|passwort|apikey|api_key|token|secret", "Schluessel"},
            {"\\b[A-Z0-9]{1,3}\\d[A-Z]{1,4}\\b", "Rufzeichen"},
        };
        for (const auto& v : verboten) {
            const QRegularExpressionMatch m =
                QRegularExpression(QString::fromUtf8(v.muster)).match(text);
            QVERIFY2(!m.hasMatch(), qPrintable(QStringLiteral("%1: %2")
                                                   .arg(QLatin1String(v.was), m.captured(0))));
        }
    }

    void geraetepanelsOhneSchalterSindAus()
    {
        LayoutProfiles lp;
        QVERIFY(lp.ladeWerksvorgabe(vorgabe()));
        for (const QString& n : lp.names()) {
            const QVariantMap s = lp.snapshot(n);
            const QVariantMap vis = s.value(QStringLiteral("visible")).toMap();
            for (const QString id : {QStringLiteral("CatRig"), QStringLiteral("KiwiSdr"),
                                     QStringLiteral("KiwiWaterfalls")}) {
                QVERIFY2(!vis.value(id, false).toBool(), qPrintable(n + QLatin1Char('/') + id));
                QVERIFY(!s.value(QStringLiteral("floatingApplets")).toMap().contains(id));
            }
        }
    }

    // ── Wer schon Profile hat ────────────────────────────────────────
    void vorhandeneProfileBleibenUnveraendert()
    {
        const QString seine = QStringLiteral(
            "{\"current\":\"Mein\",\"profiles\":[{\"name\":\"Mein\",\"state\":{\"visible\":{\"Rx\":true}},"
            "\"bands\":[],\"modes\":[],\"digis\":[]}]}");
        AppSettings::instance().setValue(LayoutProfiles::settingsKey(), seine);
        LayoutProfiles lp;
        hook(lp);
        lp.load();
        QVERIFY(!lp.ladeWerksvorgabe(vorgabe()));
        QCOMPARE(lp.names(), QStringList({QStringLiteral("Mein")}));
        QCOMPARE(lp.current(), QStringLiteral("Mein"));
        QCOMPARE(AppSettings::instance().value(LayoutProfiles::settingsKey()).toString(), seine);
    }

    void beschaedigterEintragWirdNichtUeberschrieben()
    {
        // load() laesst einen kaputten Eintrag absichtlich stehen. Die
        // Vorgabe darf ihn nicht ueberschreiben -- seine Profile stehen
        // noch darin, und jemand kann sie retten.
        const QString kaputt = QStringLiteral("{\"current\":\"Mein\",\"profiles\":[{\"na");
        AppSettings::instance().setValue(LayoutProfiles::settingsKey(), kaputt);
        LayoutProfiles lp;
        lp.load();
        QVERIFY(lp.names().isEmpty());
        QVERIFY(!lp.ladeWerksvorgabe(vorgabe()));
        QVERIFY(lp.names().isEmpty());
        QCOMPARE(AppSettings::instance().value(LayoutProfiles::settingsKey()).toString(), kaputt);
    }

    void inDieserSitzungAngelegtZaehltAuch()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("Eigenes")));
        QVERIFY(!lp.ladeWerksvorgabe(vorgabe()));
        QCOMPARE(lp.names(), QStringList({QStringLiteral("Eigenes")}));
    }

    void kaputteVorgabeLaesstAllesLeer()
    {
        LayoutProfiles lp;
        QVERIFY(!lp.ladeWerksvorgabe(QByteArray("{nicht json")));
        QVERIFY(lp.names().isEmpty());
        QVERIFY(AppSettings::instance().value(LayoutProfiles::settingsKey())
                    .toString().isEmpty());
    }

    // ── Die Ressource kommt ins Programm ─────────────────────────────
    void ressourceStehtInDerQrc()
    {
        QFile qrc(quellordner() + QStringLiteral("/resources.qrc"));
        QVERIFY(qrc.open(QIODevice::ReadOnly));
        const QString t = QString::fromUtf8(qrc.readAll());
        const QRegularExpression block(
            QStringLiteral("<qresource prefix=\"/layout\">\\s*<file alias=\"werksprofile.json\">"
                           "resources/layout/werksprofile.json</file>"));
        QVERIFY2(block.match(t).hasMatch(), "resources.qrc fehlt der Eintrag fuer /layout");
        QCOMPARE(LayoutProfiles::werksvorgabePfad(),
                 QStringLiteral(":/layout/werksprofile.json"));
    }
};

QTEST_MAIN(TestWerksprofile)
#include "tst_werksprofile.moc"
