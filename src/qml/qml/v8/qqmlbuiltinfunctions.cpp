/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlbuiltinfunctions_p.h"

#include <QtQml/qqmlcomponent.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlcomponent_p.h>
#include <private/qqmlloggingcategory_p.h>
#include <private/qqmlstringconverters_p.h>
#include <private/qqmllocale_p.h>
#include <private/qv8engine_p.h>
#include <private/qqmldelayedcallqueue_p.h>
#include <QFileInfo>

#include <private/qqmldebugconnector_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmlglobal_p.h>

#include <private/qqmlplatform_p.h>

#include <private/qv4engine_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4include_p.h>
#include <private/qv4context_p.h>
#include <private/qv4stringobject_p.h>
#include <private/qv4dateobject_p.h>
#include <private/qv4mm_p.h>
#include <private/qv4jsonobject_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4qobjectwrapper_p.h>

#include <QtCore/qstring.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qpoint.h>
#include <QtCore/qurl.h>
#include <QtCore/qfile.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qloggingcategory.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QtObject);

#define THROW_TYPE_ERROR_WITH_MESSAGE(msg) \
    do { \
        scope.result = scope.engine->throwTypeError(QString::fromUtf8(msg)); \
        return; \
    } while (false)

struct StaticQtMetaObject : public QObject
{
    static const QMetaObject *get()
        { return &staticQtMetaObject; }
};

void Heap::QtObject::init(QQmlEngine *qmlEngine)
{
    Heap::Object::init();
    enumeratorIterator = 0;
    keyIterator = 0;
    Scope scope(internalClass->engine);
    ScopedObject o(scope, this);

    {
        ScopedString str(scope);
        ScopedValue v(scope);
        o->put((str = scope.engine->newString(QStringLiteral("Asynchronous"))), (v = QV4::Primitive::fromInt32(0)));
        o->put((str = scope.engine->newString(QStringLiteral("Synchronous"))), (v = QV4::Primitive::fromInt32(1)));
    }

    o->defineDefaultProperty(QStringLiteral("include"), QV4Include::method_include);
    o->defineDefaultProperty(QStringLiteral("isQtObject"), QV4::QtObject::method_isQtObject);
    o->defineDefaultProperty(QStringLiteral("rgba"), QV4::QtObject::method_rgba);
    o->defineDefaultProperty(QStringLiteral("hsla"), QV4::QtObject::method_hsla);
    o->defineDefaultProperty(QStringLiteral("hsva"), QV4::QtObject::method_hsva);
    o->defineDefaultProperty(QStringLiteral("colorEqual"), QV4::QtObject::method_colorEqual);
    o->defineDefaultProperty(QStringLiteral("rect"), QV4::QtObject::method_rect);
    o->defineDefaultProperty(QStringLiteral("point"), QV4::QtObject::method_point);
    o->defineDefaultProperty(QStringLiteral("size"), QV4::QtObject::method_size);
    o->defineDefaultProperty(QStringLiteral("font"), QV4::QtObject::method_font);

    o->defineDefaultProperty(QStringLiteral("vector2d"), QV4::QtObject::method_vector2d);
    o->defineDefaultProperty(QStringLiteral("vector3d"), QV4::QtObject::method_vector3d);
    o->defineDefaultProperty(QStringLiteral("vector4d"), QV4::QtObject::method_vector4d);
    o->defineDefaultProperty(QStringLiteral("quaternion"), QV4::QtObject::method_quaternion);
    o->defineDefaultProperty(QStringLiteral("matrix4x4"), QV4::QtObject::method_matrix4x4);

    o->defineDefaultProperty(QStringLiteral("formatDate"), QV4::QtObject::method_formatDate);
    o->defineDefaultProperty(QStringLiteral("formatTime"), QV4::QtObject::method_formatTime);
    o->defineDefaultProperty(QStringLiteral("formatDateTime"), QV4::QtObject::method_formatDateTime);

    o->defineDefaultProperty(QStringLiteral("openUrlExternally"), QV4::QtObject::method_openUrlExternally);
    o->defineDefaultProperty(QStringLiteral("fontFamilies"), QV4::QtObject::method_fontFamilies);
    o->defineDefaultProperty(QStringLiteral("md5"), QV4::QtObject::method_md5);
    o->defineDefaultProperty(QStringLiteral("btoa"), QV4::QtObject::method_btoa);
    o->defineDefaultProperty(QStringLiteral("atob"), QV4::QtObject::method_atob);
    o->defineDefaultProperty(QStringLiteral("resolvedUrl"), QV4::QtObject::method_resolvedUrl);
    o->defineDefaultProperty(QStringLiteral("locale"), QV4::QtObject::method_locale);
    o->defineDefaultProperty(QStringLiteral("binding"), QV4::QtObject::method_binding);

    if (qmlEngine) {
        o->defineDefaultProperty(QStringLiteral("lighter"), QV4::QtObject::method_lighter);
        o->defineDefaultProperty(QStringLiteral("darker"), QV4::QtObject::method_darker);
        o->defineDefaultProperty(QStringLiteral("tint"), QV4::QtObject::method_tint);
        o->defineDefaultProperty(QStringLiteral("quit"), QV4::QtObject::method_quit);
        o->defineDefaultProperty(QStringLiteral("exit"), QV4::QtObject::method_exit);
        o->defineDefaultProperty(QStringLiteral("createQmlObject"), QV4::QtObject::method_createQmlObject);
        o->defineDefaultProperty(QStringLiteral("createComponent"), QV4::QtObject::method_createComponent);
    }

    o->defineAccessorProperty(QStringLiteral("platform"), QV4::QtObject::method_get_platform, 0);
    o->defineAccessorProperty(QStringLiteral("application"), QV4::QtObject::method_get_application, 0);
    o->defineAccessorProperty(QStringLiteral("inputMethod"), QV4::QtObject::method_get_inputMethod, 0);
    o->defineAccessorProperty(QStringLiteral("styleHints"), QV4::QtObject::method_get_styleHints, 0);

    o->defineDefaultProperty(QStringLiteral("callLater"), QV4::QtObject::method_callLater);
}

void QtObject::addAll()
{
    bool dummy = false;
    findAndAdd(nullptr, dummy);
}

ReturnedValue QtObject::findAndAdd(const QString *name, bool &foundProperty) const
{
    Scope scope(engine());
    ScopedObject o(scope, this);
    ScopedString key(scope);
    ScopedValue value(scope);

    const QMetaObject *qtMetaObject = StaticQtMetaObject::get();
    for (int enumCount = qtMetaObject->enumeratorCount(); d()->enumeratorIterator < enumCount;
         ++d()->enumeratorIterator) {
        QMetaEnum enumerator = qtMetaObject->enumerator(d()->enumeratorIterator);
        for (int keyCount = enumerator.keyCount(); d()->keyIterator < keyCount; ++d()->keyIterator) {
            key = scope.engine->newString(QString::fromUtf8(enumerator.key(d()->keyIterator)));
            value = QV4::Primitive::fromInt32(enumerator.value(d()->keyIterator));
            o->put(key, value);
            if (name && key->toQString() == *name) {
                ++d()->keyIterator;
                foundProperty = true;
                return value->asReturnedValue();
            }
        }
        d()->keyIterator = 0;
    }
    d()->enumeratorIterator = Heap::QtObject::Finished;
    foundProperty = false;
    return Encode::undefined();
}

ReturnedValue QtObject::get(const Managed *m, String *name, bool *hasProperty)
{
    bool hasProp = false;
    if (hasProperty == nullptr) {
        hasProperty = &hasProp;
    }

    ReturnedValue ret = QV4::Object::get(m, name, hasProperty);
    if (*hasProperty) {
        return ret;
    }

    auto that = static_cast<const QtObject*>(m);
    if (!that->d()->isComplete()) {
        const QString key = name->toQString();
        ret = that->findAndAdd(&key, *hasProperty);
    }

    return ret;
}

void QtObject::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attributes)
{
    auto that = static_cast<QtObject*>(m);
    if (!that->d()->isComplete()) {
        that->addAll();
    }

    QV4::Object::advanceIterator(m, it, name, index, p, attributes);
}

/*!
\qmlmethod bool Qt::isQtObject(object)
Returns true if \c object is a valid reference to a Qt or QML object, otherwise false.
*/
void QtObject::method_isQtObject(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc == 0)
        RETURN_RESULT(QV4::Encode(false));

    scope.result = QV4::Encode(callData->args[0].as<QV4::QObjectWrapper>() != 0);
}

/*!
\qmlmethod color Qt::rgba(real red, real green, real blue, real alpha)

Returns a color with the specified \c red, \c green, \c blue and \c alpha components.
All components should be in the range 0-1 inclusive.
*/
void QtObject::method_rgba(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    int argCount = callData->argc;
    if (argCount < 3 || argCount > 4)
        THROW_GENERIC_ERROR("Qt.rgba(): Invalid arguments");

    double r = callData->args[0].toNumber();
    double g = callData->args[1].toNumber();
    double b = callData->args[2].toNumber();
    double a = (argCount == 4) ? callData->args[3].toNumber() : 1;

    if (r < 0.0) r=0.0;
    if (r > 1.0) r=1.0;
    if (g < 0.0) g=0.0;
    if (g > 1.0) g=1.0;
    if (b < 0.0) b=0.0;
    if (b > 1.0) b=1.0;
    if (a < 0.0) a=0.0;
    if (a > 1.0) a=1.0;

    scope.result = scope.engine->fromVariant(QQml_colorProvider()->fromRgbF(r, g, b, a));
}

