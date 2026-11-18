#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include "WeatherData.h"

// Owns request lifetime and validation; has no dependency on widgets or storage.
class WeatherService : public QObject {
    Q_OBJECT
public:
    explicit WeatherService(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    ~WeatherService() override;
    void fetch(const QString &code);
    void cancel();
    bool isActive(const QString &code = QString()) const;
    void setTimeout(int milliseconds);
    int timeout() const { return requestTimer.interval(); }
signals:
    void succeeded(const WeatherData &data, const QByteArray &response, const QDateTime &fetchedAt);
    void failed(const QString &message);
private:
    void replyFinished(QNetworkReply *reply);
    QNetworkAccessManager *manager;
    QPointer<QNetworkReply> activeReply;
    QTimer requestTimer;
    QElapsedTimer elapsed;
    QString activeCode;
    quint64 requestId = 0;
    bool timedOut = false;
};
#endif
