// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_google_map_tiles.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der eigene Google-Schluessel fuer den 3D-Flug (2026-11-10):
//   * Ablage im Schluesselbund und zurueck (im Pruefbetrieb der Speicher-
//     Tresor von CredentialStore -- der echte Schluesselbund bleibt
//     unberuehrt, siehe CredentialStore.cpp useSessionVaultOnly()),
//   * Googles Absagen -> ein Satz, der den fehlenden Schritt nennt,
//   * die Probe gegen einen oertlichen Kachel-Ersatz (kein Netz nach
//     aussen, kein echter Schluessel),
//   * der Dialog: Anleitung mit allen Schritten und dem Rechnungskonto-
//     Hinweis, „Test", „Save", „Remove key".

#include <QtTest>

#include "core/AppSettings.h"
#include "core/CredentialStore.h"
#include "core/GoogleMapTiles.h"
#include "gui/Google3dSetupDialog.h"
#include "gui/widgets/Station3dSurface.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

using namespace Longpath;

namespace {

// Ein Schluessel in Googles Form, aber erfunden: "AIza" + 35 Zeichen.
const QString kFakeKey = QStringLiteral("AIzaPruefstandNichtEchtXXXXXXXXXXXXXXXX");
// Ein erfundener ion-Zugang in JWT-Form (drei base64url-Teile).
const QString kFakeToken = QStringLiteral(
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJwcnVlZnN0YW5kIn0.UHJ1ZWZzdGFuZE5pY2h0RWNodA");
// Was Cesium ion fuer Googles Asset liefert (externes 3D-Tileset).
const QByteArray kIonEndpoint =
    "{\"type\":\"3DTILES\",\"externalType\":\"3DTILES\","
    "\"options\":{\"url\":\"https://tile.googleapis.com/v1/3dtiles/root.json?key=x\"},"
    "\"attributions\":[]}";

// Antwortet auf genau eine Anfrage mit Status und Koerper und merkt sich
// die erste Anfragezeile.
class FakeTiles : public QObject {
public:
    int status{200};
    QByteArray body;
    QByteArray firstLine;
    QByteArray headers;

    FakeTiles()
    {
        QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket* s = m_server.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                m_buf += s->readAll();
                if (!m_buf.contains("\r\n\r\n")) { return; }
                firstLine = m_buf.left(m_buf.indexOf("\r\n"));
                headers = m_buf.left(m_buf.indexOf("\r\n\r\n"));
                const QByteArray reason = status == 200 ? "OK" : "Error";
                QByteArray resp = "HTTP/1.1 " + QByteArray::number(status) + " " + reason
                    + "\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                s->write(resp);
                s->disconnectFromHost();
            });
        });
    }
    QUrl base() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(m_server.serverPort()));
    }

private:
    QTcpServer m_server;
    QByteArray m_buf;
};

QByteArray googleError(int code, const char* status, const char* reason, const char* message)
{
    return QByteArray("{\"error\":{\"code\":") + QByteArray::number(code)
        + ",\"message\":\"" + message + "\",\"status\":\"" + status
        + "\",\"details\":[{\"@type\":\"type.googleapis.com/google.rpc.ErrorInfo\","
          "\"reason\":\"" + reason + "\",\"domain\":\"googleapis.com\"}]}}";
}

const QByteArray kTileset =
    "{\"asset\":{\"version\":\"1.0\"},\"geometricError\":1e7,"
    "\"root\":{\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
    "\"geometricError\":1e7,\"children\":[]}}";

} // namespace

class TstGoogleMapTiles : public QObject { Q_OBJECT
private slots:
    void init()
    {
        GoogleMapsKey::erase();
        GoogleMapsKey::forgetCachedForTest();
        CesiumIonToken::erase();
        CesiumIonToken::forgetCachedForTest();
        Map3dAccess::setChosenSource(Map3dAccess::Source::CesiumIon);
    }

