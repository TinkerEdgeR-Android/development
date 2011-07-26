#include "GL2Encoder.h"
#include <assert.h>


static GLubyte *gVendorString= (GLubyte *) "Android";
static GLubyte *gRendererString= (GLubyte *) "Android HW-GLES 2.0";
static GLubyte *gVersionString= (GLubyte *) "OpenGL ES 2.0";
static GLubyte *gExtensionsString= (GLubyte *) ""; // no extensions at this point;

#define SET_ERROR_IF(condition,err) if((condition)) {                            \
        LOGE("%s:%s:%d GL error 0x%x\n", __FILE__, __FUNCTION__, __LINE__, err); \
        ctx->setError(err);                                    \
        return;                                                  \
    }


#define RET_AND_SET_ERROR_IF(condition,err,ret) if((condition)) {                \
        LOGE("%s:%s:%d GL error 0x%x\n", __FILE__, __FUNCTION__, __LINE__, err); \
        ctx->setError(err);                                    \
        return ret;                                              \
    }


GL2Encoder::GL2Encoder(IOStream *stream) : gl2_encoder_context_t(stream)
{
    m_initialized = false;
    m_state = NULL;
    m_error = GL_NO_ERROR;
    m_num_compressedTextureFormats = 0;
    m_compressedTextureFormats = NULL;
    //overrides
    m_glFlush_enc = set_glFlush(s_glFlush);
    m_glPixelStorei_enc = set_glPixelStorei(s_glPixelStorei);
    m_glGetString_enc = set_glGetString(s_glGetString);
    m_glBindBuffer_enc = set_glBindBuffer(s_glBindBuffer);
    m_glDrawArrays_enc = set_glDrawArrays(s_glDrawArrays);
    m_glDrawElements_enc = set_glDrawElements(s_glDrawElements);
    m_glGetIntegerv_enc = set_glGetIntegerv(s_glGetIntegerv);
    m_glGetFloatv_enc = set_glGetFloatv(s_glGetFloatv);
    m_glGetBooleanv_enc = set_glGetBooleanv(s_glGetBooleanv);
    m_glVertexAttribPointer_enc = set_glVertexAttribPointer(s_glVertexAtrribPointer);
    m_glEnableVertexAttribArray_enc = set_glEnableVertexAttribArray(s_glEnableVertexAttribArray);
    m_glDisableVertexAttribArray_enc = set_glDisableVertexAttribArray(s_glDisableVertexAttribArray);
    m_glGetVertexAttribiv_enc = set_glGetVertexAttribiv(s_glGetVertexAttribiv);
    m_glGetVertexAttribfv_enc = set_glGetVertexAttribfv(s_glGetVertexAttribfv);
    m_glGetVertexAttribPointerv = set_glGetVertexAttribPointerv(s_glGetVertexAttribPointerv);
    set_glShaderSource(s_glShaderSource);
    set_glFinish(s_glFinish);
    m_glGetError_enc = set_glGetError(s_glGetError);
}

GL2Encoder::~GL2Encoder()
{
    delete m_compressedTextureFormats;
}

GLenum GL2Encoder::s_glGetError(void * self)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    GLenum err = ctx->getError();
    if(err != GL_NO_ERROR) {
        ctx->setError(GL_NO_ERROR);
        return err;
    }

    return ctx->m_glGetError_enc(self);

}

void GL2Encoder::s_glFlush(void *self)
{
    GL2Encoder *ctx = (GL2Encoder *) self;
    ctx->m_glFlush_enc(self);
    ctx->m_stream->flush();
}

const GLubyte *GL2Encoder::s_glGetString(void *self, GLenum name)
{
    GLubyte *retval =  (GLubyte *) "";
    switch(name) {
    case GL_VENDOR:
        retval = gVendorString;
        break;
    case GL_RENDERER:
        retval = gRendererString;
        break;
    case GL_VERSION:
        retval = gVersionString;
        break;
    case GL_EXTENSIONS:
        retval = gExtensionsString;
        break;
    }
    return retval;
}

