#include <Server/Components/Actors/actors.hpp>
#include <sdk.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>

namespace
{
constexpr int kRpcSetActorFacingAngle = 175;
constexpr StringView kCommand = "/rpc175raw";
constexpr int kRequiredActor = 0;
constexpr std::size_t kPayloadBits = 48;

struct FixedCase
{
	const char* name;
	std::array<uint8_t, 6> payload;
	uint32_t angleBits;
};

// STATIC_037:
// samp.dll 0.3.7-R5, SHA256=
// b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2,
// RPC 175 handler at samp.dll+0x1D9F0 reads uint16 actor ID followed by the
// raw uint32 bits of the float. See
// docs/re/rpc175_set_actor_facing_static_20260723.md.
//
// These are intentionally closed, fixed test vectors. There is no path from
// command text or player data to the RPC ID, payload bytes, or payload length.
// Every payload starts with the little-endian uint16 actor ID 0.
constexpr std::array<FixedCase, 9> kCases { {
	{
		"positive_zero",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		0x00000000u,
	},
	{
		"negative_zero",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 },
		0x80000000u,
	},
	{
		"positive_180",
		{ 0x00, 0x00, 0x00, 0x00, 0x34, 0x43 },
		0x43340000u,
	},
	{
		"positive_360",
		{ 0x00, 0x00, 0x00, 0x00, 0xB4, 0x43 },
		0x43B40000u,
	},
	{
		"negative_45",
		{ 0x00, 0x00, 0x00, 0x00, 0x34, 0xC2 },
		0xC2340000u,
	},
	{
		"positive_540",
		{ 0x00, 0x00, 0x00, 0x00, 0x07, 0x44 },
		0x44070000u,
	},
	{
		"positive_infinity",
		{ 0x00, 0x00, 0x00, 0x00, 0x80, 0x7F },
		0x7F800000u,
	},
	{
		"negative_infinity",
		{ 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF },
		0xFF800000u,
	},
	{
		"quiet_nan_payload_0x401234",
		{ 0x00, 0x00, 0x34, 0x12, 0xC0, 0x7F },
		0x7FC01234u,
	},
} };

constexpr bool payloadMatchesActorAndAngle(const FixedCase& test)
{
	return test.payload[0] == 0x00
		&& test.payload[1] == 0x00
		&& test.payload[2] == static_cast<uint8_t>(test.angleBits)
		&& test.payload[3] == static_cast<uint8_t>(test.angleBits >> 8)
		&& test.payload[4] == static_cast<uint8_t>(test.angleBits >> 16)
		&& test.payload[5] == static_cast<uint8_t>(test.angleBits >> 24);
}

constexpr bool allPayloadsMatchActorAndAngle()
{
	for (const FixedCase& test : kCases)
	{
		if (!payloadMatchesActorAndAngle(test))
		{
			return false;
		}
	}
	return true;
}

static_assert(kPayloadBits == 16 + 32);
static_assert(kCases.size() == 9);
static_assert(allPayloadsMatchActorAndAngle());
static_assert(kCases[0].angleBits == 0x00000000u);
static_assert(kCases[1].angleBits == 0x80000000u);
static_assert(kCases[8].angleBits == 0x7FC01234u);

class Rpc175Fixture final
	: public IComponent
	, public PlayerTextEventHandler
	, public PlayerConnectEventHandler
{
public:
	PROVIDE_UID(0x5250433137354658);

	~Rpc175Fixture() override
	{
		detach();
	}

	StringView componentName() const override
	{
		return "RPC175 fixed compatibility fixture";
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
			"[rpc175_fixture] loaded; fixed command=/rpc175raw rpc=175 "
			"actor=0 cases=9 payload_bits=48 dispatchEvents=0 channel=SyncRPC");
	}

	void onInit(IComponentList* components) override
	{
		actors_ = components == nullptr
			? nullptr
			: components->queryComponent<IActorsComponent>();

		if (core_ != nullptr)
		{
			core_->printLn(
				"[rpc175_fixture] actors_component=%s",
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

		IActor* actor = actors_->get(kRequiredActor);
		if (actor == nullptr || !actor->isStreamedInForPlayer(player))
		{
			reject(player, "actor 0 must exist and be streamed for this player");
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
				"[rpc175_fixture] trigger player=%d actor=0 cases=%u rpc=175 "
				"payload_bits=48 dispatchEvents=0 channel=%d",
				playerID,
				static_cast<unsigned>(kCases.size()),
				static_cast<int>(OrderingChannel_SyncRPC));
		}

		unsigned sentCount = 0;
		unsigned caseIndex = 0;
		for (const FixedCase& test : kCases)
		{
			// sendRPC consumes the span synchronously. The SDK documents the
			// Span size as BITS, not bytes, so this is exactly 48 bits.
			std::array<uint8_t, 6> payload = test.payload;
			const bool sent = player.sendRPC(
				kRpcSetActorFacingAngle,
				Span<uint8_t>(payload.data(), kPayloadBits),
				OrderingChannel_SyncRPC,
				false);

			if (sent)
			{
				++sentCount;
			}

			if (core_ != nullptr)
			{
				core_->printLn(
					"[rpc175_fixture] case_index=%u case=%s actor=0 "
					"angle_bits=%08x payload_bits=48 "
					"payload=%02x%02x%02x%02x%02x%02x sent=%d",
					caseIndex,
					test.name,
					static_cast<unsigned>(test.angleBits),
					static_cast<unsigned>(test.payload[0]),
					static_cast<unsigned>(test.payload[1]),
					static_cast<unsigned>(test.payload[2]),
					static_cast<unsigned>(test.payload[3]),
					static_cast<unsigned>(test.payload[4]),
					static_cast<unsigned>(test.payload[5]),
					sent ? 1 : 0);
			}
			++caseIndex;
		}

		if (sentCount == kCases.size())
		{
			player.sendClientMessage(
				Colour::White(),
				"RPC175 fixture sent 9/9 fixed cases; reconnect to rerun.");
		}
		else
		{
			player.sendClientMessage(
				Colour::White(),
				"RPC175 fixture transport failure; inspect server log before reconnecting.");
		}

		return true;
	}

private:
	static bool isValidPlayerID(int playerID)
	{
		return playerID >= 0 && playerID < PLAYER_POOL_SIZE;
	}

	void reject(IPlayer& player, const char* reason)
	{
		if (core_ != nullptr)
		{
			core_->printLn(
				"[rpc175_fixture] reject player=%d reason=%s",
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
	return new (std::nothrow) Rpc175Fixture();
}