/*!
\qmlmethod color Qt::hsla(real hue, real saturation, real lightness, real alpha)

Returns a color with the specified \c hue, \c saturation, \c lightness and \c alpha components.
All components should be in the range 0-1 inclusive.
*/
void QtObject::method_hsla(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    int argCount = callData->argc;
    if (argCount < 3 || argCount > 4)
        THROW_GENERIC_ERROR("Qt.hsla(): Invalid arguments");

    double h = callData->args[0].toNumber();
    double s = callData->args[1].toNumber();
    double l = callData->args[2].toNumber();
    double a = (argCount == 4) ? callData->args[3].toNumber() : 1;

    if (h < 0.0) h=0.0;
    if (h > 1.0) h=1.0;
    if (s < 0.0) s=0.0;
    if (s > 1.0) s=1.0;
    if (l < 0.0) l=0.0;
    if (l > 1.0) l=1.0;
    if (a < 0.0) a=0.0;
    if (a > 1.0) a=1.0;

    scope.result = scope.engine->fromVariant(QQml_colorProvider()->fromHslF(h, s, l, a));
}

/*!
\qmlmethod color Qt::hsva(real hue, real saturation, real value, real alpha)

Returns a color with the specified \c hue, \c saturation, \c value and \c alpha components.
All components should be in the range 0-1 inclusive.

\since 5.5
*/
void QtObject::method_hsva(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    int argCount = callData->argc;
    if (argCount < 3 || argCount > 4)
        THROW_GENERIC_ERROR("Qt.hsva(): Invalid arguments");

    double h = callData->args[0].toNumber();
    double s = callData->args[1].toNumber();
    double v = callData->args[2].toNumber();
    double a = (argCount == 4) ? callData->args[3].toNumber() : 1;

    h = qBound(0.0, h, 1.0);
    s = qBound(0.0, s, 1.0);
    v = qBound(0.0, v, 1.0);
    a = qBound(0.0, a, 1.0);

    scope.result = scope.engine->fromVariant(QQml_colorProvider()->fromHsvF(h, s, v, a));
}

/*!
\qmlmethod color Qt::colorEqual(color lhs, string rhs)

Returns true if both \c lhs and \c rhs yield equal color values.  Both arguments
may be either color values or string values.  If a string value is supplied it
must be convertible to a color, as described for the \l{colorbasictypedocs}{color}
basic type.
*/
void QtObject::method_colorEqual(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.colorEqual(): Invalid arguments");

    bool ok = false;

    QVariant lhs = scope.engine->toVariant(callData->args[0], -1);
    if (lhs.userType() == QVariant::String) {
        lhs = QQmlStringConverters::colorFromString(lhs.toString(), &ok);
        if (!ok) {
            THROW_GENERIC_ERROR("Qt.colorEqual(): Invalid color name");
        }
    } else if (lhs.userType() != QVariant::Color) {
        THROW_GENERIC_ERROR("Qt.colorEqual(): Invalid arguments");
    }

    QVariant rhs = scope.engine->toVariant(callData->args[1], -1);
    if (rhs.userType() == QVariant::String) {
        rhs = QQmlStringConverters::colorFromString(rhs.toString(), &ok);
        if (!ok) {
            THROW_GENERIC_ERROR("Qt.colorEqual(): Invalid color name");
        }
    } else if (rhs.userType() != QVariant::Color) {
        THROW_GENERIC_ERROR("Qt.colorEqual(): Invalid arguments");
    }

    bool equal = (lhs == rhs);
    scope.result = QV4::Encode(equal);
}

/*!
\qmlmethod rect Qt::rect(int x, int y, int width, int height)

Returns a \c rect with the top-left corner at \c x, \c y and the specified \c width and \c height.

The returned object has \c x, \c y, \c width and \c height attributes with the given values.
*/
void QtObject::method_rect(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 4)
        THROW_GENERIC_ERROR("Qt.rect(): Invalid arguments");

    double x = callData->args[0].toNumber();
    double y = callData->args[1].toNumber();
    double w = callData->args[2].toNumber();
    double h = callData->args[3].toNumber();

    scope.result = scope.engine->fromVariant(QVariant::fromValue(QRectF(x, y, w, h)));
}

/*!
\qmlmethod point Qt::point(int x, int y)
Returns a Point with the specified \c x and \c y coordinates.
*/
void QtObject::method_point(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.point(): Invalid arguments");

    double x = callData->args[0].toNumber();
    double y = callData->args[1].toNumber();

    scope.result = scope.engine->fromVariant(QVariant::fromValue(QPointF(x, y)));
}

/*!
\qmlmethod Qt::size(int width, int height)
Returns a Size with the specified \c width and \c height.
*/
void QtObject::method_size(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.size(): Invalid arguments");

    double w = callData->args[0].toNumber();
    double h = callData->args[1].toNumber();

    scope.result = scope.engine->fromVariant(QVariant::fromValue(QSizeF(w, h)));
}

/*!
\qmlmethod Qt::font(object fontSpecifier)
Returns a Font with the properties specified in the \c fontSpecifier object
or the nearest matching font.  The \c fontSpecifier object should contain
key-value pairs where valid keys are the \l{fontbasictypedocs}{font} type's
subproperty names, and the values are valid values for each subproperty.
Invalid keys will be ignored.
*/
void QtObject::method_font(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1 || !callData->args[0].isObject())
        THROW_GENERIC_ERROR("Qt.font(): Invalid arguments");

    QV4::ExecutionEngine *v4 = scope.engine;
    bool ok = false;
    QVariant v = QQml_valueTypeProvider()->createVariantFromJsObject(QMetaType::QFont, QQmlV4Handle(callData->args[0]), v4, &ok);
    if (!ok)
        THROW_GENERIC_ERROR("Qt.font(): Invalid argument: no valid font subproperties specified");
    scope.result = scope.engine->fromVariant(v);
}



/*!
\qmlmethod Qt::vector2d(real x, real y)
Returns a Vector2D with the specified \c x and \c y.
*/
void QtObject::method_vector2d(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.vector2d(): Invalid arguments");

    float xy[3]; // qvector2d uses float internally
    xy[0] = callData->args[0].toNumber();
    xy[1] = callData->args[1].toNumber();

    const void *params[] = { xy };
    scope.result = scope.engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QVector2D, 1, params));
}

/*!
\qmlmethod Qt::vector3d(real x, real y, real z)
Returns a Vector3D with the specified \c x, \c y and \c z.
*/
void QtObject::method_vector3d(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 3)
        THROW_GENERIC_ERROR("Qt.vector3d(): Invalid arguments");

    float xyz[3]; // qvector3d uses float internally
    xyz[0] = callData->args[0].toNumber();
    xyz[1] = callData->args[1].toNumber();
    xyz[2] = callData->args[2].toNumber();

    const void *params[] = { xyz };
    scope.result = scope.engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QVector3D, 1, params));
}

/*!
\qmlmethod Qt::vector4d(real x, real y, real z, real w)
Returns a Vector4D with the specified \c x, \c y, \c z and \c w.
*/
void QtObject::method_vector4d(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 4)
        THROW_GENERIC_ERROR("Qt.vector4d(): Invalid arguments");

    float xyzw[4]; // qvector4d uses float internally
    xyzw[0] = callData->args[0].toNumber();
    xyzw[1] = callData->args[1].toNumber();
    xyzw[2] = callData->args[2].toNumber();
    xyzw[3] = callData->args[3].toNumber();

    const void *params[] = { xyzw };
    scope.result = scope.engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QVector4D, 1, params));
}

/*!
\qmlmethod Qt::quaternion(real scalar, real x, real y, real z)
Returns a Quaternion with the specified \c scalar, \c x, \c y, and \c z.
*/
void QtObject::method_quaternion(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 4)
        THROW_GENERIC_ERROR("Qt.quaternion(): Invalid arguments");

    qreal sxyz[4]; // qquaternion uses qreal internally
    sxyz[0] = callData->args[0].toNumber();
    sxyz[1] = callData->args[1].toNumber();
    sxyz[2] = callData->args[2].toNumber();
    sxyz[3] = callData->args[3].toNumber();

    const void *params[] = { sxyz };
    scope.result = scope.engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QQuaternion, 1, params));
}

