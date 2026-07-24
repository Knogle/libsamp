#include "raknet/BitStream.h"
#include "raknet/PacketEnumerations.h"
#include "raknet/ReliabilityLayer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

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

RakNet::BitStream make_reliable_ordered_datagram(const uint8_t* data, std::size_t bytes,
										  RakNet::MessageNumberType messageNumber,
										  RakNet::OrderingIndexType orderingIndex, bool isSplit,
										  RakNet::SplitPacketIdType splitId = 0,
										  RakNet::SplitPacketIndexType splitIndex = 0,
										  RakNet::SplitPacketIndexType splitCount = 0)
{
	RakNet::BitStream datagram;
	const bool hasAcks = false;
	const auto reliability = static_cast<uint8_t>(RakNet::RELIABLE_ORDERED);
	const uint8_t orderingChannel = 0;
	const auto dataBitLength = static_cast<uint16_t>(bytes * 8U);

	// OPENMP_REF + PROBE_TRACE:
	// SA-MP's legacy RakNet reliability frame starts with an ACK-present bit,
	// followed by the internal-packet header written by
	// ReliabilityLayer::WriteToBitStreamFromInternalPacket.
	datagram.Write(hasAcks);
	datagram.Write(messageNumber);
	datagram.WriteBits(&reliability, 4, true);
	datagram.WriteBits(&orderingChannel, 5, true);
	datagram.Write(orderingIndex);
	datagram.Write(isSplit);
	if (isSplit)
	{
		datagram.Write(splitId);
		datagram.WriteCompressed(splitIndex);
		datagram.WriteCompressed(splitCount);
	}
	datagram.WriteCompressed(dataBitLength);
	datagram.WriteAlignedBytes(data, static_cast<int>(bytes));
	return datagram;
}

bool receive_datagram(RakNet::ReliabilityLayer& receiver, RakNet::BitStream& datagram,
					  RakNet::DataStructures::List<RakNet::PluginInterface*>& plugins, bool& shouldBan)
{
	const RakNet::PlayerID server = {0x0100007FU, 7778U};
	return receiver.HandleSocketReceiveFromConnectedPlayer(reinterpret_cast<const char*>(datagram.GetData()),
											  datagram.GetNumberOfBytesUsed(), server, plugins, 576, shouldBan);
}
}

int main()
{
	int failed = 0;
	RakNet::ReliabilityLayer receiver;
	RakNet::DataStructures::List<RakNet::PluginInterface*> plugins;
	std::vector<uint8_t> payload(2518U);
	constexpr std::size_t fragmentBytes = 512U;
	constexpr RakNet::SplitPacketIndexType fragmentCount = 5U;
	constexpr std::array<RakNet::SplitPacketIndexType, fragmentCount> receiveOrder = {2U, 0U, 4U, 1U, 3U};
	unsigned char* received = nullptr;
	bool shouldBan = false;

	// ReliabilityLayer is normally configured through RakPeer. Keep this
	// standalone harness deterministic and suppress progress pseudo-packets.
	receiver.SetSplitMessageProgressInterval(0);

	payload[0] = static_cast<uint8_t>(RakNet::ID_RPC);
	payload[1] = 61U; // ScrShowDialog
	for (std::size_t i = 2; i < payload.size(); ++i)
		payload[i] = static_cast<uint8_t>((i * 37U + 11U) & 0xFFU);

	for (std::size_t arrival = 0; arrival < receiveOrder.size(); ++arrival)
	{
		const RakNet::SplitPacketIndexType splitIndex = receiveOrder[arrival];
		const std::size_t offset = static_cast<std::size_t>(splitIndex) * fragmentBytes;
		const std::size_t bytes =
			payload.size() - offset < fragmentBytes ? payload.size() - offset : fragmentBytes;
		auto datagram = make_reliable_ordered_datagram(payload.data() + offset, bytes, splitIndex, 0U, true,
												 23U, splitIndex, fragmentCount);

		failed += assert_true(datagram.GetNumberOfBytesUsed() <= 576,
							  "split datagram fits the probe server MTU");
		shouldBan = false;
		failed += assert_true(receive_datagram(receiver, datagram, plugins, shouldBan),
							  "reliable ordered split datagram is accepted");
		failed += assert_true(!shouldBan, "valid split datagram does not request a ban");
	}

	failed += assert_true(receiver.GetStatistics()->messagesWaitingForReassembly == 0U,
					  "completed split message leaves no pending fragments");

	const int receivedBits = receiver.Receive(&received);
	failed += assert_true(receivedBits == static_cast<int>(payload.size() * 8U),
					  "reassembled payload preserves its bit length");
	if (received != nullptr)
	{
		failed += assert_true(std::memcmp(received, payload.data(), payload.size()) == 0,
						  "reassembled payload is byte-identical");
		delete[] received;
		received = nullptr;
	}
	else
	{
		failed += assert_true(false, "reassembled payload is available");
	}

	// A later ordered packet must not remain blocked behind the split RPC. This
	// models the small dialogs and chat messages that stopped arriving after the
	// large attachment-model dialog in the replacement-client trace.
	const uint8_t followUp[] = {static_cast<uint8_t>(RakNet::ID_RPC), 61U, 0xAAU, 0x55U};
	auto followUpDatagram =
		make_reliable_ordered_datagram(followUp, sizeof(followUp), fragmentCount, 1U, false);
	shouldBan = false;
	failed += assert_true(receive_datagram(receiver, followUpDatagram, plugins, shouldBan),
					  "ordered packet following a split message is accepted");
	failed += assert_true(!shouldBan, "follow-up packet does not request a ban");

	const int followUpBits = receiver.Receive(&received);
	failed += assert_true(followUpBits == static_cast<int>(sizeof(followUp) * 8U),
					  "ordered stream advances after reassembly");
	if (received != nullptr)
	{
		failed += assert_true(std::memcmp(received, followUp, sizeof(followUp)) == 0,
						  "follow-up payload is byte-identical");
		delete[] received;
	}

	if (failed != 0)
	{
		std::fprintf(stderr, "%d checks failed\n", failed);
		return 1;
	}

	std::fprintf(stdout, "All RakNet split reassembly checks passed\n");
	return 0;
}
