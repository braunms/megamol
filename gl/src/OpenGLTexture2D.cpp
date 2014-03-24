/*
 * OpenGLTexture2D.cpp
 *
 * Copyright (C) 2006 - 2009 by Visualisierungsinstitut Universitaet Stuttgart.
 * Alle Rechte vorbehalten.
 * Copyright (C) 2009 by Christoph M�ller. Alle Rechte vorbehalten.
 */

#include "vislib/OpenGLTexture2D.h"

#include "the/assert.h"
#include "the/argument_exception.h"
#include "vislib/glverify.h"
#include "vislib/mathfunctions.h"
#include "the/trace.h"
#include "the/not_supported_exception.h"



/*
 * vislib::graphics::gl::OpenGLTexture2D::SetFilter
 */
GLenum vislib::graphics::gl::OpenGLTexture2D::SetFilter(const GLint minFilter, 
                                                        const GLint magFilter) {
    THE_STACK_TRACE;
    return Super::setFilter(GL_TEXTURE_2D, minFilter, magFilter);
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::SetWrap
 */
GLenum vislib::graphics::gl::OpenGLTexture2D::SetWrap(const GLint s, 
                                                      const GLint t) {
    THE_STACK_TRACE;
    USES_GL_VERIFY;

    GL_VERIFY_RETURN(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s));
    GL_VERIFY_RETURN(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t));

    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::OpenGLTexture2D
 */
vislib::graphics::gl::OpenGLTexture2D::OpenGLTexture2D(void) : Super() {
    THE_STACK_TRACE;
    // Nothing to do.
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::~OpenGLTexture2D
 */
vislib::graphics::gl::OpenGLTexture2D::~OpenGLTexture2D(void) {
    THE_STACK_TRACE;
    // Nothing to do.
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::Bind
 */
GLenum vislib::graphics::gl::OpenGLTexture2D::Bind(void) {
    THE_STACK_TRACE;
    USES_GL_VERIFY;
    GL_VERIFY_RETURN(::glBindTexture(GL_TEXTURE_2D, this->GetId()));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::Create
 */
GLenum vislib::graphics::gl::OpenGLTexture2D::Create(const unsigned int width, 
        const unsigned int height, const void *pixels, const GLenum format, 
        const GLenum type, const GLint internalFormat, const GLint border) {
    THE_STACK_TRACE;
    USES_GL_VERIFY;

    /* Create new texture. */
    if (!this->IsValid()) {
        try {
            this->createId();
        } catch (OpenGLException e) {
            return e.GetErrorCode();
        }   
    }

    GL_VERIFY_EXPR_RETURN(this->Bind());
    GL_VERIFY_RETURN(::glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, 
        height, border, format, type, pixels));

    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::Create
 */
GLenum vislib::graphics::gl::OpenGLTexture2D::Create(const unsigned int width, 
        const unsigned int height, const bool forcePowerOfTwo, const void *pixels,
        const GLenum format, const GLenum type, const GLint internalFormat, 
        const GLint border) {
    THE_STACK_TRACE;
    USES_GL_VERIFY;

    unsigned int w = math::NextPowerOfTwo(width);
    unsigned int h = math::NextPowerOfTwo(height);

    if (!forcePowerOfTwo || ((w == width) && (h == height))) {
        return this->Create(width, height, pixels, format, type, 
            internalFormat, border);
    } else {
        GL_VERIFY_EXPR_RETURN(this->Create(w, h, NULL, format, type, 
            internalFormat, border));
        GL_VERIFY_EXPR_RETURN(this->Update(pixels, width, height, format, type, 
            0, 0, 0));
    }

    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::OpenGLTexture2D::Update
 */
GLenum vislib::graphics::gl::OpenGLTexture2D::Update(const void *pixels, 
        const unsigned int width, const unsigned int height, const GLenum format, 
        const GLenum type, const unsigned int offsetX, const unsigned int offsetY, 
        const GLint level, const bool resetBind) {
    THE_STACK_TRACE;
    USES_GL_VERIFY;
    GLint oldTex;
    GLenum retval = GL_NO_ERROR;

    if (resetBind) {
        GL_VERIFY_RETURN(::glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex));
    }
    
    if ((retval = this->Bind()) != GL_NO_ERROR) {
        goto cleanup;
    }
    ::glTexSubImage2D(GL_TEXTURE_2D, level, offsetX, offsetY, width, height, 
        format, type, pixels);
    retval = ::glGetError();

cleanup:
    if (resetBind) {
        ::glGetError();
        ::glBindTexture(GL_TEXTURE_2D, oldTex);
        if (retval == GL_NO_ERROR) {
            retval = ::glGetError();
        }
    }

    return retval;
}
