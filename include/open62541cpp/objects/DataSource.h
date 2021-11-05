/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/

#include "open62541/types.h"
#include "open62541/plugin/nodestore.h"
#include <open62541cpp/objects/UaBaseTypeTemplate.h>

namespace Open62541 {
    /**
     * Data Source read and write callbacks.
     * @class DataSource open62541objects.h
     * RAII C++ wrapper class for the UA_DataSource struct.
     * No getter or setter, use ->member_name to access them.
     * @see UA_DataSource in open62541.h
     */
    class UA_EXPORT DataSource : public TypeBase<UA_DataSource, UNKNOWN_UA_TYPE>
    {
    public:
        DataSource()
            : TypeBase(new UA_DataSource())
        {
            ref()->read  = nullptr;
            ref()->write = nullptr;
        }
    };
} // namespace Open62541