/*!
\qmlmethod Qt::matrix4x4(real m11, real m12, real m13, real m14, real m21, real m22, real m23, real m24, real m31, real m32, real m33, real m34, real m41, real m42, real m43, real m44)
Returns a Matrix4x4 with the specified values.
Alternatively, the function may be called with a single argument
where that argument is a JavaScript array which contains the sixteen
matrix values.
Finally, the function may be called with no arguments and the resulting
matrix will be the identity matrix.
*/
void QtObject::method_matrix4x4(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QV4::ExecutionEngine *v4 = scope.engine;

    if (callData->argc == 0) {
        scope.result = scope.engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QMatrix4x4, 0, Q_NULLPTR));
        return;
    }

    if (callData->argc == 1 && callData->args[0].isObject()) {
        bool ok = false;
        QVariant v = QQml_valueTypeProvider()->createVariantFromJsObject(QMetaType::QMatrix4x4, QQmlV4Handle(callData->args[0]), v4, &ok);
        if (!ok)
            THROW_GENERIC_ERROR("Qt.matrix4x4(): Invalid argument: not a valid matrix4x4 values array");
        scope.result = scope.engine->fromVariant(v);
        return;
    }

    if (callData->argc != 16)
        THROW_GENERIC_ERROR("Qt.matrix4x4(): Invalid arguments");

    qreal vals[16]; // qmatrix4x4 uses qreal internally
    vals[0] = callData->args[0].toNumber();
    vals[1] = callData->args[1].toNumber();
    vals[2] = callData->args[2].toNumber();
    vals[3] = callData->args[3].toNumber();
    vals[4] = callData->args[4].toNumber();
    vals[5] = callData->args[5].toNumber();
    vals[6] = callData->args[6].toNumber();
    vals[7] = callData->args[7].toNumber();
    vals[8] = callData->args[8].toNumber();
    vals[9] = callData->args[9].toNumber();
    vals[10] = callData->args[10].toNumber();
    vals[11] = callData->args[11].toNumber();
    vals[12] = callData->args[12].toNumber();
    vals[13] = callData->args[13].toNumber();
    vals[14] = callData->args[14].toNumber();
    vals[15] = callData->args[15].toNumber();

    const void *params[] = { vals };
    scope.result = scope.engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QMatrix4x4, 1, params));
}

/*!
\qmlmethod color Qt::lighter(color baseColor, real factor)
Returns a color lighter than \c baseColor by the \c factor provided.

If the factor is greater than 1.0, this functions returns a lighter color.
Setting factor to 1.5 returns a color that is 50% brighter. If the factor is less than 1.0,
the return color is darker, but we recommend using the Qt.darker() function for this purpose.
If the factor is 0 or negative, the return value is unspecified.

The function converts the current RGB color to HSV, multiplies the value (V) component
by factor and converts the color back to RGB.

If \c factor is not supplied, returns a color 50% lighter than \c baseColor (factor 1.5).
*/
void QtObject::method_lighter(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1 && callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.lighter(): Invalid arguments");

    QVariant v = scope.engine->toVariant(callData->args[0], -1);
    if (v.userType() == QVariant::String) {
        bool ok = false;
        v = QQmlStringConverters::colorFromString(v.toString(), &ok);
        if (!ok) {
            scope.result = QV4::Encode::null();
            return;
        }
    } else if (v.userType() != QVariant::Color) {
        scope.result = QV4::Encode::null();
        return;
    }

    qreal factor = 1.5;
    if (callData->argc == 2)
        factor = callData->args[1].toNumber();

    scope.result = scope.engine->fromVariant(QQml_colorProvider()->lighter(v, factor));
}

/*!
\qmlmethod color Qt::darker(color baseColor, real factor)
Returns a color darker than \c baseColor by the \c factor provided.

If the factor is greater than 1.0, this function returns a darker color.
Setting factor to 3.0 returns a color that has one-third the brightness.
If the factor is less than 1.0, the return color is lighter, but we recommend using
the Qt.lighter() function for this purpose. If the factor is 0 or negative, the return
value is unspecified.

The function converts the current RGB color to HSV, divides the value (V) component
by factor and converts the color back to RGB.

If \c factor is not supplied, returns a color 50% darker than \c baseColor (factor 2.0).
*/
void QtObject::method_darker(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1 && callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.darker(): Invalid arguments");

    QVariant v = scope.engine->toVariant(callData->args[0], -1);
    if (v.userType() == QVariant::String) {
        bool ok = false;
        v = QQmlStringConverters::colorFromString(v.toString(), &ok);
        if (!ok) {
            scope.result = QV4::Encode::null();
            return;
        }
    } else if (v.userType() != QVariant::Color) {
        scope.result = QV4::Encode::null();
        return;
    }

    qreal factor = 2.0;
    if (callData->argc == 2)
        factor = callData->args[1].toNumber();

    scope.result = scope.engine->fromVariant(QQml_colorProvider()->darker(v, factor));
}

/*!
    \qmlmethod color Qt::tint(color baseColor, color tintColor)
    This function allows tinting one color with another.

    The tint color should usually be mostly transparent, or you will not be
    able to see the underlying color. The below example provides a slight red
    tint by having the tint color be pure red which is only 1/16th opaque.

    \qml
    Item {
        Rectangle {
            x: 0; width: 80; height: 80
            color: "lightsteelblue"
        }
        Rectangle {
            x: 100; width: 80; height: 80
            color: Qt.tint("lightsteelblue", "#10FF0000")
        }
    }
    \endqml
    \image declarative-rect_tint.png

    Tint is most useful when a subtle change is intended to be conveyed due to some event; you can then use tinting to more effectively tune the visible color.
*/
void QtObject::method_tint(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 2)
        THROW_GENERIC_ERROR("Qt.tint(): Invalid arguments");

    // base color
    QVariant v1 = scope.engine->toVariant(callData->args[0], -1);
    if (v1.userType() == QVariant::String) {
        bool ok = false;
        v1 = QQmlStringConverters::colorFromString(v1.toString(), &ok);
        if (!ok) {
            scope.result = QV4::Encode::null();
            return;
        }
    } else if (v1.userType() != QVariant::Color) {
        scope.result = QV4::Encode::null();
        return;
    }

    // tint color
    QVariant v2 = scope.engine->toVariant(callData->args[1], -1);
    if (v2.userType() == QVariant::String) {
        bool ok = false;
        v2 = QQmlStringConverters::colorFromString(v2.toString(), &ok);
        if (!ok) {
            scope.result = QV4::Encode::null();
            return;
        }
    } else if (v2.userType() != QVariant::Color) {
        scope.result = QV4::Encode::null();
        return;
    }

    scope.result = scope.engine->fromVariant(QQml_colorProvider()->tint(v1, v2));
}

/*!
\qmlmethod string Qt::formatDate(datetime date, variant format)

Returns a string representation of \c date, optionally formatted according
to \c format.

The \a date parameter may be a JavaScript \c Date object, a \l{date}{date}
property, a QDate, or QDateTime value. The \a format parameter may be any of
the possible format values as described for
\l{QtQml::Qt::formatDateTime()}{Qt.formatDateTime()}.

If \a format is not specified, \a date is formatted using
\l {Qt::DefaultLocaleShortDate}{Qt.DefaultLocaleShortDate}.

\sa Locale
*/
void QtObject::method_formatDate(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1 || callData->argc > 2)
        THROW_GENERIC_ERROR("Qt.formatDate(): Invalid arguments");

    Qt::DateFormat enumFormat = Qt::DefaultLocaleShortDate;
    QDate date = scope.engine->toVariant(callData->args[0], -1).toDateTime().date();
    QString formattedDate;
    if (callData->argc == 2) {
        QV4::ScopedString s(scope, callData->args[1]);
        if (s) {
            QString format = s->toQString();
            formattedDate = date.toString(format);
        } else if (callData->args[1].isNumber()) {
            quint32 intFormat = callData->args[1].asDouble();
            Qt::DateFormat format = Qt::DateFormat(intFormat);
            formattedDate = date.toString(format);
        } else {
            THROW_GENERIC_ERROR("Qt.formatDate(): Invalid date format");
        }
    } else {
         formattedDate = date.toString(enumFormat);
    }

    scope.result = scope.engine->newString(formattedDate);
}

/*!
\qmlmethod string Qt::formatTime(datetime time, variant format)

Returns a string representation of \c time, optionally formatted according to
\c format.

The \a time parameter may be a JavaScript \c Date object, a QTime, or QDateTime
value. The \a format parameter may be any of the possible format values as
described for \l{QtQml::Qt::formatDateTime()}{Qt.formatDateTime()}.

If \a format is not specified, \a time is formatted using
\l {Qt::DefaultLocaleShortDate}{Qt.DefaultLocaleShortDate}.

\sa Locale
*/
void QtObject::method_formatTime(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1 || callData->argc > 2)
        THROW_GENERIC_ERROR("Qt.formatTime(): Invalid arguments");

    QVariant argVariant = scope.engine->toVariant(callData->args[0], -1);
    QTime time;
    if (callData->args[0].as<DateObject>() || (argVariant.type() == QVariant::String))
        time = argVariant.toDateTime().time();
    else // if (argVariant.type() == QVariant::Time), or invalid.
        time = argVariant.toTime();

    Qt::DateFormat enumFormat = Qt::DefaultLocaleShortDate;
    QString formattedTime;
    if (callData->argc == 2) {
        QV4::ScopedString s(scope, callData->args[1]);
        if (s) {
            QString format = s->toQString();
            formattedTime = time.toString(format);
        } else if (callData->args[1].isNumber()) {
            quint32 intFormat = callData->args[1].asDouble();
            Qt::DateFormat format = Qt::DateFormat(intFormat);
            formattedTime = time.toString(format);
        } else {
            THROW_GENERIC_ERROR("Qt.formatTime(): Invalid time format");
        }
    } else {
         formattedTime = time.toString(enumFormat);
    }

    scope.result = scope.engine->newString(formattedTime);
}

