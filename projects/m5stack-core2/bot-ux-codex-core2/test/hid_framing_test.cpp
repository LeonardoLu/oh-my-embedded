#include "HidFraming.h"

#include <assert.h>
#include <string.h>

int main()
{
    const char* message =
        "{\"method\":\"v.oai.hid\",\"params\":{\"k\":\"AG00\",\"act\":1,\"ag\":0,"
        "\"label\":\"agent-\xE5\x85\xAD-{ready}\\\"\"}}";
    const size_t length = strlen(message);
    uint8_t first[HidFraming::kPayloadSize];
    uint8_t second[HidFraming::kPayloadSize];
    const size_t firstSize = HidFraming::encodeChunk(message, length, 0, first);
    const size_t secondSize = HidFraming::encodeChunk(message, length, firstSize, second);
    assert(firstSize == HidFraming::kChunkSize);
    assert(secondSize == length - firstSize);
    assert(first[0] == HidFraming::kRpcChannel);
    assert(first[1] == HidFraming::kChunkSize);

    HidFraming::Receiver receiver;
    assert(receiver.append(first, sizeof(first)));
    char decoded[160];
    assert(!receiver.takeLine(decoded, sizeof(decoded)));
    assert(receiver.append(second, sizeof(second)));
    assert(receiver.takeLine(decoded, sizeof(decoded)));
    assert(strcmp(decoded, message) == 0);

    // A full-sized final chunk has no transport marker; balanced JSON completes it.
    const char* exact = "{\"padding\":\"12345678901234567890123456789012345678901234567\"}";
    assert(strlen(exact) == HidFraming::kChunkSize);
    uint8_t exactPayload[HidFraming::kPayloadSize];
    assert(HidFraming::encodeChunk(exact, strlen(exact), 0, exactPayload) == HidFraming::kChunkSize);
    assert(receiver.append(exactPayload, sizeof(exactPayload)));
    assert(receiver.takeLine(decoded, sizeof(decoded)));
    assert(strcmp(decoded, exact) == 0);

    const char* incomplete = "{\"id\":1";
    uint8_t malformed[HidFraming::kPayloadSize];
    assert(HidFraming::encodeChunk(incomplete, strlen(incomplete), 0, malformed) == strlen(incomplete));
    assert(!receiver.append(malformed, sizeof(malformed)));
    assert(receiver.overflowed());
    const char* recovered = "{\"method\":\"sys.version\",\"id\":2}";
    uint8_t recovery[HidFraming::kPayloadSize];
    assert(HidFraming::encodeChunk(recovered, strlen(recovered), 0, recovery) == strlen(recovered));
    assert(receiver.append(recovery, sizeof(recovery)));
    assert(receiver.takeLine(decoded, sizeof(decoded)));
    assert(strcmp(decoded, recovered) == 0);

    uint8_t invalid[HidFraming::kPayloadSize]{};
    invalid[0] = 1;
    invalid[1] = 1;
    assert(!receiver.append(invalid, sizeof(invalid)));
    invalid[0] = HidFraming::kRpcChannel;
    invalid[1] = 62;
    assert(!receiver.append(invalid, sizeof(invalid)));
    return 0;
}
