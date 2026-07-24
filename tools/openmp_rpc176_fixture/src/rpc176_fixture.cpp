#include <Server/Components/Actors/actors.hpp>
#include <sdk.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>

namespace
{
constexpr int kRpcSetActorPosition = 176;
constexpr StringView kCommand = "/rpc176raw";
constexpr int kRequiredActiveActor = 0;
constexpr int kRequiredInactiveActor = 999;

struct FixedCase
{
	const char* name;
	std::array<uint8_t, 15> payload;
	std::size_t payloadBits;
	uint16_t actorID;
	std::array<uint32_t, 3> coordinateBits;
	bool coordinatesPresent;
};

// STATIC_037:
// samp.dll 0.3.7-R5, SHA256=
// b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2,
// RPC 176 at samp.dll+0x1DAD0 reads uint16 ActorID followed by the raw
// uint32 bits of X, Y, and Z. It consumes no payload after those first 112
// bits. See docs/re/rpc176_set_actor_position_static_20260723.md.
//
// These are closed, fixed laboratory vectors. Command text and player input
// cannot select an RPC ID, actor ID, coordinate, byte, or bit length.
constexpr std::array<FixedCase, 9> kCases { {
	{
		"active_baseline_112",
		{ 0x00, 0x00, 0x00, 0x20, 0x9B, 0x43, 0x00, 0xD0,
		  0xFF, 0x44, 0x00, 0x00, 0x88, 0x41, 0x00 },
		112,
		0,
		{ 0x439B2000u, 0x44FFD000u, 0x41880000u },
		true,
	},
	{
		"active_fractional_112",
		{ 0x00, 0x00, 0x00, 0x90, 0x9B, 0x43, 0x00, 0xB8,
		  0xFF, 0x44, 0x00, 0x00, 0x94, 0x41, 0x00 },
		112,
		0,
		{ 0x439B9000u, 0x44FFB800u, 0x41940000u },
		true,
	},
	{
		"active_duplicate_112",
		{ 0x00, 0x00, 0x00, 0x90, 0x9B, 0x43, 0x00, 0xB8,
		  0xFF, 0x44, 0x00, 0x00, 0x94, 0x41, 0x00 },
		112,
		0,
		{ 0x439B9000u, 0x44FFB800u, 0x41940000u },
		true,
	},
	{
		"active_negative_zero_x_112",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xD0,
		  0xFF, 0x44, 0x00, 0x00, 0x88, 0x41, 0x00 },
		112,
		0,
		{ 0x80000000u, 0x44FFD000u, 0x41880000u },
		true,
	},
	{
		"active_extra_one_bit_113",
		{ 0x00, 0x00, 0x00, 0xC0, 0x9A, 0x43, 0x00, 0xE8,
		  0xFF, 0x44, 0x00, 0x00, 0x86, 0x41, 0x80 },
		113,
		0,
		{ 0x439AC000u, 0x44FFE800u, 0x41860000u },
		true,
	},
	{
		"inactive_999_nonfinite_112",
		{ 0xE7, 0x03, 0x34, 0x12, 0xC0, 0x7F, 0x00, 0x00,
		  0x80, 0x7F, 0x00, 0x00, 0x80, 0xFF, 0x00 },
		112,
		999,
		{ 0x7FC01234u, 0x7F800000u, 0xFF800000u },
		true,
	},
	{
		"out_of_range_1000_112",
		{ 0xE8, 0x03, 0x00, 0x80, 0xF6, 0x42, 0x00, 0x40,
		  0xE4, 0xC3, 0x00, 0x40, 0x9C, 0x42, 0x00 },
		112,
		1000,
		{ 0x42F68000u, 0xC3E44000u, 0x429C4000u },
		true,
	},
	{
		"truncated_after_actor_16",
		{ 0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		16,
		1000,
		{ 0x00000000u, 0x00000000u, 0x00000000u },
		false,
	},
	{
		"active_return_112",
		{ 0x00, 0x00, 0x6F, 0x00, 0x9B, 0x43, 0x44, 0xDD,
		  0xFF, 0x44, 0xB8, 0x1E, 0x85, 0x41, 0x00 },
		112,
		0,
		{ 0x439B006Fu, 0x44FFDD44u, 0x41851EB8u },
		true,
	},
} };

constexpr std::size_t payloadBytes(std::size_t bits)
{
	return (bits + 7u) / 8u;
}

constexpr bool arraysEqual(
	const std::array<uint8_t, 15>& left,
	const std::array<uint8_t, 15>& right)
{
	for (std::size_t index = 0; index < left.size(); ++index)
	{
		if (left[index] != right[index])
		{
			return false;
		}
	}
	return true;
}

constexpr bool payloadMatchesMetadata(const FixedCase& test)
{
	if (test.payloadBits != 16 && test.payloadBits != 112
		&& test.payloadBits != 113)
	{
		return false;
	}

	if (test.payload[0] != static_cast<uint8_t>(test.actorID)
		|| test.payload[1] != static_cast<uint8_t>(test.actorID >> 8))
	{
		return false;
	}

	if (!test.coordinatesPresent)
	{
		if (test.payloadBits != 16)
		{
			return false;
		}
	}
	else
	{
		if (test.payloadBits < 112)
		{
			return false;
		}
		for (std::size_t component = 0; component < 3; ++component)
		{
			for (std::size_t byte = 0; byte < 4; ++byte)
			{
				const uint8_t expected = static_cast<uint8_t>(
					test.coordinateBits[component] >> (byte * 8));
				if (test.payload[2 + component * 4 + byte] != expected)
				{
					return false;
				}
			}
		}
	}

	const std::size_t bytes = payloadBytes(test.payloadBits);
	for (std::size_t index = bytes; index < test.payload.size(); ++index)
	{
		if (test.payload[index] != 0)
		{
			return false;
		}
	}

	return test.payloadBits != 113 || test.payload[14] == 0x80;
}

constexpr bool allPayloadsMatchMetadata()
{
	for (const FixedCase& test : kCases)
	{
		if (!payloadMatchesMetadata(test))
		{
			return false;
		}
		if (test.actorID == 0 && test.payloadBits < 112)
		{
			// Never exercise the original handler's ignored failed reads on a
			// live actor slot.
			return false;
		}
	}
	return true;
}

static_assert(kCases.size() == 9);
static_assert(allPayloadsMatchMetadata());
static_assert(kCases[0].payloadBits == 112);
static_assert(kCases[4].payloadBits == 113);
static_assert(kCases[4].payload[14] == 0x80);
static_assert(kCases[5].actorID == 999);
static_assert(kCases[5].coordinateBits[0] == 0x7FC01234u);
static_assert(kCases[5].coordinateBits[1] == 0x7F800000u);
static_assert(kCases[5].coordinateBits[2] == 0xFF800000u);
static_assert(kCases[6].actorID == 1000);
static_assert(kCases[7].actorID == 1000);
static_assert(kCases[7].payloadBits == 16);
static_assert(!kCases[7].coordinatesPresent);
static_assert(arraysEqual(kCases[1].payload, kCases[2].payload));
static_assert(kCases[1].payloadBits == kCases[2].payloadBits);
static_assert(kCases[8].coordinateBits[0] == 0x439B006Fu);
static_assert(kCases[8].coordinateBits[1] == 0x44FFDD44u);
static_assert(kCases[8].coordinateBits[2] == 0x41851EB8u);

class Rpc176Fixture final
	: public IComponent
	, public PlayerTextEventHandler
	, public PlayerConnectEventHandler
{
public:
	PROVIDE_UID(0x5250433137364658);

	~Rpc176Fixture() override
	{
		detach();
	}

	StringView componentName() const override
	{
		return "RPC176 fixed compatibility fixture";
	}

	SemanticVersion componentVersion() const override
	{
		return SemanticVersion(0, 1, 0, 0);
	}

	void onLoad(ICore* core) override
	{
		core_ = core;
		if (core_ == nullptr)
		{
			return;
		}

		core_->getPlayers().getPlayerTextDispatcher().addEventHandler(this);
		core_->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
		attached_ = true;
		core_->printLn(
			"[rpc176_fixture] loaded; fixed command=/rpc176raw rpc=176 "
			"cases=9 dispatchEvents=0 channel=SyncRPC");
	}

	void onInit(IComponentList* components) override
	{
		actors_ = components == nullptr
			? nullptr
			: components->queryComponent<IActorsComponent>();

		if (core_ != nullptr)
		{
			core_->printLn(
				"[rpc176_fixture] actors_component=%s",
				actors_ == nullptr ? "missing" : "ready");
		}
	}

	void onFree(IComponent* component) override
	{
		if (component == actors_)
		{
			actors_ = nullptr;
		}
	}

	void free() override
	{
		delete this;
	}

	void reset() override
	{
		fired_.fill(false);
	}

	void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason) override
	{
		const int playerID = player.getID();
		if (isValidPlayerID(playerID))
		{
			fired_[static_cast<std::size_t>(playerID)] = false;
		}
	}

