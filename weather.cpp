#include "weather.h"
#include "ui_weather.h"

//#include "keyboard.h"

#include <QPainter>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QCompleter>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QSettings>
#include <QStandardItemModel>
#include <QSignalBlocker>

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#define SPAN_INDEX 3 // 温度曲线间隔指数
#define ORIGIN_SIZE 3 // 温度曲线原点大小
#define TEMPERATURE_STARTING_COORDINATE 45 // 高温平均值起始坐标

// 日出日落底线
const QPoint Weather::sun[2] = {
    QPoint(20, 75),
    QPoint(130, 75)
};

// 日出日落时间
const QRect Weather::sunRizeSet[2] = {
    QRect(0, 80, 50, 20),
    QRect(100, 80, 50, 20)
};

// 日出日落圆弧
const QRect Weather::rect[2] = {
    QRect(25, 25, 100, 100), // 虚线圆弧
    QRect(50, 80, 50, 20) // “日出日落”文本
};

Weather::Weather(QWidget *parent, const QString &cacheDirectory, QNetworkAccessManager *network) :
    QWidget(parent),
    ui(new Ui::Weather),
    cache(cacheDirectory),
    history(tool.cityNames()),
    favorites(tool)
{
    ui->setupUi(this);

    // 候选列表保留中文名称，独立搜索角色支持中文、全拼及首字母。
    const QStringList names = tool.cityNames();
    searchHistory = history.entries();
    ui->cityComboBox->addItems(names);
    ui->cityComboBox->setHistory(searchHistory);
    QFile searchFile(":/data/city-search.json");
    searchFile.open(QIODevice::ReadOnly);
    const QJsonObject searchKeys = QJsonDocument::fromJson(searchFile.readAll()).object();
    for (int i = 0; i < ui->cityComboBox->count(); ++i) {
        const QString name = ui->cityComboBox->itemText(i);
        ui->cityComboBox->setItemData(i, name + " " + searchKeys.value(name).toString(), Qt::UserRole + 1);
    }
    ui->cityComboBox->setCurrentIndex(-1);
    ui->cityComboBox->lineEdit()->setPlaceholderText(u8"城市 / 拼音 / 首字母" );
    connect(ui->cityComboBox->lineEdit(), &QLineEdit::returnPressed,
            this, &Weather::on_searchBt_clicked);

    connect(ui->favoriteBt, &QPushButton::clicked, this, [this]() {
        if (displayedCity.isEmpty()) return;
        const QString code = tool[displayedCity];
        QString error;
        const bool ok = favorites.contains(code) ? favorites.remove(code, error) : favorites.add(code, error);
        finishFavoriteChange(ok, error);
    });
    connect(ui->favoriteViewBt, &QPushButton::clicked, this, [this]() {
        const QString name = tool.cityName(ui->favoritesComboBox->currentData().toString());
        if (!name.isEmpty()) requestWeather(name);
    });
    connect(ui->favoriteRemoveBt, &QPushButton::clicked, this, [this]() {
        QString error;
        const bool ok = favorites.remove(ui->favoritesComboBox->currentData().toString(), error);
        finishFavoriteChange(ok, error);
    });
    updateFavoriteControls();
    ui->favoriteStatusLb->hide();

    // 使用系统窗口边框，支持边缘缩放、最大化和标题栏拖动。


    forecast_week_list << ui->week0Lb << ui->week1Lb << ui->week2Lb << ui->week3Lb << ui->week4Lb << ui->week5Lb;
    forecast_date_list << ui->date0Lb << ui->date1Lb << ui->date2Lb << ui->date3Lb << ui->date4Lb << ui->date5Lb;
    forecast_aqi_list << ui->quality0Lb << ui->quality1Lb << ui->quality2Lb << ui->quality3Lb << ui->quality4Lb << ui->quality5Lb;
    forecast_type_list << ui->type0Lb << ui->type1Lb << ui->type2Lb << ui->type3Lb << ui->type4Lb << ui->type5Lb;
    forecast_typeIco_list << ui->typeIco0Lb << ui->typeIco1Lb << ui->typeIco2Lb << ui->typeIco3Lb << ui->typeIco4Lb << ui->typeIco5Lb;
    forecast_high_list << ui->high0Lb << ui->high1Lb << ui->high2Lb << ui->high3Lb << ui->high4Lb << ui->high5Lb;
    forecast_low_list << ui->low0Lb << ui->low1Lb << ui->low2Lb << ui->low3Lb << ui->low4Lb << ui->low5Lb;

    // 结构体初始化
    today.date = u8"等待天气数据";
    today.city = u8"未获取";
    today.fl = u8"无数据";
    today.fx = u8"无数据";
    today.ganmao = u8"无数据";
    today.notice = u8"无数据";
    today.pm25 = u8"无数据";
    today.quality = u8"无数据";
    today.shidu = u8"无数据";
    today.sunrise = "";
    today.sunset = "";
    today.wendu = "--";
    today.type = u8"无数据";

    for (int i = 0; i < 6; i++)
    {
        forecast[i].aqi = "0";
        forecast[i].date = "00日星期0";
        forecast[i].high = "高温 0.0℃";
        forecast[i].low = "低温 0.0℃";
        forecast[i].type = "undefined";
    }

    // 右键菜单
    m_pMenu = new QMenu(this);
    m_pExitAct = new QAction;
    m_pExitAct->setText( u8"退出" );
    m_pExitAct->setIcon(QIcon(":/weatherIco/close.ico"));
    m_pMenu->addAction(m_pExitAct);
    connect( m_pExitAct, SIGNAL(triggered(bool)),
             this, SLOT(slot_exitApp()) );

    // dateLb和WeekLb样式表设置
    for (int i = 0; i < 6; i++)
    {
        forecast_date_list[i]->setStyleSheet("background-color: rgba(0, 255, 255, 100);");
        forecast_week_list[i]->setStyleSheet("background-color: rgba(0, 255, 255, 100);");
    }

    // 请求天气API信息
    city = searchHistory.isEmpty() ? QString::fromUtf8(u8"长沙") : searchHistory.first();

    service = new WeatherService(this, network);
    connect(service, &WeatherService::failed, this, [this](const QString &error) {
        if (closing) return;
        failedCity = city;
        showRequestStatus(error, true);
    });
    connect(service, &WeatherService::succeeded, this,
            [this](const WeatherData &data, const QByteArray &bytes, const QDateTime &at) {
        if (closing) return;
        commitWeather(data, city, at, false);
        failedCity.clear();
        saveSearchHistory(activeCode);
        QString error;
        if (!cache.save(activeCode, bytes, at, error)) {
            qWarning() << "[WeatherCache] Save failed:" << error;
            showRequestStatus(QString::fromUtf8(u8"天气已更新，但本地缓存保存失败"));
        } else {
            showRequestStatus(QString::fromUtf8(u8"%1天气更新成功").arg(displayedCity));
        }
    });
    ui->retryBt->hide();
    connect(ui->retryBt, &QPushButton::clicked, this, [this]() { requestWeather(failedCity); });
    requestWeather(city);

    /* 事件过滤 */
    ui->sunRiseSetLb->installEventFilter(this); // 启用事件过滤器
    ui->curveLb->installEventFilter(this);

    sunTimer = new QTimer(ui->sunRiseSetLb);
    connect(sunTimer, SIGNAL(timeout()), ui->sunRiseSetLb, SLOT(update()));
    connect(sunTimer, &QTimer::timeout, this, [this]() {
        if (!statusText.isEmpty()) showRequestStatus(statusText, statusRetry);
    });
    sunTimer->start(1000);

    setLabelContent();
}

