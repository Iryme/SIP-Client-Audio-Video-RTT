#include "SipLadderPage.h"

#include "gui/SipLadderWidget.h"
#include "gui/SipMessageDetailsDialog.h"
#include "sip/SipTraceLogger.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextStream>
#include <QVBoxLayout>
#include <QFile>

SipLadderPage::SipLadderPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SipLadderPage");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *title = new QLabel(tr("SIP Ladder"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(title);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    m_callIdFilter = new QLineEdit(this);
    m_callIdFilter->setPlaceholderText(tr("Call-ID"));
    toolbar->addWidget(m_callIdFilter, 1);

    m_methodFilter = new QLineEdit(this);
    m_methodFilter->setPlaceholderText(tr("Method / status"));
    toolbar->addWidget(m_methodFilter, 1);

    m_directionFilter = new QComboBox(this);
    m_directionFilter->addItem(tr("All directions"), QString());
    m_directionFilter->addItem(tr("Outbound"), QStringLiteral("outbound"));
    m_directionFilter->addItem(tr("Inbound"), QStringLiteral("inbound"));
    toolbar->addWidget(m_directionFilter);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_exportTextBtn = new QPushButton(tr("Export Text"), this);
    m_exportJsonBtn = new QPushButton(tr("Export JSON"), this);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_exportTextBtn);
    toolbar->addWidget(m_exportJsonBtn);
    root->addLayout(toolbar);

    auto *hint = new QLabel(
        tr("Hover a message for a compact summary. Click it to open full SIP details."),
        this);
    hint->setStyleSheet("color: #b7c4d6;");
    root->addWidget(hint);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_ladder = new SipLadderWidget(scroll);
    scroll->setWidget(m_ladder);
    root->addWidget(scroll, 1);

    connect(m_callIdFilter, &QLineEdit::textChanged, this, &SipLadderPage::applyFilters);
    connect(m_methodFilter, &QLineEdit::textChanged, this, &SipLadderPage::applyFilters);
    connect(m_directionFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SipLadderPage::applyFilters);
    connect(m_clearBtn, &QPushButton::clicked, this, &SipLadderPage::onClear);
    connect(m_exportTextBtn, &QPushButton::clicked, this, &SipLadderPage::onExportText);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &SipLadderPage::onExportJson);
    connect(m_ladder, &SipLadderWidget::traceActivated,
            this, &SipLadderPage::onTraceActivated);
    connect(&SipTraceLogger::instance(), &SipTraceLogger::messageLogged,
            m_ladder, &SipLadderWidget::onMessageLogged);
    connect(&SipTraceLogger::instance(), &SipTraceLogger::cleared,
            m_ladder, &SipLadderWidget::onCleared);

    for (const SipMessageTrace &trace : SipTraceLogger::instance().messages())
        m_ladder->onMessageLogged(trace);

    applyFilters();
}

void SipLadderPage::applyFilters()
{
    m_ladder->setCallIdFilter(m_callIdFilter->text());
    m_ladder->setMethodFilter(m_methodFilter->text());
    m_ladder->setDirectionFilter(m_directionFilter->currentData().toString());
}

void SipLadderPage::onClear()
{
    SipTraceLogger::instance().clear();
}

void SipLadderPage::onExportText()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export SIP Trace"), QStringLiteral("sip-trace.txt"),
        tr("Text files (*.txt);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;
    QTextStream(&f) << SipTraceLogger::instance().exportToText();
}

void SipLadderPage::onExportJson()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export SIP Trace"), QStringLiteral("sip-trace.json"),
        tr("JSON files (*.json);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;
    QTextStream(&f) << SipTraceLogger::instance().exportToJson();
}

void SipLadderPage::onTraceActivated(const SipMessageTrace &trace)
{
    auto *dlg = new SipMessageDetailsDialog(this);
    dlg->setTrace(trace);
    dlg->open();
}
