/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/

#ifndef OPEN62541SERVER_H
#define OPEN62541SERVER_H

#include "open62541objects.h"
#include "nodecontext.h"
#include "servermethod.h"
#include "serverrepeatedcallback.h"

namespace Open62541 {

class HistoryDataGathering;
class HistoryDataBackend;

/**
 * The Server class abstracts the server side.
 * This class wraps the corresponding C functions. Refer to the C documentation for a full explanation.
 * The main thing to watch for is Node ID objects are passed by reference. There are stock Node Id objects including NodeId::Null
 * Pass NodeId::Null where a NULL UA_NodeId pointer is expected.
 * If a NodeId is being passed to receive a value use the notNull() method to mark it as a receiver of a new node id.
 * Most functions return true if the lastError is UA_STATUSCODE_GOOD.
 */
class  UA_EXPORT  Server {
    UA_Server*        _server   = nullptr; // assume one server per application
    UA_ServerConfig*  _config   = nullptr;
    UA_Boolean        _running  = false;
    std::map<std::string, SeverRepeatedCallbackRef> _callbacks;
    ReadWriteMutex    _mutex;

    // Life cycle call backs

    /**
     * Server::constructor
     * Can be NULL. May replace the nodeContext
     * @param server
     * @param sessionId
     * @param sessionContext
     * @param nodeId
     * @param nodeContext
     * @return 
     */
    static UA_StatusCode constructor(
        UA_Server* server,
        const UA_NodeId* sessionId,
        void* sessionContext,
        const UA_NodeId* nodeId,
        void** nodeContext);

    /**
     * Server::destructor
     * Can be NULL. The context cannot be replaced since
     * the node is destroyed immediately afterwards anyway.
     * @param server
     * @param nodeId
     * @param nodeContext
     */
    static void destructor(
        UA_Server* server,
        const UA_NodeId* sessionId,
        void* sessionContext,
        const UA_NodeId* nodeId,
        void* nodeContext);

    typedef std::map<UA_Server*, Server*> ServerMap;    /**< Map of servers key by UA_Server pointer */
    static ServerMap _serverMap;                        /**< map UA_SERVER to Server objects */
    std::map<UA_UInt64, std::string> _discoveryList;    /**< set of discovery servers this server has registered with */
    std::vector<UA_UsernamePasswordLogin> _logins;      /**< set of permitted  logins */

    // Access Control Callbacks - these invoke virtual functions to control access

    static UA_Boolean   allowAddNodeHandler(UA_Server* server, UA_AccessControl* ac,
                                            const UA_NodeId* sessionId, void* sessionContext, const UA_AddNodesItem* item);

    static UA_Boolean
    allowAddReferenceHandler(UA_Server* server, UA_AccessControl* ac,
                              const UA_NodeId* sessionId, void* sessionContext,
                              const UA_AddReferencesItem* item);

    static UA_Boolean
    allowDeleteNodeHandler(UA_Server* server, UA_AccessControl* ac,
                            const UA_NodeId* sessionId, void* sessionContext, const UA_DeleteNodesItem* item);
    static UA_Boolean
    allowDeleteReferenceHandler(UA_Server* server, UA_AccessControl* ac,
                                const UA_NodeId* sessionId, void* sessionContext, const UA_DeleteReferencesItem* item);

    static UA_StatusCode activateSessionHandler(UA_Server* server, UA_AccessControl* ac,
                                                const UA_EndpointDescription* endpointDescription,
                                                const UA_ByteString* secureChannelRemoteCertificate,
                                                const UA_NodeId* sessionId,
                                                const UA_ExtensionObject* userIdentityToken,
                                                void** sessionContext);

    /**
     * De-authenticate a session and cleanup
     */
    static void closeSessionHandler(UA_Server* server, UA_AccessControl* ac,
                                    const UA_NodeId* sessionId, void* sessionContext);

    /**
     * Access control for all nodes
     */
    static UA_UInt32 getUserRightsMaskHandler(UA_Server* server, UA_AccessControl* ac,
                                              const UA_NodeId* sessionId, void* sessionContext,
                                              const UA_NodeId* nodeId, void* nodeContext);

    /**
     * Additional access control for variable nodes
     */
    static UA_Byte getUserAccessLevelHandler(UA_Server* server, UA_AccessControl* ac,
                                              const UA_NodeId* sessionId, void* sessionContext,
                                              const UA_NodeId* nodeId, void* nodeContext);

    /**
     * Additional access control for method nodes
     */
    static UA_Boolean getUserExecutableHandler(UA_Server* server, UA_AccessControl* ac,
                                                const UA_NodeId* sessionId, void* sessionContext,
                                                const UA_NodeId* methodId, void* methodContext);

    /**
     * Additional access control for calling a method node in the context of a specific object
     */
    static UA_Boolean getUserExecutableOnObjectHandler(UA_Server* server, UA_AccessControl* ac,
                                                        const UA_NodeId* sessionId, void* sessionContext,
                                                        const UA_NodeId* methodId, void* methodContext,
                                                        const UA_NodeId* objectId, void* objectContext);
    /**
     * Allow insert,replace,update of historical data
     */
    static UA_Boolean allowHistoryUpdateUpdateDataHandler(UA_Server* server, UA_AccessControl* ac,
                                                          const UA_NodeId* sessionId, void* sessionContext,
                                                          const UA_NodeId* nodeId,
                                                          UA_PerformUpdateType performInsertReplace,
                                                          const UA_DataValue* value);

    /**
     * Allow delete of historical data
     */
    static UA_Boolean allowHistoryUpdateDeleteRawModifiedHandler(UA_Server* server, UA_AccessControl* ac,
                                                                  const UA_NodeId* sessionId, void* sessionContext,
                                                                  const UA_NodeId* nodeId,
                                                                  UA_DateTime startTimestamp,
                                                                  UA_DateTime endTimestamp,
                                                                  bool isDeleteModified);

protected:
    UA_StatusCode _lastError = 0;

public:
    Server() {
        _server = UA_Server_new();
        if (_server) {
            _config = UA_Server_getConfig(_server);
            if (_config) {
                UA_ServerConfig_setDefault(_config);
                _config->nodeLifecycle.constructor = constructor; // set up the node global lifecycle
                _config->nodeLifecycle.destructor = destructor;
            }
        }
    }

    Server(int port, const UA_ByteString& certificate = UA_BYTESTRING_NULL) {
        _server = UA_Server_new();
        if (_server) {
            _config = UA_Server_getConfig(_server);
            if (_config) {
                UA_ServerConfig_setMinimal(_config, port, &certificate);
                _config->nodeLifecycle.constructor = constructor; // set up the node global lifecycle
                _config->nodeLifecycle.destructor = destructor;
            }
        }
    }

