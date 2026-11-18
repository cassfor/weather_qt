#include "../weather.h"
#include <QApplication>
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QTemporaryDir>
#include <QPushButton>
#include <cstring>

#include "TestSupport.h"
class Stage1Test {
public:
static int run() {
    WeatherData data; QString error;
    CHECK(parseWeatherData(fixture("101010100"),"101010100",data,error));
    CHECK(data.today.pm25 == QString::fromUtf8(u8"暂无数据"));
    data.today.city = "preserved";
    CHECK(!parseWeatherData("broken","101010100",data,error));
    CHECK(data.today.city == "preserved");
    CHECK(!parseWeatherData(fixture("101010100"),"wrong",data,error));
    auto missing = QJsonDocument::fromJson(fixture("101010100")).object();
    missing["data"] = QJsonObject();
    CHECK(!parseWeatherData(QJsonDocument(missing).toJson(),"101010100",data,error));
    auto invalid = QJsonDocument::fromJson(fixture("101010100")).object();
    invalid["date"] = "20261399";
    CHECK(!parseWeatherData(QJsonDocument(invalid).toJson(),"101010100",data,error));

    QTemporaryDir cacheDir;
    CHECK(cacheDir.isValid());
    Manager network;
    auto manager = &network;
    manager->responses.append({"",0,100,false});
    Weather w(nullptr, cacheDir.path(), manager); w.stopRequest();
    auto status = w.findChild<QLabel*>("requestStatusLb");
    auto retry = w.findChild<QPushButton*>("retryBt");
    const auto bj = QString::fromUtf8(u8"北京"), sh = QString::fromUtf8(u8"上海");
    manager->responses.append({fixture(w.tool[bj]),200,80,true});
    w.requestWeather(bj);
    const int count = manager->requests;
    w.requestWeather(bj); CHECK(manager->requests == count);
    w.requestWeather("invalid"); CHECK(w.service->isActive());
    manager->responses.append({fixture(w.tool[sh]),200,10,false});
    w.requestWeather(sh); QTest::qWait(130);
    CHECK(w.displayedCity == sh); CHECK(w.searchHistory.first() == sh);
    CHECK(!w.searchHistory.contains(bj));

    manager->responses.append({"failure",503,10,false});
    w.requestWeather(bj); QTest::qWait(30);
    CHECK(w.failedCity == bj); CHECK(w.displayedCity == sh); CHECK(!retry->isHidden());
    CHECK(status->text().contains("503"));
    manager->responses.append({fixture(w.tool[bj]),200,10,false});
    retry->click(); QTest::qWait(30);
    CHECK(w.displayedCity == bj); CHECK(retry->isHidden());

    manager->responses.append({"",0,10,false});
    w.requestWeather(sh); QTest::qWait(30);
    CHECK(w.failedCity == sh); CHECK(w.displayedCity == sh);
    CHECK(status->text().contains(QString::fromUtf8(u8"网络")));
    CHECK(w.service->timeout() == 10000);
    w.service->setTimeout(25);
    manager->responses.append({fixture(w.tool[sh]),200,300,false});
    w.requestWeather(sh); QTest::qWait(60);
    CHECK(!w.service->isActive()); CHECK(w.failedCity == sh);
    CHECK(status->text().contains(QString::fromUtf8(u8"超时")));

    manager->responses.append({"invalid-json",200,10,false});
    w.requestWeather(sh); QTest::qWait(30);
    CHECK(w.displayedCity == sh); CHECK(w.failedCity == sh);
    manager->responses.append({fixture(w.tool[sh]),200,10,false});
    w.on_refreshBt_clicked(); QTest::qWait(30);
    CHECK(w.displayedCity == sh); CHECK(w.failedCity.isEmpty());

    manager->responses.append({fixture(w.tool[sh]),200,100,false});
    w.requestWeather(sh); w.close();
    CHECK(!w.service->isActive()); CHECK(!w.service->isActive());
    QTest::qWait(130);
    qInfo() << "Stage 1 checks passed";
    return 0;
}};
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir dir;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
    return Stage1Test::run();
}

