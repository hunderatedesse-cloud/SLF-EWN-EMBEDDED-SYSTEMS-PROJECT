#ifndef PACKETPARSER_H
#define PACKETPARSER_H

#include "Types.h"

#include <QByteArray>
#include <QVector>

namespace Slf {

class PacketParser
{
public:
    QVector<NodeData> feed(const QByteArray &chunk);
    void reset();

    // Bytes thrown away while hunting for frame alignment. A steadily climbing
    // count with zero decoded frames means the link is delivering data in a
    // layout this parser does not recognise.
    quint64 discardedBytes() const { return discarded_; }
    int pendingBytes() const { return buffer_.size(); }

    static bool decode(const quint8 *frame, NodeData &out);
    static bool decodeDumpRecord(const quint8 *record16, NodeData &out, quint32 *ridOut = nullptr);
    static QByteArray encode(const NodeData &data);

private:
    static bool parseAsciiLine(const QByteArray &line, NodeData &out);
    static bool parseLoraLine(const QString &text, NodeData &out);
    static bool looksLikeText(const QByteArray &bytes);

    QByteArray buffer_;
    quint64 discarded_ = 0;
};

} // namespace Slf

#endif
