// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_sunsdr_netzlage.cpp  (Longpath)
// =================================================================
// Liegt die QRP ueberhaupt in einem Netz dieses Rechners?
//
// In der Nacht zum 2026-10-12 lief ein Messlauf fuenf Suchversuche
// lang ins Leere — 90 Sekunden — und meldete am Ende nur "keine
// Antwort", also dieselbe Meldung wie bei einem gesperrten Geraet.
// Die Ursache war trivial und stand nirgends: der Rechner hing in
// einem anderen Netz (192.168.78.153/24), die QRP war unter
// 192.168.16.200 eingetragen, und das Kabel, ueber das sie erreichbar
// war, gab es nicht mehr.
//
// netzlage() sagt das in einem Satz. Dieser Pruefstand gibt die Netze
// selbst vor, braucht also weder Schnittstellen noch Geraet.
//
// Modification history (Longpath):
//   2026-10-12 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include "core/SunSdrRadioConnection.h"

using namespace Longpath;

using Netz = SunSdrRadioConnection::NetzEintrag;

class TestSunSdrNetzlage : public QObject
{
    Q_OBJECT
private slots:
    void zielImSelbenNetz()
    {
        const QVector<Netz> netze{
            {QStringLiteral("en9"), QHostAddress(QStringLiteral("192.168.16.100")), 16}};
        const QString satz = SunSdrRadioConnection::netzlage(
            QHostAddress(QStringLiteral("192.168.16.200")), netze);
        QVERIFY2(satz.contains(QStringLiteral("liegt im Netz von en9")),
                 qPrintable(satz));
    }

    void derFallDieserNacht()
    {
        // Genau die Lage vom 2026-10-12: Rechner in 192.168.78.0/24,
        // Geraet eingetragen auf 192.168.16.200.
        const QVector<Netz> netze{
            {QStringLiteral("en0"), QHostAddress(QStringLiteral("192.168.78.153")), 24}};
        const QString satz = SunSdrRadioConnection::netzlage(
            QHostAddress(QStringLiteral("192.168.16.200")), netze);
        QVERIFY2(satz.contains(QStringLiteral("KEINEM Netz")), qPrintable(satz));
        // Und er muss sagen, was stattdessen da ist -- ohne das faengt
        // die Suche nach der Ursache wieder bei null an.
        QVERIFY2(satz.contains(QStringLiteral("en0 192.168.78.153/24")),
                 qPrintable(satz));
    }

    void mehrereNetzeUndEinesPasst()
    {
        const QVector<Netz> netze{
            {QStringLiteral("en0"), QHostAddress(QStringLiteral("192.168.78.153")), 24},
            {QStringLiteral("en9"), QHostAddress(QStringLiteral("192.168.16.100")), 16}};
        const QString satz = SunSdrRadioConnection::netzlage(
            QHostAddress(QStringLiteral("192.168.16.200")), netze);
        QVERIFY2(satz.contains(QStringLiteral("liegt im Netz von en9")),
                 qPrintable(satz));
    }

    void ohneNetzUndOhneZiel()
    {
        QVERIFY(SunSdrRadioConnection::netzlage(
                    QHostAddress(QStringLiteral("192.168.16.200")), {})
                    .contains(QStringLiteral("gar kein IPv4-Netz")));
        QVERIFY(SunSdrRadioConnection::netzlage(QHostAddress{}, {})
                    .contains(QStringLiteral("kein Geraet im Eintrag")));
    }

    void dieEchtenNetzeSindBrauchbar()
    {
        // Kein Inhaltsurteil -- nur: was eigeneNetze() liefert, muss sich
        // mit netzlage() auch benutzen lassen (Praefix gesetzt, IPv4,
        // keine Rueckschleife).
        const auto netze = SunSdrRadioConnection::eigeneNetze();
        for (const Netz& n : netze) {
            QVERIFY2(!n.adresse.isNull(), qPrintable(n.name));
            QVERIFY2(n.praefix > 0 && n.praefix <= 32, qPrintable(n.name));
            QVERIFY2(n.adresse != QHostAddress(QStringLiteral("127.0.0.1")),
                     "die Rueckschleife gehoert nicht in die Liste");
        }
    }
};

QTEST_MAIN(TestSunSdrNetzlage)
#include "tst_sunsdr_netzlage.moc"