Weather::~Weather()
{
    closing = true;
    stopRequest();
    delete ui;
}

void Weather::showRequestStatus(const QString &text, bool retry)
{
    statusText = text;
    statusRetry = retry;
    QString message = text;
    if (!displayedCity.isEmpty()) {
        message += QString::fromUtf8(u8"；当前显示%1天气（%2%3），获取于 %4（北京时间），天气日期 %5")
            .arg(displayedCity, displayedFromCache ? QString::fromUtf8(u8"本地缓存") : QString::fromUtf8(u8"网络数据"),
                 displayedExpired() ? QString::fromUtf8(u8"，已过期") : QString(),
                 displayedFetchedAt.toOffsetFromUtc(8 * 3600).toString("yyyy-MM-dd HH:mm:ss"), today.date);
    }
    ui->requestStatusLb->setText(message);
    ui->retryBt->setVisible(retry);
}

bool Weather::displayedExpired() const
{
    WeatherData data;
    data.today = today;
    return WeatherCache::isExpired(data, displayedFetchedAt);
}

void Weather::commitWeather(const WeatherData &data, const QString &name, const QDateTime &at, bool fromCache)
{
    today = data.today;
    for (int i = 0; i < 6; ++i) forecast[i] = data.forecast[i];
    displayedCity = name;
    displayedFetchedAt = at;
    displayedFromCache = fromCache;
    updateFavoriteControls();
    setLabelContent();
}

