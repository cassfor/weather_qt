#ifndef CITYCOMBOBOX_H
#define CITYCOMBOBOX_H

#include <QComboBox>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QKeyEvent>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>

// 单一候选窗口：空输入显示历史，有输入显示城市匹配结果。
class CityComboBox : public QComboBox
{
public:
    explicit CityComboBox(QWidget *parent = nullptr) : QComboBox(parent) {
        setEditable(true);
        setInsertPolicy(QComboBox::NoInsert);
        setCompleter(nullptr);
        popup = new QListWidget(this);
        popup->setObjectName("cityCandidates");
        popup->setWindowFlags(Qt::ToolTip);
        popup->setAttribute(Qt::WA_ShowWithoutActivating);
        popup->setFocusPolicy(Qt::NoFocus);
        popup->setStyleSheet("QListWidget { background: white; color: black; border: 1px solid gray; }"
                             "QListWidget::item:selected { background: #cce8ff; color: black; }");
        leaveTimer.setSingleShot(true);
        leaveTimer.setInterval(150);
        connect(&leaveTimer, &QTimer::timeout, this, [this]() {
            if (!containsMouse(this) && !containsMouse(popup)) hidePopup();
        });
        connect(lineEdit(), &QLineEdit::textEdited, this, [this]() { updateCandidates(false); });
        connect(popup, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { choose(item); });
        qApp->installEventFilter(this);
    }
    ~CityComboBox() override { qApp->removeEventFilter(this); }
    void setHistory(const QStringList &names) {
        history = names;
        if (popup->isVisible() && currentText().trimmed().isEmpty()) updateCandidates(false);
    }
    void showPopup() override {
        activatedByClick = true;
        lineEdit()->setFocus();
        updateCandidates(true);
    }
    void hidePopup() override {
        popup->hide();
        leaveTimer.stop();
        activatedByClick = false;
    }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        const bool editor = watched == lineEdit();
        const bool region = watched == this || editor || watched == popup || watched == popup->viewport();
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (editor && mouse->button() == Qt::LeftButton) {
                activatedByClick = true;
                updateCandidates(true);
            } else if (popup->isVisible() && !containsMouse(this) && !containsMouse(popup)) {
                hidePopup();
            }
        } else if (region && event->type() == QEvent::Leave) {
            if (popup->isVisible()) leaveTimer.start();
        } else if (region && event->type() == QEvent::Enter) {
            leaveTimer.stop();
            if ((editor || watched == this) && activatedByClick && !popup->isVisible())
                updateCandidates(false);
        } else if (editor && event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) { hidePopup(); return true; }
            if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
                if (!popup->isVisible()) return true;
                int row = popup->currentRow();
                row += key->key() == Qt::Key_Down ? 1 : -1;
                popup->setCurrentRow(qBound(0, row, popup->count() - 1));
                return true;
            }
            if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
                    && popup->isVisible() && popup->currentItem()) {
                choose(popup->currentItem());
                return true; // 首次回车选中，再次回车查询。
            }
            if (key->key() == Qt::Key_Tab) hidePopup();
        } else if ((watched == window() && (event->type() == QEvent::WindowDeactivate
                    || event->type() == QEvent::Move || event->type() == QEvent::Resize))
                   || (editor && event->type() == QEvent::FocusOut)) {
            hidePopup();
        }
        return QComboBox::eventFilter(watched, event);
    }
private:
    QListWidget *popup;
    QStringList history;
    QTimer leaveTimer;
    bool activatedByClick = false;
    static bool containsMouse(QWidget *widget) {
        return widget->isVisible() && widget->rect().contains(widget->mapFromGlobal(QCursor::pos()));
    }
    void choose(QListWidgetItem *item) {
        const QString name = item->text();
        setCurrentText(name);
        hidePopup();
        lineEdit()->setFocus();
    }
    void updateCandidates(bool clicked) {
        const QString query = currentText().trimmed();
        popup->clear();
        if (query.isEmpty()) {
            if (!activatedByClick || (!clicked && !containsMouse(this))) {
                popup->hide();
                return;
            }
            popup->addItems(history);
        } else {
            for (int i = 0; i < count(); ++i)
                if (itemData(i, Qt::UserRole + 1).toString().contains(query, Qt::CaseInsensitive))
                    popup->addItem(itemText(i));
        }
        if (!popup->count()) { popup->hide(); return; }
        const int height = qMin(10, popup->count()) * qMax(24, popup->sizeHintForRow(0)) + 4;
        QPoint position = mapToGlobal(QPoint(0, this->height()));
        const QRect screen = QGuiApplication::screenAt(position)
                ? QGuiApplication::screenAt(position)->availableGeometry()
                : QGuiApplication::primaryScreen()->availableGeometry();
        if (position.y() + height > screen.bottom()) position.setY(mapToGlobal(QPoint()).y() - height);
        position.setX(qBound(screen.left(), position.x(), qMax(screen.left(), screen.right() - width() + 1)));
        popup->setGeometry(QRect(position, QSize(width(), height)));
        popup->show();
    }
};
#endif
