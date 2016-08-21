#ifndef PROTOCOLHTTP_H
#define PROTOCOLHTTP_H

#include <QObject>

#include "protocol.h"

namespace CWSGI {

class Socket;
class ProtocolHttp : public Protocol
{
    Q_OBJECT
public:
    explicit ProtocolHttp(QObject *parent = 0);

    virtual void readyRead();

private:
    inline void processRequest(const char *ptr, const char *end, Socket *sock);
    inline void processHeader(const char *ptr, const char *end, Socket *sock);

};

}

#endif // PROTOCOLHTTP_H