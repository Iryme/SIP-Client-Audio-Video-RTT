#include "RequestBlinker.h"

RequestBlinker::RequestBlinker(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_on = !m_on;
        emit toggled();
    });
}

void RequestBlinker::start()
{
    m_on = true;
    emit toggled();
    m_timer.start();
}

void RequestBlinker::stop()
{
    m_timer.stop();
    if (m_on) {
        m_on = false;
        emit toggled();
    }
}
