# Qt 天气项目：整体结构学习

本文结合本项目代码，说明各个文件的职责、程序启动过程，以及天气数据如何最终显示到界面上。

## 1. 项目实现了什么

这是一个基于 **Qt Widgets** 的天气查询程序，主要功能包括：

- 输入城市名称，查询天气。
- 显示今天的温度、湿度、风向、风力和空气质量等信息。
- 显示六天的天气信息及高低温曲线。
- 绘制日出日落图形。
- 支持刷新、拖动无边框窗口和右键退出。

项目以 `Weather` 窗口类为核心。界面布局放在 `.ui` 文件中，网络请求、数据解析、界面更新和绘图主要集中在 `weather.cpp` 中。

## 2. 文件分工

| 文件 | 作用 | 阅读重点 |
| --- | --- | --- |
| `main.cpp` | 程序入口 | 创建应用对象、创建窗口、进入事件循环 |
| `Weather.pro` | qmake 构建配置 | Qt 模块、源文件、头文件、界面与资源文件 |
| `weather.h` | 声明 `Weather` 类及天气数据结构 | 成员变量、槽函数、事件处理函数 |
| `weather.cpp` | 实现窗口行为 | 初始化、网络请求、JSON 解析、更新界面、绘图 |
| `weather.ui` | XML 格式的界面设计文件 | 控件名称、布局、样式和初始属性 |
| `ui_weather.h` | 根据 `.ui` 生成的界面代码 | 理解来源即可，通常不直接修改 |
| `WeatherTool.h` | 城市编码查询工具 | 从 JSON 文件建立城市名称与编码的映射 |
| `images/images.qrc`、`icons/icons.qrc` | Qt 资源清单 | 将图片和图标纳入资源系统 |
| `qrc_weatypeico_manual.cpp` | 当前构建配置直接编译的天气图标资源代码 | 理解天气图标资源如何参与构建 |

可以先记住这四句话：

> `.pro` 决定如何构建。  
> `.ui` 描述界面。  
> `.h` 声明类的成员和能力。  
> `.cpp` 实现具体行为。

### 2.1 构建配置：Weather.pro

项目启用了这些 Qt 模块：

```qmake
QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
```

| 模块 | 在本项目中的用途 |
| --- | --- |
| `core` | 字符串、日期时间、定时器、JSON 等基础功能 |
| `gui` | 绘图、字体、颜色、图标等功能 |
| `widgets` | 窗口、标签、按钮、输入框等界面控件 |
| `network` | HTTP 请求与响应处理 |

配置中的 `SOURCES`、`HEADERS`、`FORMS` 和 `RESOURCES` 分别列出源文件、头文件、界面文件和资源清单。当前还通过 `SOURCES` 加入了 `qrc_weatypeico_manual.cpp`。

## 3. 程序从 main.cpp 开始

```cpp
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Weather w;
    w.show();

    return a.exec();
}
```

执行顺序如下：

1. `QApplication a`：创建 Qt GUI 应用对象。
2. `Weather w`：创建天气窗口，执行 `Weather` 构造函数。
3. `w.show()`：显示窗口。
4. `a.exec()`：进入事件循环，持续处理用户操作、网络响应、定时器及绘制事件。

**事件循环是理解 Qt 程序运行方式的关键。** 窗口创建完成后，程序会等待和处理事件。例如，点击搜索按钮会执行搜索槽函数，网络响应到达后会执行响应处理槽函数。

## 4. Weather 类是项目的中心

`weather.h` 中声明了窗口类：

```cpp
class Weather : public QWidget
{
    Q_OBJECT
    // ...
};
```

`Weather` 继承 `QWidget`，因此它本身就是一个窗口。`Q_OBJECT` 使该类参与 Qt 元对象机制，支持本类声明的槽函数等功能。

这个类同时承担几组职责：

| 职责 | 对应成员或函数 |
| --- | --- |
| 访问界面控件 | `Ui::Weather *ui` |
| 保存天气数据 | `Today today`、`Forecast forecast[6]` |
| 管理网络请求 | `QNetworkAccessManager *manager` |
| 发起天气查询 | `getWeatherInfo()` |
| 处理网络响应 | `replayFinished()` |
| 解析 JSON | `parseJson()` |
| 更新控件内容 | `setLabelContent()` |
| 绘制图形 | `paintCurve()`、`paintSunRiseSet()` |
| 处理搜索和刷新 | `on_searchBt_clicked()`、`on_refreshBt_clicked()` |
| 监听指定控件的事件 | `eventFilter()` |
| 实现窗口拖动 | `mousePressEvent()`、`mouseMoveEvent()` |
| 实现右键退出 | `contextMenuEvent()`、`slot_exitApp()` |

