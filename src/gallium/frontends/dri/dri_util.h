/*
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file dri_util.h
 * DRI utility functions definitions.
 *
 * This module acts as glue between GLX and the actual hardware driver.  A DRI
 * driver doesn't really \e have to use any of this - it's optional.  But, some
 * useful stuff is done here that otherwise would have to be duplicated in most
 * drivers.
 *
 * Basically, these utility functions take care of some of the dirty details of
 * screen initialization, context creation, context binding, DRM setup, etc.
 *
 * These functions are compiled into each DRI driver so libGL.so knows nothing
 * about them.
 *
 * \sa dri_util.c.
 *
 * \author Kevin E. Martin <kevin@precisioninsight.com>
 * \author Brian Paul <brian@precisioninsight.com>
 */

/**
 * The following structs are shared between DRISW and DRI2, the DRISW structs
 * are essentially base classes of the DRI2 structs. DRISW needs to compile on
 * platforms without DRM, so keep the structs opaque to DRM.
 */

#ifndef _DRI_UTIL_H_
#define _DRI_UTIL_H_

#include <GL/gl.h>
#include <GL/internal/dri_interface.h>
#include "kopper_interface.h"
#include "main/formats.h"
#include "main/glconfig.h"
#include "main/menums.h"
#include "util/xmlconfig.h"
#include <stdbool.h>

struct __DRIconfigRec {
    struct gl_config modes;
};

/**
 * Extensions.
 */
extern const __DRIcoreExtension driCoreExtension;
extern const __DRIswrastExtension driSWRastExtension;
extern const __DRIdri2Extension driDRI2Extension;
extern const __DRIdri2Extension swkmsDRI2Extension;
extern const __DRI2configQueryExtension dri2ConfigQueryExtension;
extern const __DRI2flushControlExtension dri2FlushControlExtension;

#if defined(GALLIUM_PVR)
extern const __DRIdri2Extension pvrDRI2Extension;
#endif

/**
 * Description of the attributes used to create a config.
 *
 * This is passed as the context_config parameter to CreateContext. The idea
 * with this struct is that it can be extended without having to modify all of
 * the drivers. The first three members (major/minor_version and flags) are
 * always valid, but the remaining members are only valid if the corresponding
 * flag is set for the attribute. If the flag is not set then the default
 * value should be assumed. That way the driver can quickly check if any
 * attributes were set that it doesn't understand and report an error.
 */
struct __DriverContextConfig {
    /* These members are always valid */
    unsigned major_version;
    unsigned minor_version;
    uint32_t flags;

    /* Flags describing which of the remaining members are valid */
    uint32_t attribute_mask;

    /* Only valid if __DRIVER_CONTEXT_ATTRIB_RESET_STRATEGY is set */
    int reset_strategy;

    /* Only valid if __DRIVER_CONTEXT_PRIORITY is set */
    unsigned priority;

    /* Only valid if __DRIVER_CONTEXT_ATTRIB_RELEASE_BEHAVIOR is set */
    int release_behavior;

    /* Only valid if __DRIVER_CONTEXT_ATTRIB_NO_ERROR is set */
    int no_error;

    /* Only valid if __DRIVER_CONTEXT_ATTRIB_PROTECTED is set */
    int protected_context;
};

#define __DRIVER_CONTEXT_ATTRIB_RESET_STRATEGY   (1 << 0)
#define __DRIVER_CONTEXT_ATTRIB_PRIORITY         (1 << 1)
#define __DRIVER_CONTEXT_ATTRIB_RELEASE_BEHAVIOR (1 << 2)
#define __DRIVER_CONTEXT_ATTRIB_NO_ERROR         (1 << 3)
#define __DRIVER_CONTEXT_ATTRIB_PROTECTED        (1 << 4)

/**
 * Driver callback functions.
 *
 * Each DRI driver must have one of these structures with all the pointers set
 * to appropriate functions within the driver.
 * 
 * When glXCreateContext() is called, for example, it'll call a helper function
 * dri_util.c which in turn will jump through the \a CreateContext pointer in
 * this structure.
 */
struct __DriverAPIRec {
    const __DRIconfig **(*InitScreen) (__DRIscreen * priv);

    void (*DestroyScreen)(__DRIscreen *driScrnPriv);

    GLboolean (*CreateContext)(gl_api api,
                               const struct gl_config *glVis,
                               __DRIcontext *driContextPriv,
                               const struct __DriverContextConfig *ctx_config,
                               unsigned *error,
                               void *sharedContextPrivate);

    void (*DestroyContext)(__DRIcontext *driContextPriv);

    GLboolean (*CreateBuffer)(__DRIscreen *driScrnPriv,
                              __DRIdrawable *driDrawPriv,
                              const struct gl_config *glVis,
                              GLboolean pixmapBuffer);

