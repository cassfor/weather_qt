#include "WeatherService.h"
#include <QNetworkRequest>
#include <QDebug>

WeatherService::WeatherService(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), manager(network ? network : new QNetworkAccessManager(this)) {
    requestTimer.setSingleShot(true);
    requestTimer.setInterval(10000);
    connect(&requestTimer, &QTimer::timeout, this, [this]() {
        if (activeReply) { timedOut = true; activeReply->abort(); }
    });
}
WeatherService::~WeatherService() { cancel(); }
void WeatherService::setTimeout(int milliseconds) { requestTimer.setInterval(qMax(1, milliseconds)); }
bool WeatherService::isActive(const QString &code) const {
    return activeReply && (code.isEmpty() || activeCode == code);
}
void WeatherService::fetch(const QString &code) {
    if (isActive(code)) return;
    cancel();
    activeCode = code;
    timedOut = false;
    elapsed.start();
    activeReply = manager->get(QNetworkRequest(QUrl("http://t.weather.itboy.net/api/weather/city/" + code)));
    activeReply->setProperty("requestId", QVariant::fromValue(requestId));
    QNetworkReply *reply = activeReply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { replyFinished(reply); });
    qInfo() << "[Weather] Request" << requestId << code << activeReply->url();
    requestTimer.start();
}
void WeatherService::cancel()
{
    ++requestId; // abort 可能同步发出 finished，必须先使旧请求失效。
    requestTimer.stop();
    auto previous = activeReply;
    activeReply.clear();
    if (previous) previous->abort();
}

void WeatherService::replyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    const auto id = reply->property("requestId").toULongLong();
    if (id != requestId || reply != activeReply) {
        qInfo() << "[Weather] Ignored obsolete request" << id;
        return;
    }
    requestTimer.stop();
    activeReply.clear();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray bytes = reply->readAll();
    qInfo().noquote() << "[Weather] Response" << id << activeCode << "elapsed ms:" << elapsed.elapsed()
                      << "HTTP:" << status << "Network error:" << int(reply->error());
    qInfo().noquote() << "[Weather] Body:\n" << QString::fromUtf8(bytes);
    QString error;
    QString category;
    WeatherData parsed;
    if (timedOut) {
        category = "timeout"; error = QString::fromUtf8(u8"请求超时，请稍后重试");
    } else if (status != 0 && status != 200) {
        category = "http"; error = QString::fromUtf8(u8"天气服务暂时不可用（HTTP %1）").arg(status);
    } else if (reply->error() != QNetworkReply::NoError || status == 0) {
        category = "network"; error = QString::fromUtf8(u8"网络连接失败，请检查网络");
    } else if (!parseWeatherData(bytes, activeCode, parsed, error)) {
        category = "data";
    }
    if (!error.isEmpty()) {
        qWarning().noquote() << "[Weather] Failed" << id << category << error << reply->errorString();
        emit failed(error);
        return;
    }
    emit succeeded(parsed, bytes, QDateTime::currentDateTimeUtc());
}