本项目没有把网络、数据和界面拆分为独立的业务层，而是集中在一个窗口类中。阅读时可以按职责分组理解。

## 5. weather.ui 怎样连接到 C++

```text
weather.ui：描述控件、布局和属性
    │
    │ Qt 的 uic 工具生成
    ▼
ui_weather.h：提供 Ui::Weather 和控件成员
    │
    │ weather.cpp 包含该头文件
    ▼
通过 ui 指针访问界面控件
```

`weather.ui` 可以在 Qt Designer 中可视化编辑。构建时，`uic` 将界面描述转换成 C++ 代码。

窗口构造函数中的关键代码是：

```cpp
Weather::Weather(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Weather)
{
    ui->setupUi(this);
    // ...
}
```

`new Ui::Weather` 创建界面辅助对象，`setupUi(this)` 在当前窗口中创建并配置控件。

随后就可以通过控件名称读取或修改内容：

```cpp
// 从输入框读取用户输入的城市
city = ui->cityLineEdit->text();

// 把天气数据中的城市名称显示到标签
ui->cityLb->setText(today.city);
```

这里要区分两个名字相似的类：

- `Weather`：手写的窗口类，负责程序逻辑。
- `Ui::Weather`：生成的界面辅助类，负责创建和配置控件。

修改界面通常应修改 `weather.ui`，而不是直接修改生成的 `ui_weather.h`，因为重新生成时可能覆盖手动修改。

## 6. 构造函数如何组装整个窗口

`Weather::Weather()` 是理解项目初始化过程的入口，主要完成以下工作：

1. 调用 `setupUi()` 创建界面，并设置无边框窗口和输入框样式。
2. 将六天预报对应的标签放入多个 `QList<QLabel *>` 中，方便循环更新。
3. 初始化今天及六天天气数据的默认值。
4. 创建右键菜单和退出动作，连接退出槽函数。
5. 设置部分标签样式。
6. 设置接口地址和默认城市“长沙”，创建网络管理器，连接响应信号，发起首次请求。
7. 为日出日落标签、温度曲线标签、城市输入框安装事件过滤器。
8. 创建定时器，每秒请求重绘日出日落区域。

例如，多个日期标签被组织成一个列表：

```cpp
forecast_date_list << ui->date0Lb << ui->date1Lb
                   << ui->date2Lb << ui->date3Lb
                   << ui->date4Lb << ui->date5Lb;
```

这样更新六天日期时就可以使用循环，而不用分别写六次赋值。

## 7. 一次天气查询的数据流

```text
启动程序（默认长沙）或点击搜索、刷新
                    │
                    ▼
         getWeatherInfo(manager)
         城市名称 → 城市编码 → 请求 URL
                    │
                    ▼
         manager->get(...) 发起请求
                    │
                    ▼
         响应到达，发出 finished 信号
                    │
                    ▼
         replayFinished(reply)
         检查响应状态，读取响应内容
                    │
                    ▼
         parseJson(bytes)
         将 JSON 数据写入天气结构体
                    │
                    ▼
         setLabelContent()
         更新文字、图标、颜色，并请求重绘曲线
```

### 7.1 城市名称转换为编码

`getWeatherInfo()` 中通过以下代码查询城市编码：

```cpp
QString citycode = tool[city];
```

这里的 `tool` 是 `WeatherTool` 对象，`tool[city]` 调用了它重载的 `operator[]`。

`WeatherTool` 在构造时读取嵌入 Qt 资源的 `citycode-2019-08-23.json`，建立城市名称与编码的映射。查询时先查原始名称，再尝试在名称后加“市”；未找到时返回 `"000000000"`。

**该 JSON 文件通过 `citydata.qrc` 编译到程序中。** 运行时读取 `:/data/citycode-2019-08-23.json`，无需再把城市表复制到可执行文件目录。

### 7.2 网络请求是异步的

```cpp
connect(manager, SIGNAL(finished(QNetworkReply*)),
        this, SLOT(replayFinished(QNetworkReply*)));
```

这条连接表示：当网络管理器发出 `finished` 信号时，调用当前窗口的 `replayFinished()` 槽函数。

