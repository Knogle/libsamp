#include <Server/Components/Actors/actors.hpp>
#include <sdk.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>

namespace
{
constexpr int kRpcSetActorHealth = 178;
constexpr StringView kCommand = "/rpc178raw";
constexpr int kRequiredActiveActor = 0;
constexpr int kRequiredInactiveActor = 999;

struct FixedCase
{
	const char* name;
	std::array<uint8_t, 7> payload;
	std::size_t payloadBits;
	uint16_t actorID;
	uint32_t healthBits;
};

// STATIC_037:
// samp.dll 0.3.7-R5, SHA256=
// b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2,
// RPC 178 handler at samp.dll+0x1DBE0 reads uint16 actor ID followed by the
// raw uint32 bits of the float. See docs/re/rpc178_set_actor_health_static_20260723.md.
//
// These are intentionally closed, fixed test vectors. There is no path from
// command text or player data to the RPC ID, payload bytes, or payload length.
constexpr std::array<FixedCase, 5> kCases { {
	{
		"valid_48",
		{ 0x00, 0x00, 0x00, 0x00, 0xC8, 0x42, 0x00 },
		48,
		0,
		0x42C80000u, // 100.0f
	},
	{
		"out_of_range_1000_48",
		{ 0xE8, 0x03, 0x00, 0x00, 0xAA, 0x42, 0x00 },
		48,
		1000,
		0x42AA0000u, // 85.0f
	},
	{
		"inactive_999_48",
		{ 0xE7, 0x03, 0x00, 0x00, 0x8C, 0x42, 0x00 },
		48,
		999,
		0x428C0000u, // 70.0f
	},
	{
		"truncated_47",
		{ 0x00, 0x00, 0x00, 0x00, 0x5C, 0x42, 0x00 },
		47,
		0,
		0x425C0000u, // 55.0f before the final payload bit is removed
	},
	{
		"extra_one_bit_49",
		{ 0x00, 0x00, 0x00, 0x00, 0x20, 0x42, 0x80 },
		49,
		0,
		0x42200000u, // 40.0f, followed by one fixed set bit
	},
} };

static_assert(kCases[0].payloadBits == 48);
static_assert(kCases[3].payloadBits == 47);
static_assert(kCases[4].payloadBits == 49);
static_assert(kCases[1].actorID == 1000);
static_assert(kCases[2].actorID == 999);

class Rpc178Fixture final
	: public IComponent
	, public PlayerTextEventHandler
	, public PlayerConnectEventHandler
{
public:
	PROVIDE_UID(0x5250433137384544);

	~Rpc178Fixture() override
	{
		detach();
	}

	StringView componentName() const override
	{
		return "RPC178 fixed compatibility fixture";
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
			"[rpc178_fixture] loaded; fixed command=/rpc178raw rpc=178 "
			"dispatchEvents=0 channel=SyncRPC");
	}

	void onInit(IComponentList* components) override
	{
		actors_ = components == nullptr
			? nullptr
			: components->queryComponent<IActorsComponent>();

		if (core_ != nullptr)
		{
			core_->printLn(
				"[rpc178_fixture] actors_component=%s",
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

		std::size_t firedIndex = static_cast<std::size_t>(playerID);
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
				"[rpc178_fixture] trigger player=%d cases=%u rpc=178 "
				"dispatchEvents=0 channel=%d",
				playerID,
				static_cast<unsigned>(kCases.size()),
				static_cast<int>(OrderingChannel_SyncRPC));
		}

		unsigned sentCount = 0;
		for (const FixedCase& test : kCases)
		{
			// sendRPC consumes the span synchronously. The SDK documents the
			// Span size as BITS, not bytes, so 47 and 49 stay bit-exact.
			std::array<uint8_t, 7> payload = test.payload;
			const bool sent = player.sendRPC(
				kRpcSetActorHealth,
				Span<uint8_t>(payload.data(), test.payloadBits),
				OrderingChannel_SyncRPC,
				false);

			if (sent)
			{
				++sentCount;
			}

			if (core_ != nullptr)
			{
				core_->printLn(
					"[rpc178_fixture] case=%s actor=%u health_bits=%08x "
					"payload_bits=%u payload=%02x%02x%02x%02x%02x%02x%02x sent=%d",
					test.name,
					static_cast<unsigned>(test.actorID),
					static_cast<unsigned>(test.healthBits),
					static_cast<unsigned>(test.payloadBits),
					static_cast<unsigned>(test.payload[0]),
					static_cast<unsigned>(test.payload[1]),
					static_cast<unsigned>(test.payload[2]),
					static_cast<unsigned>(test.payload[3]),
					static_cast<unsigned>(test.payload[4]),
					static_cast<unsigned>(test.payload[5]),
					static_cast<unsigned>(test.payload[6]),
					sent ? 1 : 0);
			}
		}

		if (sentCount == kCases.size())
		{
			player.sendClientMessage(
				Colour::White(),
				"RPC178 fixture sent 5/5 fixed cases; reconnect to rerun.");
		}
		else
		{
			player.sendClientMessage(
				Colour::White(),
				"RPC178 fixture transport failure; inspect server log before reconnecting.");
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
				"[rpc178_fixture] reject player=%d reason=%s",
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
	return new (std::nothrow) Rpc178Fixture();
}
