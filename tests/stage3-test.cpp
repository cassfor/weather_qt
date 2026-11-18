#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSettings>
#include "../SearchHistory.h"
#include "../WeatherService.h"

static int runModules() {
    QStringList names;
    for (int i = 0; i < 25; ++i) names.append(QString::number(i));
    SearchHistory history(names);
    CHECK(history.entries().isEmpty());
    for (const auto &name : names) CHECK(history.record(name));
    CHECK(history.entries().size() == 20);
    CHECK(history.entries().first() == "24");
    CHECK(history.record("10"));
    CHECK(history.entries().first() == "10");
    CHECK(history.entries().count("10") == 1);
    CHECK(!history.record("invalid"));
    CHECK(SearchHistory(names).entries() == history.entries());
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"WeatherProject","Weather");
    settings.setValue("search/recentCities",QStringList{"invalid","1","1","2"});
    settings.sync();
    CHECK(SearchHistory(names).entries() == (QStringList{"1","2"}));

    Manager network;
    WeatherService service(nullptr, &network);
    int successes = 0, failures = 0;
    QString received, message;
    QObject::connect(&service,&WeatherService::succeeded,
        [&](const WeatherData &, const QByteArray &bytes, const QDateTime &at) {
            ++successes;
            received = QJsonDocument::fromJson(bytes).object()["cityInfo"].toObject()["citykey"].toString();
            Q_ASSERT(at.isValid());
        });
    QObject::connect(&service,&WeatherService::failed,[&](const QString &error) { ++failures; message = error; });
    network.responses.append({fixture("101010100"),200,100,true});
    service.fetch("101010100"); service.fetch("101010100");
    CHECK(network.requests == 1);
    network.responses.append({fixture("101020100"),200,10,false});
    service.fetch("101020100"); QTest::qWait(150);
    CHECK(successes == 1 && failures == 0 && received == "101020100");
    network.responses.append({fixture("101010100"),200,10,false});
    service.fetch("101010100"); service.cancel(); QTest::qWait(30);
    CHECK(successes == 1 && failures == 0 && !service.isActive());
    service.setTimeout(20);
    network.responses.append({fixture("101010100"),200,100,false});
    service.fetch("101010100"); QTest::qWait(60);
    CHECK(failures == 1 && message.contains(QString::fromUtf8(u8"超时")));
    service.setTimeout(10000);
    for (const Response &response : QList<Response>{{"",0,10,false},{"",503,10,false},{"bad",200,10,false},{fixture("wrong"),200,10,false}}) {
        network.responses.append(response); service.fetch("101010100"); QTest::qWait(30);
    }
    CHECK(failures == 5 && successes == 1);
    network.responses.append({fixture("101010100"),200,30,true});
    auto temporary = new WeatherService(nullptr, &network);
    int afterDestruction = 0;
    QObject::connect(temporary,&WeatherService::succeeded,[&]() { ++afterDestruction; });
    temporary->fetch("101010100"); delete temporary; QTest::qWait(60);
    CHECK(afterDestruction == 0);
    qInfo() << "Stage 3 module checks passed (QCoreApplication, no window)";
    return 0;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir settings;
    if (!settings.isValid()) return 2;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    return runModules();
}
