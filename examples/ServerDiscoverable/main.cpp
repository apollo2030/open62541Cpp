#include <open62541server.h>
#include <open62541client.h>
#include <serverrepeatedcallback.h>

using namespace std;
#define DISCOVERY_SERVER_ENDPOINT "opc.tcp://localhost:4850"

// This is an example server that registers with the discovery server
// give port and server name as arguments
class TestServer : public Open62541::Server {
    int                               _idx; // namespace index
    UA_UInt64                         _discoveryId;
    Open62541::SeverRepeatedCallback  _repeatedEvent;
    Open62541::Client                 _client;

public:
    TestServer(int port)
        : Open62541::Server(port)
        , _repeatedEvent(*this, 2000, [&](Open62541::SeverRepeatedCallback & s) {
            Open62541::NodeId nodeNumber(_idx, "Number_Value");
            int v = std::rand() % 100;
            Open62541::Variant numberValue(v);
            cout << "_repeatedEvent called setting number value = " << v << endl;
            s.server().writeValue(nodeNumber, numberValue);
        }) {}

    /**
     * initialise the server before it runs but after it has been configured
     */
    void initialise(); 
};

//*****************************************************************************

void TestServer::initialise() {
    // create a name space
    _idx = addNamespace("urn:test:test");

    // Add a node and set its context to test context
    Open62541::NodeId newFolder(_idx, "ServerItems");
    if (!addFolder(Open62541::NodeId::Objects, "ServerItems", newFolder, Open62541::NodeId::Null))
        return;

    cout << "Create Number_Value" << endl;
    Open62541::NodeId nodeNumber(_idx, "Number_Value");
    Open62541::Variant numberValue(1);
    if (!addVariable(Open62541::NodeId::Objects, "Number_Value", numberValue, nodeNumber, Open62541::NodeId::Null)) {
        cout << "Failed to create Number Value Node " << endl;
    }

    // Start repeated event - so it does something
    _repeatedEvent.start();

    // connect to the discovery server
    if (!_client.connect(DISCOVERY_SERVER_ENDPOINT)) {
        cerr << "Failed to connect with discovery server" << endl;
        return;
    }

    cerr << "Register with discovery server" << endl;
    static std::string endpoint(DISCOVERY_SERVER_ENDPOINT);

    if (!registerDiscovery(_client)) {
        cerr << "Failed to register with discovery server" << endl;
        return;
    }

    cerr << "Registered with discovery server" << endl;
}

//*****************************************************************************

int main(int argc, char **argv) {
    if (argc != 3) {
        cerr << "Usage: ServerDiscoverable <port> <Server Name>" << endl;
        return 0;
    }

    int port = ::atoi(argv[1]);
    std::string name(argv[2]);

    cerr << "Port " << port << " Name " << name << endl;
    TestServer server(port);
    server.setMdnsServerName(name);
    server.setServerUri("Test Discoverable Server");
    cerr << "Starting server" << endl;
    server.start();
    cerr << "Server Finished" << endl;

    return 0;
}