    void key_goes_to_the_credential_store_and_back()
    {
        QSignalSpy changed(&Map3dAccess::notifier(), &Map3dAccess::changed);
        QVERIFY(!GoogleMapsKey::isSet());

        QVERIFY(GoogleMapsKey::save(QStringLiteral("  ") + kFakeKey + QStringLiteral("\n")));
        QCOMPARE(changed.count(), 1);
        // Getrimmt abgelegt, und zwar im Schluesselbund-Eintrag, nicht
        // nur im Speicher dieser Klasse.
        QCOMPARE(CredentialStore::retrieve(GoogleMapsKey::credentialKey(),
                                           GoogleMapsKey::credentialAccount()), kFakeKey);
        GoogleMapsKey::forgetCachedForTest();
        QCOMPARE(GoogleMapsKey::load(), kFakeKey);
        QVERIFY(GoogleMapsKey::isSet());

        // Leer speichern heisst loeschen.
        QVERIFY(GoogleMapsKey::save(QString()));
        QCOMPARE(changed.count(), 2);
        GoogleMapsKey::forgetCachedForTest();
        QVERIFY(GoogleMapsKey::load().isEmpty());
        QVERIFY(CredentialStore::retrieve(GoogleMapsKey::credentialKey(),
                                          GoogleMapsKey::credentialAccount()).isEmpty());
    }

    void key_shape_hint()
    {
        QVERIFY(GoogleMapsKey::looksLikeApiKey(kFakeKey));
        QVERIFY(GoogleMapsKey::looksLikeApiKey(QStringLiteral(" ") + kFakeKey));
        QVERIFY(!GoogleMapsKey::looksLikeApiKey(kFakeKey.left(30)));   // abgeschnitten
        QVERIFY(!GoogleMapsKey::looksLikeApiKey(QStringLiteral("BIza") + kFakeKey.mid(4)));
        QVERIFY(!GoogleMapsKey::looksLikeApiKey(QString()));
    }

    void root_url_carries_the_key()
    {
        const QUrl u = GoogleTilesProbe::rootTilesetUrl(kFakeKey);
        QCOMPARE(u.scheme(), QStringLiteral("https"));
        QCOMPARE(u.host(), QStringLiteral("tile.googleapis.com"));
        QCOMPARE(u.path(), QStringLiteral("/v1/3dtiles/root.json"));
        QCOMPARE(QUrlQuery(u).queryItemValue(QStringLiteral("key")), kFakeKey);
    }

    void google_answers_become_sentences_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("verdict");
        QTest::addColumn<QString>("mentions");
        using V = GoogleTilesProbe::Verdict;