	bool onPlayerCommandText(IPlayer& player, StringView message) override
	{
		if (message != kCommand)
		{
			return false;
		}

		const int playerID = player.getID();
		if (!isValidPlayerID(playerID))
		{
			reject(player, "invalid player ID");
			return true;
		}

		if (player.isBot())
		{
			reject(player, "bots are not eligible");
			return true;
		}

		if (player.getClientVersion() != ClientVersion::ClientVersion_SAMP_037)
		{
			reject(player, "requires the SA-MP 0.3.7 protocol");
			return true;
		}

		if (player.getState() == PlayerState_None)
		{
			reject(player, "player is not initialized");
			return true;
		}

		const PeerNetworkData& networkData = player.getNetworkData();
		if (networkData.network == nullptr
			|| networkData.network->getNetworkType() != ENetworkType_RakNetLegacy)
		{
			reject(player, "requires the legacy RakNet transport");
			return true;
		}

		if (actors_ == nullptr)
		{
			reject(player, "Actors component is unavailable");
			return true;
		}

		IActor* activeActor = actors_->get(kRequiredActiveActor);
		if (activeActor == nullptr || !activeActor->isStreamedInForPlayer(player))
		{
			reject(player, "actor 0 must exist and be streamed for this player");
			return true;
		}

		if (actors_->get(kRequiredInactiveActor) != nullptr)
		{
			reject(player, "actor 999 must be inactive");
			return true;
		}

		const std::size_t firedIndex = static_cast<std::size_t>(playerID);
		if (fired_[firedIndex])
		{
			reject(player, "fixture is one-shot per connection; reconnect to rerun");
			return true;
		}

		// Mark before the first send so a partial transport failure cannot be
		// retried into an ambiguous duplicate sequence on this connection.
		fired_[firedIndex] = true;

		if (core_ != nullptr)
		{
			core_->printLn(
				"[rpc176_fixture] trigger player=%d active_actor=0 "
				"inactive_actor=999 cases=%u rpc=176 dispatchEvents=0 channel=%d",
				playerID,
				static_cast<unsigned>(kCases.size()),
				static_cast<int>(OrderingChannel_SyncRPC));
		}

		unsigned sentCount = 0;
		unsigned caseIndex = 0;
		for (const FixedCase& test : kCases)
		{
			// sendRPC consumes the span synchronously. The SDK documents the
			// Span size as BITS, not bytes. The 113-bit case therefore sends
			// exactly one extra set bit, not all 15 backing bytes.
			std::array<uint8_t, 15> payload = test.payload;
			const bool sent = player.sendRPC(
				kRpcSetActorPosition,
				Span<uint8_t>(payload.data(), test.payloadBits),
				OrderingChannel_SyncRPC,
				false);

			if (sent)
			{
				++sentCount;
			}

			logCase(caseIndex, test, sent);
			++caseIndex;
		}

		if (sentCount == kCases.size())
		{
			player.sendClientMessage(
				Colour::White(),
				"RPC176 fixture sent 9/9 fixed cases; actor 0 restored.");
		}
		else
		{
			player.sendClientMessage(
				Colour::White(),
				"RPC176 fixture transport failure; inspect server log before reconnecting.");
		}

		return true;
	}

private:
	static bool isValidPlayerID(int playerID)
	{
		return playerID >= 0 && playerID < PLAYER_POOL_SIZE;
	}

