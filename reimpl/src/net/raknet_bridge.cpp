#include "sampdll/net/raknet_bridge.h"

#include "raknet/RakClientInterface.h"
#include "raknet/RakNetworkFactory.h"
#include "raknet/RakPeerInterface.h"

int samp_raknet_bridge_is_available(void) { return 1; }

int samp_raknet_bridge_self_test(void) {
  RakNet::RakPeerInterface *peer = RakNet::RakNetworkFactory::GetRakPeerInterface();
  RakNet::RakClientInterface *client = RakNet::RakNetworkFactory::GetRakClientInterface();

  if (peer == nullptr || client == nullptr) {
    if (peer != nullptr) {
      RakNet::RakNetworkFactory::DestroyRakPeerInterface(peer);
    }
    if (client != nullptr) {
      RakNet::RakNetworkFactory::DestroyRakClientInterface(client);
    }
    return -1;
  }

  RakNet::RakNetworkFactory::DestroyRakPeerInterface(peer);
  RakNet::RakNetworkFactory::DestroyRakClientInterface(client);
  return 0;
}

const char *samp_raknet_bridge_variant_name(void) { return "Knogle/RakNet (RakNet 2.52 SA:MP variant)"; }