    void (*DestroyBuffer)(__DRIdrawable *driDrawPriv);

    void (*SwapBuffers)(__DRIdrawable *driDrawPriv);

    GLboolean (*MakeCurrent)(__DRIcontext *driContextPriv,
                             __DRIdrawable *driDrawPriv,
                             __DRIdrawable *driReadPriv);

    GLboolean (*UnbindContext)(__DRIcontext *driContextPriv);

    __DRIbuffer *(*AllocateBuffer) (__DRIscreen *screenPrivate,
                                    unsigned int attachment,
                                    unsigned int format,
                                    int width, int height);

    void (*ReleaseBuffer) (__DRIscreen *screenPrivate, __DRIbuffer *buffer);

    void (*CopySubBuffer)(__DRIdrawable *driDrawPriv, int x, int y,
                          int w, int h);
};

/**
 * Per-screen private driver information.
 */
struct __DRIscreenRec {
    /**
     * Driver-specific entrypoints provided by the driver's
     * __DRIDriverVtableExtensionRec.
     */
    const struct __DriverAPIRec *driver;

    /**
     * Current screen's number
     */
    int myNum;

    /**
     * File descriptor returned when the kernel device driver is opened.
     *
     * Used to:
     *   - authenticate client to kernel
     *   - map the frame buffer, SAREA, etc.
     *   - close the kernel device driver
     */
    int fd;

    /**
     * Device-dependent private information (not stored in the SAREA).
     *
     * This pointer is never touched by the DRI layer.
     */
    void *driverPrivate;

    void *loaderPrivate;

    int max_gl_core_version;
    int max_gl_compat_version;
    int max_gl_es1_version;
    int max_gl_es2_version;

    const __DRIextension **extensions;

    const __DRIswrastLoaderExtension *swrast_loader;
    const __DRIkopperLoaderExtension *kopper_loader;

    struct {
	/* Flag to indicate that this is a DRI2 screen.  Many of the above
	 * fields will not be valid or initializaed in that case. */
	const __DRIdri2LoaderExtension *loader;
	const __DRIimageLookupExtension *image;
	const __DRIuseInvalidateExtension *useInvalidate;
        const __DRIbackgroundCallableExtension *backgroundCallable;
    } dri2;

    struct {
        const __DRIimageLoaderExtension *loader;
    } image;

    struct {
       const __DRImutableRenderBufferLoaderExtension *loader;
    } mutableRenderBuffer;

    driOptionCache optionInfo;
    driOptionCache optionCache;

    unsigned int api_mask;
};

/**
 * Per-context private driver information.
 */
struct __DRIcontextRec {
    /**
     * Device driver's private context data.  This structure is opaque.
     */
    void *driverPrivate;

    /**
     * The loaders's private context data.  This structure is opaque.
     */
    void *loaderPrivate;

    /**
     * Pointer to drawable currently bound to this context for drawing.
     */
    __DRIdrawable *driDrawablePriv;

    /**
     * Pointer to drawable currently bound to this context for reading.
     */
    __DRIdrawable *driReadablePriv;

    /**
     * Pointer to screen on which this context was created.
     */
    __DRIscreen *driScreenPriv;

    struct {
	int draw_stamp;
	int read_stamp;
    } dri2;
};

/**
 * Per-drawable private DRI driver information.
 */
struct __DRIdrawableRec {
    /**
     * Driver's private drawable information.
     *
     * This structure is opaque.
     */
    void *driverPrivate;

    /**
     * Private data from the loader.  We just hold on to it and pass
     * it back when calling into loader provided functions.
     */
    void *loaderPrivate;

    /**
     * Pointer to context to which this drawable is currently bound.
     */
    __DRIcontext *driContextPriv;

    /**
     * Pointer to screen on which this drawable was created.
     */
    __DRIscreen *driScreenPriv;

    /**
     * Reference count for number of context's currently bound to this
     * drawable.
     *
     * Once it reaches zero, the drawable can be destroyed.
     *
     * \note This behavior will change with GLX 1.3.
     */
    int refcount;

    /**
     * Last value of the stamp.
     *
     * If this differs from the value stored at __DRIdrawable::dri2.stamp,
     * then the drawable information has been modified by the X server, and the
     * drawable information (below) should be retrieved from the X server.
     */
    unsigned int lastStamp;

    int w, h;

    /**
     * Drawable timestamp.  Increased when the loader calls invalidate.
     */
    struct {
	unsigned int stamp;
    } dri2;
};

extern uint32_t
driGLFormatToImageFormat(mesa_format format);

extern uint32_t
driGLFormatToSizedInternalGLFormat(mesa_format format);

extern mesa_format
driImageFormatToGLFormat(uint32_t image_format);

extern const __DRIimageDriverExtension driImageDriverExtension;

#endif /* _DRI_UTIL_H_ */
