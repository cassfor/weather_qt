#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
#include "../WeatherService.h"
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

#endif
