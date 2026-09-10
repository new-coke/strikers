// Byte-swap pass for .cam files: convert the chunks LoadAnimCameraData reads.

#include <stdint.h>
#include <string.h>

#include "port/endian.h"

#define CAM_CHUNK_ROOT       0x15501
#define CAM_CHUNK_KEY_COUNT  0x15508   // u32
#define CAM_CHUNK_CAMERA_POS 0x15509   // nlVector3[]
#define CAM_CHUNK_TARGET_POS 0x1550C   // nlVector3[]
#define CAM_CHUNK_CAMERA_ROT 0x15511   // nlQuaternion[]
#define CAM_CHUNK_FOV        0x1550F   // float[]
#define CAM_CHUNK_FOCAL_LEN  0x15510   // float[]

static int cam_chunk_is_read(uint32_t type)
{
    return type == CAM_CHUNK_KEY_COUNT || type == CAM_CHUNK_CAMERA_POS
        || type == CAM_CHUNK_TARGET_POS || type == CAM_CHUNK_CAMERA_ROT
        || type == CAM_CHUNK_FOV || type == CAM_CHUNK_FOCAL_LEN;
}

// Bytes per key in each array chunk, which LoadAnimCameraData copies keyCount of.
static unsigned long cam_chunk_element(uint32_t type)
{
    switch (type)
    {
    case CAM_CHUNK_CAMERA_POS:
    case CAM_CHUNK_TARGET_POS: return 12;
    case CAM_CHUNK_CAMERA_ROT: return 16;
    case CAM_CHUNK_FOV:
    case CAM_CHUNK_FOCAL_LEN:  return 4;
    default:                   return 0;
    }
}

// Returns the number of chunks converted. 0 means the buffer is not to be walked: not a .cam, a
// chunk sized past the buffer or aligned past its end, no whole key count, or a key array shorter
// than that count.
unsigned long port_cam_swap(void* data, unsigned long size)
{
    uint8_t* base = (uint8_t*)data;
    uint32_t rootId, rootSize;
    uint8_t* p;
    uint8_t* end;
    unsigned long n = 0;
    int haveCount = 0;
    uint32_t keyCount = 0;
    // Array chunks seen before the count chunk, checked once the count is known.
    unsigned long pending[8][2];
    int nPending = 0;

    if (data == NULL || size < 8)
        return 0;

    rootId = port_be32(base);
    rootSize = port_be32(base + 4);
    if ((rootId & 0x00FFFFFFu) != CAM_CHUNK_ROOT || rootSize > size - 8)
        return 0;

    memcpy(base, &rootId, 4);
    memcpy(base + 4, &rootSize, 4);
    n++;

    p = base + 8;
    end = p + rootSize;
    while (p + 8 <= end)
    {
        uint32_t id = port_be32(p);
        uint32_t chunkSize = port_be32(p + 4);

        // An alignment no asset uses is a shift the game's GetAlignedData cannot make.
        if (chunkSize > (uint32_t)(end - p - 8) || ((id & 0x7F000000u) >> 24) > 16)
            return 0;

        memcpy(p, &id, 4);
        memcpy(p + 4, &chunkSize, 4);
        n++;

        if (cam_chunk_is_read(id & 0x00FFFFFFu))
        {
            // Measured to the chunk end, so alignment padding is not counted twice; aligned past
            // the end, the file is refused.
            unsigned long len;
            uint32_t type = id & 0x00FFFFFFu;
            uint8_t* payload = port_chunk_payload(p, id, chunkSize, &len);
            if (payload == NULL)
                return 0;
            port_be32_array(payload, len / 4);

            if (type == CAM_CHUNK_KEY_COUNT)
            {
                if (len < 4)
                    return 0;
                memcpy(&keyCount, payload, 4);
                haveCount = 1;
            }
            else if (haveCount)
            {
                // By division: count * element wraps Windows' 32-bit unsigned long.
                if (keyCount > len / cam_chunk_element(type))
                    return 0;
            }
            else
            {
                if (nPending == 8)
                    return 0;
                pending[nPending][0] = cam_chunk_element(type);
                pending[nPending][1] = len;
                nPending++;
            }
        }

        p += 8 + chunkSize;
    }

    // No count, or an array shorter than it says, is read out of bounds by the loader.
    if (!haveCount)
        return 0;
    while (nPending-- > 0)
        if (keyCount > pending[nPending][1] / pending[nPending][0])
            return 0;

    return n;
}
