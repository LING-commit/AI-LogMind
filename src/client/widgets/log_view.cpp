#include "log_view.h"
#include "../models/log_filter_model.h"
#include "../models/log_model.h"
#include "search_bar.h"
#include <QPainter>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QWheelEvent>
#include <QScrollBar>

LogDelegate::LogDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void LogDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const
{
    painter->save();

    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, QColor("#2B3D54"));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor("#1E2A3A"));
    } else {
        painter->fillRect(option.rect, QColor("#0D1117"));
    }

    if (!(option.state & QStyle::State_Selected) && index.row() % 2 == 1) {
        painter->fillRect(option.rect, QColor("#111820"));
    }

    auto ts    = index.data(TimestampRole).toDateTime().toString("HH:mm:ss.zzz");
    auto level = index.data(LevelRole).toString();
    auto src   = index.data(SourceRole).toString();
    auto msg   = index.data(MessageRole).toString();
    auto color = index.data(LevelColorRole).value<QColor>();

    QRect r = option.rect.adjusted(4, 2, -4, -2);
    int h = r.height();

    // \u2500\u2500 \u5217\u5e03\u5c40\uff08\u56fa\u5b9a\u987a\u5e8f\uff1a\u8272\u6761 | \u65f6\u95f4 | \u6765\u6e90 | \u7ea7\u522b\u5fbd\u7ae0 | \u6d88\u606f\uff09 \u2500\u2500
    constexpr int kTimeX       = 12;
    constexpr int kTimeWidth   = 100;
    constexpr int kSourceX     = kTimeX + kTimeWidth;      // 112
    constexpr int kSourceWidth = 140;
    constexpr int kLevelX      = kSourceX + kSourceWidth + 8; // 260
    constexpr int kLevelWidth  = 56;
    constexpr int kMessageX    = kLevelX + kLevelWidth + 12;  // 328

    painter->fillRect(option.rect.x(), option.rect.y(), 3, option.rect.height(), color);

    QFont mono("Cascadia Code, JetBrains Mono, monospace", 10);
    painter->setFont(mono);

    painter->setPen(QColor("#8B949E"));
    painter->drawText(r.adjusted(kTimeX, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, ts);

    painter->setPen(QColor("#58A6FF"));
    QString elidedSource = painter->fontMetrics().elidedText(
        src, Qt::ElideMiddle, kSourceWidth - 8);
    QRect sourceRect(option.rect.x() + kSourceX, option.rect.y(), kSourceWidth, option.rect.height());
    painter->drawText(sourceRect, Qt::AlignLeft | Qt::AlignVCenter, elidedSource);

    QRect levelRect(option.rect.x() + kLevelX, option.rect.y() + 3, kLevelWidth - 4, h - 6);
    painter->setPen(color);
    painter->setBrush(color.darker(200));
    painter->drawRoundedRect(levelRect, 3, 3);
    painter->setPen(color.lighter());
    painter->drawText(levelRect, Qt::AlignCenter, level);

    painter->setPen(QColor("#C9D1D9"));
    QString elidedMsg = painter->fontMetrics().elidedText(
        msg, Qt::ElideRight, option.rect.width() - kMessageX - 8);
    painter->drawText(option.rect.adjusted(kMessageX, 0, -8, 0),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedMsg);

    if (index.data(HasAiReportRole).toBool()) {
        painter->setPen(QColor("#7EE787"));
        painter->drawText(option.rect.adjusted(-28, 0, -8, 0),
                          Qt::AlignRight | Qt::AlignVCenter, "\u2728");
    }

    painter->restore();
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    Q_UNUSED(index);
    return {option.rect.width() > 0 ? option.rect.width() : 400, 26};
}

LogView::LogView(QWidget* parent) : QListView(parent) {
    setItemDelegate(new LogDelegate(this));
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
}

void LogView::setFilterModel(LogFilterModel* model) {
    m_filter_model = model;
    setModel(model);
}

void LogView::setAutoScroll(bool enabled) {
    m_auto_scroll = enabled;
    if (enabled) scrollToBottom();
}

void LogView::scrollToBottom() {
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void LogView::rowsInserted(const QModelIndex& parent, int start, int end) {
    QListView::rowsInserted(parent, start, end);
    if (m_auto_scroll) {
        scrollToBottom();
    }
}

void LogView::mouseDoubleClickEvent(QMouseEvent* event) {
    auto idx = indexAt(event->pos());
    if (idx.isValid() && m_filter_model) {
        auto item = m_filter_model->sourceItem(idx.row());
        emit logDoubleClicked(item.id);
    }
    QListView::mouseDoubleClickEvent(event);
}

void LogView::contextMenuEvent(QContextMenuEvent* event) {
    auto idx = indexAt(event->pos());
    if (!idx.isValid() || !m_filter_model) {
        QListView::contextMenuEvent(event);
        return;
    }

    auto item = m_filter_model->sourceItem(idx.row());
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #161B22; border: 1px solid #30363D; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; color: #C9D1D9; }"
        "QMenu::item:selected { background: #1F6FEB; }"
    );

    auto* copyAct = menu.addAction("\u590d\u5236\u539f\u6587");
    auto* copyMsg = menu.addAction("\u590d\u5236\u6d88\u606f");
    menu.addSeparator();
    auto* aiAct   = menu.addAction("AI \u5206\u6790");
    menu.addSeparator();
    auto* filterSrc = menu.addAction(
        QString("\u4ec5\u663e\u793a\u6765\u6e90: %1").arg(item.source));
    auto* filterLvl = menu.addAction(
        QString("\u4ec5\u663e\u793a\u7ea7\u522b: %1").arg(item.level));
    connect(filterSrc, &QAction::triggered, this, [this, item] {
        emit sourceFilterRequested(item.source);
    });
    connect(filterLvl, &QAction::triggered, this, [this, item] {
        emit levelFilterRequested(item.level);
    });

    auto action = menu.exec(event->globalPos());

    if (action == copyAct) {
        QApplication::clipboard()->setText(item.raw);
    } else if (action == copyMsg) {
        QApplication::clipboard()->setText(item.message);
    } else if (action == aiAct) {
        QStringList ids = {item.id};
        emit analysisRequested(ids);
    }
}

void LogView::wheelEvent(QWheelEvent* event) {
    auto sb = verticalScrollBar();
    if (event->angleDelta().y() < 0 && sb->value() < sb->maximum() - 10) {
        m_auto_scroll = false;
    }
    QListView::wheelEvent(event);
}