/*!
\qmlmethod string Qt::formatDateTime(datetime dateTime, variant format)

Returns a string representation of \c datetime, optionally formatted according to
\c format.

The \a date parameter may be a JavaScript \c Date object, a \l{date}{date}
property, a QDate, QTime, or QDateTime value.

If \a format is not provided, \a dateTime is formatted using
\l {Qt::DefaultLocaleShortDate}{Qt.DefaultLocaleShortDate}. Otherwise,
\a format should be either:

\list
\li One of the Qt::DateFormat enumeration values, such as
   \c Qt.DefaultLocaleShortDate or \c Qt.ISODate
\li A string that specifies the format of the returned string, as detailed below.
\endlist

If \a format specifies a format string, it should use the following expressions
to specify the date:

    \table
    \header \li Expression \li Output
    \row \li d \li the day as number without a leading zero (1 to 31)
    \row \li dd \li the day as number with a leading zero (01 to 31)
    \row \li ddd
            \li the abbreviated localized day name (e.g. 'Mon' to 'Sun').
            Uses QDate::shortDayName().
    \row \li dddd
            \li the long localized day name (e.g. 'Monday' to 'Qt::Sunday').
            Uses QDate::longDayName().
    \row \li M \li the month as number without a leading zero (1-12)
    \row \li MM \li the month as number with a leading zero (01-12)
    \row \li MMM
            \li the abbreviated localized month name (e.g. 'Jan' to 'Dec').
            Uses QDate::shortMonthName().
    \row \li MMMM
            \li the long localized month name (e.g. 'January' to 'December').
            Uses QDate::longMonthName().
    \row \li yy \li the year as two digit number (00-99)
    \row \li yyyy \li the year as four digit number
    \endtable

In addition the following expressions can be used to specify the time:

    \table
    \header \li Expression \li Output
    \row \li h
         \li the hour without a leading zero (0 to 23 or 1 to 12 if AM/PM display)
    \row \li hh
         \li the hour with a leading zero (00 to 23 or 01 to 12 if AM/PM display)
    \row \li m \li the minute without a leading zero (0 to 59)
    \row \li mm \li the minute with a leading zero (00 to 59)
    \row \li s \li the second without a leading zero (0 to 59)
    \row \li ss \li the second with a leading zero (00 to 59)
    \row \li z \li the milliseconds without leading zeroes (0 to 999)
    \row \li zzz \li the milliseconds with leading zeroes (000 to 999)
    \row \li AP
            \li use AM/PM display. \e AP will be replaced by either "AM" or "PM".
    \row \li ap
            \li use am/pm display. \e ap will be replaced by either "am" or "pm".
    \endtable

    All other input characters will be ignored. Any sequence of characters that
    are enclosed in single quotes will be treated as text and not be used as an
    expression. Two consecutive single quotes ("''") are replaced by a single quote
    in the output.

For example, if the following date/time value was specified:

    \code
    // 21 May 2001 14:13:09
    var dateTime = new Date(2001, 5, 21, 14, 13, 09)
    \endcode

This \a dateTime value could be passed to \c Qt.formatDateTime(),
\l {QtQml::Qt::formatDate()}{Qt.formatDate()} or \l {QtQml::Qt::formatTime()}{Qt.formatTime()}
with the \a format values below to produce the following results:

    \table
    \header \li Format \li Result
    \row \li "dd.MM.yyyy"      \li 21.05.2001
    \row \li "ddd MMMM d yy"   \li Tue May 21 01
    \row \li "hh:mm:ss.zzz"    \li 14:13:09.042
    \row \li "h:m:s ap"        \li 2:13:9 pm
    \endtable

    \sa Locale
*/
void QtObject::method_formatDateTime(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1 || callData->argc > 2)
        THROW_GENERIC_ERROR("Qt.formatDateTime(): Invalid arguments");

    Qt::DateFormat enumFormat = Qt::DefaultLocaleShortDate;
    QDateTime dt = scope.engine->toVariant(callData->args[0], -1).toDateTime();
    QString formattedDt;
    if (callData->argc == 2) {
        QV4::ScopedString s(scope, callData->args[1]);
        if (s) {
            QString format = s->toQString();
            formattedDt = dt.toString(format);
        } else if (callData->args[1].isNumber()) {
            quint32 intFormat = callData->args[1].asDouble();
            Qt::DateFormat format = Qt::DateFormat(intFormat);
            formattedDt = dt.toString(format);
        } else {
            THROW_GENERIC_ERROR("Qt.formatDateTime(): Invalid datetime format");
        }
    } else {
         formattedDt = dt.toString(enumFormat);
    }

    scope.result = scope.engine->newString(formattedDt);
}

/*!
    \qmlmethod bool Qt::openUrlExternally(url target)

    Attempts to open the specified \c target url in an external application, based on the user's
    desktop preferences. Returns true if it succeeds, and false otherwise.

    \warning A return value of \c true indicates that the application has successfully requested
    the operating system to open the URL in an external application. The external application may
    still fail to launch or fail to open the requested URL. This result will not be reported back
    to the application.
*/
void QtObject::method_openUrlExternally(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    if (callData->argc != 1) {
        scope.result = QV4::Encode(false);
        return;
    }

    method_resolvedUrl(b, scope, callData);
    QUrl url(scope.result.toQStringNoThrow());
    scope.result = scope.engine->fromVariant(QQml_guiProvider()->openUrlExternally(url));
}

/*!
  \qmlmethod url Qt::resolvedUrl(url url)
  Returns \a url resolved relative to the URL of the caller.
*/
void QtObject::method_resolvedUrl(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = scope.engine;

    QUrl url = v4->toVariant(callData->args[0], -1).toUrl();
    QQmlEngine *e = v4->qmlEngine();
    QQmlEnginePrivate *p = 0;
    if (e) p = QQmlEnginePrivate::get(e);
    if (p) {
        QQmlContextData *ctxt = v4->callingQmlContext();
        if (ctxt)
            scope.result = v4->newString(ctxt->resolvedUrl(url).toString());
        else
            scope.result = v4->newString(url.toString());
        return;
    }

    scope.result = v4->newString(e->baseUrl().resolved(url).toString());
}

/*!
\qmlmethod list<string> Qt::fontFamilies()
Returns a list of the font families available to the application.
*/
void QtObject::method_fontFamilies(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 0)
        THROW_GENERIC_ERROR("Qt.fontFamilies(): Invalid arguments");

    scope.result = scope.engine->fromVariant(QVariant(QQml_guiProvider()->fontFamilies()));
}

/*!
\qmlmethod string Qt::md5(data)
Returns a hex string of the md5 hash of \c data.
*/
void QtObject::method_md5(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("Qt.md5(): Invalid arguments");

    QByteArray data = callData->args[0].toQStringNoThrow().toUtf8();
    QByteArray result = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    scope.result = scope.engine->newString(QLatin1String(result.toHex()));
}

/*!
\qmlmethod string Qt::btoa(data)
Binary to ASCII - this function returns a base64 encoding of \c data.
*/
void QtObject::method_btoa(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("Qt.btoa(): Invalid arguments");

    QByteArray data = callData->args[0].toQStringNoThrow().toUtf8();

    scope.result = scope.engine->newString(QLatin1String(data.toBase64()));
}

/*!
\qmlmethod string Qt::atob(data)
ASCII to binary - this function decodes the base64 encoded \a data string and returns it.
*/
void QtObject::method_atob(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("Qt.atob(): Invalid arguments");

    QByteArray data = callData->args[0].toQStringNoThrow().toLatin1();

    scope.result = scope.engine->newString(QString::fromUtf8(QByteArray::fromBase64(data)));
}

/*!
\qmlmethod Qt::quit()
This function causes the QQmlEngine::quit() signal to be emitted.
Within the \l {Prototyping with qmlscene}, this causes the launcher application to exit;
to quit a C++ application when this method is called, connect the
QQmlEngine::quit() signal to the QCoreApplication::quit() slot.

\sa exit()
*/
void QtObject::method_quit(const BuiltinFunction *, Scope &scope, CallData *)
{
    QQmlEnginePrivate::get(scope.engine->qmlEngine())->sendQuit();
    scope.result = Encode::undefined();
}

/*!
    \qmlmethod Qt::exit(int retCode)

    This function causes the QQmlEngine::exit(int) signal to be emitted.
    Within the \l {Prototyping with qmlscene}, this causes the launcher application to exit
    the specified return code. To exit from the event loop with a specified return code when this
    method is called, a C++ application can connect the QQmlEngine::exit(int) signal
    to the QCoreApplication::exit(int) slot.

    \sa quit()
*/
void QtObject::method_exit(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("Qt.exit(): Invalid arguments");

    int retCode = callData->args[0].toNumber();

    QQmlEnginePrivate::get(scope.engine->qmlEngine())->sendExit(retCode);
    scope.result = QV4::Encode::undefined();
}

