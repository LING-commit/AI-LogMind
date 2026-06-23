#include "log_detail_panel.h"
#include <QVBoxLayout>

LogDetailPanel::LogDetailPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_title = new QLabel("<b>\u65e5\u5fd7\u8be6\u60c5</b>", this);
    m_raw_view = new QTextEdit(this);
    m_raw_view->setReadOnly(true);
    m_raw_view->setMaximumHeight(120);
    m_raw_view->setPlaceholderText(
        "\u70b9\u51fb\u65e5\u5fd7\u884c\u67e5\u770b\u8be6\u60c5");

    m_ai_label = new QLabel(this);
    m_ai_label->setWordWrap(true);
    m_ai_label->setStyleSheet("color: #7EE787; font-size: 11px;");

    layout->addWidget(m_title);
    layout->addWidget(m_raw_view);
    layout->addWidget(m_ai_label);
}

void LogDetailPanel::showLog(const QVariantMap& entry) {
    m_raw_view->setPlainText(entry.value("raw").toString());
    QString ai = entry.value("ai_summary").toString();
    m_ai_label->setText(ai.isEmpty() ? "" : QString("AI: %1").arg(ai));
}

void LogDetailPanel::clear() {
    m_raw_view->clear();
    m_ai_label->clear();
}