void GL2Encoder::s_glPixelStorei(void *self, GLenum param, GLint value)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    ctx->m_glPixelStorei_enc(ctx, param, value);
    assert(ctx->m_state != NULL);
    ctx->m_state->setPixelStore(param, value);
}


void GL2Encoder::s_glBindBuffer(void *self, GLenum target, GLuint id)
{
    GL2Encoder *ctx = (GL2Encoder *) self;
    assert(ctx->m_state != NULL);
    ctx->m_state->bindBuffer(target, id);
    // TODO set error state if needed;
    ctx->m_glBindBuffer_enc(self, target, id);
}

void GL2Encoder::s_glBufferData(void * self, GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
    GL2Encoder *ctx = (GL2Encoder *) self;
    GLuint bufferId = ctx->m_state->getBuffer(target);
    SET_ERROR_IF(bufferId==0, GL_INVALID_OPERATION);
    SET_ERROR_IF(size<0, GL_INVALID_VALUE);

    ctx->m_shared->updateBufferData(bufferId, size, (void*)data);
    ctx->m_glBufferData_enc(self, target, size, data, usage);
}

void GL2Encoder::s_glBufferSubData(void * self, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data)
{
    GL2Encoder *ctx = (GL2Encoder *) self;
    GLuint bufferId = ctx->m_state->getBuffer(target);
    SET_ERROR_IF(bufferId==0, GL_INVALID_OPERATION);

    GLenum res = ctx->m_shared->subUpdateBufferData(bufferId, offset, size, (void*)data);
    SET_ERROR_IF(res, res);

    ctx->m_glBufferSubData_enc(self, target, offset, size, data);
}

void GL2Encoder::s_glDeleteBuffers(void * self, GLsizei n, const GLuint * buffers)
{
    GL2Encoder *ctx = (GL2Encoder *) self;
    SET_ERROR_IF(n<0, GL_INVALID_VALUE);
    for (int i=0; i<n; i++) {
        ctx->m_shared->deleteBufferData(buffers[i]);
        ctx->m_glDeleteBuffers_enc(self,1,&buffers[i]);
    }
}

void GL2Encoder::s_glVertexAtrribPointer(void *self, GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * ptr)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state != NULL);
    ctx->m_state->setState(indx, size, type, normalized, stride, ptr);
}

void GL2Encoder::s_glGetIntegerv(void *self, GLenum param, GLint *params)
{
    GL2Encoder *ctx = (GL2Encoder *) self;
    assert(ctx->m_state != NULL);
    if (param == GL_NUM_SHADER_BINARY_FORMATS) {
        *params = 0;
    } else if (param == GL_SHADER_BINARY_FORMATS) {
        // do nothing
    } else  if (param == GL_COMPRESSED_TEXTURE_FORMATS) {
        GLint *compressedTextureFormats = ctx->getCompressedTextureFormats();
        if (ctx->m_num_compressedTextureFormats > 0 && compressedTextureFormats != NULL) {
            memcpy(params, compressedTextureFormats, ctx->m_num_compressedTextureFormats * sizeof(GLint));
        }
    } else if (!ctx->m_state->getClientStateParameter<GLint>(param, params)) {
        ctx->m_glGetIntegerv_enc(self, param, params);
    }
}


void GL2Encoder::s_glGetFloatv(void *self, GLenum param, GLfloat *ptr)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state != NULL);
    if (param == GL_NUM_SHADER_BINARY_FORMATS) {
        *ptr = 0;
    } else if (param == GL_SHADER_BINARY_FORMATS) {
        // do nothing;
    } else  if (param == GL_COMPRESSED_TEXTURE_FORMATS) {
        GLint * compressedTextureFormats = ctx->getCompressedTextureFormats();
        if (ctx->m_num_compressedTextureFormats > 0 && compressedTextureFormats != NULL) {
            for (int i = 0; i < ctx->m_num_compressedTextureFormats; i++) {
                ptr[i] = (GLfloat) compressedTextureFormats[i];
            }
        }
    }
    else if (!ctx->m_state->getClientStateParameter<GLfloat>(param,ptr)) {
        ctx->m_glGetFloatv_enc(self, param, ptr);
    }
}


