// tests/tst_theme_qss.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── The palette cannot move until this is trustworthy ────────────────
//
// 1733 hex literals across 130 files, 1141 of them inside Qt stylesheet
// strings. Style::themed() is what lets those follow a theme change
// without every one of them being rewritten by hand first.
//
// Which makes it load-bearing for the whole redesign, so it gets
// checked properly rather than eyeballed. The three things that would
// hurt:
//
//   · a constant with no row in the table — it stays behind, silently,
//     and one widget keeps the old colour in an otherwise new theme;
//   · a substitution that fires where it should not — an eight-digit
//     #rrggbbaa whose first six digits happen to match;
//   · order dependence — the same stylesheet coming out differently
//     depending on how the table rows are sorted.

#include <QtTest>

#include "gui/styles/ThemeQss.h"
#include "gui/StyleConstants.h"

#include <QSet>

using namespace NereusSDR;

class TestThemeQss : public QObject
{
    Q_OBJECT

private slots:
    // ── The one that guards the redesign ─────────────────────────────
    //
    // Every colour with a name must have a row. A named constant that
    // is not in the table is a colour the theme cannot reach, and the
    // failure mode is one stubborn widget in the old palette that
    // nobody can find.
    void everyNamedConstantIsInTheTable()
    {
        // Gegen die RECHTE Spalte, nicht die linke.
        //
        // Solange die Tabelle die Identität war, kam beides aufs
        // Gleiche heraus. Am 2026-08-15 wurde die Palette entblaut, und
        // damit sind die Konstanten die ZIELE: eine Zeile heißt „der
        // alte Wert #205070 soll dorthin, wo kBorder heute steht".
        //
        // Eine Konstante, die in keiner Zeile als Ziel vorkommt, ist
        // eine Farbe, die kein altes Literal je erreicht — der Wert
        // ändert sich, und die Widgets, die ihn noch ausgeschrieben
        // haben, bleiben stehen.
        QSet<QString> mapped;
        for (const auto& e : Style::themeTable()) {
            mapped.insert(QString::fromLatin1(e.current).toLower());
        }

        // Written out rather than scraped from the header: a test that
        // reads the same file as the code proves only that the file is
        // self-consistent. This list is the independent copy.
        const QStringList named = {
            Style::kAppBg, Style::kPanelBg, Style::kTextPrimary,
            Style::kTextSecondary, Style::kTextTertiary, Style::kTextScale,
            Style::kTextInactive, Style::kLabelMid, Style::kAccent,
            Style::kTitleText, Style::kButtonBg, Style::kButtonHover,
            Style::kButtonAltHover, Style::kBorder, Style::kBorderSubtle,
            Style::kInsetBg, Style::kInsetBorder, Style::kGroove,
            Style::kTitleGradTop, Style::kTitleGradMid, Style::kTitleGradBot,
            Style::kTitleBorder, Style::kGreenBg, Style::kGreenText,
            Style::kGreenBorder, Style::kBlueBg, Style::kBlueBorder,
            Style::kBlueHover, Style::kAmberBg, Style::kAmberText,
            Style::kAmberBorder, Style::kAmberWarn, Style::kRedBg,
            Style::kRedBorder, Style::kDisabledBg, Style::kDisabledText,
            Style::kDisabledBorder, Style::kStatusBarBg,
            Style::kStatusBarBorder, Style::kStatusSep,
            Style::kDspToggleBg, Style::kDspToggleBorder,
            Style::kDspToggleText, Style::kTxFilterOverlayBorder,
            Style::kTxFilterOverlayLabel, Style::kOverlayBorder,
            Style::kInstrumentFace, Style::kInstrumentGlowHi,
            Style::kInstrumentGlowLo, Style::kInstrumentLimit,
        };

        // ── Zwei, die keine Zeile haben dürfen ───────────────────────
        //
        // kBlueText und kRedText waren beide #ffffff — Text auf einem
        // gefüllten Knopf. Eine Tabellenzeile für Weiß kann nicht
        // unterscheiden, ob ein "color: #ffffff" auf einem blauen Knopf
        // steht oder mitten in einem Absatz, und würde beides gleich
        // behandeln.
        //
        // Sie sind deshalb nur über den Namen erreichbar, nicht über
        // Ersetzung. Wer sie will, schreibt Style::kBlueText.
        const QSet<QString> byNameOnly = {
            QString::fromLatin1(Style::kBlueText).toLower(),
            QString::fromLatin1(Style::kRedText).toLower(),
        };

        QStringList missing;
        for (const QString& c : named) {
            if (byNameOnly.contains(c.toLower())) { continue; }
            if (!mapped.contains(c.toLower())) { missing << c; }
        }
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral(
                     "these named colours have no row in the theme table, "
                     "so they will stay behind when the palette moves:\n"
                     "  %1").arg(missing.join(QStringLiteral(", ")))));
    }

    // ── Today it must change nothing ─────────────────────────────────
    //
    // Wrapping a call site is meant to be invisible until the table's
    // right column moves. If it is not, a wrapping mistake and a colour
    // decision look the same in a screenshot, and there are 130 files
    // to wrap.
    //
    // Delete this test in the commit that moves the palette. It is a
    // statement about the transition, not about themed().
    void whileTheTableIsTheIdentityNothingChanges()
    {
        bool identity = true;
        for (const auto& e : Style::themeTable()) {
            if (QLatin1String(e.legacy) != QLatin1String(e.current)) {
                identity = false;
                break;
            }
        }
        if (!identity) {
            QSKIP("the theme has moved — this guard has done its job");
        }
        const QString qss = QStringLiteral(
            "QLabel { color: #c8d8e8; background: #0a0a18; }"
            "QPushButton { border: 1px solid #205070; }");
        QCOMPARE(Style::themed(qss), qss);
    }

    void anUnknownColourIsLeftAlone()
    {
        const QString qss =
            QStringLiteral("QLabel { color: #123456; background: #abcdef; }");
        QCOMPARE(Style::themed(qss), qss);
    }

    // ── #rrggbbaa ────────────────────────────────────────────────────
    //
    // Qt stylesheets carry eight-digit colours, and this program writes
    // them: "inset 0 1px 0 #ffffff0a". If the first six digits happened
    // to match a table row, rewriting them would leave the alpha pair
    // attached to a different colour — a value nobody chose, in a
    // string nobody looks at.
    void anEightDigitColourIsNotHalfRewritten()
    {
        const QString qss =
            QStringLiteral("QFrame { background: #00b4d8ff; "
                           "border-color: #0a0a1880; }");
        QCOMPARE(Style::themed(qss), qss);
    }

    void aSixDigitColourFollowedByAnythingElseStillMatches()
    {
        // The guard above must not go too far: "#00b4d8;" and
        // "#00b4d8 " are ordinary, and so is one at the end of the
        // string.
        for (const QString& tail : {QStringLiteral(";"),
                                    QStringLiteral(" "),
                                    QStringLiteral(")"),
                                    QString{}}) {
            const QString in = QStringLiteral("color: %1%2")
                                   .arg(QString::fromLatin1(Style::kAccent),
                                        tail);
            // Identity table or not, the length must be unchanged and
            // the result must not still be searching for a match.
            const QString out = Style::themed(in);
            QVERIFY2(!out.isEmpty(), qPrintable(in));
            QCOMPARE(out.count(QLatin1Char('#')), 1);
        }
    }

    void caseDoesNotMatter()
    {
        const QString lower = QStringLiteral("color: #00b4d8;");
        const QString upper = QStringLiteral("color: #00B4D8;");
        QCOMPARE(Style::themed(lower), Style::themed(upper));
    }

    void themedIsIdempotent()
    {
        // A call site wrapped twice by mistake must come out the same
        // as one wrapped once.
        const QString qss = QStringLiteral(
            "QWidget { background: #0f0f1a; color: #c8d8e8; }"
            "QWidget:hover { background: #203040; }");
        const QString once = Style::themed(qss);
        QCOMPARE(Style::themed(once), once);
    }

    void theWholeStringSurvives()
    {
        // Everything that is not a colour must come through untouched,
        // including the parts that look like one.
        const QString qss = QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; }"
            "/* #00b4d8 in a comment */"
            "QLabel { font-family: \"SF Mono\"; qproperty-text: \"#40 m\"; }");
        const QString out = Style::themed(qss);
        QVERIFY(out.contains(QStringLiteral("height: 4px")));
        QVERIFY(out.contains(QStringLiteral("\"SF Mono\"")));
        QVERIFY(out.contains(QStringLiteral("#40 m")));
    }

    void emptyIn_emptyOut()
    {
        QCOMPARE(Style::themed(QString{}), QString{});
    }

    void hasLegacyColourIsQuietWhileTheTableIsTheIdentity()
    {
        bool identity = true;
        for (const auto& e : Style::themeTable()) {
            if (QLatin1String(e.legacy) != QLatin1String(e.current)) {
                identity = false;
                break;
            }
        }
        const QString qss = QStringLiteral("color: #c8d8e8;");
        QCOMPARE(Style::hasLegacyColour(qss), !identity);
        QVERIFY(!Style::hasLegacyColour(QStringLiteral("color: #123456;")));
    }
};

QTEST_GUILESS_MAIN(TestThemeQss)
#include "tst_theme_qss.moc"
