#define main stage1_main
#include "stage1-test.cpp"
#undef main
#include <QFile>
#include <QDir>
#include <QLineEdit>

class Stage2Test {
public:
static int run() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString code = "101010100";
    const auto at = QDateTime::fromString("2026-11-12T02:00:00.000Z", Qt::ISODateWithMs);
    WeatherCache cache(dir.path());
    QString error; CachedWeather value;
    CHECK(cache.save(code, fixture(code), at, error));
    WeatherCache reopened(dir.path());
    CHECK(reopened.load(code,value,error,at));
    CHECK(value.fetchedAt == at && !value.expired);
    CHECK(reopened.load(code,value,error,at.addSecs(1800)) && !value.expired);
    CHECK(reopened.load(code,value,error,at.addMSecs(1800001)) && value.expired);
    CHECK(!reopened.load(code,value,error,at.addSecs(-1)));
    auto midnight = QDateTime::fromString("2026-11-12T15:59:00.000Z",Qt::ISODateWithMs);
    CHECK(cache.save(code,fixture(code),midnight,error));
    CHECK(cache.load(code,value,error,midnight.addSecs(120)) && value.expired);
    CHECK(!cache.save(code,"bad",at,error));
    CHECK(cache.load(code,value,error,midnight) && value.fetchedAt == midnight);
    const QString path = dir.path()+"/"+code+".json";
    QFile file(path); CHECK(file.open(QIODevice::ReadOnly));
    const auto original = file.readAll(); file.close();
    auto write = [&](const QByteArray &bytes) {
        if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate)) return false;
        bool ok = file.write(bytes) == bytes.size(); file.close(); return ok;
    };
    CHECK(write("broken")); CHECK(!cache.load(code,value,error,midnight));
    for (const QString &key : {QString("version"),QString("cityCode"),QString("fetchedAt"),QString("response")}) {
        auto obj = QJsonDocument::fromJson(original).object();
        obj[key] = key == "version" ? QJsonValue(99) : QJsonValue("invalid");
        CHECK(write(QJsonDocument(obj).toJson()));
        CHECK(!cache.load(code,value,error,midnight));
    }
    auto obj = QJsonDocument::fromJson(original).object(); obj.remove("response");
    CHECK(write(QJsonDocument(obj).toJson())); CHECK(!cache.load(code,value,error,midnight));
    CHECK(write(original));
    // A regular file cannot serve as a directory: deterministic write failure.
    WeatherCache blocked(path);
    CHECK(!blocked.save(code,fixture(code),at,error));
    CHECK(cache.load(code,value,error,midnight) && value.fetchedAt == midnight);

    WeatherTool tool;
    const auto bj = QString::fromUtf8(u8"北京"), sh = QString::fromUtf8(u8"上海");
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"WeatherProject","Weather");
    settings.setValue("search/recentCities", QStringList{bj}); settings.sync();
    Weather w(nullptr,dir.path());
    CHECK(w.displayedCity == bj && w.displayedFromCache);
    CHECK(w.displayedFetchedAt == midnight && w.displayedExpired());
    CHECK(w.findChild<QLineEdit*>()->text().isEmpty());
    w.stopRequest(); delete w.manager;
    auto manager = new Manager(&w); w.manager = manager;
    QObject::connect(manager,&QNetworkAccessManager::finished,&w,&Weather::replayFinished);
    auto status = w.findChild<QLabel*>("requestStatusLb");
    for (const Response &response : QList<Response>{{"",0,10,false},{"bad",503,10,false},{"bad",200,10,false}}) {
        manager->responses.append(response); w.requestWeather(bj);
        CHECK(w.displayedFromCache && w.displayedCity == bj);
        QTest::qWait(150); CHECK(w.displayedFromCache && w.failedCity == bj);
        CHECK(status->text().contains(QString::fromUtf8(u8"本地缓存")));
        CHECK(w.searchHistory == QStringList{bj});
    }
    w.requestTimer.setInterval(20);
    manager->responses.append({fixture(code),200,100,false});
    w.requestWeather(bj); QTest::qWait(150);
    CHECK(w.displayedFromCache && w.failedCity == bj);
    w.requestTimer.setInterval(10000);
    manager->responses.append({fixture(code),200,10,false});
    w.findChild<QPushButton*>("retryBt")->click(); QTest::qWait(150);
    CHECK(!w.displayedFromCache && w.failedCity.isEmpty());
    CHECK(cache.load(code,value,error) && value.fetchedAt == w.displayedFetchedAt);
    const auto savedAt = value.fetchedAt;
    manager->responses.append({fixture(code),200,300,true}); w.requestWeather(bj);
    manager->responses.append({fixture(tool[sh]),200,10,false}); w.requestWeather(sh);
    QTest::qWait(400);
    CHECK(w.displayedCity == sh && !w.displayedFromCache);
    CHECK(cache.load(code,value,error) && value.fetchedAt == savedAt);
    CHECK(cache.load(tool[sh],value,error));
    w.cache = WeatherCache(path);
    manager->responses.append({fixture(tool[sh]),200,10,false});
    w.on_refreshBt_clicked(); QTest::qWait(150);
    CHECK(w.displayedCity == sh && !w.displayedFromCache);
    CHECK(status->text().contains(QString::fromUtf8(u8"缓存保存失败")));
    CHECK(cache.load(code,value,error) && value.fetchedAt == savedAt);
    w.close();
    qInfo() << "Stage 2 checks passed";
    return 0;
}};
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir settings;
    if (!settings.isValid()) return 2;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    return Stage2Test::run();
}
