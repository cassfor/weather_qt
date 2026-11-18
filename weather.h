#ifndef WEATHER_H
#define WEATHER_H

#include <QWidget>
#include <QList>
#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QByteArray>

#include "WeatherTool.h"
#include "WeatherData.h"
#include "WeatherCache.h"
#include "WeatherService.h"
#include "SearchHistory.h"
#include <QPointer>
#include <QElapsedTimer>
#include <QCloseEvent>

namespace Ui {
class Weather;
}

class Weather : public QWidget
{
    Q_OBJECT
    friend class Stage1Test;
    friend class Stage2Test;

public:
    explicit Weather(QWidget *parent = 0, const QString &cacheDirectory = QString(),
                     QNetworkAccessManager *network = nullptr);
    ~Weather();

protected:
    bool eventFilter(QObject *watched, QEvent *event);
    void closeEvent(QCloseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void contextMenuEvent(QContextMenuEvent *menuEvent);

private slots:
    void on_searchBt_clicked();
    void on_refreshBt_clicked();
    void slot_exitApp();

private:
    Ui::Weather *ui;

    QPoint mPos; // 窗口移动

    QList<QLabel *> forecast_week_list;
    QList<QLabel *> forecast_date_list;
    QList<QLabel *> forecast_aqi_list;
    QList<QLabel *> forecast_type_list;
    QList<QLabel *> forecast_typeIco_list;
    QList<QLabel *> forecast_high_list;
    QList<QLabel *> forecast_low_list;

    Today today;
    Forecast forecast[6];

    static const QPoint sun[2];
    static const QRect sunRizeSet[2];
    static const QRect rect[2];

    QTimer *sunTimer;

    WeatherService *service;
    QString city;
    QString displayedCity;
    WeatherCache cache;
    bool displayedFromCache = false;
    QDateTime displayedFetchedAt;
    QString statusText;
    bool statusRetry = false;
    void commitWeather(const WeatherData &data, const QString &name, const QDateTime &at, bool fromCache);
    bool displayedExpired() const;
    QString failedCity;
    QString activeCode;
    bool closing = false;
    void requestWeather(const QString &cityName);
    void showRequestStatus(const QString &text, bool retry = false);
    void stopRequest();
    WeatherTool tool;
    SearchHistory history;
    QStringList searchHistory;
    void saveSearchHistory(const QString &cityCode);

    /* 右键菜单退出程序 */
    QMenu *m_pMenu;
    QAction *m_pExitAct;

    void setLabelContent();
    void paintSunRiseSet();
    void paintCurve();
    void callKeyBoard();
};

#endif // WEATHER_H
