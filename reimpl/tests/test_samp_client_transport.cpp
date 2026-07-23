#include "raknet/SAMPSocketTransform.h"
#include "raknet/PacketEnumerations.h"
#include "../../third_party/raknet-knogle/SAMPRakNet.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" const char* samp_raknet_knogle_auth_response(const char* challenge);

namespace
{
int assert_true(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		return 1;
	}
	return 0;
}

int assert_bytes(const uint8_t* actual, int actualLength, const uint8_t* expected, int expectedLength,
				 const char* message)
{
	if (actualLength != expectedLength || std::memcmp(actual, expected, static_cast<size_t>(expectedLength)) != 0)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::fprintf(stderr, "  actual:");
		for (int i = 0; i < actualLength; ++i)
			std::fprintf(stderr, " %02x", actual[i]);
		std::fprintf(stderr, "\n  expect:");
		for (int i = 0; i < expectedLength; ++i)
			std::fprintf(stderr, " %02x", expected[i]);
		std::fprintf(stderr, "\n");
		return 1;
	}
	return 0;
}
}

int main()
{
	int failed = 0;
	uint8_t output[16] = {};
	const char* knownAuthResponse = nullptr;

	const uint8_t plainOpenRequest1[] = { 0x18, 0x69, 0x69 };
	const uint8_t wireOpenRequest1[] = { 0x08, 0x1e, 0x77, 0xda };
	int length = RakNet::SampLegacyClientEncryptDatagram(plainOpenRequest1, 3, 7777, output, sizeof(output));
	failed += assert_bytes(output, length, wireOpenRequest1, 4, "open request magic transform matches original samp.dll");

	const uint8_t plainOpenRequest2[] = { 0x18, 0xd2, 0x16 };
	const uint8_t wireOpenRequest2[] = { 0x88, 0x1e, 0xe3, 0x9b };
	length = RakNet::SampLegacyClientEncryptDatagram(plainOpenRequest2, 3, 7777, output, sizeof(output));
	failed += assert_bytes(output, length, wireOpenRequest2, 4, "open request cookie transform matches original samp.dll");

	length = RakNet::SampLegacyClientEncryptDatagram(plainOpenRequest1, 3, 7777, output, 3);
	failed += assert_true(length == -1, "small output buffer is rejected");
	failed += assert_true(RakNet::SampLegacyClientTransportEnabled(), "SA-MP client transport is enabled in reimpl build");
	failed += assert_true(RakNet::ID_OPEN_CONNECTION_REQUEST == 0x18, "SA-MP open connection request id");
	failed += assert_true(RakNet::ID_OPEN_CONNECTION_COOKIE == 0x1a, "SA-MP open connection cookie id");
	failed += assert_true(RakNet::ID_CONNECTION_REQUEST_ACCEPTED == 0x22, "SA-MP connection accepted id");
	failed += assert_true(SAMPRakNet::GetMinimumSendBitsPerSecond() > 0.0f,
						  "standalone SAMPRakNet stub allows reliable handshake packets to flush");
	failed += assert_true(!SAMPRakNet::ShouldEnforceNetworkLimits(),
						  "standalone client does not apply open.mp server flood limits to its server");

	knownAuthResponse = samp_raknet_knogle_auth_response("58901CF451C93E3");
	failed += assert_true(knownAuthResponse != nullptr &&
							  std::strcmp(knownAuthResponse, "A24C762722180B42D75D32641BA1F5BD5705498A") == 0,
						  "SA-MP auth challenge table contains captured response vector");

	if (failed != 0)
	{
		std::fprintf(stderr, "%d checks failed\n", failed);
		return 1;
	}

	std::fprintf(stdout, "All SA-MP client transport checks passed\n");
	return 0;
}
