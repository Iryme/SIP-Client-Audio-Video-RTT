#pragma once
#include <QString>

// Minimal RFC 3862 CPIM ("Common Presence and Instant Messaging") header
// wrapper info. CPIM messages look like a small MIME-ish header block
// followed by a blank line and the wrapped body:
//
//   From: MR SANDERS <im:piglet@100acre.com>
//   To: Depressed Donkey <im:eeyore@100acre.com>
//   DateTime: 2000-12-13T13:40:00-08:00
//   Subject: the weather will be fine today
//   Content-Type: text/plain; charset=utf-8
//
//   Wheee!
struct CpimInfo
{
    bool    present{false};
    QString from;
    QString to;
    QString dateTime;
    QString subject;
    QString contentType;  // Content-Type of the wrapped body
    QString wrappedBody;  // Body text after the CPIM header block
};