/*!
\qmlmethod object Qt::createQmlObject(string qml, object parent, string filepath)

Returns a new object created from the given \a qml string which will have the specified \a parent,
or \c null if there was an error in creating the object.

If \a filepath is specified, it will be used for error reporting for the created object.

Example (where \c parentItem is the id of an existing QML item):

\snippet qml/createQmlObject.qml 0

In the case of an error, a \l {Qt Script} Error object is thrown. This object has an additional property,
\c qmlErrors, which is an array of the errors encountered.
Each object in this array has the members \c lineNumber, \c columnNumber, \c fileName and \c message.
For example, if the above snippet had misspelled color as 'colro' then the array would contain an object like the following:
{ "lineNumber" : 1, "columnNumber" : 32, "fileName" : "dynamicSnippet1", "message" : "Cannot assign to non-existent property \"colro\""}.

Note that this function returns immediately, and therefore may not work if
the \a qml string loads new components (that is, external QML files that have not yet been loaded).
If this is the case, consider using \l{QtQml::Qt::createComponent()}{Qt.createComponent()} instead.

See \l {Dynamic QML Object Creation from JavaScript} for more information on using this function.
*/
void QtObject::method_createQmlObject(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 2 || callData->argc > 3)
        THROW_GENERIC_ERROR("Qt.createQmlObject(): Invalid arguments");

    struct Error {
        static ReturnedValue create(QV4::ExecutionEngine *v4, const QList<QQmlError> &errors) {
            Scope scope(v4);
            QString errorstr;
            // '+=' reserves extra capacity. Follow-up appending will be probably free.
            errorstr += QLatin1String("Qt.createQmlObject(): failed to create object: ");

            QV4::ScopedArrayObject qmlerrors(scope, v4->newArrayObject());
            QV4::ScopedObject qmlerror(scope);
            QV4::ScopedString s(scope);
            QV4::ScopedValue v(scope);
            for (int ii = 0; ii < errors.count(); ++ii) {
                const QQmlError &error = errors.at(ii);
                errorstr += QLatin1String("\n    ") + error.toString();
                qmlerror = v4->newObject();
                qmlerror->put((s = v4->newString(QStringLiteral("lineNumber"))), (v = QV4::Primitive::fromInt32(error.line())));
                qmlerror->put((s = v4->newString(QStringLiteral("columnNumber"))), (v = QV4::Primitive::fromInt32(error.column())));
                qmlerror->put((s = v4->newString(QStringLiteral("fileName"))), (v = v4->newString(error.url().toString())));
                qmlerror->put((s = v4->newString(QStringLiteral("message"))), (v = v4->newString(error.description())));
                qmlerrors->putIndexed(ii, qmlerror);
            }

            v = v4->newString(errorstr);
            ScopedObject errorObject(scope, v4->newErrorObject(v));
            errorObject->put((s = v4->newString(QStringLiteral("qmlErrors"))), qmlerrors);
            return errorObject.asReturnedValue();
        }
    };

    QV8Engine *v8engine = scope.engine->v8Engine;
    QQmlEngine *engine = v8engine->engine();

    QQmlContextData *context = scope.engine->callingQmlContext();
    Q_ASSERT(context);
    QQmlContext *effectiveContext = 0;
    if (context->isPragmaLibraryContext)
        effectiveContext = engine->rootContext();
    else
        effectiveContext = context->asQQmlContext();
    Q_ASSERT(effectiveContext);

    QString qml = callData->args[0].toQStringNoThrow();
    if (qml.isEmpty())
        RETURN_RESULT(Encode::null());

    QUrl url;
    if (callData->argc > 2)
        url = QUrl(callData->args[2].toQStringNoThrow());
    else
        url = QUrl(QLatin1String("inline"));

    if (url.isValid() && url.isRelative())
        url = context->resolvedUrl(url);

    QObject *parentArg = 0;
    QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, callData->args[1]);
    if (!!qobjectWrapper)
        parentArg = qobjectWrapper->object();
    if (!parentArg)
        THROW_GENERIC_ERROR("Qt.createQmlObject(): Missing parent object");

    QQmlTypeData *typeData = QQmlEnginePrivate::get(engine)->typeLoader.getType(
                qml.toUtf8(), url, QQmlTypeLoader::Synchronous);
    Q_ASSERT(typeData->isCompleteOrError());
    QQmlComponent component(engine);
    QQmlComponentPrivate *componentPrivate = QQmlComponentPrivate::get(&component);
    componentPrivate->fromTypeData(typeData);
    componentPrivate->progress = 1.0;

    if (component.isError()) {
        ScopedValue v(scope, Error::create(scope.engine, component.errors()));
        RETURN_RESULT(scope.engine->throwError(v));
    }

    if (!component.isReady())
        THROW_GENERIC_ERROR("Qt.createQmlObject(): Component is not ready");

    QObject *obj = component.beginCreate(effectiveContext);
    if (obj) {
        QQmlData::get(obj, true)->explicitIndestructibleSet = false;
        QQmlData::get(obj)->indestructible = false;


        obj->setParent(parentArg);

        QList<QQmlPrivate::AutoParentFunction> functions = QQmlMetaType::parentFunctions();
        for (int ii = 0; ii < functions.count(); ++ii) {
            if (QQmlPrivate::Parented == functions.at(ii)(obj, parentArg))
                break;
        }
    }
    component.completeCreate();

    if (component.isError()) {
        ScopedValue v(scope, Error::create(scope.engine, component.errors()));
        scope.result = scope.engine->throwError(v);
        return;
    }

    Q_ASSERT(obj);

    scope.result = QV4::QObjectWrapper::wrap(scope.engine, obj);
}

/*!
\qmlmethod object Qt::createComponent(url, mode, parent)

Returns a \l Component object created using the QML file at the specified \a url,
or \c null if an empty string was given.

The returned component's \l Component::status property indicates whether the
component was successfully created. If the status is \c Component.Error,
see \l Component::errorString() for an error description.

If the optional \a mode parameter is set to \c Component.Asynchronous, the
component will be loaded in a background thread.  The Component::status property
will be \c Component.Loading while it is loading.  The status will change to
\c Component.Ready if the component loads successfully, or \c Component.Error
if loading fails. This parameter defaults to \c Component.PreferSynchronous
if omitted.

If \a mode is set to \c Component.PreferSynchronous, Qt will attempt to load
the component synchronously, but may end up loading it asynchronously if
necessary. Scenarios that may cause asynchronous loading include, but are not
limited to, the following:

\list
\li The URL refers to a network resource
\li The component is being created as a result of another component that is
being loaded asynchronously
\endlist

If the optional \a parent parameter is given, it should refer to the object
that will become the parent for the created \l Component object. If no mode
was passed, this can be the second argument.

Call \l {Component::createObject()}{Component.createObject()} on the returned
component to create an object instance of the component.

For example:

\snippet qml/createComponent-simple.qml 0

See \l {Dynamic QML Object Creation from JavaScript} for more information on using this function.

To create a QML object from an arbitrary string of QML (instead of a file),
use \l{QtQml::Qt::createQmlObject()}{Qt.createQmlObject()}.
*/
void QtObject::method_createComponent(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1 || callData->argc > 3)
        THROW_GENERIC_ERROR("Qt.createComponent(): Invalid arguments");

    QV8Engine *v8engine = scope.engine->v8Engine;
    QQmlEngine *engine = v8engine->engine();

    QQmlContextData *context = scope.engine->callingQmlContext();
    Q_ASSERT(context);
    QQmlContextData *effectiveContext = context;
    if (context->isPragmaLibraryContext)
        effectiveContext = 0;

    QString arg = callData->args[0].toQStringNoThrow();
    if (arg.isEmpty())
        RETURN_RESULT(QV4::Encode::null());

    QQmlComponent::CompilationMode compileMode = QQmlComponent::PreferSynchronous;
    QObject *parentArg = 0;

    int consumedCount = 1;
    if (callData->argc > 1) {
        ScopedValue lastArg(scope, callData->args[callData->argc-1]);

        // The second argument could be the mode enum
        if (callData->args[1].isInteger()) {
            int mode = callData->args[1].integerValue();
            if (mode != int(QQmlComponent::PreferSynchronous) && mode != int(QQmlComponent::Asynchronous))
                THROW_GENERIC_ERROR("Qt.createComponent(): Invalid arguments");
            compileMode = QQmlComponent::CompilationMode(mode);
            consumedCount += 1;
        } else {
            // The second argument could be the parent only if there are exactly two args
            if ((callData->argc != 2) || !(lastArg->isObject() || lastArg->isNull()))
                THROW_GENERIC_ERROR("Qt.createComponent(): Invalid arguments");
        }

        if (consumedCount < callData->argc) {
            if (lastArg->isObject()) {
                Scoped<QObjectWrapper> qobjectWrapper(scope, lastArg);
                if (qobjectWrapper)
                    parentArg = qobjectWrapper->object();
                if (!parentArg)
                    THROW_GENERIC_ERROR("Qt.createComponent(): Invalid parent object");
            } else if (lastArg->isNull()) {
                parentArg = 0;
            } else {
                THROW_GENERIC_ERROR("Qt.createComponent(): Invalid parent object");
            }
        }
    }

    QUrl url = context->resolvedUrl(QUrl(arg));
    QQmlComponent *c = new QQmlComponent(engine, url, compileMode, parentArg);
    QQmlComponentPrivate::get(c)->creationContext = effectiveContext;
    QQmlData::get(c, true)->explicitIndestructibleSet = false;
    QQmlData::get(c)->indestructible = false;

    scope.result = QV4::QObjectWrapper::wrap(scope.engine, c);
}

