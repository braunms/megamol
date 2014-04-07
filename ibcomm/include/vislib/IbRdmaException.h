/*
 * IbRdmaException.h
 *
 * Copyright (C) 2006 - 2012 by Visualisierungsinstitut Universitaet Stuttgart. 
 * Alle Rechte vorbehalten.
 */

#ifndef VISLIB_IBRDMAEXCEPTION_H_INCLUDED
#define VISLIB_IBRDMAEXCEPTION_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */
#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(push, off)
#endif /* defined(_WIN32) && defined(_MANAGED) */


#include "the/exception.h"

#ifdef HAVE_OFED_SDK


namespace vislib {
namespace net {
namespace ib {


    /**
     * These exceptions are thrown by the RDMA communication channels.
     */
    class IbRdmaException : public the::exception {
    private:

        /** Superclass typedef. */
        typedef the::exception Super;

    public:

        IbRdmaException(const int errorCode, const char *file, const int line);

        IbRdmaException(const char *call, const int errorCode, 
            const char *file, const int line);

        IbRdmaException(const IbRdmaException& rhs);

        /** Dtor. */
        virtual ~IbRdmaException(void);

        inline int GetErrorCode(void) const {
            return this->errorCode;
        }

        IbRdmaException& operator =(const IbRdmaException& rhs);

    private:

        int errorCode;

    };
    
} /* end namespace ib */
} /* end namespace net */
} /* end namespace vislib */

#endif /* HAVE_OFED_SDK */

#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(pop)
#endif /* defined(_WIN32) && defined(_MANAGED) */
#endif /* VISLIB_IBRDMAEXCEPTION_H_INCLUDED */

