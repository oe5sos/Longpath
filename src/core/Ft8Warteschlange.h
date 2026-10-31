#pragma once

// =================================================================
// src/core/Ft8Warteschlange.h  (Longpath)
// =================================================================
//
// Wer mich gerufen hat und noch drankommt — und wer gerade dran war
// und darum NICHT noch einmal drankommt.
//
// Longpath-original, aber nach MSHVs Vorbild: dessen
// `hvmultianswermodw.h` [2.76.7, LZ2HV] haelt `LsQueue` neben
// `LsNow`. Die Schlange ist dort an die Plaetze gebunden.
//
// ── Warum es das zusaetzlich geben muss ─────────────────────────────
//
// Die Schlange in `Ft8Mehrfach` laeuft nur mit mehr als einem Platz
// (`Ft8Applet::mehrfachAn()` ist `m_plaetze > 1`). Mit einem Platz —
// der Voreinstellung — gibt es sie nicht, und wer waehrend eines
// laufenden QSOs anruft, faellt zu Boden.
//
// Am 2026-10-30 im Betrieb gemessen. Waehrend des QSOs mit HB9ZHK:
//
//     124945  12 1.2 1338  OE5SOS PD4RJ JO33
//     125015  17 1.2 1338  OE5SOS PD4RJ JO33
//     125045  13 1.2 1338  OE5SOS PD4RJ JO33
//     125015  11 1.0 1428  OE5SOS 9A6T -06
//     125045  14 1.0 1428  OE5SOS 9A6T -06
//
// Fuenf Anrufe von zwei Stationen, keiner beantwortet. Stattdessen
// ging dieselbe Sendung viermal an HB9ZHK, der laengst fertig war.
// Betreiber danach: „Antworten muessen immer automatisch sofort
// funktionieren, ohne meine Beihilfe."
//
// ── Die beiden Listen ───────────────────────────────────────────────
//
// `Schlange`  wer gerufen hat und wartet — der naechste Partner
// `Karenz`    wer gerade gearbeitet wurde — eine Weile tabu
//
// Die Karenz ist der zweite Riegel gegen das Doppel-QSO. Der erste
// sitzt in `Ft8Ablauf::verarbeite` (ein „73" ist kein Anruf); dieser
// hier faengt auch den Fall, dass die Gegenstation nach dem RR73
// wirklich noch einmal regulaer anruft, weil sie meinen Schluss nicht
// gehoert hat.
//
// ── Warum Karenz und nicht Sperre ───────────────────────────────────
//
// Eine Liste, die nie vergisst, verbietet irgendwann das halbe Band.
// Ein QSO auf demselben Band eine Stunde spaeter ist ein neues QSO —
// das Rufzeichen ist dann nur noch Geschichte, kein Hindernis. Dasselbe
// Argument wie bei `m_toeneJePeriode` im Applet: ein Gedaechtnis
// braucht ein Ende.
//
// ── Keine Uhr ───────────────────────────────────────────────────────
//
// Die Zeit kommt von aussen herein, wie bei `Ft8Stream`. Eine Klasse
// mit eigener Uhr laesst sich nur in Echtzeit pruefen, und eine
// Karenz von zehn Minuten waere dann ein Prueflauf von zehn Minuten.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QString>
#include <QVector>

namespace Longpath {

class Ft8Warteschlange
{
public:
    struct Wartend {
        QString partner;
        int     snr{0};
        QString locator;
        qint64  seitMs{0};
    };

    /// Wie lange ein Anruf gilt. Wer vor fuenf Minuten gerufen hat,
    /// sitzt laengst auf einer anderen Frequenz; ihn dann anzurufen
    /// heisst, zwanzig Sekunden in eine Leere zu senden.
    static constexpr qint64 kHaltbarkeitMs = 5 * 60 * 1000;

    /// Wie lange eine fertig gearbeitete Station nicht von selbst
    /// wieder angenommen wird.
    static constexpr qint64 kKarenzMs = 10 * 60 * 1000;

    /// Mehr Anrufer merkt sich niemand. Kaeme je einer dazu, faellt
    /// der aelteste heraus -- nicht der neueste: wer eben erst rief,
    /// ist am ehesten noch da.
    static constexpr int kMaxWartende = 16;

    /// Jemand hat mich gerufen. Gibt true, wenn er neu in die
    /// Schlange kam. Ein Rufzeichen, das schon wartet, rutscht nicht
    /// nach vorn -- aber sein Rapport wird aufgefrischt, denn der
    /// neuere ist der richtige.
    bool merkeAnruf(const QString& partner, int snr,
                    const QString& locator, qint64 jetztMs);

    /// Mit dieser Station ist ein QSO fertig. Sie faellt aus der
    /// Schlange und kommt in die Karenz.
    void merkeGearbeitet(const QString& partner, qint64 jetztMs);

    /// Steht sie noch in der Karenz?
    bool inKarenz(const QString& partner, qint64 jetztMs) const;

    /// Den naechsten Wartenden HERAUSNEHMEN. Abgelaufene und solche in
    /// Karenz werden dabei uebergangen und entfernt. Leerer `partner`
    /// heisst: niemand wartet.
    Wartend nimmNaechsten(qint64 jetztMs);

    /// Wie viele gueltig warten (ohne abgelaufene). Veraendert nichts.
    int wartende(qint64 jetztMs) const;

    /// Dieses Rufzeichen aus der Schlange nehmen, ohne es zu arbeiten
    /// -- fuer den Fall, dass es gerade zum Partner geworden ist.
    void vergiss(const QString& partner);

    void leeren();

    /// Nur fuer Prueflaeufe und Anzeige.
    QVector<Wartend> alle() const { return m_schlange; }

private:
    static QString schluessel(const QString& ruf) { return ruf.toUpper(); }

    QVector<Wartend>       m_schlange;
    QHash<QString, qint64> m_gearbeitet;
};

}  // namespace Longpath
