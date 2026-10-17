// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/tst_profilschiene_richtung.cpp  (Longpath)
//
// Die Profilschiene liegt seit dem 2026-10-06 waagrecht in der
// Kommandoleiste statt senkrecht am linken Rand.
//
// Betreiber: „meine profile links im eck sollten oben in die taksleite
// neben 48 khz" -- und direkt danach: „macht sinn, dass man dies
// individuell verschieben und anpassen kann". Also beides, und beides
// muss gehen.
//
// Geprueft wird, was man am Bauteil sieht: senkrecht ist es 44 Pixel
// breit und beliebig hoch, waagrecht genau ein Abzeichen hoch und
// beliebig breit. Und in BEIDEN Lagen traegt es dieselben Abzeichen --
// eine Lage, in der die Profile verschwinden, waere der eigentliche
// Fehler.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-06 — Original fuer Longpath, KI-gestuetzt (Anthropic
//                 Claude), Betreiber Martin Fischer.
// =================================================================

#include <QtTest>

#include "gui/LayoutProfiles.h"
#include "gui/styles/AppTheme.h"
#include "gui/widgets/CommandBar.h"
#include "gui/widgets/ProfileRail.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QStyle>
#include <QStyleOptionButton>
#include <QLabel>
#include <QPushButton>

using namespace Longpath;

class TestProfilschieneRichtung : public QObject
{
    Q_OBJECT

private:
    /// Das Abzeichen zu einem Profilnamen. Die Schiene gibt die Knoepfe
    /// nicht heraus -- sie traegt den Namen aber als Kurzhinweis, und
    /// genau daran findet man sie wieder.
    /// Irgendeine Pille aus einer Gruppe der Leiste -- das Mass, an dem
    /// sich die Abzeichen messen lassen muessen. Die Pillen sind
    /// umschaltbar, das Plus und die „…" nicht; daran sind sie zu
    /// erkennen, ohne eine Beschriftung festzunageln.
    static QPushButton* ersteGruppenPille(const CommandBar& leiste)
    {
        for (QPushButton* b : leiste.findChildren<QPushButton*>()) {
            if (b->isCheckable()) { return b; }
        }
        return nullptr;
    }

    /// Der Plus-Knopf der Schiene: der einzige ohne Kurzhinweis mit
    /// einem Profilnamen.
    /// Passt die Beschriftung in den Knopf, oder schneidet der
    /// Innenabstand sie weg? Dasselbe Mass wie in
    /// tst_zoom_buttons_legible -- die Textflaeche, die der Stil dem
    /// Knopf laesst, gegen das, was das Zeichen wirklich braucht.
    static void pruefeLesbar(QPushButton* b, const char* wo)
    {
        b->ensurePolished();
        QStyleOptionButton opt;
        opt.initFrom(b);
        opt.text = b->text();
        const QRect flaeche =
            b->style()->subElementRect(QStyle::SE_PushButtonContents, &opt, b);
        const QRect zeichen = QFontMetrics(b->font()).tightBoundingRect(b->text());
        QVERIFY2(flaeche.width() >= zeichen.width()
                     && flaeche.height() >= zeichen.height(),
                 qPrintable(QStringLiteral(
                     "%1: „%2\u201C hat %3x%4 Platz, braucht aber %5x%6 -- "
                     "es wird weggeschnitten")
                         .arg(QString::fromLatin1(wo), b->text())
                         .arg(flaeche.width()).arg(flaeche.height())
                         .arg(zeichen.width()).arg(zeichen.height())));
    }

    static QPushButton* plusKnopf(const ProfileRail& rail)
    {
        for (QPushButton* b : rail.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("+")) { return b; }
        }
        return nullptr;
    }

    static QPushButton* badgeFor(const ProfileRail& rail, const QString& name)
    {
        const QList<QPushButton*> alle = rail.findChildren<QPushButton*>();
        for (QPushButton* b : alle) {
            if (b->toolTip() == name) { return b; }
        }
        return nullptr;
    }

