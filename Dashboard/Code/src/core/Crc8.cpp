#include "Crc8.h"

namespace Slf {

quint8 Crc8::compute(const quint8 *data, int length)
{
    quint8 crc = 0x00;
    for (int i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80)
                crc = static_cast<quint8>((crc << 1) ^ 0x07);
            else
                crc = static_cast<quint8>(crc << 1);
        }
    }
    return crc;
}

quint8 Crc8::compute(const QByteArray &bytes)
{
    return compute(reinterpret_cast<const quint8 *>(bytes.constData()), bytes.size());
}

} // namespace Slf