void Weather::updateFavoriteControls()
{
    const QString selected = ui->favoritesComboBox->currentData().toString();
    const QSignalBlocker blocker(ui->favoritesComboBox);
    ui->favoritesComboBox->clear();
    for (const QString &code : favorites.entries())
        ui->favoritesComboBox->addItem(tool.cityName(code), code);
    const bool available = !favorites.entries().isEmpty();
    if (!available) ui->favoritesComboBox->addItem(QString::fromUtf8(u8"暂无收藏城市"));
    const int index = ui->favoritesComboBox->findData(selected);
    if (index >= 0) ui->favoritesComboBox->setCurrentIndex(index);
    ui->favoritesComboBox->setEnabled(available);
    ui->favoriteViewBt->setEnabled(available);
    ui->favoriteRemoveBt->setEnabled(available);
    const QString code = tool[displayedCity];
    ui->favoriteBt->setEnabled(!displayedCity.isEmpty() && !tool.cityName(code).isEmpty());
    ui->favoriteBt->setText(favorites.contains(code) ? QString::fromUtf8(u8"取消收藏") : QString::fromUtf8(u8"收藏"));
    ui->favoriteBt->setToolTip(QString::fromUtf8(u8"收藏或取消收藏当前展示城市：%1").arg(displayedCity));
}

void Weather::finishFavoriteChange(bool ok, const QString &error)
{
    updateFavoriteControls();
    // Keep weather errors/retry state independent of favorites persistence messages.
    ui->favoriteStatusLb->setText(ok ? QString() : error);
    ui->favoriteStatusLb->setVisible(!ok);
    if (!ok) qWarning().noquote() << "[FavoriteCities]" << error;
}

void Weather::stopRequest()
{
    service->cancel();
}

void Weather::closeEvent(QCloseEvent *event)
{
    closing = true;
    stopRequest();
    QWidget::closeEvent(event);
}

void Weather::requestWeather(const QString &cityName)
{
    if (closing) return;
    const QString target = cityName.trimmed();
    const QString code = tool[target];
    if (code == "000000000") {
        showRequestStatus(QString::fromUtf8(u8"请输入有效城市或从候选列表选择"), !failedCity.isEmpty());
        return;
    }
    if (service->isActive(code)) return;
    stopRequest();
    city = target;
    activeCode = code;
    failedCity.clear();
    CachedWeather cached;
    QString cacheError;
    if (cache.load(code, cached, cacheError))
        commitWeather(cached.data, target, cached.fetchedAt, true);
    service->fetch(code);
    showRequestStatus(QString::fromUtf8(u8"正在联网更新%1天气……").arg(target));
}

void Weather::saveSearchHistory(const QString &cityCode)
{
    // 根据响应中的编码保存，避免连续查询时把另一笔请求的城市写入历史。
    QString name;
    for (const QString &candidate : tool.cityNames()) {
        if (tool[candidate] == cityCode) {
            name = candidate;
            break;
        }
    }
    if (name.isEmpty()) return;
    history.record(name);
    searchHistory = history.entries();
    ui->cityComboBox->setHistory(searchHistory);
}