    virtual ~Server() {
        // possible abnormal exit
        if (_server) {
            WriteLock l(_mutex);
            terminate();
        }
    }

    void setMdnsServerName(const std::string& name) {
        if (_config) {
            _config->discovery.mdnsEnable = true;

#ifdef UA_ENABLE_DISCOVERY_MULTICAST
            _config->discovery.mdns.mdnsServerName = UA_String_fromChars(name.c_str());
#else
            (void)name;
#endif
        }
    }

    /**
     * logins
     * @todo add clear, add, delete update
     * @return a Array of user name / passwords
     */
    std::vector<UA_UsernamePasswordLogin>& logins() {
        return  _logins;
    }

    void applyEndpoints(EndpointDescriptionArray& endpoints) {
        _config->endpoints = endpoints.data();
        _config->endpointsSize = endpoints.length();
        // Transfer ownership
        endpoints.release();
    }

    void configClean() {
        if (_config) UA_ServerConfig_clean(_config);
    }

    /**
     * Set up for simple login
     * assumes the permitted logins have been set up beforehand.
     * This gives username / password access and disables anonymous access
     * @return true on success 
     */
    bool enableSimpleLogin();

    /**
     * Set a custom host name in server configuration
     */
    void setCustomHostname(const std::string& customHostname) {
        UA_String s =   toUA_String(customHostname); // shallow copy
        UA_ServerConfig_setCustomHostname(_config, s);
    }

    void setServerUri(const std::string& s) {
        UA_String_deleteMembers(&_config->applicationDescription.applicationUri);
        _config->applicationDescription.applicationUri = UA_String_fromChars(s.c_str());
    }

    static Server* findServer(UA_Server* s) {
        return _serverMap[s];
    }

    // Discovery

    /**
     * registerDiscovery
     * @param discoveryServerUrl
     * @param semaphoreFilePath
     * @return true on success
     */
    bool registerDiscovery(Client& client,  const std::string& semaphoreFilePath = "");

    /**
     * unregisterDiscovery
     * @return  true on success
     */
    bool unregisterDiscovery(Client& client);

    /**
     * addPeriodicServerRegister
     * @param discoveryServerUrl
     * @param intervalMs
     * @param delayFirstRegisterMs
     * @param periodicCallbackId
     * @return true on success
     */
    bool  addPeriodicServerRegister(const std::string& discoveryServerUrl, // url must persist - that is be static
                                    Client& client,
                                    UA_UInt64& periodicCallbackId,
                                    UA_UInt32 intervalMs = 600 * 1000, // default to 10 minutes
                                    UA_UInt32 delayFirstRegisterMs = 1000);
    
    /**
     * registerServer
     */
    virtual void registerServer(const UA_RegisteredServer* /*registeredServer*/) {
        OPEN62541_TRC
    }

    /**
     * registerServerCallback
     * @param registeredServer
     * @param data
     */
    static void registerServerCallback(const UA_RegisteredServer* registeredServer, void* data);

    /**
     * setRegisterServerCallback
     */
    void setRegisterServerCallback() {
        UA_Server_setRegisterServerCallback(server(), registerServerCallback, (void*)(this));
    }

    /**
     * serverOnNetwork
     * @param serverOnNetwork
     * @param isServerAnnounce
     * @param isTxtReceived
     */
    virtual void serverOnNetwork(const UA_ServerOnNetwork* /*serverOnNetwork*/,
                                  UA_Boolean /*isServerAnnounce*/,
                                  UA_Boolean /*isTxtReceived*/) {
        OPEN62541_TRC

    }

    /**
     * serverOnNetworkCallback
     * @param serverNetwork
     * @param isServerAnnounce
     * @param isTxtReceived
     * @param data
     */
    static void serverOnNetworkCallback(const UA_ServerOnNetwork* serverNetwork,
                                        UA_Boolean isServerAnnounce,
                                        UA_Boolean isTxtReceived,
                                        void* data);
    #ifdef UA_ENABLE_DISCOVERY_MULTICAST
    /**
     * setServerOnNetworkCallback
     */
    void setServerOnNetworkCallback() {
        UA_Server_setServerOnNetworkCallback(server(), serverOnNetworkCallback, (void*)(this));
    }
    #endif

    /**
     * start the server
     * @param iterate
     */
    virtual void start();

    /**
     * stop the server (prior to delete) - do not try start-stop-start
     */
    virtual void stop();

    /**
     * initialise
     * called after the server object has been created but before run has been called
     * load configuration files and set up the address space
     * create namespaces and endpoints
     * set up methods and stuff
     */
    virtual void initialise();

    /**
     * process
     * called between server loop iterations - hook thread event processing
     */
    virtual void process() {}

    /**
     * terminate
     * called before server is closed
     */
    virtual void terminate();

    /**
     * lastError
     * @return 
     */
    UA_StatusCode lastError() const {
        return _lastError;
    }

    /**
     * server
     * @return pointer to underlying server structure
     */
    UA_Server* server() const {
        return _server;
    }

    /**
     * running
     * @return running state
     */
    UA_Boolean  running() const {
        return _running;
    }

    /**
     * getNodeContext
     * @param n node if
     * @param c pointer to context
     * @return true on success
     */
    bool getNodeContext(NodeId& n, NodeContext*& c) {
        if (!server()) return false;
        void* p = (void*)(c);
        _lastError = UA_Server_getNodeContext(_server, n.get(), &p);
        return lastOK();
    }

    /**
     * findContext
     * @param s name of context
     * @return named context
     */
    static NodeContext* findContext(const std::string& s);

    /**
     * setNodeContext
     * @warning The user has to ensure that the destructor callbacks still work.
     * @param n node id
     * @param c context
     * @return true on success
     */
    bool setNodeContext(NodeId& n, const NodeContext* c) {
        if (!server()) return false;
        _lastError = UA_Server_setNodeContext(_server, n.get(), (void*)(c));
        return lastOK();
    }

    /**
     * readAttribute
     * @param nodeId
     * @param attributeId
     * @param v data pointer
     * @return true on success
     */
    bool readAttribute(const UA_NodeId* nodeId, UA_AttributeId attributeId, void* v) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  __UA_Server_read(_server, nodeId, attributeId, v);
        return lastOK();
    }

