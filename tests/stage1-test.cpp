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

struct Response { QByteArray body; int status; int delay; bool ignoreAbort; };
class Reply : public QNetworkReply {
    QByteArray body;
    qint64 offset = 0;
    bool ignore;
public:
    Reply(const QNetworkRequest &r, const Response &s, QObject *parent) : QNetworkReply(parent), body(s.body), ignore(s.ignoreAbort) {
        setRequest(r); setUrl(r.url()); open(ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, s.status);
        QTimer::singleShot(s.delay, this, [this]() {
            if (isFinished()) return;
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override {
        if (ignore || isFinished()) return;
        setError(OperationCanceledError, "cancelled");
        setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return body.size() - offset + QNetworkReply::bytesAvailable(); }
    qint64 readData(char *out, qint64 max) override {
        const qint64 n = qMin(max, body.size() - offset);
        if (!n) return -1;
        std::memcpy(out, body.constData() + offset, size_t(n)); offset += n; return n;
    }
};
class Manager : public QNetworkAccessManager {
public:
    QList<Response> responses;
    int requests = 0;
    using QNetworkAccessManager::QNetworkAccessManager;
    QNetworkReply *createRequest(Operation, const QNetworkRequest &r, QIODevice *) override {
        ++requests;
        return new Reply(r, responses.takeFirst(), this);
    }
};
static QByteArray fixture(const QString &code) {
    QJsonObject day{{"ymd","2026-11-12"},{"high",QString::fromUtf8(u8"高温 20.0℃")},
                    {"low",QString::fromUtf8(u8"低温 10.0℃")},{"type",QString::fromUtf8(u8"晴")}};
    QJsonArray days; for (int i=0;i<5;++i) days.append(day);
    return QJsonDocument(QJsonObject{{"status",200},{"message","success"},{"date","20261112"},
        {"cityInfo",QJsonObject{{"citykey",code},{"city","Test"}}},
        {"data",QJsonObject{{"wendu","15"},{"yesterday",day},{"forecast",days}}}}).toJson();
}
#define CHECK(x) do { if (!(x)) { qCritical() << "FAILED" << __LINE__ << #x; return 1; } } while(false)
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
    Weather w(nullptr, cacheDir.path()); w.stopRequest();
    delete w.manager;
    auto manager = new Manager(&w); w.manager = manager;
    QObject::connect(manager, &QNetworkAccessManager::finished, &w, &Weather::replayFinished);
    auto status = w.findChild<QLabel*>("requestStatusLb");
    auto retry = w.findChild<QPushButton*>("retryBt");
    const auto bj = QString::fromUtf8(u8"北京"), sh = QString::fromUtf8(u8"上海");
    manager->responses.append({fixture(w.tool[bj]),200,80,true});
    w.requestWeather(bj);
    const int count = manager->requests;
    w.requestWeather(bj); CHECK(manager->requests == count);
    w.requestWeather("invalid"); CHECK(w.activeReply);
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
    CHECK(w.requestTimer.interval() == 10000);
    w.requestTimer.setInterval(25);
    manager->responses.append({fixture(w.tool[sh]),200,300,false});
    w.requestWeather(sh); QTest::qWait(60);
    CHECK(!w.activeReply); CHECK(w.failedCity == sh);
    CHECK(status->text().contains(QString::fromUtf8(u8"超时")));

    manager->responses.append({"invalid-json",200,10,false});
    w.requestWeather(sh); QTest::qWait(30);
    CHECK(w.displayedCity == sh); CHECK(w.failedCity == sh);
    manager->responses.append({fixture(w.tool[sh]),200,10,false});
    w.on_refreshBt_clicked(); QTest::qWait(30);
    CHECK(w.displayedCity == sh); CHECK(w.failedCity.isEmpty());

    manager->responses.append({fixture(w.tool[sh]),200,100,false});
    w.requestWeather(sh); w.close();
    CHECK(!w.activeReply); CHECK(!w.requestTimer.isActive());
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

