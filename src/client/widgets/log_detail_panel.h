#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>

class LogDetailPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogDetailPanel(QWidget* parent = nullptr);

    void showLog(const QVariantMap& entry);
    void clear();

private:
    QLabel*     m_title;
    QTextEdit*  m_raw_view;
    QLabel*     m_ai_label;
};