/*!
    \qmlmethod Qt::locale(name)

    Returns a JS object representing the locale with the specified
    name, which has the format "language[_territory][.codeset][@modifier]"
    or "C", where:

    \list
    \li language is a lowercase, two-letter, ISO 639 language code,
    \li territory is an uppercase, two-letter, ISO 3166 country code,
    \li and codeset and modifier are ignored.
    \endlist

    If the string violates the locale format, or language is not a
    valid ISO 369 code, the "C" locale is used instead. If country
    is not present, or is not a valid ISO 3166 code, the most
    appropriate country is chosen for the specified language.

    \sa Locale
*/
void QtObject::method_locale(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString code;
    if (callData->argc > 1)
        THROW_GENERIC_ERROR("locale() requires 0 or 1 argument");
    if (callData->argc == 1 && !callData->args[0].isString())
        THROW_TYPE_ERROR_WITH_MESSAGE("locale(): argument (locale code) must be a string");

    if (callData->argc == 1)
        code = callData->args[0].toQStringNoThrow();

    scope.result = QQmlLocale::locale(scope.engine, code);
}

void Heap::QQmlBindingFunction::init(const QV4::FunctionObject *originalFunction)
{
    Scope scope(originalFunction->engine());
    ScopedContext context(scope, originalFunction->scope());
    FunctionObject::init(context, originalFunction->function());
}

QQmlSourceLocation QQmlBindingFunction::currentLocation() const
{
    QV4::StackFrame frame = engine()->currentStackFrame();
    return QQmlSourceLocation(frame.source, frame.line, 0);
}

DEFINE_OBJECT_VTABLE(QQmlBindingFunction);

/*!
    \qmlmethod Qt::binding(function)

    Returns a JavaScript object representing a \l{Property Binding}{property binding}.

    There are two main use-cases for the function: firstly, to apply a
    property binding imperatively from JavaScript code:

    \snippet qml/qtBinding.1.qml 0

    and secondly, to apply a property binding when initializing property values
    of dynamically constructed objects (via \l{Component::createObject()}
    {Component.createObject()} or \l{Loader::setSource()}{Loader.setSource()}).

    For example, assuming the existence of a DynamicText component:
    \snippet qml/DynamicText.qml 0

    the output from:
    \snippet qml/qtBinding.2.qml 0

    and from:
    \snippet qml/qtBinding.3.qml 0

    should both be:
    \code
    Root text extra text
    Modified root text extra text
    Dynamic text extra text
    Modified dynamic text extra text
    \endcode

    This function cannot be used in property binding declarations
    (see the documentation on \l{qml-javascript-assignment}{binding
    declarations and binding assignments}) except when the result is
    stored in an array bound to a var property.

    \snippet qml/qtBinding.4.qml 0

    \note In \l {Qt Quick 1}, all function assignments were treated as
    binding assignments. The Qt.binding() function is new to
    \l {Qt Quick}{Qt Quick 2}.

    \since 5.0
*/
void QtObject::method_binding(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("binding() requires 1 argument");
    const QV4::FunctionObject *f = callData->args[0].as<FunctionObject>();
    if (!f)
        THROW_TYPE_ERROR_WITH_MESSAGE("binding(): argument (binding expression) must be a function");

    scope.result = scope.engine->memoryManager->allocObject<QQmlBindingFunction>(f);
}


void QtObject::method_get_platform(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    // ### inefficient. Should be just a value based getter
    Object *o = callData->thisObject.as<Object>();
    if (!o)
        THROW_TYPE_ERROR();
    QtObject *qt = o->as<QtObject>();
    if (!qt)
        THROW_TYPE_ERROR();

    if (!qt->d()->platform)
        // Only allocate a platform object once
        qt->d()->platform = new QQmlPlatform(scope.engine->jsEngine());

    scope.result = QV4::QObjectWrapper::wrap(scope.engine, qt->d()->platform);
}

void QtObject::method_get_application(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    // ### inefficient. Should be just a value based getter
    Object *o = callData->thisObject.as<Object>();
    if (!o)
        THROW_TYPE_ERROR();
    QtObject *qt = o->as<QtObject>();
    if (!qt)
        THROW_TYPE_ERROR();

    if (!qt->d()->application)
        // Only allocate an application object once
        qt->d()->application = QQml_guiProvider()->application(scope.engine->jsEngine());

    scope.result = QV4::QObjectWrapper::wrap(scope.engine, qt->d()->application);
}

void QtObject::method_get_inputMethod(const BuiltinFunction *, Scope &scope, CallData *)
{
    QObject *o = QQml_guiProvider()->inputMethod();
    scope.result = QV4::QObjectWrapper::wrap(scope.engine, o);
}

void QtObject::method_get_styleHints(const BuiltinFunction *, Scope &scope, CallData *)
{
    QObject *o = QQml_guiProvider()->styleHints();
    scope.result = QV4::QObjectWrapper::wrap(scope.engine, o);
}


void QV4::Heap::ConsoleObject::init()
{
    Object::init();
    QV4::Scope scope(internalClass->engine);
    QV4::ScopedObject o(scope, this);

    o->defineDefaultProperty(QStringLiteral("debug"), QV4::ConsoleObject::method_log);
    o->defineDefaultProperty(QStringLiteral("log"), QV4::ConsoleObject::method_log);
    o->defineDefaultProperty(QStringLiteral("info"), QV4::ConsoleObject::method_info);
    o->defineDefaultProperty(QStringLiteral("warn"), QV4::ConsoleObject::method_warn);
    o->defineDefaultProperty(QStringLiteral("error"), QV4::ConsoleObject::method_error);
    o->defineDefaultProperty(QStringLiteral("assert"), QV4::ConsoleObject::method_assert);

    o->defineDefaultProperty(QStringLiteral("count"), QV4::ConsoleObject::method_count);
    o->defineDefaultProperty(QStringLiteral("profile"), QV4::ConsoleObject::method_profile);
    o->defineDefaultProperty(QStringLiteral("profileEnd"), QV4::ConsoleObject::method_profileEnd);
    o->defineDefaultProperty(QStringLiteral("time"), QV4::ConsoleObject::method_time);
    o->defineDefaultProperty(QStringLiteral("timeEnd"), QV4::ConsoleObject::method_timeEnd);
    o->defineDefaultProperty(QStringLiteral("trace"), QV4::ConsoleObject::method_trace);
    o->defineDefaultProperty(QStringLiteral("exception"), QV4::ConsoleObject::method_exception);
}


enum ConsoleLogTypes {
    Log,
    Info,
    Warn,
    Error
};

static QString jsStack(QV4::ExecutionEngine *engine) {
    QString stack;

    QVector<QV4::StackFrame> stackTrace = engine->stackTrace(10);

    for (int i = 0; i < stackTrace.count(); i++) {
        const QV4::StackFrame &frame = stackTrace.at(i);

        QString stackFrame;
        if (frame.column >= 0)
            stackFrame = QStringLiteral("%1 (%2:%3:%4)").arg(frame.function,
                                                             frame.source,
                                                             QString::number(frame.line),
                                                             QString::number(frame.column));
        else
            stackFrame = QStringLiteral("%1 (%2:%3)").arg(frame.function,
                                                             frame.source,
                                                             QString::number(frame.line));

        if (i)
            stack += QLatin1Char('\n');
        stack += stackFrame;
    }
    return stack;
}

