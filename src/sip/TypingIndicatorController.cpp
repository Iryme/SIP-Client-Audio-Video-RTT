#include "TypingIndicatorController.h"

TypingIndicatorController::TypingIndicatorController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<IsComposingInfo::State>("IsComposingInfo::State");

    m_refreshTimer.setSingleShot(true);
    m_idleTimer.setSingleShot(true);
    m_goneTimer.setSingleShot(true);

    connect(&m_refreshTimer, &QTimer::timeout, this, &TypingIndicatorController::triggerRefreshTimeout);
    connect(&m_idleTimer, &QTimer::timeout, this, &TypingIndicatorController::triggerIdleTimeout);
    connect(&m_goneTimer, &QTimer::timeout, this, &TypingIndicatorController::triggerGoneTimeout);
}

void TypingIndicatorController::setConfig(const Config &config)
{
    m_config = config;
}

TypingIndicatorController::Config TypingIndicatorController::config() const
{
    return m_config;
}

IsComposingInfo::State TypingIndicatorController::currentState() const
{
    return m_state;
}

void TypingIndicatorController::enterActive()
{
    m_state = IsComposingInfo::State::Active;
    m_goneTimer.stop();
    if (m_config.refreshSeconds > 0)
        m_refreshTimer.start(m_config.refreshSeconds * 1000);
    if (m_config.idleSeconds > 0)
        m_idleTimer.start(m_config.idleSeconds * 1000);
    emit sendIsComposingRequested(IsComposingInfo::State::Active, m_config.refreshSeconds);
}

void TypingIndicatorController::enterIdle()
{
    m_state = IsComposingInfo::State::Idle;
    m_refreshTimer.stop();
    m_idleTimer.stop();
    if (m_config.goneDelaySeconds > 0)
        m_goneTimer.start(m_config.goneDelaySeconds * 1000);
    emit sendIsComposingRequested(IsComposingInfo::State::Idle, 0);
}

void TypingIndicatorController::enterGone()
{
    m_state = IsComposingInfo::State::Unknown; // Stopped
    m_refreshTimer.stop();
    m_idleTimer.stop();
    m_goneTimer.stop();
    emit sendIsComposingRequested(IsComposingInfo::State::Gone, 0);
}

void TypingIndicatorController::onTextChanged(bool nonEmpty)
{
    if (!nonEmpty)
        return; // emptying the body alone is not a state transition — only inactivity/stop() drive idle/gone

    if (m_state == IsComposingInfo::State::Active) {
        // Debounce: already announced active — just push the idle deadline
        // back out. Re-sending "active" itself is throttled to the
        // periodic refresh timer (triggerRefreshTimeout), never per keystroke.
        if (m_config.idleSeconds > 0)
            m_idleTimer.start(m_config.idleSeconds * 1000);
        return;
    }

    // Stopped or Idle -> Active is a real state transition; announce it.
    enterActive();
}

void TypingIndicatorController::stop()
{
    if (m_state == IsComposingInfo::State::Unknown)
        return; // already stopped — never send a duplicate "gone"
    enterGone();
}

void TypingIndicatorController::triggerRefreshTimeout()
{
    if (m_state != IsComposingInfo::State::Active)
        return;
    // Keep-alive resend — restart the cadence, do not touch the idle timer.
    if (m_config.refreshSeconds > 0)
        m_refreshTimer.start(m_config.refreshSeconds * 1000);
    emit sendIsComposingRequested(IsComposingInfo::State::Active, m_config.refreshSeconds);
}

void TypingIndicatorController::triggerIdleTimeout()
{
    if (m_state != IsComposingInfo::State::Active)
        return;
    enterIdle();
}

void TypingIndicatorController::triggerGoneTimeout()
{
    if (m_state != IsComposingInfo::State::Idle)
        return;
    enterGone();
}