	void logCase(unsigned caseIndex, const FixedCase& test, bool sent)
	{
		if (core_ == nullptr)
		{
			return;
		}

		const unsigned bytes = static_cast<unsigned>(payloadBytes(test.payloadBits));
		if (!test.coordinatesPresent)
		{
			core_->printLn(
				"[rpc176_fixture] case_index=%u case=%s actor=%u "
				"xyz_bits=omitted payload_bits=%u payload_bytes=%u "
				"payload=%02x%02x sent=%d",
				caseIndex,
				test.name,
				static_cast<unsigned>(test.actorID),
				static_cast<unsigned>(test.payloadBits),
				bytes,
				static_cast<unsigned>(test.payload[0]),
				static_cast<unsigned>(test.payload[1]),
				sent ? 1 : 0);
			return;
		}

		if (test.payloadBits == 113)
		{
			core_->printLn(
				"[rpc176_fixture] case_index=%u case=%s actor=%u "
				"xyz_bits=%08x,%08x,%08x payload_bits=113 payload_bytes=15 "
				"payload=%02x%02x%02x%02x%02x%02x%02x%02x"
				"%02x%02x%02x%02x%02x%02x%02x sent=%d",
				caseIndex,
				test.name,
				static_cast<unsigned>(test.actorID),
				static_cast<unsigned>(test.coordinateBits[0]),
				static_cast<unsigned>(test.coordinateBits[1]),
				static_cast<unsigned>(test.coordinateBits[2]),
				static_cast<unsigned>(test.payload[0]),
				static_cast<unsigned>(test.payload[1]),
				static_cast<unsigned>(test.payload[2]),
				static_cast<unsigned>(test.payload[3]),
				static_cast<unsigned>(test.payload[4]),
				static_cast<unsigned>(test.payload[5]),
				static_cast<unsigned>(test.payload[6]),
				static_cast<unsigned>(test.payload[7]),
				static_cast<unsigned>(test.payload[8]),
				static_cast<unsigned>(test.payload[9]),
				static_cast<unsigned>(test.payload[10]),
				static_cast<unsigned>(test.payload[11]),
				static_cast<unsigned>(test.payload[12]),
				static_cast<unsigned>(test.payload[13]),
				static_cast<unsigned>(test.payload[14]),
				sent ? 1 : 0);
			return;
		}

		core_->printLn(
			"[rpc176_fixture] case_index=%u case=%s actor=%u "
			"xyz_bits=%08x,%08x,%08x payload_bits=112 payload_bytes=14 "
			"payload=%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x sent=%d",
			caseIndex,
			test.name,
			static_cast<unsigned>(test.actorID),
			static_cast<unsigned>(test.coordinateBits[0]),
			static_cast<unsigned>(test.coordinateBits[1]),
			static_cast<unsigned>(test.coordinateBits[2]),
			static_cast<unsigned>(test.payload[0]),
			static_cast<unsigned>(test.payload[1]),
			static_cast<unsigned>(test.payload[2]),
			static_cast<unsigned>(test.payload[3]),
			static_cast<unsigned>(test.payload[4]),
			static_cast<unsigned>(test.payload[5]),
			static_cast<unsigned>(test.payload[6]),
			static_cast<unsigned>(test.payload[7]),
			static_cast<unsigned>(test.payload[8]),
			static_cast<unsigned>(test.payload[9]),
			static_cast<unsigned>(test.payload[10]),
			static_cast<unsigned>(test.payload[11]),
			static_cast<unsigned>(test.payload[12]),
			static_cast<unsigned>(test.payload[13]),
			sent ? 1 : 0);
	}

	void reject(IPlayer& player, const char* reason)
	{
		if (core_ != nullptr)
		{
			core_->printLn(
				"[rpc176_fixture] reject player=%d reason=%s",
				player.getID(),
				reason);
		}
		player.sendClientMessage(Colour::White(), reason);
	}

	void detach()
	{
		if (core_ != nullptr && attached_)
		{
			core_->getPlayers().getPlayerTextDispatcher().removeEventHandler(this);
			core_->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
		}
		attached_ = false;
		core_ = nullptr;
		actors_ = nullptr;
	}

	ICore* core_ = nullptr;
	IActorsComponent* actors_ = nullptr;
	std::array<bool, PLAYER_POOL_SIZE> fired_ {};
	bool attached_ = false;
};
} // namespace

COMPONENT_ENTRY_POINT()
{
	return new (std::nothrow) Rpc176Fixture();
}