static void writeToConsole(const BuiltinFunction *, Scope &scope, CallData *callData,
                                         ConsoleLogTypes logType, bool printStack = false)
{
    QLoggingCategory *loggingCategory = 0;
    QString result;
    QV4::ExecutionEngine *v4 = scope.engine;

    int start = 0;
    if (callData->argc > 0) {
        if (const QObjectWrapper* wrapper = callData->args[0].as<QObjectWrapper>()) {
            if (QQmlLoggingCategory* category = qobject_cast<QQmlLoggingCategory*>(wrapper->object())) {
                if (category->category())
                    loggingCategory = category->category();
                else
                    THROW_GENERIC_ERROR("A QmlLoggingCatgory was provided without a valid name");
                start = 1;
            }
        }
    }


    for (int i = start; i < callData->argc; ++i) {
        if (i != start)
            result.append(QLatin1Char(' '));

        if (callData->args[i].as<ArrayObject>())
            result += QLatin1Char('[') + callData->args[i].toQStringNoThrow() + QLatin1Char(']');
        else
            result.append(callData->args[i].toQStringNoThrow());
    }

    if (printStack)
        result += QLatin1Char('\n') + jsStack(v4);

    static QLoggingCategory qmlLoggingCategory("qml");
    static QLoggingCategory jsLoggingCategory("js");

    if (!loggingCategory)
        loggingCategory = v4->qmlEngine() ? &qmlLoggingCategory : &jsLoggingCategory;
    QV4::StackFrame frame = v4->currentStackFrame();
    const QByteArray baSource = frame.source.toUtf8();
    const QByteArray baFunction = frame.function.toUtf8();
    QMessageLogger logger(baSource.constData(), frame.line, baFunction.constData(), loggingCategory->categoryName());

    switch (logType) {
    case Log:
        if (loggingCategory->isDebugEnabled())
            logger.debug("%s", result.toUtf8().constData());
        break;
    case Info:
        if (loggingCategory->isInfoEnabled())
            logger.info("%s", result.toUtf8().constData());
        break;
    case Warn:
        if (loggingCategory->isWarningEnabled())
            logger.warning("%s", result.toUtf8().constData());
        break;
    case Error:
        if (loggingCategory->isCriticalEnabled())
            logger.critical("%s", result.toUtf8().constData());
        break;
    default:
        break;
    }

    scope.result = QV4::Encode::undefined();
}

DEFINE_OBJECT_VTABLE(ConsoleObject);

void ConsoleObject::method_error(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    writeToConsole(b, scope, callData, Error);
}

void ConsoleObject::method_log(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    //console.log
    //console.debug
    //print
    writeToConsole(b, scope, callData, Log);
}

void ConsoleObject::method_info(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    writeToConsole(b, scope, callData, Info);
}