/* 设置控件文本 */
void Weather::setLabelContent()
{
    // 今日数据
    ui->dateLb->setText(today.date);
    ui->temLb->setText(today.wendu);
    ui->cityLb->setText(today.city);
    ui->typeLb->setText(today.type);
    ui->noticeLb->setText(today.notice);
    ui->shiduLb->setText(today.shidu);
    ui->pm25Lb->setText(today.pm25);
    ui->fxLb->setText(today.fx);
    ui->flLb->setText(today.fl);
    ui->ganmaoBrowser->setText(today.ganmao);

    // 判断白天还是夜晚图标
    const QTime sunrise = QTime::fromString(today.sunrise, "HH:mm");
    const QTime sunset = QTime::fromString(today.sunset, "HH:mm");
    const QTime now = QDateTime::currentDateTimeUtc().addSecs(8 * 3600).time();
    if (sunrise.isValid() && sunset.isValid() && now >= sunrise && now < sunset)
    {
        ui->typeIcoLb->setStyleSheet( tr("border-image: url(:/day/day/%1.png); background-color: rgba(60, 60, 60, 0);").arg(today.type) );
    }
    else
    {
        ui->typeIcoLb->setStyleSheet( tr("border-image: url(:/night/night/%1.png); background-color: rgba(60, 60, 60, 0);").arg(today.type) );
    }

    ui->qualityLb->setText(today.quality == "" ? "无" : today.quality);
    if (today.quality == "优")
    {
        ui->qualityLb->setStyleSheet("color: rgb(0, 255, 0); background-color: rgba(255, 255, 255, 0);");
    }
    else if (today.quality == "良")
    {
        ui->qualityLb->setStyleSheet("color: rgb(255, 255, 0); background-color: rgba(255, 255, 255, 0);");
    }
    else if (today.quality == "轻度污染")
    {
        ui->qualityLb->setStyleSheet("color: rgb(255, 170, 0); background-color: rgba(255, 255, 255, 0);");
    }
    else if (today.quality == "重度污染")
    {
        ui->qualityLb->setStyleSheet("color: rgb(255, 0, 0); background-color: rgba(255, 255, 255, 0);");
    }
    else if (today.quality == "严重污染")
    {
        ui->qualityLb->setStyleSheet("color: rgb(170, 0, 0); background-color: rgba(255, 255, 255, 0);");
    }
    else
    {
        ui->qualityLb->setStyleSheet("color: rgb(255, 255, 255); background-color: rgba(255, 255, 255, 0);");
    }

    // 六天数据
    for (int i = 0; i < 6; i++)
    {
        const QDate date = QDate::fromString(forecast[i].date, "yyyy-MM-dd");
        const QStringList weekdays = QString::fromUtf8(u8"星期一,星期二,星期三,星期四,星期五,星期六,星期日").split(',');
        forecast_week_list[i]->setText(date.isValid() ? weekdays.at(date.dayOfWeek() - 1) : "--");
        forecast_date_list[i]->setText(date.isValid() ? date.toString("MM-dd") : "--");
        forecast_type_list[i]->setText(forecast[i].type);
        forecast_high_list[i]->setText(forecast[i].high.split(" ").at(1));
        forecast_low_list[i]->setText(forecast[i].low.split(" ").at(1));
        forecast_typeIco_list[i]->setStyleSheet( tr("image: url(:/day/day/%1.png);").arg(forecast[i].type) );

        bool aqiValid = false;
        forecast[i].aqi.toInt(&aqiValid);
        if (!aqiValid) {
            forecast_aqi_list[i]->setText(u8"暂无数据");
            forecast_aqi_list[i]->setStyleSheet("color: white;");
        }
        else if (forecast[i].aqi.toInt() >= 0 && forecast[i].aqi.toInt() <= 50)
        {
            forecast_aqi_list[i]->setText(u8"优质");
            forecast_aqi_list[i]->setStyleSheet("color: rgb(0, 255, 0);");
        }
        else if (forecast[i].aqi.toInt() > 50 && forecast[i].aqi.toInt() <= 100)
        {
            forecast_aqi_list[i]->setText(u8"良好");
            forecast_aqi_list[i]->setStyleSheet("color: rgb(255, 255, 0);");
        }
        else if (forecast[i].aqi.toInt() > 100 && forecast[i].aqi.toInt() <= 150)
        {
            forecast_aqi_list[i]->setText(u8"轻度污染");
            forecast_aqi_list[i]->setStyleSheet("color: rgb(255, 170, 0);");
        }
        else if (forecast[i].aqi.toInt() > 150 && forecast[i].aqi.toInt() <= 200)
        {
            forecast_aqi_list[i]->setText(u8"重度污染");
            forecast_aqi_list[i]->setStyleSheet("color: rgb(255, 0, 0);");
        }
        else
        {
            forecast_aqi_list[i]->setText(u8"严重污染");
            forecast_aqi_list[i]->setStyleSheet("color: rgb(170, 0, 0);");
        }
    }//for

    const QDate currentDate = QDateTime::currentDateTimeUtc().addSecs(8 * 3600).date();
    if (QDate::fromString(forecast[0].date, "yyyy-MM-dd") == currentDate.addDays(-1))
        ui->week0Lb->setText(u8"昨天");
    if (QDate::fromString(forecast[1].date, "yyyy-MM-dd") == currentDate)
        ui->week1Lb->setText(u8"今天");

    ui->curveLb->update();
}

