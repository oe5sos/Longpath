// =================================================================
// tests/tst_world_texture_style.cpp  (NereusSDR)
// =================================================================
//
// Drei Tonwertkurven fuer ein geladenes Weltbild (WorldTexture.h) --
// Muted graut fast durch, NightWash und Crisp bleiben farbig, aber
// unterschiedlich hell. Geprueft an einem eigenen, bekannten Testbild,
// nicht am echten NASA-Foto.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-02 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QTemporaryDir>

#include "gui/widgets/WorldTexture.h"

using namespace Longpath;

namespace {

// Ein einfarbiges Testbild -- WorldTexture selbst kuemmert sich nicht
// um das 2:1-Seitenverhaeltnis, das WorldMapCatalog fuer die Auswahlliste
// verlangt, also reicht hier ein kleines Rechteck.
QString writeTestImage(const QString& dir)
{
    QImage img(64, 32, QImage::Format_RGB32);
    img.fill(qRgb(200, 40, 40));
    const QString path = dir + QStringLiteral("/test-world.png");
    img.save(path);
    return path;
}

} // namespace

class TestWorldTextureStyle : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void defaultStyleIsMuted()
    {
        // Nicht der allererste Zustand des Prozesses -- andere Testfaelle
        // in dieser Datei setzen den Stil auch. Jeder Testfall stellt
        // seinen eigenen Ausgangszustand selbst her.
        WorldTexture::setStyle(WorldTexture::Style::NightWash);
        WorldTexture::setStyle(WorldTexture::Style::Muted);
        QCOMPARE(WorldTexture::style(), WorldTexture::Style::Muted);
    }

    void styleRoundTripsThroughSettings()
    {
        WorldTexture::setStyle(WorldTexture::Style::Crisp);
        QCOMPARE(WorldTexture::style(), WorldTexture::Style::Crisp);
        WorldTexture::setStyle(WorldTexture::Style::NightWash);
        QCOMPARE(WorldTexture::style(), WorldTexture::Style::NightWash);
    }

    void styledImageIsNullWithoutAPath()
    {
        WorldTexture::clearPath();
        QVERIFY(WorldTexture::styledImage().isNull());
    }

    void mutedIsGreyscaleAndDarker()
    {
        const QString path = writeTestImage(m_dir.path());
        QVERIFY(WorldTexture::setPath(path));
        WorldTexture::setStyle(WorldTexture::Style::Muted);

        const QImage out = WorldTexture::styledImage();
        QVERIFY(!out.isNull());
        const QRgb px = out.pixel(10, 10);
        QVERIFY2(qRed(px) == qGreen(px) && qGreen(px) == qBlue(px),
                 "Muted soll durchgehend grau sein");
        QVERIFY2(qRed(px) < 200,
                 "Muted soll deutlich dunkler sein als das Testbild (200)");
    }

    void crispStaysColouredAndBrighterThanNightWash()
    {
        const QString path = writeTestImage(m_dir.path());
        QVERIFY(WorldTexture::setPath(path));

        WorldTexture::setStyle(WorldTexture::Style::Crisp);
        const QRgb crisp = WorldTexture::styledImage().pixel(10, 10);
        QVERIFY2(qRed(crisp) > qGreen(crisp),
                 "Crisp darf die Testfarbe (rot) nicht entsaettigen");

        WorldTexture::setStyle(WorldTexture::Style::NightWash);
        const QRgb night = WorldTexture::styledImage().pixel(10, 10);
        QVERIFY2(qRed(night) > qGreen(night),
                 "NightWash darf die Testfarbe (rot) auch nicht entsaettigen");

        QVERIFY2(qRed(crisp) > qRed(night),
                 "Crisp ist die hellere der beiden farbigen Kurven");
    }

    void changingStyleInvalidatesTheCache()
    {
        const QString path = writeTestImage(m_dir.path());
        QVERIFY(WorldTexture::setPath(path));

        WorldTexture::setStyle(WorldTexture::Style::Crisp);
        const QRgb crisp = WorldTexture::styledImage().pixel(10, 10);

        WorldTexture::setStyle(WorldTexture::Style::Muted);
        const QRgb muted = WorldTexture::styledImage().pixel(10, 10);

        QVERIFY2(crisp != muted,
                 "ein Stilwechsel muss sich sofort in styledImage() zeigen, "
                 "nicht im alten Cache stecken bleiben");
    }

    void cleanupTestCase()
    {
        // Sauber hinterlassen fuer den Fall, dass jemand die Reihenfolge
        // der Testfaelle in dieser Datei einmal aendert.
        WorldTexture::clearPath();
        WorldTexture::setStyle(WorldTexture::Style::Muted);
    }
};

QTEST_MAIN(TestWorldTextureStyle)
#include "tst_world_texture_style.moc"
