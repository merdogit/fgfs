/*
 * Copyright (c) 2007 Ivan Leben
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library in the file COPYING;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define VG_API_EXPORT
#include "vg/openvg.h"
#include "shContext.h"
#include "shPaint.h"
#include <stdio.h>

#define _ITEM_T SHStop
#define _ARRAY_T SHStopArray
#define _FUNC_T shStopArray
#define _COMPARE_T(s1, s2) 0
#define _ARRAY_DEFINE
#include "shArrayBase.h"

#define _ITEM_T SHPaint*
#define _ARRAY_T SHPaintArray
#define _FUNC_T shPaintArray
#define _ARRAY_DEFINE
#include "shArrayBase.h"


void SHPaint_ctor(SHPaint* p)
{
    int i;

    p->type = VG_PAINT_TYPE_COLOR;
    CSET(p->color, 0, 0, 0, 1);
    SH_INITOBJ(SHStopArray, p->instops);
    SH_INITOBJ(SHStopArray, p->stops);
    p->premultiplied = VG_FALSE;
    p->spreadMode = VG_COLOR_RAMP_SPREAD_PAD;
    p->tilingMode = VG_TILE_FILL;
    for (i = 0; i < 4; ++i) p->linearGradient[i] = 0.0f;
    for (i = 0; i < 5; ++i) p->radialGradient[i] = 0.0f;
    p->pattern = VG_INVALID_HANDLE;

    glGenTextures(1, &p->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SH_GRADIENT_TEX_WIDTH, SH_GRADIENT_TEX_HEIGHT, 0,
                 GL_RGBA, GL_FLOAT, NULL);
    GL_CHECK_ERROR;
}

void SHPaint_dtor(SHPaint* p)
{
    SH_DEINITOBJ(SHStopArray, p->instops);
    SH_DEINITOBJ(SHStopArray, p->stops);

    if (glIsTexture(p->texture))
        glDeleteTextures(1, &p->texture);
}

VG_API_CALL VGPaint vgCreatePaint(void)
{
    SHPaint* p = NULL;
    VG_GETCONTEXT(VG_INVALID_HANDLE);

    /* Create new paint object */
    SH_NEWOBJ(SHPaint, p);
    VG_RETURN_ERR_IF(!p, VG_OUT_OF_MEMORY_ERROR,
                     VG_INVALID_HANDLE);

    /* Add to resource list */
    shPaintArrayPushBack(&context->paints, p);

    VG_RETURN((VGPaint)p);
}