/* 搜索按钮 */
void Weather::on_searchBt_clicked()
{
    requestWeather(ui->cityComboBox->currentText());
}

/* 刷新按钮 */
void Weather::on_refreshBt_clicked()
{
    requestWeather(displayedCity.isEmpty() ? city : displayedCity);
    ui->curveLb->update();
}

/* 事件过滤 */
bool Weather::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->sunRiseSetLb && event->type() == QEvent::Paint)
    {
        paintSunRiseSet();
    }
    else if (watched == ui->curveLb && event->type() == QEvent::Paint)
    {
        paintCurve();
    }

    return QWidget::eventFilter(watched,event);
}

/* 日出日落图形绘制 */
void Weather::paintSunRiseSet()
{
    QPainter painter(ui->sunRiseSetLb);
    painter.translate((ui->sunRiseSetLb->width() - 150) / 2.0,
                      (ui->sunRiseSetLb->height() - 100) / 2.0);
    painter.setRenderHint(QPainter::Antialiasing, true); // 反锯齿

    // 绘制日出日落线和文本
    painter.save();
    QPen pen = painter.pen();
    pen.setWidthF(0.5);
    pen.setColor(Qt::yellow);
    painter.setPen(pen);
    painter.drawLine(sun[0], sun[1]);
    painter.restore();

    painter.save();
    painter.setFont( QFont("Microsoft Yahei", 8, QFont::Normal) ); // 字体、大小、正常粗细
    painter.setPen(Qt::white);

    if (today.sunrise != "" && today.sunset != "")
    {
        painter.drawText(sunRizeSet[0], Qt::AlignHCenter, today.sunrise);
        painter.drawText(sunRizeSet[1], Qt::AlignHCenter, today.sunset);
    }
    painter.drawText(rect[1], Qt::AlignHCenter, u8"日出日落");
    painter.restore();

    // 绘制圆弧
    painter.save();
    //    pen.setWidth(1);
    pen.setWidthF(0.5);
    pen.setStyle(Qt::DotLine); // 虚线
    pen.setColor(Qt::green);
    painter.setPen(pen);
    painter.drawArc(rect[0], 0 * 16, 180 * 16);
    painter.restore();

    // 过期数据只展示原始日出日落时间，不绘制实时进度。
    if (!displayedExpired() && today.sunrise != "" && today.sunset != "")
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 85, 0, 100));

        const QDate date = QDate::fromString(today.date, "yyyy-MM-dd");
        const QTime rise = QTime::fromString(today.sunrise, "HH:mm");
        const QTime set = QTime::fromString(today.sunset, "HH:mm");
        if (!date.isValid() || !rise.isValid() || !set.isValid() || rise >= set)
            return;

        // 国内城市的日出日落按北京时间计算，每次绘制读取最新数据。
        const QDateTime sunrise(date, rise, Qt::OffsetFromUTC, 8 * 3600);
        const QDateTime sunset(date, set, Qt::OffsetFromUTC, 8 * 3600);
        const qint64 duration = sunrise.secsTo(sunset);
        const qint64 elapsed = qBound(qint64(0), sunrise.secsTo(QDateTime::currentDateTimeUtc()), duration);
        const int spanAngle = int(double(elapsed) / duration * 180 * 16);
        const int startAngle = 180 * 16 - spanAngle;
        if (startAngle >= 0 && spanAngle >= 0)
        {
            painter.drawPie(rect[0], startAngle, spanAngle); // 扇形绘制
        }
    }
}