private slots:
    // ── Das app-weite Blatt gehoert dazu ────────────────────────────
    //
    // Ohne diese Zeile prueft die Datei eine Schiene, die es so nicht
    // gibt. Style::kButtonStyle traegt `padding: 4px 12px` und gilt
    // fuer JEDEN Knopf ohne eigene Angabe -- es hat am 2026-09-25 die
    // Zoomknoepfe ihre Zeichen gekostet (tst_zoom_buttons_legible) und
    // am 2026-10-07 das Plus der Schiene acht Pixel zu hoch gemacht.
    // Beides faellt nur auf, wenn die Pruefung dasselbe Blatt traegt
    // wie das Fenster.
    void initTestCase()
    {
        applyAppBaselineQss(*qApp);
    }

    void senkrechtIstDieSchmaleSchiene()
    {
        LayoutProfiles profiles;
        ProfileRail rail(&profiles, Qt::Vertical);
        QCOMPARE(rail.richtung(), Qt::Vertical);
        // Feste Breite, damit sie am Rand nicht wandert.
        QCOMPARE(rail.width(), ProfileRail::kWidth);
        QCOMPARE(rail.minimumWidth(), ProfileRail::kWidth);
        QCOMPARE(rail.maximumWidth(), ProfileRail::kWidth);
    }

    void waagrechtIstGenauEinAbzeichenHoch()
    {
        LayoutProfiles profiles;
        ProfileRail rail(&profiles, Qt::Horizontal);
        QCOMPARE(rail.richtung(), Qt::Horizontal);
        // Feste Hoehe statt fester Breite -- sonst saesse in der Leiste
        // ein 44 Pixel breiter Klotz, in den die Abzeichen nicht passen.
        QCOMPARE(rail.height(), ProfileRail::kLeistenSeite);
        QCOMPARE(rail.minimumHeight(), ProfileRail::kLeistenSeite);
        QCOMPARE(rail.maximumHeight(), ProfileRail::kLeistenSeite);
        // Und ausdruecklich KEINE feste Breite mehr.
        //
        // maximumWidth() > kWidth allein war zu schwach: das ist Qts
        // Vorgabe (QWIDGETSIZE_MAX) und gilt auch, wenn jemand
        // zusaetzlich die Hoehe festnagelt. Darum gegen die senkrechte
        // Lage stellen -- DORT ist die Breite fest, hier nicht.
        QVERIFY2(rail.maximumWidth() > ProfileRail::kWidth,
                 "waagrecht darf die Breite nicht festgenagelt sein");
        ProfileRail senkrecht(&profiles, Qt::Vertical);
        QVERIFY2(senkrecht.maximumWidth() != rail.maximumWidth(),
                 "senkrecht und waagrecht duerfen nicht dieselbe "
                 "Breitenfestlegung haben");
        QVERIFY2(senkrecht.maximumHeight() != rail.maximumHeight(),
                 "senkrecht und waagrecht duerfen nicht dieselbe "
                 "Hoehenfestlegung haben");
    }

    // ── Die Form am Platz (2026-10-07) ──────────────────────────────
    //
    // Betreiber vor diesem Bild: „die leiste oben passt aber noch
    // immer nicht". Am Bildschirm nachgesehen: die Abzeichen sassen
    // als Kreise von 30 px zwischen den Pillen von 27 px, mit eigener
    // Schrift und eigener Fuellung. Vier Unterschiede in einer Reihe.
    //
    // Diese beiden Pruefungen halten fest, was dabei herauskam: in der
    // Leiste traegt die Schiene den Stil der Pillen, am Rand bleibt sie
    // die Scheibe. Die erste war gegen die alte Fassung rot (30 != 27
    // und ein eigenes Stilblatt statt pillStyle()), die zweite gruen --
    // sie ist der Waechter dagegen, dass die Aenderung auch den Rand
    // erwischt, wo die Scheibe richtig ist.
    void inDerLeisteTraegtSieDieFormDerPillen()
    {
        LayoutProfiles profiles;
        QVERIFY(profiles.create(QStringLiteral("Neu")));
        QVERIFY(profiles.create(QStringLiteral("Betrieb")));
        ProfileRail rail(&profiles, Qt::Horizontal);

        QPushButton* abzeichen = badgeFor(rail, QStringLiteral("Betrieb"));
        QVERIFY2(abzeichen, "Abzeichen „Betrieb\u201C nicht gefunden");

        // ── Gegen eine ECHTE Pille, nicht gegen eine Zahl ────────────
        //
        // kPillHeight ist 27, eine Pille auf dem Schirm aber 29 hoch:
        // `min-height` im Stilblatt gilt der Inhaltsflaeche, die beiden
        // Raender kommen dazu. Gegen die Zahl zu pruefen hiesse, die
        // Abzeichen zwei Pixel flacher zu machen als ihre Nachbarn und
        // das auch noch gruen zu melden.
        //
        // Darum eine leibhaftige Leiste bauen und eine ihrer Pillen als
        // Mass nehmen. Das ueberlebt auch das naechste Stilblatt.
        CommandBar leiste;
        QPushButton* pille = ersteGruppenPille(leiste);
        QVERIFY2(pille, "keine Pille in der Leiste gefunden");
        QCOMPARE(abzeichen->sizeHint().height(), pille->sizeHint().height());
        QCOMPARE(rail.height(), pille->sizeHint().height());
        QCOMPARE(ProfileRail::kLeistenSeite, pille->sizeHint().height());

        // Auch das Plus. Es traegt als einziges ein eigenes Stilblatt
        // (gestrichelt) und faellt darum als einziges wieder in die
        // Innenabstands-Falle, wenn jemand `padding` dort streicht.
        QPushButton* plus = plusKnopf(rail);
        QVERIFY2(plus, "kein Plus in der Schiene gefunden");
        QCOMPARE(plus->sizeHint().height(), pille->sizeHint().height());

        pruefeLesbar(abzeichen, "Abzeichen in der Leiste");
        pruefeLesbar(plus, "Plus in der Leiste");

        // Das Kreuz zum Loeschen gibt es nur ab zwei Profilen, und es
        // ist bis zum Ueberfahren verborgen -- aber es ist ein Knopf mit
        // einem Zeichen, und 14 Pixel minus 2x12 Innenabstand waeren
        // negativ. Genau die Rechnung, die oben schon zweimal stimmte.
        for (QPushButton* k : rail.findChildren<QPushButton*>()) {
            if (k->text() == QString(QChar(0x00D7))) {
                pruefeLesbar(k, "Kreuz am Abzeichen");
            }
        }

        // ── Der Buchstabe muss hineinpassen ─────────────────────────
        //
        // Der erste Anlauf nagelte auch die Breite auf 27 fest. Am
        // Bildschirm wurde daraus ein Schraegstrich statt eines „N":
        // CommandBar vererbt `padding: 0 11px` auf jeden Knopf in ihr,
        // und von 27 Pixeln blieben 3 fuer die Schrift. Dieselbe Falle
        // wie bei den Pillen selbst am 2026-08-23.
        //
        // Darum nicht die Breite pruefen, sondern den Platz: so breit,
        // wie der Buchstabe plus Innenabstand braucht -- nach genau der
        // Regel, die CommandBar::addPill fuer ihre eigenen Pillen zieht.
        QVERIFY2(abzeichen->minimumWidth()
                     >= abzeichen->fontMetrics().horizontalAdvance(
                            abzeichen->text()) + 20,
                 "das Abzeichen muss breit genug fuer seinen Buchstaben sein");
        QVERIFY2(abzeichen->maximumWidth() > CommandBar::kPillHeight,
                 "waagrecht darf die Breite des Abzeichens nicht fest sein");

        // Nicht „sieht aehnlich aus", sondern DERSELBE Stil. Ein
        // nachgebauter Verlauf liefe beim naechsten Stilblatt
        // auseinander, ohne dass es jemandem auffiele.
        QCOMPARE(abzeichen->styleSheet(), CommandBar::pillStyle());

        // Die Hervorhebung laeuft ueber :checked, wie bei BAND und MODE.
        QVERIFY2(abzeichen->isCheckable(),
                 "in der Leiste muss das Abzeichen ueber :checked leuchten");
        QPushButton* aktiv = badgeFor(rail, profiles.current());
        QVERIFY(aktiv);
        QVERIFY2(aktiv->isChecked(), "das aktive Profil muss gedrueckt sein");
        // Ohne autoExclusive naehme ein Klick auf das schon aktive
        // Abzeichen ihm die Fuellung, und nichts baute die Schiene neu.
        QVERIFY2(aktiv->autoExclusive(),
                 "ein Klick auf das aktive Abzeichen darf es nicht leeren");
        aktiv->click();
        QVERIFY2(aktiv->isChecked(),
                 "nach dem Klick auf das aktive Abzeichen muss es gedrueckt bleiben");
    }

    void amRandBleibtDieScheibe()
    {
        LayoutProfiles profiles;
        QVERIFY(profiles.create(QStringLiteral("Neu")));
        ProfileRail rail(&profiles, Qt::Vertical);

        QPushButton* abzeichen = badgeFor(rail, QStringLiteral("Neu"));
        QVERIFY(abzeichen);
        QCOMPARE(abzeichen->height(), ProfileRail::kBadgeSide);
        // Voll gerundet = Scheibe. Genau die Haelfte der Kantenlaenge.
        QVERIFY2(abzeichen->styleSheet().contains(
                     QStringLiteral("border-radius: %1px")
                         .arg(ProfileRail::kBadgeSide / 2)),
                 "am Rand muss das Abzeichen eine Scheibe bleiben");
        QVERIFY2(abzeichen->styleSheet() != CommandBar::pillStyle(),
                 "am Rand gehoert NICHT der Pillenstil hin");

        // ── Und der Buchstabe muss zu sehen sein ────────────────────
        //
        // Am 2026-10-07 am Schirm gefunden: aus dem „N" war ein
        // Schraegstrich geworden, aus dem „B" ein halbes Zeichen. Der
        // app-weite Grundstil vom 2026-09-18 bringt `padding: 4px 12px`
        // mit, und von 30 Pixeln blieben vier. Drei Wochen lang, ohne
        // dass eine Pruefung es gemerkt haette -- weil keine das
        // app-weite Blatt trug.
        pruefeLesbar(abzeichen, "Abzeichen am Rand");
        pruefeLesbar(plusKnopf(rail), "Plus am Rand");
    }

    // Der eigentliche Waechter: die Lage darf die Profile nicht kosten.
    void beideLagenTragenDieselbenAbzeichen()
    {
        LayoutProfiles profiles;
        // Eine frische LayoutProfiles hat KEINE Profile. Ohne die zwei
        // hier verglich die Pruefung zwei leere Listen und war gruen,
        // ohne irgendetwas zu belegen -- die Zusicherung weiter unten
        // hat genau das gefangen.
        QVERIFY(profiles.create(QStringLiteral("Neu")));
        QVERIFY(profiles.create(QStringLiteral("Betrieb")));

        ProfileRail senkrecht(&profiles, Qt::Vertical);
        ProfileRail waagrecht(&profiles, Qt::Horizontal);
        QCOMPARE(waagrecht.badges(), senkrecht.badges());
        QVERIFY2(!senkrecht.badges().isEmpty(),
                 "ohne Abzeichen sagt der Vergleich nichts");
        QCOMPARE(waagrecht.activeBadge(), senkrecht.activeBadge());

        // badges() liest die eigene Liste, nicht die ANORDNUNG -- bindet
        // jemand das Einhaengen an die Richtung, bleibt die Leiste leer
        // und dieser Vergleich trotzdem gruen (2026-10-07 gefunden).
        // Also auch nachsehen, dass die Abzeichen wirklich in der
        // Anordnung stecken.
        for (ProfileRail* r : {&senkrecht, &waagrecht}) {
            QLayout* anordnung = r->layout();
            QVERIFY(anordnung != nullptr);
            int knoepfe = 0;
            for (int i = 0; i < anordnung->count(); ++i) {
                if (anordnung->itemAt(i) && anordnung->itemAt(i)->widget()) {
                    ++knoepfe;
                }
            }
            // Zwei Abzeichen plus das gestrichelte Plus.
            QCOMPARE(knoepfe, senkrecht.badges().size() + 1);
        }
    }

    // ── Die Leiste: Platz und Aufraeumen (2026-10-07) ────────────────
    //
    // Beides am 2026-10-07 beim Gegenlesen gefunden, beides von mir
    // eingebaut:
    //
    //   1. addGroupWidget setzte bei m_row->count() - 1 ein, mit der
    //      Begruendung "die Dehnung ist das letzte Element". Nach
    //      addTrailing() ist sie das NICHT mehr -- dann landet die
    //      Gruppe hinter der Dehnung und sitzt ganz rechts.
    //   2. Es gab kein Gegenstueck: nahm man das Bauteil wieder weg,
    //      blieb die Versalzeile stehen, und beim naechsten Mal kam eine
    //      zweite dazu.
    void dieGruppeSitztVorDerDehnungUndRaeumtSichWiederWeg()
    {
        CommandBar leiste;
        // Genau wie im echten Fenster: das Plus haengt RECHTS, also
        // hinter der Dehnung. Ohne diese Zeile faellt der Fehler nicht
        // auf -- und genau deshalb ist er mir durchgerutscht.
        auto* plus = new QPushButton(QStringLiteral("+"), &leiste);
        leiste.addTrailing(plus);

        auto* bauteil = new QWidget(&leiste);
        auto zaehleVersalzeilen = [&leiste]() {
            int n = 0;
            for (QLabel* l : leiste.findChildren<QLabel*>()) {
                if (l->text().compare(QStringLiteral("PROFIL"),
                                      Qt::CaseInsensitive) == 0) { ++n; }
            }
            return n;
        };
        auto platzVon = [&leiste](QWidget* w) {
            auto* row = leiste.findChild<QHBoxLayout*>();
            if (!row) { return -1; }
            for (int i = 0; i < row->count(); ++i) {
                QLayoutItem* it = row->itemAt(i);
                if (it && it->layout() && it->layout()->indexOf(w) >= 0) { return i; }
            }
            return -1;
        };
        // ── Wogegen diese Pruefung wirklich schuetzt ────────────────
        //
        // Hier stand "vor der ERSTEN Dehnung". Das war richtig, solange
        // genau eine Dehnung ganz hinten stand. Seit verteileGruppen()
        // (2026-10-07) steht zwischen jedem Gruppenpaar eine -- die
        // erste ist jetzt die zwischen BAND und MODE, und "davor" hiesse
        // "ganz links".
        //
        // Der Fehler, den die Pruefung gefangen hat, war nie "hinter der
        // Dehnung", sondern "hinter dem Plus", also am rechten Rand
        // statt neben der Rate. Genau das steht jetzt da -- und es bleibt
        // richtig, wie viele Dehnungen auch dazwischenstehen.
        auto platzDesPlus = [&leiste, plus]() {
            auto* row = leiste.findChild<QHBoxLayout*>();
            if (!row) { return -1; }
            for (int i = 0; i < row->count(); ++i) {
                if (row->itemAt(i) && row->itemAt(i)->widget() == plus) { return i; }
            }
            return -1;
        };

        leiste.addGroupWidget(QStringLiteral("Profil"), bauteil);
        QCOMPARE(zaehleVersalzeilen(), 1);
        const int platz = platzVon(bauteil);
        const int plusPlatz = platzDesPlus();
        QVERIFY2(platz >= 0 && plusPlatz >= 0, "Gruppe oder Plus nicht gefunden");
        QVERIFY2(platz < plusPlatz,
                 "die Gruppe muss VOR dem Plus sitzen, sonst steht sie "
                 "am rechten Rand statt neben der Rate");

        // Dreimal hin und her: es darf genau eine Versalzeile bleiben.
        for (int runde = 0; runde < 3; ++runde) {
            leiste.removeGroupWidget(bauteil);
            QCOMPARE(zaehleVersalzeilen(), 0);
            leiste.addGroupWidget(QStringLiteral("Profil"), bauteil);
            QCOMPARE(zaehleVersalzeilen(), 1);
            QVERIFY(platzVon(bauteil) < platzDesPlus());
        }
    }

    // Und das Umschalten selbst muss ankommen: die Schiene legt sich
    // nicht selbst um, sie meldet nur -- also muss die Meldung die
    // andere Richtung tragen.
    // Die Schiene muss die ANDERE Richtung melden, nicht die eigene.
    //
    // Hier stand bis zum 2026-10-07 nur QVERIFY(spy.isValid()) -- das
    // prueft, dass es das Signal GIBT, nicht was darin steht. Dreht man
    // in ProfileRail das Fragezeichen um und meldet die eigene Richtung,
    // bleibt die Schiene beim Umschalten liegen (MainWindow steigt bei
    // gleicher Richtung frueh aus) -- und die Pruefung blieb gruen.
    void dieSchieneMeldetDieAndereRichtung()
    {
        LayoutProfiles profiles;
        QVERIFY(profiles.create(QStringLiteral("Neu")));

        for (auto richtung : {Qt::Horizontal, Qt::Vertical}) {
            ProfileRail schiene(&profiles, richtung);
            QSignalSpy spy(&schiene, &ProfileRail::placementToggleRequested);
            QVERIFY(spy.isValid());

            schiene.meldeUmhaengenForTest();
            QCOMPARE(spy.count(), 1);
            const auto gemeldet =
                spy.at(0).at(0).value<Qt::Orientation>();
            QVERIFY2(gemeldet != richtung,
                     "die Schiene muss die andere Richtung melden, sonst "
                     "passiert beim Umschalten nichts");
        }
    }
};

QTEST_MAIN(TestProfilschieneRichtung)
#include "tst_profilschiene_richtung.moc"
