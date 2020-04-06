/*
 * Copyright (C) 2017 -  B. J. Hill
 *
 * This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
 * redistribute it and/or modify it under the terms of the Mozilla Public
 * License v2.0 as stated in the LICENSE file provided with open62541.
 *
 * open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 */
#ifndef SERVERREPEATEDCALLBACK_H
#define SERVERREPEATEDCALLBACK_H
#include "open62541objects.h"
namespace Open62541 {


typedef std::function<void (SeverRepeatedCallback &)> SeverRepeatedCallbackFunc;
/**
 * The SeverRepeatedCallback class
 */
class UA_EXPORT SeverRepeatedCallback {
    Server &_server; // parent server
    UA_UInt32 _interval = 1000;
    UA_UInt64 _id = 0;
    SeverRepeatedCallbackFunc _func; // functior to handle event

protected:
    UA_StatusCode _lastError = 0;
public:
    /**
     * callbackFunction
     * @param server
     * @param data
     */
    static void callbackFunction(UA_Server *server, void *data);
    /**
     * SeverRepeatedCallback
     * @param s
     * @param interval
     */
    SeverRepeatedCallback(Server &s, UA_UInt32 interval);
    SeverRepeatedCallback(Server &s, UA_UInt32 interval, SeverRepeatedCallbackFunc func);
    //
    //
    /**
     * ~SeverRepeatedCallback
     */
    virtual ~SeverRepeatedCallback();

    /**
     * start
     * @return 
     */
    bool start();


    /**
     * changeInterval
     * @param i
     * @return 
     */
    bool changeInterval(unsigned i);
    /**
     * stop
     * @return 
     */
    bool stop();

    /**
     * lastError
     * @return 
     */
    UA_StatusCode lastError() const {
        return _lastError;
    }
    /**
     * server
     * @return 
     */
    Server &server() {
        return _server;
    }
    /**
     * id
     * @return 
     */
    UA_UInt64 id() const {
        return _id;
    }
    /**
     * callback
     */
    virtual void callback() {
        // if the functor is valid call it - no need to derive a handler class, unless you want to
        if (_func) _func(*this);
    } // The callback
    /**
     * lastOK
     * @return 
     */
    bool lastOK() const {
        return _lastError == UA_STATUSCODE_GOOD;
    }
};
/**
 * SeverRepeatedCallbackRef
 */
typedef std::shared_ptr<SeverRepeatedCallback> SeverRepeatedCallbackRef;
} // namespace Open62541


#endif // SERVERREPEATEDCALLBACK_H