`manager->get(...)` 发起请求后会返回，程序可以继续处理界面事件。响应到达后，槽函数再读取数据并调用 `parseJson()`。

### 7.3 数据先保存，再显示

```cpp
Today today;
Forecast forecast[6];
```

`Today` 保存今天的详细信息，例如城市、温度、湿度、风向、日出和日落时间。

`Forecast` 保存每天的简要信息，例如日期、最高温、最低温、空气质量指数和天气类型。

| 数组位置 | 内容 |
| --- | --- |
| `forecast[0]` | 昨天的数据，来自 JSON 的 `yesterday` |
| `forecast[1]` | 今天的数据，来自 JSON 的 `forecast` 数组第一项 |
| `forecast[2]` 至 `forecast[5]` | 后面四天的数据 |

因此，界面上的六天信息包含昨天、今天和后面四天。

`parseJson()` 完成数据填充后调用 `setLabelContent()`，再把结构体中的内容写入控件。

## 8. 普通显示与自定义绘图

普通文字通过控件接口显示，例如：

```cpp
ui->temLb->setText(today.wendu);
ui->shiduLb->setText(today.shidu);
```

天气图标通过样式表中的资源路径设置，例如 `:/day/day/...png`。以 `:/` 开头的路径属于 Qt 资源系统。

温度曲线和日出日落图形使用 `QPainter` 自定义绘制：

| 绘制区域 | 对应函数 | 绘制内容 |
| --- | --- | --- |
| `curveLb` | `paintCurve()` | 六天高低温的点和连线 |
| `sunRiseSetLb` | `paintSunRiseSet()` | 时间文字、圆弧及表示时间进度的扇形 |

这些控件安装了事件过滤器。收到 `QEvent::Paint` 时，`eventFilter()` 调用相应绘图函数。

```text
调用控件的 update()
        │
        ▼
请求重绘，稍后处理绘制事件
        │
        ▼
eventFilter() 监听到 QEvent::Paint
        │
        ▼
调用 paintCurve() 或 paintSunRiseSet()
```

日出日落区域由 `QTimer` 每秒请求更新一次。**这个定时器用于刷新图形，不会每秒重新查询天气接口。**

## 9. 用户操作怎样触发代码

| 用户操作 | 处理位置 | 主要行为 |
| --- | --- | --- |
| 点击搜索 | `on_searchBt_clicked()` | 读取输入框城市并请求天气 |
| 点击刷新 | `on_refreshBt_clicked()` | 重新请求当前城市天气，并请求重绘曲线 |
| 在窗口上按下、移动鼠标 | `mousePressEvent()`、`mouseMoveEvent()` | 记录位置差并移动窗口 |
| 右键窗口 | `contextMenuEvent()` | 显示退出菜单 |
| 点击退出动作 | `slot_exitApp()` | 退出应用 |

搜索和刷新槽函数采用 `on_控件名_信号名()` 命名形式，这是 Qt 界面自动连接机制使用的约定。网络响应和退出动作则在构造函数中显式使用 `connect()` 连接。

城市输入框的鼠标按下事件还会调用 `callKeyBoard()`，但当前函数内部的软键盘代码被注释，因此没有实际弹出软键盘的功能。

## 10. 推荐阅读顺序

1. **`main.cpp`**：理解应用对象、窗口对象和事件循环。
2. **`weather.h`**：认识数据结构、成员变量和函数分类。
3. **`Weather::Weather()`**：理解整个窗口如何初始化和连接各项功能。
4. **`weather.ui`**：对照代码中的控件名称，找到对应界面元素。
5. **天气查询主线**：依次阅读 `getWeatherInfo()` → `replayFinished()` → `parseJson()` → `setLabelContent()`。
6. **交互与绘图**：最后阅读搜索刷新、事件过滤、绘图和窗口拖动。

第一轮阅读完成后，可以用以下问题检查理解：

- 创建 `Weather w` 时，哪些初始化工作会发生？
- `Weather` 与 `Ui::Weather` 分别负责什么？
- 输入框里的城市名称如何变成网络请求中的城市编码？
- 网络响应回来后，哪个函数首先处理它？
- JSON 数据经过哪些步骤显示到标签上？
- 为什么调用 `update()` 会与绘图函数产生联系？
- 六天天气数组中，昨天和今天分别存在哪里？

本文基于项目源码说明结构与执行流程，未编译运行项目，也未验证天气接口当前是否可用。
