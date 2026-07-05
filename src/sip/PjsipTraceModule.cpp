#include "PjsipTraceModule.h"
#ifdef HAVE_PJSIP

#include <pjsua-lib/pjsua.h>

#include <QMetaObject>
#include <QString>

#include "sip/SipMessageTrace.h"
#include "sip/SipRawMessageParser.h"
#include "sip/SipTraceLogger.h"

namespace {

void dispatchRawMessage(const QString &rawText, SipMessageTrace::Direction direction)
{
    if (rawText.isEmpty())
        return;
    // PJSIP callbacks run on a PJSIP worker thread; SipTraceLogger storage and
    // its Qt signal emission must happen on the Qt main thread.
    const SipMessageTrace trace = SipRawMessageParser::parse(rawText, direction);
    QMetaObject::invokeMethod(&SipTraceLogger::instance(), [trace]() {
        SipTraceLogger::instance().logMessage(trace);
    }, Qt::QueuedConnection);
}

pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
    if (rdata && rdata->msg_info.msg_buf && rdata->msg_info.len > 0) {
        dispatchRawMessage(QString::fromLatin1(rdata->msg_info.msg_buf, rdata->msg_info.len),
                            SipMessageTrace::Direction::Inbound);
    }
    return PJ_FALSE; // do not consume — let normal dialog/transaction processing continue
}

pj_bool_t onRxResponse(pjsip_rx_data *rdata)
{
    if (rdata && rdata->msg_info.msg_buf && rdata->msg_info.len > 0) {
        dispatchRawMessage(QString::fromLatin1(rdata->msg_info.msg_buf, rdata->msg_info.len),
                            SipMessageTrace::Direction::Inbound);
    }
    return PJ_FALSE;
}

pj_status_t onTxRequest(pjsip_tx_data *tdata)
{
    if (tdata && pjsip_tx_data_encode(tdata) == PJ_SUCCESS) {
        dispatchRawMessage(
            QString::fromLatin1(tdata->buf.start,
                                 static_cast<int>(tdata->buf.cur - tdata->buf.start)),
            SipMessageTrace::Direction::Outbound);
    }
    return PJ_SUCCESS; // must always return success — non-zero would drop the message
}

pj_status_t onTxResponse(pjsip_tx_data *tdata)
{
    if (tdata && pjsip_tx_data_encode(tdata) == PJ_SUCCESS) {
        dispatchRawMessage(
            QString::fromLatin1(tdata->buf.start,
                                 static_cast<int>(tdata->buf.cur - tdata->buf.start)),
            SipMessageTrace::Direction::Outbound);
    }
    return PJ_SUCCESS;
}

pjsip_module g_traceModule;
bool g_installed = false;

} // namespace

namespace PjsipTraceModule {

void install()
{
    if (g_installed)
        return;

    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
    if (!endpt)
        return;

    pj_bzero(&g_traceModule, sizeof(g_traceModule));
    g_traceModule.name         = pj_str(const_cast<char *>("mod-sip-trace-capture"));
    g_traceModule.id           = -1;
    g_traceModule.priority     = PJSIP_MOD_PRIORITY_TRANSPORT_LAYER + 1;
    g_traceModule.on_rx_request  = &onRxRequest;
    g_traceModule.on_rx_response = &onRxResponse;
    g_traceModule.on_tx_request  = &onTxRequest;
    g_traceModule.on_tx_response = &onTxResponse;

    g_installed = (pjsip_endpt_register_module(endpt, &g_traceModule) == PJ_SUCCESS);
}

void uninstall()
{
    if (!g_installed)
        return;
    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
    if (endpt)
        pjsip_endpt_unregister_module(endpt, &g_traceModule);
    g_installed = false;
}

} // namespace PjsipTraceModule

#endif