VG_API_CALL void vgDestroyPaint(VGPaint paint)
{
    SHint index;
    VG_GETCONTEXT(VG_NO_RETVAL);

    /* Check if handle valid */
    index = shPaintArrayFind(&context->paints, (SHPaint*)paint);
    VG_RETURN_ERR_IF(index == -1, VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

    /* Delete object and remove resource */
    SH_DELETEOBJ(SHPaint, (SHPaint*)paint);
    shPaintArrayRemoveAt(&context->paints, index);

    VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgSetPaint(VGPaint paint, VGbitfield paintModes)
{
    VG_GETCONTEXT(VG_NO_RETVAL);

    /* Check if handle valid */
    VG_RETURN_ERR_IF(!shIsValidPaint(context, paint) &&
                         paint != VG_INVALID_HANDLE,
                     VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

    /* Check for invalid mode */
    VG_RETURN_ERR_IF(paintModes & ~(VG_STROKE_PATH | VG_FILL_PATH),
                     VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);

    /* Set stroke / fill */
    if (paintModes & VG_STROKE_PATH)
        context->strokePaint = (SHPaint*)paint;
    if (paintModes & VG_FILL_PATH)
        context->fillPaint = (SHPaint*)paint;

    VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgPaintPattern(VGPaint paint, VGImage pattern)
{
    VG_GETCONTEXT(VG_NO_RETVAL);

    /* Check if handle valid */
    VG_RETURN_ERR_IF(!shIsValidPaint(context, paint),
                     VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

    /* Check if pattern image valid */
    VG_RETURN_ERR_IF(!shIsValidImage(context, pattern),
                     VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

    /* TODO: Check if pattern image is current rendering target */

    /* Set pattern image */
    ((SHPaint*)paint)->pattern = pattern;

    VG_RETURN(VG_NO_RETVAL);
}

void shUpdateColorRampTexture(SHPaint* p)
{
    SHint s = 0;
    SHStop *stop1, *stop2;
    SHfloat rgba[SH_GRADIENT_TEX_COORDSIZE];
    SHint x1 = 0, x2 = 0, dx, x;
    SHColor dc, c;
    SHfloat k;

    /* Write first pixel color */
    stop1 = &p->stops.items[0];
    CSTORE_RGBA1D_F(stop1->color, rgba, x1);

    /* Walk stops */
    for (s = 1; s < p->stops.size; ++s, x1 = x2, stop1 = stop2) {
        /* Pick next stop */
        stop2 = &p->stops.items[s];
        x2 = (SHint)(stop2->offset * (SH_GRADIENT_TEX_WIDTH - 1));

        SH_ASSERT(x1 >= 0 && x1 < SH_GRADIENT_TEX_WIDTH &&
                  x2 >= 0 && x2 < SH_GRADIENT_TEX_WIDTH &&
                  x1 <= x2);

        dx = x2 - x1;
        CSUBCTO(stop2->color, stop1->color, dc);

        /* Interpolate inbetween */
        for (x = x1 + 1; x <= x2; ++x) {
            k = (SHfloat)(x - x1) / dx;
            CSETC(c, stop1->color);
            CADDCK(c, dc, k);
            CSTORE_RGBA1D_F(c, rgba, x);
        }
    }

    /* Update texture image */
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int i = 0; i < SH_GRADIENT_TEX_HEIGHT; i++)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, SH_GRADIENT_TEX_WIDTH, 1, GL_RGBA, GL_FLOAT, rgba);

    GL_CHECK_ERROR;
}

void shValidateInputStops(SHPaint* p)
{
    SHStop *instop, stop = {0, {0, 0, 0, 0}};
    SHfloat lastOffset = 0.0f;
    int i;

    shStopArrayClear(&p->stops);
    shStopArrayReserve(&p->stops, p->instops.size);

    /* Assure input stops are properly defined */
    for (i = 0; i < p->instops.size; ++i) {
        /* Copy stop color */
        instop = &p->instops.items[i];
        stop.color = instop->color;

        /* Offset must be in [0,1] */
        if (instop->offset < 0.0f || instop->offset > 1.0f)
            continue;

        /* Discard whole sequence if not in ascending order */
        if (instop->offset < lastOffset) {
            shStopArrayClear(&p->stops);
            break;
        }

        /* Add stop at offset 0 with same color if first not at 0 */
        if (p->stops.size == 0 && instop->offset != 0.0f) {
            stop.offset = 0.0f;
            shStopArrayPushBackP(&p->stops, &stop);
        }

        /* Add current stop to array */
        stop.offset = instop->offset;
        shStopArrayPushBackP(&p->stops, &stop);

        /* Save last offset */
        lastOffset = instop->offset;
    }

    /* Add stop at offset 1 with same color if last not at 1 */
    if (p->stops.size > 0 && lastOffset != 1.0f) {
        stop.offset = 1.0f;
        shStopArrayPushBackP(&p->stops, &stop);
    }

    /* Add 2 default stops if no valid found */
    if (p->stops.size == 0) {
        /* First opaque black */
        stop.offset = 0.0f;
        CSET(stop.color, 0, 0, 0, 1);
        shStopArrayPushBackP(&p->stops, &stop);
        /* Last opaque white */
        stop.offset = 1.0f;
        CSET(stop.color, 1, 1, 1, 1);
        shStopArrayPushBackP(&p->stops, &stop);
    }

    /* Update texture */
    shUpdateColorRampTexture(p);
}

void shGenerateStops(SHPaint* p, SHfloat minOffset, SHfloat maxOffset,
                     SHStopArray* outStops)
{
    SHStop *s1, *s2;
    SHint i1, i2;
    SHfloat o = 0.0f;
    SHfloat ostep = 0.0f;
    SHint istep = 1;
    SHint istart = 0;
    SHint iend = p->stops.size - 1;
    SHint minDone = 0;
    SHint maxDone = 0;
    SHStop outStop;

    /* Start below zero? */
    if (minOffset < 0.0f) {
        if (p->spreadMode == VG_COLOR_RAMP_SPREAD_PAD) {
            /* Add min offset stop */
            outStop = p->stops.items[0];
            outStop.offset = minOffset;
            shStopArrayPushBackP(outStops, &outStop);
            /* Add max offset stop and exit */
            if (maxOffset < 0.0f) {
                outStop.offset = maxOffset;
                shStopArrayPushBackP(outStops, &outStop);
                return;
            }
        } else {
            /* Pad starting offset to nearest factor of 2 */
            SHint ioff = (SHint)SH_FLOOR(minOffset);
            o = (SHfloat)(ioff - (ioff & 1));
        }
    }

    /* Construct stops until max offset reached */
    for (i1 = istart, i2 = istart + istep; maxDone != 1;
         i1 += istep, i2 += istep, o += ostep) {
        /* All stops consumed? */
        if (i1 == iend) {
            switch (p->spreadMode) {
            case VG_COLOR_RAMP_SPREAD_PAD:
                /* Pick last stop */
                outStop = p->stops.items[i1];
                if (!minDone) {
                    /* Add min offset stop with last color */
                    outStop.offset = minOffset;
                    shStopArrayPushBackP(outStops, &outStop);
                }
                /* Add max offset stop with last color */
                outStop.offset = maxOffset;
                shStopArrayPushBackP(outStops, &outStop);
                return;

            case VG_COLOR_RAMP_SPREAD_REPEAT:
                /* Reset iteration */
                i1 = istart;
                i2 = istart + istep;
                /* Add stop1 if past min offset */
                if (minDone) {
                    outStop = p->stops.items[0];
                    outStop.offset = o;
                    shStopArrayPushBackP(outStops, &outStop);
                }
                break;

            case VG_COLOR_RAMP_SPREAD_REFLECT:
                /* Reflect iteration direction */
                istep = -istep;
                i2 = i1 + istep;
                iend = (istep == 1) ? p->stops.size - 1 : 0;
                break;
            }
        }

        /* 2 stops and their offset distance */
        s1 = &p->stops.items[i1];
        s2 = &p->stops.items[i2];
        ostep = s2->offset - s1->offset;
        ostep = SH_ABS(ostep);

        /* Add stop1 if reached min offset */
        if (!minDone && o + ostep > minOffset) {
            minDone = 1;
            outStop = *s1;
            outStop.offset = o;
            shStopArrayPushBackP(outStops, &outStop);
        }

        /* Mark done if reached max offset */
        if (o + ostep > maxOffset)
            maxDone = 1;

        /* Add stop2 if past min offset */
        if (minDone) {
            outStop = *s2;
            outStop.offset = o + ostep;
            shStopArrayPushBackP(outStops, &outStop);
        }
    }
}

void shSetGradientTexGLState(SHPaint* p)
{
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    switch (p->spreadMode) {
    case VG_COLOR_RAMP_SPREAD_PAD:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        break;
    case VG_COLOR_RAMP_SPREAD_REPEAT:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        break;
    case VG_COLOR_RAMP_SPREAD_REFLECT:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        break;
    }
}

void shSetPatternTexGLState(SHPaint* p, VGContext* c)
{
    glBindTexture(GL_TEXTURE_2D, ((SHImage*)p->pattern)->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    switch (p->tilingMode) {
    case VG_TILE_FILL:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                         (GLfloat*)&c->tileFillColor);
        break;
    case VG_TILE_PAD:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        break;
    case VG_TILE_REPEAT:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        break;
    case VG_TILE_REFLECT:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        break;
    }
}

int shLoadLinearGradientMesh(SHPaint* p, VGPaintMode mode, VGMatrixMode matrixMode)
{
    SHMatrix3x3* m = 0;
    SHMatrix3x3 mu2p;
    GLfloat u2p[9];

    /* Pick paint transform matrix */
    SH_GETCONTEXT(0);
    if (mode == VG_FILL_PATH)
        m = &context->fillTransform;
    else if (mode == VG_STROKE_PATH)
        m = &context->strokeTransform;

    /* Back to paint space */
    shInvertMatrix(m, &mu2p);
    shMatrixToVG(&mu2p, (SHfloat*)u2p);

    /* Setup shader */
    glUniform1i(context->locationDraw.paintType, VG_PAINT_TYPE_LINEAR_GRADIENT);
    glUniform2fv(context->locationDraw.paintParams, 2, p->linearGradient);
    glUniformMatrix3fv(context->locationDraw.paintInverted, 1, GL_FALSE, u2p);
    glActiveTexture(GL_TEXTURE1);
    shSetGradientTexGLState(p);
    glEnable(GL_TEXTURE_2D);
    glUniform1i(context->locationDraw.rampSampler, 1);
    GL_CHECK_ERROR;

    return 1;
}

int shLoadRadialGradientMesh(SHPaint* p, VGPaintMode mode, VGMatrixMode matrixMode)
{
    SHMatrix3x3* m = 0;
    SHMatrix3x3 mu2p;
    GLfloat u2p[9];

    /* Pick paint transform matrix */
    SH_GETCONTEXT(0);
    if (mode == VG_FILL_PATH)
        m = &context->fillTransform;
    else if (mode == VG_STROKE_PATH)
        m = &context->strokeTransform;

    /* Back to paint space */
    shInvertMatrix(m, &mu2p);
    shMatrixToVG(&mu2p, (SHfloat*)u2p);

    /* Setup shader */
    glUniform1i(context->locationDraw.paintType, VG_PAINT_TYPE_RADIAL_GRADIENT);
    glUniform2fv(context->locationDraw.paintParams, 3, p->radialGradient);
    glUniformMatrix3fv(context->locationDraw.paintInverted, 1, GL_FALSE, u2p);
    glActiveTexture(GL_TEXTURE1);
    shSetGradientTexGLState(p);
    glEnable(GL_TEXTURE_2D);
    glUniform1i(context->locationDraw.rampSampler, 1);
    GL_CHECK_ERROR;

    return 1;
}

int shLoadPatternMesh(SHPaint* p, VGPaintMode mode, VGMatrixMode matrixMode)
{
    SHImage* i = (SHImage*)p->pattern;
    SHMatrix3x3* m = 0;
    SHMatrix3x3 mu2p;
    GLfloat u2p[9];

    /* Pick paint transform matrix */
    SH_GETCONTEXT(0);
    if (mode == VG_FILL_PATH)
        m = &context->fillTransform;
    else if (mode == VG_STROKE_PATH)
        m = &context->strokeTransform;

    /* Back to paint space */
    shInvertMatrix(m, &mu2p);
    shMatrixToVG(&mu2p, (SHfloat*)u2p);

    /* Setup shader */
    glUniform1i(context->locationDraw.paintType, VG_PAINT_TYPE_PATTERN);
    glUniform2f(context->locationDraw.paintParams, (GLfloat)i->width, (GLfloat)i->height);
    glUniformMatrix3fv(context->locationDraw.paintInverted, 1, GL_FALSE, u2p);
    glActiveTexture(GL_TEXTURE1);
    shSetPatternTexGLState(p, context);
    glEnable(GL_TEXTURE_2D);
    glUniform1i(context->locationDraw.patternSampler, 1);
    GL_CHECK_ERROR;

    return 1;
}

int shLoadOneColorMesh(SHPaint* p)
{
    static GLfloat id[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f};
    SH_GETCONTEXT(0);

    /* Setup shader */
    glUniform1i(context->locationDraw.paintType, VG_PAINT_TYPE_COLOR);
    glUniform4fv(context->locationDraw.paintColor, 1, (GLfloat*)&p->color);
    glUniformMatrix3fv(context->locationDraw.paintInverted, 1, GL_FALSE, id);

    return 1;
}