        QTest::newRow("ok") << 200 << kTileset << int(V::Ok) << QStringLiteral("accepted");
        QTest::newRow("portal") << 200 << QByteArray("<html>login</html>")
                                << int(V::Other) << QStringLiteral("refused");
        QTest::newRow("invalid") << 400
            << googleError(400, "INVALID_ARGUMENT", "API_KEY_INVALID",
                           "API key not valid. Please pass a valid API key.")
            << int(V::InvalidKey) << QStringLiteral("AIza");
        QTest::newRow("disabled") << 403
            << googleError(403, "PERMISSION_DENIED", "SERVICE_DISABLED",
                           "Map Tiles API has not been used in project 123 before or it is disabled.")
            << int(V::ApiNotEnabled) << QStringLiteral("step 3");
        QTest::newRow("api-restricted") << 403
            << googleError(403, "PERMISSION_DENIED", "API_KEY_SERVICE_BLOCKED",
                           "Requests to this API tile.googleapis.com method are blocked.")
            << int(V::KeyRestricted) << QStringLiteral("API restrictions");
        QTest::newRow("referer") << 403
            << googleError(403, "PERMISSION_DENIED", "API_KEY_HTTP_REFERRER_BLOCKED",
                           "Requests from referer <empty> are blocked.")
            << int(V::AppRestricted) << QStringLiteral("Application restrictions");
        QTest::newRow("billing") << 403
            << googleError(403, "PERMISSION_DENIED", "BILLING_DISABLED",
                           "This API method requires billing to be enabled.")
            << int(V::NoBilling) << QStringLiteral("even for the free");
        QTest::newRow("quota") << 429
            << googleError(429, "RESOURCE_EXHAUSTED", "RATE_LIMIT_EXCEEDED", "Quota exceeded.")
            << int(V::Quota) << QStringLiteral("quota");
        // Ohne "details" (aeltere Antwortform) entscheidet der Text --
        // und "are blocked" steht in ZWEI verschiedenen Absagen.
        QTest::newRow("text-referer") << 403
            << QByteArray("{\"error\":{\"code\":403,\"message\":\"Requests from referer "
                          "<empty> are blocked.\",\"status\":\"PERMISSION_DENIED\"}}")
            << int(V::AppRestricted) << QStringLiteral("None");
        QTest::newRow("text-service") << 403
            << QByteArray("{\"error\":{\"code\":403,\"message\":\"Requests to this API "
                          "tile.googleapis.com method google.maps.tiles are blocked.\","
                          "\"status\":\"PERMISSION_DENIED\"}}")
            << int(V::KeyRestricted) << QStringLiteral("Map Tiles");
    }
    void google_answers_become_sentences()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, verdict);
        QFETCH(QString, mentions);
        QString detail;
        const GoogleTilesProbe::Verdict v = GoogleTilesProbe::interpret(status, body, &detail);
        QCOMPARE(int(v), verdict);
        const QString s = GoogleTilesProbe::sentence(v, detail);
        QVERIFY2(s.contains(mentions, Qt::CaseInsensitive), qPrintable(s));
    }

    void probe_asks_the_tiles_endpoint()
    {
        FakeTiles fake;
        fake.status = 403;
        fake.body = googleError(403, "PERMISSION_DENIED", "SERVICE_DISABLED",
                                "Map Tiles API has not been used in project 1.");
        GoogleTilesProbe probe;
        probe.setEndpointForTest(fake.base());
        QSignalSpy done(&probe, &GoogleTilesProbe::finished);
        probe.check(kFakeKey);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QCOMPARE(done.at(0).at(0).value<GoogleTilesProbe::Verdict>(),
                 GoogleTilesProbe::Verdict::ApiNotEnabled);
        QVERIFY2(fake.firstLine.startsWith("GET /v1/3dtiles/root.json?key=" + kFakeKey.toLatin1()),
                 fake.firstLine.constData());

        FakeTiles ok;
        ok.body = kTileset;
        probe.setEndpointForTest(ok.base());
        probe.check(kFakeKey);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 10000);
        QCOMPARE(done.at(1).at(0).value<GoogleTilesProbe::Verdict>(),
                 GoogleTilesProbe::Verdict::Ok);
    }

    void probe_without_key_does_not_ask()
    {
        GoogleTilesProbe probe;
        probe.setEndpointForTest(QUrl(QStringLiteral("http://127.0.0.1:9/")));
        QSignalSpy done(&probe, &GoogleTilesProbe::finished);
        probe.check(QStringLiteral("   "));
        QCOMPARE(done.count(), 1);
        QCOMPARE(done.at(0).at(0).value<GoogleTilesProbe::Verdict>(),
                 GoogleTilesProbe::Verdict::Empty);
    }

    void guide_names_every_step()
    {
        const QString g = Google3dSetupDialog::guideHtml(Map3dAccess::Source::Google);
        for (const char* must : {"Create a project", "console.cloud.google.com",
                                 "billing account", "even for the free",
                                 "Enable the Map Tiles API", "tile.googleapis.com",
                                 "Create an API key", "Restrict the key",
                                 "API restrictions", "Map Tiles API",
                                 "Application restrictions", "Test", "Save"}) {
            QVERIFY2(g.contains(QLatin1String(must)), must);
        }
        // Die Reihenfolge, in der Google sie verlangt.
        QVERIFY(g.indexOf(QLatin1String("Create a project"))
                < g.indexOf(QLatin1String("billing account")));
        QVERIFY(g.indexOf(QLatin1String("billing account"))
                < g.indexOf(QLatin1String("Enable the Map Tiles API")));
        QVERIFY(g.indexOf(QLatin1String("Enable the Map Tiles API"))
                < g.indexOf(QLatin1String("Create an API key")));
        QVERIFY(g.indexOf(QLatin1String("Create an API key"))
                < g.indexOf(QLatin1String("Restrict the key")));
    }

    void dialog_tests_saves_and_removes()
    {
        FakeTiles fake;
        fake.body = kTileset;

        auto* dlg = new Google3dSetupDialog;
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->probeForTest()->setEndpointForTest(fake.base());
        dlg->show();
        QVERIFY(QTest::qWaitForWindowExposed(dlg));
        // Vorgabe ist Cesium ion; hier der Google-Weg.
        QCOMPARE(dlg->shownSource(), Map3dAccess::Source::CesiumIon);
        dlg->googleChoiceForTest()->click();
        QCOMPARE(dlg->shownSource(), Map3dAccess::Source::Google);

        // Mit LONGPATH_GRAB_DIR: das Bild des Dialogs, wie der Bediener
        // ihn zuerst sieht (das Blatt, das kein Pruefstand ersetzt).
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(dlg->grab().save(grabDir + QStringLiteral("/longpath-grab-Google3dSetupDialog.png")));
        }

        // Die Anleitung steht im Dialog, nicht nur in einer Funktion.
        auto* guide = dlg->findChild<QLabel*>(QStringLiteral("google3dGuide"));
        QVERIFY(guide);
        QVERIFY(guide->text().contains(QLatin1String("even for the free")));
        QVERIFY(guide->openExternalLinks());

        // Ohne angemeldete WebEngine-Flaeche sagt der Dialog es.
        QCOMPARE(Station3dSurface::isAvailable(), false);
        QVERIFY(dlg->findChild<QLabel*>(QStringLiteral("google3dNoWebEngine")));

        // Leeres Feld: kein Netz, ein Satz.
        dlg->testButtonForTest()->click();
        QVERIFY(dlg->statusForTest()->text().contains(QLatin1String("Paste your API key")));

        // Abgeschnitten eingefuegt: der Hinweis kommt sofort.
        dlg->keyEditForTest()->setText(kFakeKey.left(20));
        QVERIFY(dlg->statusForTest()->text().contains(QLatin1String("copied completely")));

        dlg->keyEditForTest()->setText(kFakeKey);
        dlg->testButtonForTest()->click();
        QTRY_VERIFY_WITH_TIMEOUT(
            dlg->statusForTest()->text().contains(QLatin1String("Key accepted")), 10000);
        QVERIFY(!GoogleMapsKey::isSet());   // Test allein speichert nichts

        QPointer<Google3dSetupDialog> alive(dlg);
        dlg->saveButtonForTest()->click();
        QCOMPARE(GoogleMapsKey::load(), kFakeKey);
        // Gespeichert heisst auch: diese Quelle gilt jetzt.
        QCOMPARE(Map3dAccess::chosenSource(), Map3dAccess::Source::Google);
        QCOMPARE(Map3dAccess::secret(), kFakeKey);
        QVERIFY(CesiumIonToken::load().isEmpty());
        QTRY_VERIFY(alive.isNull());   // geschlossen und freigegeben

        // Wieder auf: die Quelle von zuletzt, der Schluessel steht drin,
        // „Remove" loescht ihn.
        Google3dSetupDialog* again = Google3dSetupDialog::showFor(nullptr);
        QCOMPARE(again->shownSource(), Map3dAccess::Source::Google);
        QCOMPARE(again->keyEditForTest()->text(), kFakeKey);
        QVERIFY(again->removeButtonForTest()->isEnabled());
        again->removeButtonForTest()->click();
        QVERIFY(!GoogleMapsKey::isSet());
        QVERIFY(again->keyEditForTest()->text().isEmpty());
        // showFor() holt denselben offenen Dialog statt eines zweiten.
        QCOMPARE(Google3dSetupDialog::showFor(nullptr), again);
        again->close();
    }

    // ── Cesium ion: der kostenlose Weg (2026-11-10, abends) ─────────────

    void ion_token_goes_to_the_credential_store_and_back()
    {
        QSignalSpy changed(&Map3dAccess::notifier(), &Map3dAccess::changed);
        QVERIFY(!Map3dAccess::isSet());
        QVERIFY(CesiumIonToken::save(QStringLiteral(" ") + kFakeToken));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(CredentialStore::retrieve(CesiumIonToken::credentialKey(),
                                           CesiumIonToken::credentialAccount()), kFakeToken);
        CesiumIonToken::forgetCachedForTest();
        QCOMPARE(CesiumIonToken::load(), kFakeToken);
        // Vorgabe-Quelle ist ion: der Zugang gilt sofort.
        QCOMPARE(Map3dAccess::chosenSource(), Map3dAccess::Source::CesiumIon);
        QCOMPARE(Map3dAccess::secret(), kFakeToken);
        // Ein Google-Schluessel daneben aendert nichts, solange ion gewaehlt ist.
        GoogleMapsKey::save(kFakeKey);
        QCOMPARE(Map3dAccess::secret(), kFakeToken);
        Map3dAccess::setChosenSource(Map3dAccess::Source::Google);
        QCOMPARE(Map3dAccess::secret(), kFakeKey);
        Map3dAccess::setChosenSource(Map3dAccess::Source::CesiumIon);

        QVERIFY(CesiumIonToken::save(QString()));
        CesiumIonToken::forgetCachedForTest();
        QVERIFY(CesiumIonToken::load().isEmpty());
        QVERIFY(!Map3dAccess::isSet());
    }

    // Die VORGABE, ohne dass je jemand gewaehlt hat: Cesium ion -- der
    // Weg ohne Kreditkarte (Betreiber 2026-11-10). Die anderen Faelle
    // setzen die Quelle ausdruecklich; dieser loescht sie.
    void default_source_is_cesium_ion()
    {
        AppSettings::instance().remove(QStringLiteral("Map3dSource"));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Map3dSource")));
        QCOMPARE(Map3dAccess::chosenSource(), Map3dAccess::Source::CesiumIon);

        auto* dlg = new Google3dSetupDialog;
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        QCOMPARE(dlg->shownSource(), Map3dAccess::Source::CesiumIon);
        QVERIFY(dlg->ionChoiceForTest()->isChecked());
        QVERIFY(dlg->guideForTest()->text().contains(QLatin1String("ion.cesium.com")));
        dlg->close();
    }

    void ion_token_shape_hint()
    {
        QVERIFY(CesiumIonToken::looksLikeToken(kFakeToken));
        QVERIFY(!CesiumIonToken::looksLikeToken(kFakeToken.left(40)));   // abgeschnitten
        QVERIFY(!CesiumIonToken::looksLikeToken(kFakeKey));              // Google-Schluessel
        QVERIFY(!CesiumIonToken::looksLikeToken(QString()));
    }

    void ion_answers_become_sentences_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("verdict");
        QTest::addColumn<QString>("mentions");
        using V = GoogleTilesProbe::Verdict;
        QTest::newRow("ok-external") << 200 << kIonEndpoint << int(V::Ok)
                                     << QStringLiteral("Token accepted");
        QTest::newRow("ok-own") << 200
            << QByteArray("{\"type\":\"3DTILES\",\"url\":\"https://assets.ion.cesium.com/1/tileset.json\","
                          "\"accessToken\":\"x\"}")
            << int(V::Ok) << QStringLiteral("Token accepted");
        QTest::newRow("portal") << 200 << QByteArray("<html>login</html>")
                                << int(V::Other) << QStringLiteral("refused");
        // So am 2026-11-10 gemessen, mit einem erfundenen Zugang.
        QTest::newRow("invalid") << 401
            << QByteArray("{\"code\":\"INVALID_TOKEN\",\"message\":\"Invalid access token\"}")
            << int(V::InvalidKey) << QStringLiteral("eyJ");
        QTest::newRow("missing") << 404
            << QByteArray("{\"code\":\"ResourceNotFound\",\"message\":\"Asset not found\"}")
            << int(V::AssetMissing) << QStringLiteral("My Assets");
        QTest::newRow("scope") << 403
            << QByteArray("{\"code\":\"Forbidden\",\"message\":\"Insufficient scope\"}")
            << int(V::AssetMissing) << QStringLiteral("read assets");
        QTest::newRow("quota") << 429
            << QByteArray("{\"code\":\"TooManyRequests\",\"message\":\"Slow down\"}")
            << int(V::Quota) << QStringLiteral("next month");
    }
    void ion_answers_become_sentences()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, verdict);
        QFETCH(QString, mentions);
        QString detail;
        const GoogleTilesProbe::Verdict v = GoogleTilesProbe::interpretIon(status, body, &detail);
        QCOMPARE(int(v), verdict);
        const QString s = GoogleTilesProbe::sentenceIon(v, detail);
        QVERIFY2(s.contains(mentions, Qt::CaseInsensitive), qPrintable(s));
    }

    void ion_probe_asks_asset_2275207_with_the_token_in_the_header()
    {
        FakeTiles fake;
        fake.status = 401;
        fake.body = "{\"code\":\"INVALID_TOKEN\",\"message\":\"Invalid access token\"}";
        GoogleTilesProbe probe;
        probe.setIonEndpointForTest(fake.base());
        QSignalSpy done(&probe, &GoogleTilesProbe::finished);
        probe.check(Map3dAccess::Source::CesiumIon, kFakeToken);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QCOMPARE(done.at(0).at(0).value<GoogleTilesProbe::Verdict>(),
                 GoogleTilesProbe::Verdict::InvalidKey);
        QVERIFY2(done.at(0).at(1).toString().contains(QLatin1String("Cesium ion")),
                 qPrintable(done.at(0).at(1).toString()));
        QVERIFY2(fake.firstLine.startsWith("GET /v1/assets/2275207/endpoint "),
                 fake.firstLine.constData());
        // Der Zugang steht im Kopf, nicht in der Adresse.
        QVERIFY(!fake.firstLine.contains(kFakeToken.toLatin1()));
        // Kopfnamen sind nach RFC 9110 gross/klein-unabhaengig; Qt unter
        // Linux schickt "authorization:", unter macOS "Authorization:"
        // (CI #422, Linux 3/4). Der Name wird darum klein verglichen,
        // der Wert (Bearer + Zugang) wortgetreu.
        QVERIFY2(fake.headers.toLower().contains("authorization: bearer ")
                     && fake.headers.contains("Bearer " + kFakeToken.toLatin1()),
                 fake.headers.constData());
    }

    void guide_for_ion_names_every_step()
    {
        const QString g = Google3dSetupDialog::guideHtml(Map3dAccess::Source::CesiumIon);
        for (const char* must : {"Create a free account", "ion.cesium.com/signup",
                                 "Community", "non-commercial", "no payment details",
                                 "Access Tokens", "ion.cesium.com/tokens", "eyJ",
                                 "My Assets", "Google Photorealistic 3D Tiles",
                                 "assetdepot/2275207", "Test", "Save"}) {
            QVERIFY2(g.contains(QLatin1String(must)), must);
        }
        QVERIFY(!g.contains(QLatin1String("billing")));
    }

    void dialog_with_ion_tests_and_saves()
    {
        FakeTiles fake;
        fake.body = kIonEndpoint;
        auto* dlg = new Google3dSetupDialog;
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->probeForTest()->setIonEndpointForTest(fake.base());
        dlg->show();
        QVERIFY(QTest::qWaitForWindowExposed(dlg));

        // Vorgabe: Cesium ion, mit der ion-Anleitung.
        QVERIFY(dlg->ionChoiceForTest()->isChecked());
        QVERIFY(dlg->guideForTest()->text().contains(QLatin1String("ion.cesium.com")));
        QVERIFY(dlg->keyEditForTest()->placeholderText().contains(QLatin1String("Cesium ion")));

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(dlg->grab().save(grabDir + QStringLiteral("/longpath-grab-Google3dSetupDialog-ion.png")));
        }

        dlg->testButtonForTest()->click();
        QVERIFY(dlg->statusForTest()->text().contains(QLatin1String("Cesium ion token first")));
        dlg->keyEditForTest()->setText(kFakeKey);   // Google-Schluessel ins ion-Feld
        QVERIFY(dlg->statusForTest()->text().contains(QLatin1String("Cesium ion token")));

        dlg->keyEditForTest()->setText(kFakeToken);
        dlg->testButtonForTest()->click();
        QTRY_VERIFY_WITH_TIMEOUT(
            dlg->statusForTest()->text().contains(QLatin1String("Token accepted")), 10000);
        QVERIFY(!CesiumIonToken::isSet());

        // Umschalten und zurueck verliert nichts und mischt nichts.
        dlg->googleChoiceForTest()->click();
        QVERIFY(dlg->keyEditForTest()->text().isEmpty());
        QVERIFY(dlg->guideForTest()->text().contains(QLatin1String("billing")));
        dlg->ionChoiceForTest()->click();

        dlg->keyEditForTest()->setText(kFakeToken);
        QPointer<Google3dSetupDialog> alive(dlg);
        dlg->saveButtonForTest()->click();
        QCOMPARE(CesiumIonToken::load(), kFakeToken);
        QVERIFY(GoogleMapsKey::load().isEmpty());
        QCOMPARE(Map3dAccess::chosenSource(), Map3dAccess::Source::CesiumIon);
        QCOMPARE(Map3dAccess::secret(), kFakeToken);
        QTRY_VERIFY(alive.isNull());
    }
};

QTEST_MAIN(TstGoogleMapTiles)
#include "tst_google_map_tiles.moc"
