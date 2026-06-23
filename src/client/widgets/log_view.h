#pragma once
#include <QListView>
#include <QStyledItemDelegate>

class LogFilterModel;

class LogDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit LogDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};

class LogView : public QListView {
    Q_OBJECT
public:
    explicit LogView(QWidget* parent = nullptr);

    void setFilterModel(LogFilterModel* model);
    void scrollToBottom();
    bool autoScroll() const { return m_auto_scroll; }
    void setAutoScroll(bool enabled);

signals:
    void logDoubleClicked(const QString& logId);
    void contextMenuRequested(const QString& logId, const QPoint& pos);
    void analysisRequested(const QStringList& logIds);

protected:
    void rowsInserted(const QModelIndex& parent, int start, int end) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    LogFilterModel* m_filter_model = nullptr;
    bool m_auto_scroll = true;
};