void GL2Encoder::s_glGetBooleanv(void *self, GLenum param, GLboolean *ptr)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state != NULL);
    if (param == GL_COMPRESSED_TEXTURE_FORMATS) {
        // ignore the command, although we should have generated a GLerror;
    }
    else if (!ctx->m_state->getClientStateParameter<GLboolean>(param,ptr)) {
        ctx->m_glGetBooleanv_enc(self, param, ptr);
    }
}


void GL2Encoder::s_glEnableVertexAttribArray(void *self, GLuint index)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state);
    ctx->m_state->enable(index, 1);
}

void GL2Encoder::s_glDisableVertexAttribArray(void *self, GLuint index)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state);
    ctx->m_state->enable(index, 0);
}


void GL2Encoder::s_glGetVertexAttribiv(void *self, GLuint index, GLenum pname, GLint *params)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state);

    if (!ctx->m_state->getVertexAttribParameter<GLint>(index, pname, params)) {
        ctx->m_glGetVertexAttribiv_enc(self, index, pname, params);
    }
}

void GL2Encoder::s_glGetVertexAttribfv(void *self, GLuint index, GLenum pname, GLfloat *params)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state);

    if (!ctx->m_state->getVertexAttribParameter<GLfloat>(index, pname, params)) {
        ctx->m_glGetVertexAttribfv_enc(self, index, pname, params);
    }
}

void GL2Encoder::s_glGetVertexAttribPointerv(void *self, GLuint index, GLenum pname, GLvoid **pointer)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    if (ctx->m_state == NULL) return;

    const GLClientState::VertexAttribState *va_state = ctx->m_state->getState(index);
    if (va_state != NULL) {
        *pointer = va_state->data;
    }
}


void GL2Encoder::sendVertexAttributes(GLint first, GLsizei count)
{
    assert(m_state);

    for (int i = 0; i < m_state->nLocations(); i++) {
        bool enableDirty;
        const GLClientState::VertexAttribState *state = m_state->getStateAndEnableDirty(i, &enableDirty);

        if (!state) {
            continue;
        }

        if (!enableDirty && !state->enabled) {
            continue;
        }


        if (state->enabled) {
            m_glEnableVertexAttribArray_enc(this, i);

            unsigned int datalen = state->elementSize * count;
            int stride = state->stride == 0 ? state->elementSize : state->stride;
            int firstIndex = stride * first;

            if (state->bufferObject == 0) {
                this->glVertexAttribPointerData(this, i, state->size, state->type, state->normalized, state->stride,
                                                (unsigned char *)state->data + firstIndex, datalen);
            } else {
                this->m_glBindBuffer_enc(this, GL_ARRAY_BUFFER, state->bufferObject);
                this->glVertexAttribPointerOffset(this, i, state->size, state->type, state->normalized, state->stride,
                                                  (GLuint) state->data + firstIndex);
                this->m_glBindBuffer_enc(this, GL_ARRAY_BUFFER, m_state->currentArrayVbo());
            }
        } else {
            this->m_glDisableVertexAttribArray_enc(this, i);
        }
    }
}

void GL2Encoder::s_glDrawArrays(void *self, GLenum mode, GLint first, GLsizei count)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    ctx->sendVertexAttributes(first, count);
    ctx->m_glDrawArrays_enc(ctx, mode, 0, count);
}