/* 温度曲线绘制 */
void Weather::paintCurve()
{
    QPainter painter(ui->curveLb);
    painter.setRenderHint(QPainter::Antialiasing, true); // 反锯齿

    // 将温度转换为int类型，并计算平均值，平均值作为curveLb曲线的参考值，参考Y坐标为45
    int tempTotal = 0;
    int high[6] = {};
    int low[6] = {};

    QString h, l;
    for (int i = 0; i < 6; i++)
    {
        h = forecast[i].high.split(" ").at(1);
        h = h.left(h.length() - 1);
        high[i] = (int)(h.toDouble());
        tempTotal += high[i];

        l = forecast[i].low.split(" ").at(1);
        l = l.left(l.length() - 1);
        low[i] = (int)(l.toDouble());
    }
    int tempAverage = (int)(tempTotal / 6); // 最高温平均值

    // 算出温度对应坐标
    int pointX[6] = {};
    for (int i = 0; i < 6; ++i) {
        const QLabel *label = forecast_high_list[i];
        pointX[i] = label->mapTo(ui->curveLb->parentWidget(), label->rect().center()).x()
                - ui->curveLb->x();
    }
    int pointHY[6] = {0};
    int pointLY[6] = {0};
    for (int i = 0; i < 6; i++)
    {
        pointHY[i] = TEMPERATURE_STARTING_COORDINATE - ((high[i] - tempAverage) * SPAN_INDEX);
        pointLY[i] = TEMPERATURE_STARTING_COORDINATE + ((tempAverage - low[i]) * SPAN_INDEX);
    }

    QPen pen = painter.pen();
    pen.setWidth(1);

    // 高温曲线绘制
    painter.save();
    pen.setColor(QColor(255, 170, 0));
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);
    painter.setBrush(QColor(255, 170, 0));
    painter.drawEllipse(QPoint(pointX[0], pointHY[0]), ORIGIN_SIZE, ORIGIN_SIZE);
    painter.drawEllipse(QPoint(pointX[1], pointHY[1]), ORIGIN_SIZE, ORIGIN_SIZE);
    painter.drawLine(pointX[0], pointHY[0], pointX[1], pointHY[1]);

    pen.setStyle(Qt::SolidLine);
    pen.setWidth(1);
    painter.setPen(pen);

    for (int i = 1; i < 5; i++)
    {
        painter.drawEllipse(QPoint(pointX[i+1], pointHY[i+1]), ORIGIN_SIZE, ORIGIN_SIZE);
        painter.drawLine(pointX[i], pointHY[i], pointX[i+1], pointHY[i+1]);
    }
    painter.restore();

    // 低温曲线绘制
    pen.setColor(QColor(0, 255, 255));
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);
    painter.setBrush(QColor(0, 255, 255));
    painter.drawEllipse(QPoint(pointX[0], pointLY[0]), ORIGIN_SIZE, ORIGIN_SIZE);
    painter.drawEllipse(QPoint(pointX[1], pointLY[1]), ORIGIN_SIZE, ORIGIN_SIZE);
    painter.drawLine(pointX[0], pointLY[0], pointX[1], pointLY[1]);

    pen.setColor(QColor(0, 255, 255));
    pen.setStyle(Qt::SolidLine);
    painter.setPen(pen);
    for (int i = 1; i < 5; i++)
    {
        painter.drawEllipse(QPoint(pointX[i+1], pointLY[i+1]), ORIGIN_SIZE, ORIGIN_SIZE);
        painter.drawLine(pointX[i], pointLY[i], pointX[i+1], pointLY[i+1]);
    }
}

/* 调用软键盘 */
void Weather::callKeyBoard()
{
    //    KeyBoard *keyBoard = new KeyBoard(0, ui->cityLineEdit);
    //    keyBoard->show();
}

/* 窗口移动 */
void Weather::mousePressEvent(QMouseEvent *event)
{
    mPos = event->globalPos() - this->pos();
}

void Weather::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
}

/* 右键菜单 */
void Weather::contextMenuEvent(QContextMenuEvent *menuEvent)
{
    m_pMenu->exec(QCursor::pos());
    menuEvent->accept();
}

void Weather::slot_exitApp()
{
    qApp->exit(0);
}