void ConsoleObject::method_profile(const BuiltinFunction *, Scope &scope, CallData *)
{
    QV4::ExecutionEngine *v4 = scope.engine;

    QV4::StackFrame frame = v4->currentStackFrame();
    const QByteArray baSource = frame.source.toUtf8();
    const QByteArray baFunction = frame.function.toUtf8();
    QMessageLogger logger(baSource.constData(), frame.line, baFunction.constData());
    QQmlProfilerService *service = QQmlDebugConnector::service<QQmlProfilerService>();
    if (!service) {
        logger.warning("Cannot start profiling because debug service is disabled. Start with -qmljsdebugger=port:XXXXX.");
    } else {
        service->startProfiling(v4->jsEngine());
        logger.debug("Profiling started.");
    }

    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_profileEnd(const BuiltinFunction *, Scope &scope, CallData *)
{
    QV4::ExecutionEngine *v4 = scope.engine;

    QV4::StackFrame frame = v4->currentStackFrame();
    const QByteArray baSource = frame.source.toUtf8();
    const QByteArray baFunction = frame.function.toUtf8();
    QMessageLogger logger(baSource.constData(), frame.line, baFunction.constData());

    QQmlProfilerService *service = QQmlDebugConnector::service<QQmlProfilerService>();
    if (!service) {
        logger.warning("Ignoring console.profileEnd(): the debug service is disabled.");
    } else {
        service->stopProfiling(v4->jsEngine());
        logger.debug("Profiling ended.");
    }

    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_time(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("console.time(): Invalid arguments");

    QV8Engine *v8engine = scope.engine->v8Engine;

    QString name = callData->args[0].toQStringNoThrow();
    v8engine->startTimer(name);
    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_timeEnd(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("console.timeEnd(): Invalid arguments");

    QV8Engine *v8engine = scope.engine->v8Engine;

    QString name = callData->args[0].toQStringNoThrow();
    bool wasRunning;
    qint64 elapsed = v8engine->stopTimer(name, &wasRunning);
    if (wasRunning) {
        qDebug("%s: %llims", qPrintable(name), elapsed);
    }
    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_count(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    // first argument: name to print. Ignore any additional arguments
    QString name;
    if (callData->argc > 0)
        name = callData->args[0].toQStringNoThrow();

    QV4::ExecutionEngine *v4 = scope.engine;
    QV8Engine *v8engine = scope.engine->v8Engine;

    QV4::StackFrame frame = v4->currentStackFrame();

    QString scriptName = frame.source;

    int value = v8engine->consoleCountHelper(scriptName, frame.line, frame.column);
    QString message = name + QLatin1String(": ") + QString::number(value);

    QMessageLogger(qPrintable(scriptName), frame.line,
                   qPrintable(frame.function))
        .debug("%s", qPrintable(message));

    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_trace(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 0)
        THROW_GENERIC_ERROR("console.trace(): Invalid arguments");

    QV4::ExecutionEngine *v4 = scope.engine;

    QString stack = jsStack(v4);

    QV4::StackFrame frame = v4->currentStackFrame();
    QMessageLogger(frame.source.toUtf8().constData(), frame.line,
                   frame.function.toUtf8().constData())
        .debug("%s", qPrintable(stack));

    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_warn(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    return writeToConsole(b, scope, callData, Warn);
}

void ConsoleObject::method_assert(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc == 0)
        THROW_GENERIC_ERROR("console.assert(): Missing argument");

    QV4::ExecutionEngine *v4 = scope.engine;

    if (!callData->args[0].toBoolean()) {
        QString message;
        for (int i = 1; i < callData->argc; ++i) {
            if (i != 1)
                message.append(QLatin1Char(' '));

            message.append(callData->args[i].toQStringNoThrow());
        }

        QString stack = jsStack(v4);

        QV4::StackFrame frame = v4->currentStackFrame();
        QMessageLogger(frame.source.toUtf8().constData(), frame.line,
                       frame.function.toUtf8().constData())
            .critical("%s\n%s",qPrintable(message), qPrintable(stack));

    }
    scope.result = QV4::Encode::undefined();
}

void ConsoleObject::method_exception(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    if (callData->argc == 0)
        THROW_GENERIC_ERROR("console.exception(): Missing argument");

    writeToConsole(b, scope, callData, Error, true);

    scope.result = QV4::Encode::undefined();
}



void QV4::GlobalExtensions::init(Object *globalObject, QJSEngine::Extensions extensions)
{
    ExecutionEngine *v4 = globalObject->engine();
    Scope scope(v4);

    if (extensions.testFlag(QJSEngine::TranslationExtension)) {
    #if QT_CONFIG(translation)
        globalObject->defineDefaultProperty(QStringLiteral("qsTranslate"), QV4::GlobalExtensions::method_qsTranslate);
        globalObject->defineDefaultProperty(QStringLiteral("QT_TRANSLATE_NOOP"), QV4::GlobalExtensions::method_qsTranslateNoOp);
        globalObject->defineDefaultProperty(QStringLiteral("qsTr"), QV4::GlobalExtensions::method_qsTr);
        globalObject->defineDefaultProperty(QStringLiteral("QT_TR_NOOP"), QV4::GlobalExtensions::method_qsTrNoOp);
        globalObject->defineDefaultProperty(QStringLiteral("qsTrId"), QV4::GlobalExtensions::method_qsTrId);
        globalObject->defineDefaultProperty(QStringLiteral("QT_TRID_NOOP"), QV4::GlobalExtensions::method_qsTrIdNoOp);

        // string prototype extension
        scope.engine->stringPrototype()->defineDefaultProperty(QStringLiteral("arg"), QV4::GlobalExtensions::method_string_arg);
    #endif
    }

    if (extensions.testFlag(QJSEngine::ConsoleExtension)) {
        globalObject->defineDefaultProperty(QStringLiteral("print"), QV4::ConsoleObject::method_log);


        QV4::ScopedObject console(scope, globalObject->engine()->memoryManager->allocObject<QV4::ConsoleObject>());
        globalObject->defineDefaultProperty(QStringLiteral("console"), console);
    }

    if (extensions.testFlag(QJSEngine::GarbageCollectionExtension)) {
        globalObject->defineDefaultProperty(QStringLiteral("gc"), QV4::GlobalExtensions::method_gc);
    }
}


#if QT_CONFIG(translation)
/*!
    \qmlmethod string Qt::qsTranslate(string context, string sourceText, string disambiguation, int n)

    Returns a translated version of \a sourceText within the given \a context, optionally based on a
    \a disambiguation string and value of \a n for strings containing plurals;
    otherwise returns \a sourceText itself if no appropriate translated string
    is available.

    If the same \a sourceText is used in different roles within the
    same translation \a context, an additional identifying string may be passed in
    for \a disambiguation.

    Example:
    \snippet qml/qsTranslate.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
void GlobalExtensions::method_qsTranslate(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 2)
        THROW_GENERIC_ERROR("qsTranslate() requires at least two arguments");
    if (!callData->args[0].isString())
        THROW_GENERIC_ERROR("qsTranslate(): first argument (context) must be a string");
    if (!callData->args[1].isString())
        THROW_GENERIC_ERROR("qsTranslate(): second argument (sourceText) must be a string");
    if ((callData->argc > 2) && !callData->args[2].isString())
        THROW_GENERIC_ERROR("qsTranslate(): third argument (disambiguation) must be a string");

    QString context = callData->args[0].toQStringNoThrow();
    QString text = callData->args[1].toQStringNoThrow();
    QString comment;
    if (callData->argc > 2) comment = callData->args[2].toQStringNoThrow();

    int i = 3;
    if (callData->argc > i && callData->args[i].isString()) {
        qWarning("qsTranslate(): specifying the encoding as fourth argument is deprecated");
        ++i;
    }

    int n = -1;
    if (callData->argc > i)
        n = callData->args[i].toInt32();

    QString result = QCoreApplication::translate(context.toUtf8().constData(),
                                                 text.toUtf8().constData(),
                                                 comment.toUtf8().constData(),
                                                 n);

    scope.result = scope.engine->newString(result);
}

/*!
    \qmlmethod string Qt::qsTranslateNoOp(string context, string sourceText, string disambiguation)

    Marks \a sourceText for dynamic translation in the given \a context; i.e, the stored \a sourceText
    will not be altered.

    If the same \a sourceText is used in different roles within the
    same translation context, an additional identifying string may be passed in
    for \a disambiguation.

    Returns the \a sourceText.

    QT_TRANSLATE_NOOP is used in conjunction with the dynamic translation functions
    qsTr() and qsTranslate(). It identifies a string as requiring
    translation (so it can be identified by \c lupdate), but leaves the actual
    translation to the dynamic functions.

    Example:
    \snippet qml/qtTranslateNoOp.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
void GlobalExtensions::method_qsTranslateNoOp(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 2)
        scope.result = QV4::Encode::undefined();
    else
        scope.result = callData->args[1];
}

/*!
    \qmlmethod string Qt::qsTr(string sourceText, string disambiguation, int n)

    Returns a translated version of \a sourceText, optionally based on a
    \a disambiguation string and value of \a n for strings containing plurals;
    otherwise returns \a sourceText itself if no appropriate translated string
    is available.

    If the same \a sourceText is used in different roles within the
    same translation context, an additional identifying string may be passed in
    for \a disambiguation.

    Example:
    \snippet qml/qsTr.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
void GlobalExtensions::method_qsTr(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1)
        THROW_GENERIC_ERROR("qsTr() requires at least one argument");
    if (!callData->args[0].isString())
        THROW_GENERIC_ERROR("qsTr(): first argument (sourceText) must be a string");
    if ((callData->argc > 1) && !callData->args[1].isString())
        THROW_GENERIC_ERROR("qsTr(): second argument (disambiguation) must be a string");
    if ((callData->argc > 2) && !callData->args[2].isNumber())
        THROW_GENERIC_ERROR("qsTr(): third argument (n) must be a number");

    QString context;
    if (QQmlContextData *ctxt = scope.engine->callingQmlContext()) {
        QString path = ctxt->urlString();
        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        int lastDot = path.lastIndexOf(QLatin1Char('.'));
        int length = lastDot - (lastSlash + 1);
        context = (lastSlash > -1) ? path.mid(lastSlash + 1, (length > -1) ? length : -1) : QString();
    } else {
        ExecutionContext *parentCtx = scope.engine->currentContext;
        // The first non-empty source URL in the call stack determines the translation context.
        while (!!parentCtx && context.isEmpty()) {
            if (CompiledData::CompilationUnit *unit = static_cast<CompiledData::CompilationUnit*>(parentCtx->d()->compilationUnit)) {
                QString fileName = unit->fileName();
                QUrl url(unit->fileName());
                if (url.isValid() && url.isRelative()) {
                    context = url.fileName();
                } else {
                    context = QQmlFile::urlToLocalFileOrQrc(fileName);
                    if (context.isEmpty() && fileName.startsWith(QLatin1String(":/")))
                        context = fileName;
                }
                context = QFileInfo(context).baseName();
            }
            parentCtx = scope.engine->parentContext(parentCtx);
        }
    }

    QString text = callData->args[0].toQStringNoThrow();
    QString comment;
    if (callData->argc > 1)
        comment = callData->args[1].toQStringNoThrow();
    int n = -1;
    if (callData->argc > 2)
        n = callData->args[2].toInt32();

    QString result = QCoreApplication::translate(context.toUtf8().constData(), text.toUtf8().constData(),
                                                 comment.toUtf8().constData(), n);

    scope.result = scope.engine->newString(result);
}

/*!
    \qmlmethod string Qt::qsTrNoOp(string sourceText, string disambiguation)

    Marks \a sourceText for dynamic translation; i.e, the stored \a sourceText
    will not be altered.

    If the same \a sourceText is used in different roles within the
    same translation context, an additional identifying string may be passed in
    for \a disambiguation.

    Returns the \a sourceText.

    QT_TR_NOOP is used in conjunction with the dynamic translation functions
    qsTr() and qsTranslate(). It identifies a string as requiring
    translation (so it can be identified by \c lupdate), but leaves the actual
    translation to the dynamic functions.

    Example:
    \snippet qml/qtTrNoOp.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
void GlobalExtensions::method_qsTrNoOp(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1)
        scope.result = QV4::Encode::undefined();
    else
        scope.result = callData->args[0];
}

/*!
    \qmlmethod string Qt::qsTrId(string id, int n)

    Returns a translated string identified by \a id.
    If no matching string is found, the id itself is returned. This
    should not happen under normal conditions.

    If \a n >= 0, all occurrences of \c %n in the resulting string
    are replaced with a decimal representation of \a n. In addition,
    depending on \a n's value, the translation text may vary.

    Example:
    \snippet qml/qsTrId.qml 0

    It is possible to supply a source string template like:

    \tt{//% <string>}

    or

    \tt{\\begincomment% <string> \\endcomment}

    Example:
    \snippet qml/qsTrId.1.qml 0

    Creating binary translation (QM) files suitable for use with this function requires passing
    the \c -idbased option to the \c lrelease tool.

    \sa QT_TRID_NOOP(), {Internationalization and Localization with Qt Quick}
*/
void GlobalExtensions::method_qsTrId(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1)
        THROW_GENERIC_ERROR("qsTrId() requires at least one argument");
    if (!callData->args[0].isString())
        THROW_TYPE_ERROR_WITH_MESSAGE("qsTrId(): first argument (id) must be a string");
    if (callData->argc > 1 && !callData->args[1].isNumber())
        THROW_TYPE_ERROR_WITH_MESSAGE("qsTrId(): second argument (n) must be a number");

    int n = -1;
    if (callData->argc > 1)
        n = callData->args[1].toInt32();

    scope.result = scope.engine->newString(qtTrId(callData->args[0].toQStringNoThrow().toUtf8().constData(), n));
}

/*!
    \qmlmethod string Qt::qsTrIdNoOp(string id)

    Marks \a id for dynamic translation.

    Returns the \a id.

    QT_TRID_NOOP is used in conjunction with the dynamic translation function
    qsTrId(). It identifies a string as requiring translation (so it can be identified
    by \c lupdate), but leaves the actual translation to qsTrId().

    Example:
    \snippet qml/qtTrIdNoOp.qml 0

    \sa qsTrId(), {Internationalization and Localization with Qt Quick}
*/
void GlobalExtensions::method_qsTrIdNoOp(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 1)
        scope.result = QV4::Encode::undefined();
    else
        scope.result = callData->args[0];
}
#endif // translation


void GlobalExtensions::method_gc(const BuiltinFunction *, Scope &scope, CallData *)
{
    scope.engine->memoryManager->runGC(/* forceFullCollection = */ true);

    scope.result = QV4::Encode::undefined();
}



void GlobalExtensions::method_string_arg(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc != 1)
        THROW_GENERIC_ERROR("String.arg(): Invalid arguments");

    QString value = callData->thisObject.toQString();

    QV4::ScopedValue arg(scope, callData->args[0]);
    if (arg->isInteger())
        RETURN_RESULT(scope.engine->newString(value.arg(arg->integerValue())));
    else if (arg->isDouble())
        RETURN_RESULT(scope.engine->newString(value.arg(arg->doubleValue())));
    else if (arg->isBoolean())
        RETURN_RESULT(scope.engine->newString(value.arg(arg->booleanValue())));

    RETURN_RESULT(scope.engine->newString(value.arg(arg->toQString())));
}

/*!
\qmlmethod Qt::callLater(function)
\qmlmethod Qt::callLater(function, argument1, argument2, ...)
\since 5.8
Use this function to eliminate redundant calls to a function or signal.

The function passed as the first argument to Qt.callLater()
will be called later, once the QML engine returns to the event loop.

When this function is called multiple times in quick succession with the
same function as its first argument, that function will be called only once.

For example:
\snippet qml/qtLater.qml 0

Any additional arguments passed to Qt.callLater() will
be passed on to the function invoked. Note that if redundant calls
are eliminated, then only the last set of arguments will be passed to the
function.
*/
void QtObject::method_callLater(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    QV8Engine *v8engine = scope.engine->v8Engine;
    v8engine->delayedCallQueue()->addUniquelyAndExecuteLater(b, scope, callData);
}

QT_END_NAMESPACE