void GL2Encoder::s_glDrawElements(void *self, GLenum mode, GLsizei count, GLenum type, const void *indices)
{

    GL2Encoder *ctx = (GL2Encoder *)self;
    assert(ctx->m_state != NULL);
    SET_ERROR_IF(count<0, GL_INVALID_VALUE);

    bool has_immediate_arrays = false;
    bool has_indirect_arrays = false;
    int nLocations = ctx->m_state->nLocations();

    for (int i = 0; i < nLocations; i++) {
        const GLClientState::VertexAttribState *state = ctx->m_state->getState(i);
        if (state->enabled) {
            if (state->bufferObject != 0) {
                has_indirect_arrays = true;
            } else {
                has_immediate_arrays = true;
            }
        }
    }

    if (!has_immediate_arrays && !has_indirect_arrays) {
        LOGE("glDrawElements: no data bound to the command - ignoring\n");
        return;
    }

    bool adjustIndices = true;
    if (ctx->m_state->currentIndexVbo() != 0) {
        if (!has_immediate_arrays) {
            ctx->sendVertexAttributes(0, count);
            ctx->m_glBindBuffer_enc(self, GL_ELEMENT_ARRAY_BUFFER, ctx->m_state->currentIndexVbo());
            ctx->glDrawElementsOffset(ctx, mode, count, type, (GLuint)indices);
            adjustIndices = false;
        } else {
            BufferData * buf = ctx->m_shared->getBufferData(ctx->m_state->currentIndexVbo());
            ctx->m_glBindBuffer_enc(self, GL_ELEMENT_ARRAY_BUFFER, 0);
            indices = (void*)((GLintptr)buf->m_fixedBuffer.ptr() + (GLintptr)indices);
        }
    } 
    if (adjustIndices) {
        void *adjustedIndices = (void*)indices;
        int minIndex = 0, maxIndex = 0;

        switch(type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            GLUtils::minmax<unsigned char>((unsigned char *)indices, count, &minIndex, &maxIndex);
            if (minIndex != 0) {
                adjustedIndices =  ctx->m_fixedBuffer.alloc(glSizeof(type) * count);
                GLUtils::shiftIndices<unsigned char>((unsigned char *)indices,
                                                 (unsigned char *)adjustedIndices,
                                                 count, -minIndex);
            }
            break;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            GLUtils::minmax<unsigned short>((unsigned short *)indices, count, &minIndex, &maxIndex);
            if (minIndex != 0) {
                adjustedIndices = ctx->m_fixedBuffer.alloc(glSizeof(type) * count);
                GLUtils::shiftIndices<unsigned short>((unsigned short *)indices,
                                                  (unsigned short *)adjustedIndices,
                                                  count, -minIndex);
            }
            break;
        default:
            LOGE("unsupported index buffer type %d\n", type);
        }
        if (has_indirect_arrays || 1) {
            ctx->sendVertexAttributes(minIndex, maxIndex - minIndex + 1);
            ctx->glDrawElementsData(ctx, mode, count, type, adjustedIndices,
                                    count * glSizeof(type));
            // XXX - OPTIMIZATION (see the other else branch) should be implemented
            if(!has_indirect_arrays) {
                //LOGD("unoptimized drawelements !!!\n");
            }
        } else {
            // we are all direct arrays and immidate mode index array -
            // rebuild the arrays and the index array;
            LOGE("glDrawElements: direct index & direct buffer data - will be implemented in later versions;\n");
        }
    }
}


GLint * GL2Encoder::getCompressedTextureFormats()
{
    if (m_compressedTextureFormats == NULL) {
        this->glGetIntegerv(this, GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                            &m_num_compressedTextureFormats);
        if (m_num_compressedTextureFormats > 0) {
            // get number of texture formats;
            m_compressedTextureFormats = new GLint[m_num_compressedTextureFormats];
            this->glGetCompressedTextureFormats(this, m_num_compressedTextureFormats, m_compressedTextureFormats);
        }
    }
    return m_compressedTextureFormats;
}

void GL2Encoder::s_glShaderSource(void *self, GLuint shader, GLsizei count, const GLchar **string, const GLint *length)
{
    int len = glUtilsCalcShaderSourceLen((char**)string, (GLint*)length, count);
    char *str = new char[len + 1];
    glUtilsPackStrings(str, (char**)string, (GLint*)length, count);

    GL2Encoder *ctx = (GL2Encoder *)self;
    ctx->glShaderString(ctx, shader, str, len + 1);
    delete str;
}

void GL2Encoder::s_glFinish(void *self)
{
    GL2Encoder *ctx = (GL2Encoder *)self;
    ctx->glFinishRoundTrip(self);
}