    /**
     * writeAttribute
     * @param nodeId
     * @param attributeId
     * @param attr_type
     * @param attr data pointer
     * @return true on success
     */
    bool writeAttribute(const UA_NodeId* nodeId, const UA_AttributeId attributeId, const UA_DataType* attr_type, const void* attr) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  __UA_Server_write(_server, nodeId, attributeId, attr_type, attr) == UA_STATUSCODE_GOOD;
        return lastOK();
    }

    /**
     * mutex
     * @return server mutex
     */
    ReadWriteMutex& mutex() {
        return _mutex; // access mutex - most accesses need a write lock
    }

    /**
     * deleteTree
     * @param nodeId node to be deleted with its children
     * @return true on success
     */
    bool deleteTree(NodeId& nodeId);

    /**
     * browseTree
     * add child nodes to property tree node
     * @param nodeId  start point
     * @param node point in tree to add nodes to
     * @return true on success
     */
    bool browseTree(UA_NodeId& nodeId, UANode* node); 

    /**
     * browseTree
     * produces an addressable tree using dot separated browse path
     * @param nodeId start point to browse from
     * @return true on success
     */
    bool browseTree(NodeId& nodeId, UANodeTree& tree); 

    /**
     * browseTree
     * @param nodeId start node to browse from
     * @param tree tree to fill
     * @return true on success
     */
    bool browseTree(NodeId& nodeId, UANode* tree);

    /**
     * browseTree
     * browse and create a map of string version of NodeId ids to node ids
     * @param nodeId
     * @param tree
     * @return true on success
     */
    bool browseTree(NodeId& nodeId, NodeIdMap& m);

    /**
     * browseChildren
     * @param nodeId parent of children to browse
     * @param m map to fill
     * @return true on success
     */
    bool browseChildren(UA_NodeId& nodeId, NodeIdMap& m);

    /**
     * A simplified TranslateBrowsePathsToNodeIds based on the
     * SimpleAttributeOperand type (Part 4, 7.4.4.5).
     * This specifies a relative path using a list of BrowseNames instead of the
     * RelativePath structure. The list of BrowseNames is equivalent to a
     * RelativePath that specifies forward references which are subtypes of the
     * HierarchicalReferences ReferenceType. All Nodes followed by the browsePath
     * shall be of the NodeClass Object or Variable.
     */
    bool browseSimplifiedBrowsePath(NodeId origin,
                                    size_t browsePathSize,
                                    QualifiedName& browsePath,
                                    BrowsePathResult& result) {
        result.get() = UA_Server_browseSimplifiedBrowsePath(_server,
                                                            origin,
                                                            browsePathSize,
                                                            browsePath.constRef());
        _lastError = result.ref()->statusCode;
        return lastOK();

    }
    /**
     * create a browse path and add it to the tree
     * @param parent node to start with
     * @param p path to create
     * @param tree
     * @return true on success
     */
    bool createBrowsePath(NodeId& parent, UAPath& p, UANodeTree& tree);

    /**
     * addNamespace
     * @param s name of name space
     * @return name space index
     */
    UA_UInt16 addNamespace(const std::string s) {
        if (!server()) return 0;
        UA_UInt16 ret = 0;
        {
            WriteLock l(mutex());
            ret =   UA_Server_addNamespace(_server, s.c_str());
        }
        return ret;
    }

    /**
     * serverConfig
     * @return  server configuration
     */
    UA_ServerConfig& serverConfig() {
        return* UA_Server_getConfig(server());
    }

    /**
     * addServerMethod
     * @param parent
     * @param nodeId
     * @param newNode
     * @param nameSpaceIndex
     * @return true on success
     */
    bool addServerMethod(ServerMethod* method, const std::string& browseName,
                          NodeId& parent,  NodeId& nodeId,
                          NodeId& newNode,  int nameSpaceIndex = 0) {
        if (!server()) return false;

        if (nameSpaceIndex == 0) nameSpaceIndex = parent.nameSpaceIndex(); // inherit parent by default
        
        MethodAttributes attr;
        attr.setDefault();
        attr.setDisplayName(browseName);
        attr.setDescription(browseName);
        attr.setExecutable();

        QualifiedName qn(nameSpaceIndex, browseName);
        {
            WriteLock l(mutex());
            _lastError = UA_Server_addMethodNode(_server,
                                                  nodeId,
                                                  parent,
                                                  NodeId::HasOrderedComponent,
                                                  qn,
                                                  attr,
                                                  ServerMethod::methodCallback,
                                                  method->in().size() - 1,
                                                  method->in().data(),
                                                  method->out().size() - 1,
                                                  method->out().data(),
                                                  (void*)(method), // method context is reference to the call handler
                                                  newNode.isNull() ? nullptr : newNode.ref());

        }
        return lastOK();
    }

    /**
     * addRepeatedCallback
     * @param id
     * @param p
     */
    void addRepeatedCallback(const std::string& id, SeverRepeatedCallback* p) {
        _callbacks[id] = SeverRepeatedCallbackRef(p);
    }

    /**
     * addRepeatedCallback
     * @param id
     * @param interval
     * @param f
     */
    void addRepeatedCallback(const std::string& id, int interval, SeverRepeatedCallbackFunc f) {
        auto p = new SeverRepeatedCallback(*this, interval, f);
        _callbacks[id] = SeverRepeatedCallbackRef(p);
    }

    /**
     * removeRepeatedCallback
     * @param id
     */
    void removeRepeatedCallback(const std::string& id) {
        _callbacks.erase(id);
    }

    /**
     * repeatedCallback
     * @param s
     * @return 
     */
    SeverRepeatedCallbackRef& repeatedCallback(const std::string& s) {
        return _callbacks[s];
    }

    /**
     * browseName
     * @param nodeId
     * @return 
     */
    bool  browseName(NodeId& nodeId, std::string& s, int& ns) {
        if (!_server) throw std::runtime_error("Null server");
        QualifiedName outBrowseName;
        if (UA_Server_readBrowseName(_server, nodeId, outBrowseName) == UA_STATUSCODE_GOOD) {
            s =   toString(outBrowseName.get().name);
            ns = outBrowseName.get().namespaceIndex;
        }
        return lastOK();
    }


    /**
     * setBrowseName
     * @param nodeId
     * @param nameSpaceIndex
     * @param name
     */
    void setBrowseName(NodeId& nodeId, int nameSpaceIndex, const std::string& name) {
        if (!server()) return;
        QualifiedName newBrowseName(nameSpaceIndex, name);
        WriteLock l(_mutex);
        UA_Server_writeBrowseName(_server, nodeId, newBrowseName);
    }

    /**
     * NodeIdFromPath get the node id from the path of browse names in the given namespace. Tests for node existance
     * @param path
     * @param nodeId
     * @return true on success
     */
    bool nodeIdFromPath(NodeId& start, Path& path,  NodeId& nodeId);

    /**
     * Create a path
     * Create folder path first then add variables to path's end leaf
     * @param start
     * @param path
     * @param nameSpaceIndex
     * @param nodeId is a shallow copy - do not delete and is volatile
     * @return true on success
     */
    bool createFolderPath(NodeId& start, Path& path, int nameSpaceIndex, NodeId& nodeId);

    /**
     * getChild
     * @param nameSpaceIndex
     * @param childName
     * @return true on success
     */
    bool  getChild(NodeId& start, const std::string& childName, NodeId& ret);

    /**
     * addFolder
     * @param parent parent node
     * @param childName browse name of child node
     * @param nodeId  assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param nameSpaceIndex name space index of new node, if non-zero otherwise namespace of parent
     * @return true on success
     */
    bool addFolder(NodeId& parent,  const std::string& childName,
                    NodeId& nodeId, NodeId& newNode = NodeId::Null, int nameSpaceIndex = 0);

    /**
     * addVariable
     * @param parent
     * @param nameSpaceIndex
     * @param childName
     * @return true on success
     */
    bool addVariable(NodeId& parent, const std::string& childName,
                      Variant& value, NodeId& nodeId,  NodeId& newNode = NodeId::Null,
                      NodeContext* c = nullptr,
                      int nameSpaceIndex = 0);

    /**
     * Add a variable of the given type
     * @param parent
     * @param childName
     * @param nodeId
     * @param c
     * @param newNode
     * @param nameSpaceIndex
     * @return true on success
     */
    template<typename T>
    bool addVariable(NodeId& parent,  const std::string& childName,
                      NodeId& nodeId, const std::string& c,
                      NodeId& newNode = NodeId::Null,
                      int nameSpaceIndex = 0) {
        NodeContext* cp = findContext(c);
        if (cp) {
            Variant v(T());
            return  addVariable(parent, childName, v, nodeId,  newNode, cp, nameSpaceIndex);
        }
        return false;
    }

    /**
     * addVariable
     * @param parent
     * @param nameSpaceIndex
     * @param childName
     * @return true on success
     */
    bool addHistoricalVariable(NodeId& parent, const std::string& childName,
                                Variant& value, NodeId& nodeId,  NodeId& newNode = NodeId::Null,
                                NodeContext* c = nullptr,
                                int nameSpaceIndex = 0);

    /**
     * Add a variable of the given type
     * @param parent
     * @param childName
     * @param nodeId
     * @param c
     * @param newNode
     * @param nameSpaceIndex
     * @return true on success
     */
    template<typename T>
    bool addHistoricalVariable(NodeId& parent,  const std::string& childName,
                                NodeId& nodeId, const std::string& c,
                                NodeId& newNode = NodeId::Null,
                                int nameSpaceIndex = 0) {
        NodeContext* cp = findContext(c);
        if (cp) {
            Variant v(T());
            return  addHistoricalVariable(parent, childName, v, nodeId,  newNode, cp, nameSpaceIndex);
        }
        return false;
    }

    /**
     * Add a property of the given type
     * @param parent
     * @param key
     * @param value
     * @param nodeId
     * @param newNode
     * @param c
     * @param nameSpaceIndex
     * @return true on success
     */
    template <typename T>
    bool addProperty(NodeId& parent,
                      const std::string& key,
                      const T& value,
                      NodeId& nodeId  = NodeId::Null,
                      NodeId& newNode = NodeId::Null,
                      NodeContext* c = nullptr,
                      int nameSpaceIndex = 0) {
        Variant v(value);
        return addProperty(parent, key, v, nodeId, newNode, c, nameSpaceIndex);
    }

    /**
     * addProperty
     * @param parent
     * @param key
     * @param value
     * @param nodeId
     * @param newNode
     * @param c
     * @param nameSpaceIndex
     * @return true on success
     */
    bool addProperty(NodeId& parent,
                      const std::string& key,
                      Variant& value,
                      NodeId& nodeId  = NodeId::Null,
                      NodeId& newNode = NodeId::Null,
                      NodeContext* c = nullptr,
                      int nameSpaceIndex = 0);

    /**
     * variable
     * @param nodeId
     * @param value
     * @return true on success
     */
    bool  variable(NodeId& nodeId,  Variant& value) {
        if (!server()) return false;

        // outValue is managed by caller - transfer to output value
        value.null();
        WriteLock l(_mutex);
        UA_Server_readValue(_server, nodeId, value.ref());
        return lastOK();
    }

    /**
     * deleteNode
     * @param nodeId
     * @param deleteReferences
     * @return true on success
     */
    bool deleteNode(NodeId& nodeId, bool  deleteReferences) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError =  UA_Server_deleteNode(_server, nodeId, UA_Boolean(deleteReferences));
        return _lastError != UA_STATUSCODE_GOOD;
    }

    /**
     * call
     * @param request
     * @param ret
     * @return true on success
     */
    bool call(CallMethodRequest& request, CallMethodResult& ret) {
        if (!server()) return false;

        WriteLock l(_mutex);
        ret.get() = UA_Server_call(_server, request);
        return ret.get().statusCode == UA_STATUSCODE_GOOD;
    }

    /**
     * translateBrowsePathToNodeIds
     * @param path
     * @param result
     * @return true on success
     */
    bool translateBrowsePathToNodeIds(BrowsePath& path, BrowsePathResult& result) {
        if (!server()) return false;

        WriteLock l(_mutex);
        result.get() = UA_Server_translateBrowsePathToNodeIds(_server, path);
        return result.get().statusCode  == UA_STATUSCODE_GOOD;
    }

    /**
     * lastOK
     * @return last error code
     */
    bool lastOK() const {
        return _lastError == UA_STATUSCODE_GOOD;
    }
    //
    // Attributes
    //
    /**
     * readNodeId
     * @param nodeId
     * @param outNodeId
     * @return true on success
     */
    bool readNodeId(NodeId& nodeId, NodeId& outNodeId) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_NODEID, outNodeId);
    }

    /**
     * readNodeClass
     * @param nodeId
     * @param outNodeClass
     * @return true on success
     */
    bool readNodeClass(NodeId& nodeId, UA_NodeClass& outNodeClass) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_NODECLASS,
                              &outNodeClass);
    }

    /**
     * readBrowseName
     * @param nodeId
     * @param outBrowseName
     * @return true on success
     */
    bool readBrowseName(NodeId& nodeId, QualifiedName& outBrowseName) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_BROWSENAME,
                              outBrowseName);
    }

    /**
     * readDisplayName
     * @param nodeId
     * @param outDisplayName
     * @return true on success
     */
    bool readDisplayName(NodeId& nodeId, LocalizedText& outDisplayName) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_DISPLAYNAME,
                              outDisplayName);
    }

    /**
     * readDescription
     * @param nodeId
     * @param outDescription
     * @return true on success
     */
    bool readDescription(NodeId& nodeId, LocalizedText& outDescription) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_DESCRIPTION,
                              outDescription);
    }

    /**
     * readWriteMask
     * @param nodeId
     * @param outWriteMask
     * @return true on success
     */
    bool readWriteMask(NodeId& nodeId, UA_UInt32& outWriteMask) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_WRITEMASK,
                              &outWriteMask);
    }

    /**
     * readIsAbstract
     * @param nodeId
     * @param outIsAbstract
     * @return true on success
     */
    bool readIsAbstract(NodeId& nodeId, UA_Boolean& outIsAbstract) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_ISABSTRACT,
                              &outIsAbstract);
    }

    /**
     * readSymmetric
     * @param nodeId
     * @param outSymmetric
     * @return true on success
     */
    bool readSymmetric(NodeId& nodeId, UA_Boolean& outSymmetric) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_SYMMETRIC,
                              &outSymmetric);
    }

    /**
     * readInverseName
     * @param nodeId
     * @param outInverseName
     * @return true on success
     */
    bool readInverseName(NodeId& nodeId, LocalizedText& outInverseName) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_INVERSENAME,
                              outInverseName);
    }

    /**
     * readContainsNoLoop
     * @param nodeId
     * @param outContainsNoLoops
     * @return true on success
     */
    bool readContainsNoLoop(NodeId& nodeId, UA_Boolean& outContainsNoLoops) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_CONTAINSNOLOOPS,
                              &outContainsNoLoops);
    }

    /**
     * readEventNotifier
     * @param nodeId
     * @param outEventNotifier
     * @return 
     */
    bool readEventNotifier(NodeId& nodeId, UA_Byte& outEventNotifier) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_EVENTNOTIFIER,
                              &outEventNotifier);
    }

    /**
     * readValue
     * @param nodeId
     * @param outValue
     * @return 
     */
    bool readValue(NodeId& nodeId, Variant& outValue) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_VALUE, outValue);
    }

    /**
     * readDataType
     * @param nodeId
     * @param outDataType
     * @return 
     */
    bool readDataType(NodeId& nodeId, NodeId& outDataType) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_DATATYPE,
                              outDataType);
    }

    /**
     * readValueRank
     * @param nodeId
     * @param outValueRank
     * @return 
     */
    bool readValueRank(NodeId& nodeId, UA_Int32& outValueRank) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_VALUERANK,
                              &outValueRank);
    }

    /* Returns a variant with an int32 array */
    /**
     * readArrayDimensions
     * @param nodeId
     * @param outArrayDimensions
     * @return 
     */
    bool readArrayDimensions(NodeId& nodeId, Variant& outArrayDimensions) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_ARRAYDIMENSIONS,
                              outArrayDimensions);
    }

    /**
     * readAccessLevel
     * @param nodeId
     * @param outAccessLevel
     * @return 
     */
    bool readAccessLevel(NodeId& nodeId, UA_Byte& outAccessLevel) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_ACCESSLEVEL,
                              &outAccessLevel);
    }

    /**
     * readMinimumSamplingInterval
     * @param nodeId
     * @param outMinimumSamplingInterval
     * @return 
     */
    bool readMinimumSamplingInterval(NodeId& nodeId, UA_Double& outMinimumSamplingInterval) {
        return  readAttribute(nodeId,
                              UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL,
                              &outMinimumSamplingInterval);
    }

    /**
     * readHistorizing
     * @param nodeId
     * @param outHistorizing
     * @return 
     */
    bool readHistorizing(NodeId& nodeId, UA_Boolean& outHistorizing) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_HISTORIZING,
                              &outHistorizing);
    }

    /**
     * readExecutable
     * @param nodeId
     * @param outExecutable
     * @return 
     */
    bool readExecutable(NodeId& nodeId, UA_Boolean& outExecutable) {
        return  readAttribute(nodeId, UA_ATTRIBUTEID_EXECUTABLE,
                              &outExecutable);
    }

    /**
     * writeBrowseName
     * @param nodeId
     * @param browseName
     * @return 
     */
    bool writeBrowseName(NodeId& nodeId, QualifiedName& browseName) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_BROWSENAME,
                                &UA_TYPES[UA_TYPES_QUALIFIEDNAME], browseName);
    }

    /**
     * writeDisplayName
     * @param nodeId
     * @param displayName
     * @return 
     */
    bool writeDisplayName(NodeId& nodeId, LocalizedText& displayName) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_DISPLAYNAME,
                                &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], displayName);
    }

    /**
     * writeDescription
     * @param nodeId
     * @param description
     * @return 
     */
    bool writeDescription(NodeId& nodeId, LocalizedText& description) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_DESCRIPTION,
                                &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], description);
    }

    /**
     * writeWriteMask
     * @param nodeId
     * @param writeMask
     * @return 
     */
    bool writeWriteMask(NodeId& nodeId, const UA_UInt32 writeMask) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_WRITEMASK,
                                &UA_TYPES[UA_TYPES_UINT32], &writeMask);
    }

    /**
     * writeIsAbstract
     * @param nodeId
     * @param isAbstract
     * @return 
     */
    bool writeIsAbstract(NodeId& nodeId, const UA_Boolean isAbstract) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_ISABSTRACT,
                                &UA_TYPES[UA_TYPES_BOOLEAN], &isAbstract);
    }

    /**
     * writeInverseName
     * @param nodeId
     * @param inverseName
     * @return 
     */
    bool writeInverseName(NodeId& nodeId,
                      const UA_LocalizedText inverseName) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_INVERSENAME,
                                &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], &inverseName);
    }

    /**
     * writeEventNotifier
     * @param nodeId
     * @param eventNotifier
     * @return 
     */
    bool writeEventNotifier(NodeId& nodeId, const UA_Byte eventNotifier) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_EVENTNOTIFIER,
                                &UA_TYPES[UA_TYPES_BYTE], &eventNotifier);
    }

    /**
     * writeValue
     * @param nodeId
     * @param value
     * @return 
     */
    bool writeValue(NodeId& nodeId, Variant& value) {
        if (!server()) return false;

        return  UA_STATUSCODE_GOOD == (_lastError = __UA_Server_write(_server, nodeId, UA_ATTRIBUTEID_VALUE,
                                                                      &UA_TYPES[UA_TYPES_VARIANT], value));
    }

    /**
     * writeDataType
     * @param nodeId
     * @param dataType
     * @return 
     */
    bool writeDataType(NodeId& nodeId, NodeId& dataType) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_DATATYPE,
                                &UA_TYPES[UA_TYPES_NODEID], dataType);
    }

    /**
     * writeValueRank
     * @param nodeId
     * @param valueRank
     * @return 
     */
    bool writeValueRank(NodeId& nodeId, const UA_Int32 valueRank) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_VALUERANK,
                                &UA_TYPES[UA_TYPES_INT32], &valueRank);
    }

    /**
     * writeArrayDimensions
     * @param nodeId
     * @param arrayDimensions
     * @return 
     */
    bool writeArrayDimensions(NodeId& nodeId, Variant arrayDimensions) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_VALUE,
                                &UA_TYPES[UA_TYPES_VARIANT], arrayDimensions.constRef());
    }

    /**
     * writeAccessLevel
     * @param nodeId
     * @param accessLevel
     * @return 
     */
    bool writeAccessLevel(NodeId& nodeId, const UA_Byte accessLevel) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_ACCESSLEVEL,
                                &UA_TYPES[UA_TYPES_BYTE], &accessLevel);
    }

    // Some short cuts

    /**
     * writeEnable
     * @param nodeId
     * @return 
     */
    bool writeEnable(NodeId& nodeId) {
        UA_Byte accessLevel;
        if (readAccessLevel(nodeId, accessLevel)) {
            accessLevel |= UA_ACCESSLEVELMASK_WRITE;
            return writeAccessLevel(nodeId, accessLevel);
        }
        return false;
    }

    /**
     * setReadOnly
     * @param nodeId
     * @param historyEnable
     * @return 
     */
    bool setReadOnly(NodeId& nodeId, bool historyEnable = false) {
        UA_Byte accessLevel;
        if (readAccessLevel(nodeId, accessLevel)) {
            // remove the write bits
            accessLevel &= ~(UA_ACCESSLEVELMASK_WRITE | UA_ACCESSLEVELMASK_HISTORYWRITE);
            // add the read bits
            accessLevel |= UA_ACCESSLEVELMASK_READ;
            if (historyEnable) accessLevel |= UA_ACCESSLEVELMASK_HISTORYREAD;
            return writeAccessLevel(nodeId, accessLevel);
        }
        return false;
    }

    /**
     * writeMinimumSamplingInterval
     * @param nodeId
     * @param miniumSamplingInterval
     * @return 
     */
    bool writeMinimumSamplingInterval(NodeId& nodeId, const UA_Double miniumSamplingInterval) {
        return  writeAttribute(nodeId,
                                UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL,
                                &UA_TYPES[UA_TYPES_DOUBLE],
                                &miniumSamplingInterval);
    }

    /**
     * writeExecutable
     * @param nodeId
     * @param executable
     * @return 
     */
    bool writeExecutable(NodeId& nodeId, const UA_Boolean executable) {
        return  writeAttribute(nodeId, UA_ATTRIBUTEID_EXECUTABLE,
                                &UA_TYPES[UA_TYPES_BOOLEAN], &executable);
    }

    // Add Nodes - taken from docs

    /**
     * addVariableNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param typeDefinition
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addVariableNode(NodeId& requestedNewNodeId,
                    NodeId& parentNodeId,
                    NodeId& referenceTypeId,
                    QualifiedName& browseName,
                    NodeId& typeDefinition,
                    VariableAttributes& attr,
                    NodeId& outNewNodeId = NodeId::Null,
                    NodeContext* nc = nullptr) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError = UA_Server_addVariableNode(_server,
                                                requestedNewNodeId,
                                                parentNodeId,
                                                referenceTypeId,
                                                browseName,
                                                typeDefinition,
                                                attr,
                                                nc,
                                                outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());

        return lastOK();
    }

    /**
     * addVariableTypeNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param typeDefinition
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addVariableTypeNode(
        NodeId& requestedNewNodeId,
        NodeId& parentNodeId,
        NodeId& referenceTypeId,
        QualifiedName& browseName,
        NodeId& typeDefinition,
        VariableTypeAttributes& attr,
        NodeId& outNewNodeId = NodeId::Null,
        NodeContext* instantiationCallback = nullptr) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  UA_Server_addVariableTypeNode(_server,
                                                    requestedNewNodeId,
                                                    parentNodeId,
                                                    referenceTypeId,
                                                    browseName,
                                                    typeDefinition,
                                                    attr,
                                                    instantiationCallback,
                                                    outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());
        return lastOK();
    }

    /**
     * addObjectNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param typeDefinition
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addObjectNode(NodeId& requestedNewNodeId,
                  NodeId& parentNodeId,
                  NodeId& referenceTypeId,
                  QualifiedName& browseName,
                  NodeId& typeDefinition,
                  ObjectAttributes& attr,
                  NodeId& outNewNodeId = NodeId::Null,
                  NodeContext* instantiationCallback = nullptr) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError = UA_Server_addObjectNode(_server,
                                              requestedNewNodeId,
                                              parentNodeId,
                                              referenceTypeId,
                                              browseName,
                                              typeDefinition,
                                              attr,
                                              instantiationCallback,
                                              outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());
        return lastOK();
    }

    /**
     * addObjectTypeNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addObjectTypeNode(NodeId& requestedNewNodeId,
                      NodeId& parentNodeId,
                      NodeId& referenceTypeId,
                      QualifiedName& browseName,
                      ObjectTypeAttributes& attr,
                      NodeId& outNewNodeId = NodeId::Null,
                      NodeContext* instantiationCallback = nullptr) {
        if (!server()) return false;

        _lastError = UA_Server_addObjectTypeNode(_server,
                                                  requestedNewNodeId,
                                                  parentNodeId,
                                                  referenceTypeId,
                                                  browseName,
                                                  attr,
                                                  instantiationCallback,
                                                  outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());
        return lastOK();
    }

    /**
     * addViewNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addViewNode(NodeId& requestedNewNodeId,
                NodeId& parentNodeId,
                NodeId& referenceTypeId,
                QualifiedName& browseName,
                ViewAttributes& attr,
                NodeId& outNewNodeId = NodeId::Null,
                NodeContext* instantiationCallback = nullptr
                ) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError = UA_Server_addViewNode(_server,
                                            requestedNewNodeId,
                                            parentNodeId,
                                            referenceTypeId,
                                            browseName,
                                            attr,
                                            instantiationCallback,
                                            outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());
        return lastOK();
    }

    /**
     * addReferenceTypeNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addReferenceTypeNode(
        NodeId& requestedNewNodeId,
        NodeId& parentNodeId,
        NodeId& referenceTypeId,
        QualifiedName& browseName,
        ReferenceTypeAttributes& attr,
        NodeId& outNewNodeId = NodeId::Null,
        NodeContext* instantiationCallback = nullptr
    ) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError = UA_Server_addReferenceTypeNode(_server,
                                                    requestedNewNodeId,
                                                    parentNodeId,
                                                    referenceTypeId,
                                                    browseName,
                                                    attr,
                                                    instantiationCallback,
                                                    outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());
        return lastOK();
    }

    /**
     * addDataTypeNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param attr
     * @param outNewNodeId
     * @param instantiationCallback
     * @return 
     */
    bool addDataTypeNode(
        NodeId& requestedNewNodeId,
        NodeId& parentNodeId,
        NodeId& referenceTypeId,
        QualifiedName& browseName,
        DataTypeAttributes& attr,
        NodeId& outNewNodeId = NodeId::Null,
        NodeContext* instantiationCallback = nullptr
    ) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError = UA_Server_addDataTypeNode(_server,
                                                requestedNewNodeId,
                                                parentNodeId,
                                                referenceTypeId,
                                                browseName,
                                                attr,
                                                instantiationCallback,
                                                outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());
        return lastOK();
    }

    /**
     * addDataSourceVariableNode
     * @param requestedNewNodeId
     * @param parentNodeId
     * @param referenceTypeId
     * @param browseName
     * @param typeDefinition
     * @param attr
     * @param dataSource
     * @param outNewNodeId
     * @return 
     */
    bool addDataSourceVariableNode(
        NodeId& requestedNewNodeId,
        NodeId& parentNodeId,
        NodeId& referenceTypeId,
        QualifiedName& browseName,
        NodeId& typeDefinition,
        VariableAttributes& attr,
        DataSource& dataSource,
        NodeId& outNewNodeId = NodeId::Null,
        NodeContext* instantiationCallback = nullptr
    ) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError = UA_Server_addDataSourceVariableNode(_server,
                                                          requestedNewNodeId,
                                                          parentNodeId,
                                                          referenceTypeId,
                                                          browseName,
                                                          typeDefinition,
                                                          attr,
                                                          dataSource,
                                                          instantiationCallback,
                                                          outNewNodeId.isNull() ? nullptr : outNewNodeId.ref());

        return lastOK();
    }

    /**
     * addReference
     * @param sourceId
     * @param refTypeId
     * @param targetId
     * @param isForward
     * @return 
     */
    bool addReference(NodeId& sourceId, NodeId& refTypeId, ExpandedNodeId& targetId, bool isForward) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  UA_Server_addReference(server(), sourceId, refTypeId, targetId, isForward);
        return lastOK();
    }

    /**
     * markMandatory
     * @param nodeId
     * @return 
     */
    bool markMandatory(NodeId& nodeId) {
        return addReference(nodeId, NodeId::HasModellingRule, ExpandedNodeId::ModellingRuleMandatory, true);
    }

    /**
     * deleteReference
     * @param sourceNodeId
     * @param referenceTypeId
     * @param isForward
     * @param targetNodeId
     * @param deleteBidirectional
     * @return 
     */
    bool deleteReference(NodeId& sourceNodeId,
                          NodeId& referenceTypeId, bool isForward,
                          ExpandedNodeId& targetNodeId,
                          bool deleteBidirectional) {
        if (!server()) return false;

        WriteLock l(_mutex);
        _lastError =  UA_Server_deleteReference(server(), sourceNodeId, referenceTypeId,
                                                isForward, targetNodeId, deleteBidirectional);
        return lastOK();

    }


    /**
     * addInstance
     * @param n
     * @param parent
     * @param nodeId
     * @return 
     */
    bool addInstance(const std::string& n, NodeId& requestedNewNodeId, NodeId& parent,
                      NodeId& typeId, NodeId& nodeId = NodeId::Null, NodeContext* context = nullptr) {
        if (!server()) return false;

        ObjectAttributes oAttr;
        oAttr.setDefault();
        oAttr.setDisplayName(n);
        QualifiedName qn(parent.nameSpaceIndex(), n);
        return addObjectNode(requestedNewNodeId,
                              parent,
                              NodeId::Organizes,
                              qn,
                              typeId,
                              oAttr,
                              nodeId,
                              context);
    }

    /**
     * Creates a node representation of an event
     * @param server The server object
     * @param eventType The type of the event for which a node should be created
     * @param outNodeId The NodeId of the newly created node for the event
     * @return The StatusCode of the UA_Server_createEvent method */
    bool createEvent(const NodeId& eventType, NodeId& outNodeId) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError = UA_Server_createEvent(_server, eventType, outNodeId.ref());
        return lastOK();
    }

    /**
     * Triggers a node representation of an event by applying EventFilters and adding the event to the appropriate queues.
     * @param server The server object
     * @param eventNodeId The NodeId of the node representation of the event which should be triggered
     * @param outEvent the EventId of the new event
     * @param deleteEventNode Specifies whether the node representation of the event should be deleted
     * @return The StatusCode of the UA_Server_triggerEvent method */
    bool triggerEvent(NodeId& eventNodeId,    UA_ByteString* outEventId = nullptr, bool deleteEventNode = true) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError = UA_Server_triggerEvent(_server,
                                            eventNodeId,
                                            UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER),
                                            outEventId,
                                            deleteEventNode);
        return lastOK();
    }


    /**
     * addNewEventType
     * @param name
     * @param description
     * @param eventType  the event type node
     * @return true on success
     */
    bool  addNewEventType(const std::string& name, NodeId& eventType, const std::string& description = std::string()) {
        if (!server()) return false;
        ObjectTypeAttributes attr;
        attr.setDefault();
        attr.setDisplayName(name);
        attr.setDescription((description.empty() ? name : description));
        QualifiedName qn(0, name);
        WriteLock l(_mutex);
        _lastError =  UA_Server_addObjectTypeNode(server(),
                                                  UA_NODEID_NULL,
                                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE),
                                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                                  qn,
                                                  attr,
                                                  NULL,
                                                  eventType.ref());
        return lastOK();
    }


    /**
     * setUpEvent
     * @param outId
     * @param eventMessage
     * @param eventSourceName
     * @param eventSeverity
     * @param eventTime
     * @return true on success
     */
    bool  setUpEvent(NodeId& outId, NodeId& eventType, const std::string& eventMessage,
                      const std::string& eventSourceName, int eventSeverity = 100,
                      UA_DateTime eventTime = UA_DateTime_now()
                    ) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError = UA_Server_createEvent(server(), eventType, outId);
        if (_lastError == UA_STATUSCODE_GOOD) {

            /* Set the Event Attributes */
            /* Setting the Time is required or else the event will not show up in UAExpert! */
            UA_Server_writeObjectProperty_scalar(server(), outId, UA_QUALIFIEDNAME(0, const_cast<char*>("Time")),
                                                  &eventTime, &UA_TYPES[UA_TYPES_DATETIME]);

            UA_Server_writeObjectProperty_scalar(server(), outId, UA_QUALIFIEDNAME(0, const_cast<char*>("Severity")),
                                                  &eventSeverity, &UA_TYPES[UA_TYPES_UINT16]);

            LocalizedText eM(const_cast<char*>("en-US"), eventMessage);
            UA_Server_writeObjectProperty_scalar(server(), outId, UA_QUALIFIEDNAME(0, const_cast<char*>("Message")),
                                                  eM.ref(), &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);

            UA_String eSN = UA_STRING(const_cast<char*>(eventSourceName.c_str()));
            UA_Server_writeObjectProperty_scalar(server(), outId, UA_QUALIFIEDNAME(0, const_cast<char*>("SourceName")),
                                                  &eSN, &UA_TYPES[UA_TYPES_STRING]);
        }
        return lastOK();
    }

    /**
     * updateCertificate
     * @param oldCertificate
     * @param newCertificate
     * @param newPrivateKey
     * @param closeSessions
     * @param closeSecureChannels
     * @return true on success
     */
    bool updateCertificate(const UA_ByteString* oldCertificate,
                            const UA_ByteString* newCertificate,
                            const UA_ByteString* newPrivateKey,
                            bool closeSessions = true,
                            bool closeSecureChannels = true) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  UA_Server_updateCertificate(_server,
                                                  oldCertificate,
                                                  newCertificate,
                                                  newPrivateKey,
                                                  closeSessions,
                                                  closeSecureChannels);
        return lastOK();
    }

    /**
     * accessControlAllowHistoryUpdateUpdateData
     * @param sessionId
     * @param sessionContext
     * @param nodeId
     * @param performInsertReplace
     * @param value
     * @return 
     */
    bool accessControlAllowHistoryUpdateUpdateData(const NodeId& sessionId,
                                                    void* sessionContext,
                                                    const NodeId& nodeId,
                                                    UA_PerformUpdateType performInsertReplace,
                                                    UA_DataValue& value) {
        if (!server()) return false;
        WriteLock l(_mutex);
        return  UA_Server_AccessControl_allowHistoryUpdateUpdateData(_server, sessionId.constRef(), sessionContext,
                                                                      nodeId.constRef(), performInsertReplace, &value) == UA_TRUE;
    }

    bool accessControlAllowHistoryUpdateDeleteRawModified(const NodeId& sessionId, void* sessionContext,
                                                          const NodeId& nodeId,
                                                          UA_DateTime startTimestamp,
                                                          UA_DateTime endTimestamp,
                                                          bool isDeleteModified = true) {
        if (!server()) return false;
        WriteLock l(_mutex);
        return   UA_Server_AccessControl_allowHistoryUpdateDeleteRawModified(_server,
                                                                              sessionId.constRef(), sessionContext,
                                                                              nodeId.constRef(),
                                                                              startTimestamp,
                                                                              endTimestamp,
                                                                              isDeleteModified);

    }

    // Access control

    /**
     * allowAddNode
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return 
     */
    virtual bool allowAddNode(UA_AccessControl* /*ac*/,
                              const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                              const UA_AddNodesItem* /*item*/) {
        return true;
    }

    /**
     * allowAddReference
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return 
     */
    virtual bool allowAddReference(UA_AccessControl* /*ac*/,
                                    const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                    const UA_AddReferencesItem* /*item*/) {
        return true;
    }

    /**
     * allowDeleteNode
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return 
     */
    virtual bool allowDeleteNode(UA_AccessControl* /*ac*/,
                                  const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                  const UA_DeleteNodesItem* /*item*/) {
        return false; // Do not allow deletion from client
    }

    /**
     * allowDeleteReference
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return 
     */
    virtual bool allowDeleteReference(UA_AccessControl* /*ac*/,
                                      const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                      const UA_DeleteReferencesItem* /*item*/) {
        return true;
    }

    /**
     * activateSession
     * @return 
     */
    virtual UA_StatusCode activateSession(UA_AccessControl* /*ac*/,
                                          const UA_EndpointDescription* /*endpointDescription*/,
                                          const UA_ByteString* /*secureChannelRemoteCertificate*/,
                                          const UA_NodeId* /*sessionId*/,
                                          const UA_ExtensionObject* /*userIdentityToken*/,
                                          void** /*sessionContext*/) {
        return UA_STATUSCODE_BADSESSIONIDINVALID;
    }

    /**
     * De-authenticate a session and cleanup
     */
    virtual void closeSession(UA_AccessControl* /*ac*/,
                              const UA_NodeId* /*sessionId*/, void* /*sessionContext*/) {

    }

    /**
     * Access control for all nodes
     */
    virtual uint32_t getUserRightsMask(UA_AccessControl* /*ac*/,
                                        const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                        const UA_NodeId* /*nodeId*/, void* /*nodeContext*/) {
        return 0;
    }

    /**
     * Additional access control for variable nodes
     */
    virtual uint8_t getUserAccessLevel(UA_AccessControl* /*ac*/,
                                        const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                        const UA_NodeId* /*nodeId*/, void* /*nodeContext*/) {
        return 0;
    }

    /**
    * Additional access control for method nodes
    */
    virtual bool getUserExecutable(UA_AccessControl* /*ac*/,
                                    const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                    const UA_NodeId* /*methodId*/, void* /*methodContext*/) {
        return false;
    }

    /** 
     * Additional access control for calling a method node
     * in the context of a specific object
     */
    virtual bool getUserExecutableOnObject(UA_AccessControl* ac,
                                            const UA_NodeId* sessionId, void* sessionContext,
                                            const UA_NodeId* methodId, void* methodContext,
                                            const UA_NodeId* objectId, void* objectContext) {
        return false;
    }
    
    /**
     * Allow insert, replace, update of historical data
     */
    virtual bool allowHistoryUpdateUpdateData(UA_AccessControl* /*ac*/,
                                              const UA_NodeId* /*sessionId*/, void* /*sessionContext*/,
                                              const UA_NodeId* /*nodeId*/,
                                              UA_PerformUpdateType /*performInsertReplace*/,
                                              const UA_DataValue* /*value*/) {
        return false;
    }

    /**
     *Allow delete of historical data
     */
    virtual bool allowHistoryUpdateDeleteRawModified(UA_AccessControl* /*ac*/,
                                                      const UA_NodeId* /*sessionId*/,
                                                      void* /*sessionContext*/,
                                                      const UA_NodeId* /*nodeId*/,
                                                      UA_DateTime /*startTimestamp*/,
                                                      UA_DateTime/* endTimestamp*/,
                                                      bool /*isDeleteModified*/) {
        return false;
    }

    /**
     * setHistoryDatabase
     * Publish - Subscribe interface
     * @param h
     */
    void setHistoryDatabase(UA_HistoryDatabase& h);
};

} // namespace open62541

#endif // OPEN62541SERVER_H
