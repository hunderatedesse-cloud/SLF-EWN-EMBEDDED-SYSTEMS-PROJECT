#include "core/Crc8.h"
#include "core/PacketParser.h"
#include "core/Types.h"

#include <QByteArray>
#include <cstdio>

using namespace Slf;

static int fails = 0;

static void expect(bool ok, const char *what)
{
    if (ok) {
        std::printf("  OK  %s\n", what);
        return;
    }
    ++fails;
    std::printf("FAIL  %s\n", what);
}

static quint32 be32(const quint8 *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

int main()
{
    std::printf("SLF-EWN protocol v1.0 smoke test\n");

    NodeData src;
    src.deviceId = 260;
    src.moistureRaw = 4120;
    src.tiltX = 12;
    src.tiltY = -3;
    src.tiltZ = 980;
    src.hardwareStatus = 0x80;

    const QByteArray frame = PacketParser::encode(src);
    expect(frame.size() == kPacketSize, "LoRa frame is 12 bytes");
    expect(quint8(frame[11]) == Crc8::compute(reinterpret_cast<const quint8 *>(frame.constData()), 11),
           "CRC-8/SMBUS over 11 payload bytes");

    NodeData decoded;
    expect(PacketParser::decode(reinterpret_cast<const quint8 *>(frame.constData()), decoded),
           "decode 12-byte frame");
    expect(decoded.deviceId == 260 && decoded.moistureRaw == 4120 && decoded.tiltY == -3
               && decoded.hardwareEmergency(),
           "node / moisture / tilt / emergency bits");

    QByteArray rec(kDumpRecordSize, '\0');
    rec[0] = 0;
    rec[1] = 0;
    rec[2] = 0;
    rec[3] = 42;
    rec.replace(4, 12, frame);

    quint32 rid = 0;
    NodeData fromDump;
    expect(PacketParser::decodeDumpRecord(reinterpret_cast<const quint8 *>(rec.constData()), fromDump, &rid),
           "decode 16-byte dump record");
    expect(rid == 42 && fromDump.deviceId == 260 && fromDump.moistureRaw == 4120,
           "RID 42 and NODE260 from dump record");

    QByteArray incr;
    incr.append(char(kCmdIncremental));
    const quint32 last = 42;
    incr.append(char((last >> 24) & 0xFF));
    incr.append(char((last >> 16) & 0xFF));
    incr.append(char((last >> 8) & 0xFF));
    incr.append(char(last & 0xFF));
    expect(incr.size() == 5 && quint8(incr[0]) == 0x4E && be32(reinterpret_cast<const quint8 *>(incr.constData()) + 1) == 42,
           "INCREMENTAL 0x4E + big-endian last RID 42");

    QByteArray status;
    status.append(char(kProtoMagic));
    status.append(char(kCmdStatus));
    status.append(char(0));
    status.append(char(0));
    status.append(char(0));
    status.append(char(8));
    const quint32 writeCount = 3;
    const quint32 freeRec = 8000;
    for (int shift = 24; shift >= 0; shift -= 8)
        status.append(char((writeCount >> shift) & 0xFF));
    for (int shift = 24; shift >= 0; shift -= 8)
        status.append(char((freeRec >> shift) & 0xFF));
    expect(status.size() == 14 && quint8(status[0]) == 0xAA && quint8(status[1]) == 0x53
               && be32(reinterpret_cast<const quint8 *>(status.constData()) + 2) == 8
               && be32(reinterpret_cast<const quint8 *>(status.constData()) + 6) == 3,
           "STATUS header AA 53 length 8 write_count=3");

    QByteArray dump;
    dump.append(char(kProtoMagic));
    dump.append(char(kCmdFullDump));
    dump.append(char(0));
    dump.append(char(0));
    dump.append(char(0));
    dump.append(char(kDumpRecordSize));
    dump.append(rec);

    // Chunked arrival: 3 + 5 + rest, matching readyRead splits.
    QByteArray buf;
    int accepted = 0;
    auto drain = [&](const QByteArray &chunk) {
        buf.append(chunk);
        while (buf.size() >= 6 && quint8(buf[0]) == kProtoMagic) {
            const quint32 length = be32(reinterpret_cast<const quint8 *>(buf.constData()) + 2);
            if (quint32(buf.size()) < 6 + length)
                return;
            const QByteArray payload = buf.mid(6, int(length));
            buf.remove(0, 6 + int(length));
            if (quint8(dump[1]) == kCmdFullDump) {
                quint32 gotRid = 0;
                NodeData d;
                if (payload.size() == kDumpRecordSize
                    && PacketParser::decodeDumpRecord(reinterpret_cast<const quint8 *>(payload.constData()), d, &gotRid)
                    && gotRid == 42)
                    ++accepted;
            }
        }
    };
    drain(dump.left(3));
    drain(dump.mid(3, 5));
    drain(dump.mid(8));
    expect(accepted == 1 && buf.isEmpty(), "chunked FULL_DUMP reassembles to one record");

    QByteArray bad = rec;
    bad[15] = char(quint8(bad[15]) ^ 0xFF);
    NodeData junk;
    expect(!PacketParser::decodeDumpRecord(reinterpret_cast<const quint8 *>(bad.constData()), junk, nullptr),
           "bad CRC is rejected");

    std::printf(fails ? "\n%d check(s) failed\n" : "\nAll checks passed\n", fails);
    return fails ? 1 : 0;
}
