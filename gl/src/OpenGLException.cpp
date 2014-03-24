/*
 * OpenGLException.cpp
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

#include "vislib/OpenGLException.h"


#include <GL/glu.h>


/*
 * vislib::graphics::gl::OpenGLException::OpenGLException
 */
vislib::graphics::gl::OpenGLException::OpenGLException(const GLenum errorCode, 
        const char *file, const int line) 
        : the::exception(file, line), errorCode(errorCode) {
    the::exception::set_msg(reinterpret_cast<const char *>(
        ::gluErrorString(this->errorCode)));
}


/*
 * vislib::graphics::gl::OpenGLException::OpenGLException
 */
vislib::graphics::gl::OpenGLException::OpenGLException(const char *file, 
        const int line)
        : the::exception(file, line), errorCode(::glGetError()) {
    the::exception::set_msg(reinterpret_cast<const char *>(
        ::gluErrorString(this->errorCode)));
}


/*
 * vislib::graphics::gl::OpenGLException::OpenGLException
 */
vislib::graphics::gl::OpenGLException::OpenGLException(
        const OpenGLException& rhs)
        : the::exception(rhs), errorCode(rhs.errorCode) {
}


/*
 * vislib::graphics::gl::OpenGLException::~OpenGLException
 */
vislib::graphics::gl::OpenGLException::~OpenGLException(void) {
}


/*
 * vislib::graphics::gl::OpenGLException::operator =
 */
vislib::graphics::gl::OpenGLException& 
vislib::graphics::gl::OpenGLException::operator =(const OpenGLException& rhs) {
    the::exception::operator =(rhs);
    
    if (this != &rhs) {
        this->errorCode = rhs.errorCode;
    }

    return *this;
}
