/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmltypescreator.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QtDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QFile>
#include <QScopedPointer>
#include <QSaveFile>
#include <QQueue>

#include <cstdlib>

struct ScopedPointerFileCloser
{
    static inline void cleanup(FILE *handle) { if (handle) fclose(handle); }
};

static bool acceptClassForQmlTypeRegistration(const QJsonObject &classDef)
{
    const QJsonArray classInfos = classDef[QLatin1String("classInfos")].toArray();
    for (const QJsonValue &info: classInfos) {
        if (info[QLatin1String("name")].toString().startsWith(QLatin1String("QML.")))
            return true;
    }
    return false;
}

static QVector<QJsonObject> foreignRelatedTypes(const QVector<QJsonObject> &types,
                                                const QVector<QJsonObject> &foreignTypes)
{
    const QLatin1String classInfosKey("classInfos");
    const QLatin1String nameKey("name");
    const QLatin1String qualifiedClassNameKey("qualifiedClassName");
    const QLatin1String qmlNamePrefix("QML.");
    const QLatin1String qmlForeignName("QML.Foreign");
    const QLatin1String qmlAttachedName("QML.Attached");
    const QLatin1String valueKey("value");
    const QLatin1String superClassesKey("superClasses");
    const QLatin1String accessKey("access");
    const QLatin1String publicAccess("public");

    QSet<QString> processedRelatedNames;
    QQueue<QJsonObject> typeQueue;
    typeQueue.append(types.toList());
    QVector<QJsonObject> relatedTypes;

    // First mark all classes registered from this module as already processed.
    for (const QJsonObject &type : types) {
        processedRelatedNames.insert(type.value(qualifiedClassNameKey).toString());
        const auto classInfos = type.value(classInfosKey).toArray();
        for (const QJsonValue &classInfo : classInfos) {
            const QJsonObject obj = classInfo.toObject();
            if (obj.value(nameKey).toString() == qmlForeignName) {
                processedRelatedNames.insert(obj.value(valueKey).toString());
                break;
            }
        }
    }

    // Then mark all classes registered from other modules as already processed.
    // We don't want to generate them again for this module.
    for (const QJsonObject &foreignType : foreignTypes) {
        const auto classInfos = foreignType.value(classInfosKey).toArray();
        bool seenQmlPrefix = false;
        for (const QJsonValue &classInfo : classInfos) {
            const QJsonObject obj = classInfo.toObject();
            const QString name = obj.value(nameKey).toString();
            if (!seenQmlPrefix && name.startsWith(qmlNamePrefix)) {
                processedRelatedNames.insert(foreignType.value(qualifiedClassNameKey).toString());
                seenQmlPrefix = true;
            }
            if (name == qmlForeignName) {
                processedRelatedNames.insert(obj.value(valueKey).toString());
                break;
            }
        }
    }

    auto addType = [&](const QString &typeName) {
        if (processedRelatedNames.contains(typeName))
            return;
        processedRelatedNames.insert(typeName);
        if (const QJsonObject *other = QmlTypesClassDescription::findType(foreignTypes, typeName)) {
            relatedTypes.append(*other);
            typeQueue.enqueue(*other);
        }
    };

    // Then recursively iterate the super types and attached types, marking the
    // ones we are interested in as related.
    while (!typeQueue.isEmpty()) {
        const QJsonObject classDef = typeQueue.dequeue();

        const auto classInfos = classDef.value(classInfosKey).toArray();
        for (const QJsonValue &classInfo : classInfos) {
            const QJsonObject obj = classInfo.toObject();
            if (obj.value(nameKey).toString() == qmlAttachedName) {
                addType(obj.value(valueKey).toString());
            } else if (obj.value(nameKey).toString() == qmlForeignName) {
                const QString foreignClassName = obj.value(valueKey).toString();
                if (const QJsonObject *other = QmlTypesClassDescription::findType(
                            foreignTypes, foreignClassName)) {
                    const auto otherSupers = other->value(superClassesKey).toArray();
                    if (!otherSupers.isEmpty()) {
                        const QJsonObject otherSuperObject = otherSupers.first().toObject();
                        if (otherSuperObject.value(accessKey).toString() == publicAccess)
                            addType(otherSuperObject.value(nameKey).toString());
                    }

                    const auto otherClassInfos = other->value(classInfosKey).toArray();
                    for (const QJsonValue &otherClassInfo : otherClassInfos) {
                        const QJsonObject obj = otherClassInfo.toObject();
                        if (obj.value(nameKey).toString() == qmlAttachedName) {
                            addType(obj.value(valueKey).toString());
                            break;
                        }
                        // No, you cannot chain QML_FOREIGN declarations. Sorry.
                    }
                    break;
                }
            }
        }

        const auto supers = classDef.value(superClassesKey).toArray();
        if (!supers.isEmpty()) {
            const QJsonObject superObject = supers.first().toObject();
            if (superObject.value(accessKey).toString() == publicAccess)
                addType(superObject.value(nameKey).toString());
        }
    }

    return relatedTypes;
}

int main(int argc, char **argv)
{
    // Produce reliably the same output for the same input by disabling QHash's random seeding.
    qSetGlobalQHashSeed(0);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qmltyperegistrar"));
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption outputOption(QStringLiteral("o"));
    outputOption.setDescription(QStringLiteral("Write output to specified file."));
    outputOption.setValueName(QStringLiteral("file"));
    outputOption.setFlags(QCommandLineOption::ShortOptionStyle);
    parser.addOption(outputOption);

    QCommandLineOption privateIncludesOption(
            QStringLiteral("private-includes"),
            QStringLiteral("Include headers ending in \"_p.h\" using \"#include <private/foo_p.h>\""
                           "rather than \"#include <foo_p.h>\"."));
    parser.addOption(privateIncludesOption);

    QCommandLineOption importNameOption(QStringLiteral("import-name"));
    importNameOption.setDescription(QStringLiteral("Name of the module to use with QML type registrations."));
    importNameOption.setValueName(QStringLiteral("QML module name"));
    parser.addOption(importNameOption);

    QCommandLineOption majorVersionOption(QStringLiteral("major-version"));
    majorVersionOption.setDescription(QStringLiteral("Major version to use for type registrations."));
    majorVersionOption.setValueName(QStringLiteral("major version"));
    parser.addOption(majorVersionOption);

    QCommandLineOption pluginTypesOption(QStringLiteral("generate-plugintypes"));
    pluginTypesOption.setDescription(QStringLiteral("Generate plugins.qmltypes into specified directory."));
    pluginTypesOption.setValueName(QStringLiteral("qmltypes target Directory"));
    parser.addOption(pluginTypesOption);

    QCommandLineOption foreignTypesOption(QStringLiteral("foreign-types"));
    foreignTypesOption.setDescription(QStringLiteral("Consider foreign types when generating plugins.qmltypes."));
    foreignTypesOption.setValueName(QStringLiteral("Comma separated list of other modules to consult for types."));
    parser.addOption(foreignTypesOption);

    QCommandLineOption dependenciesOption(QStringLiteral("dependencies"));
    dependenciesOption.setDescription(QStringLiteral("Dependencies to be stated in plugins.qmltypes"));
    dependenciesOption.setValueName(QStringLiteral("name of JSON file with dependencies"));
    parser.addOption(dependenciesOption);

    parser.addPositionalArgument(QStringLiteral("[MOC generated json file]"),
                                 QStringLiteral("MOC generated json output"));

    parser.process(app);

    FILE *output = stdout;
    QScopedPointer<FILE, ScopedPointerFileCloser> outputFile;

    if (parser.isSet(outputOption)) {
        QString outputName = parser.value(outputOption);
#if defined(_MSC_VER)
        if (_wfopen_s(&output, reinterpret_cast<const wchar_t *>(outputName.utf16()), L"w") != 0) {
#else
        output = fopen(QFile::encodeName(outputName).constData(), "w"); // create output file
        if (!output) {
#endif
            fprintf(stderr, "Error: Cannot open %s for writing\n", qPrintable(outputName));
            return EXIT_FAILURE;
        }
        outputFile.reset(output);
    }

    fprintf(output,
            "/****************************************************************************\n"
            "** Generated QML type registration code\n**\n");
    fprintf(output,
            "** WARNING! All changes made in this file will be lost!\n"
            "*****************************************************************************/\n\n");
    fprintf(output,
            "#include <QtQml/qqmlengine.h>\n");

    QStringList includes;
    QVector<QJsonObject> types;
    QVector<QJsonObject> foreignTypes;

    const QString module = parser.value(importNameOption);
    const QStringList files = parser.positionalArguments();
    for (const QString &source: files) {
        QJsonDocument metaObjects;
        {
            QFile f(source);
            if (!f.open(QIODevice::ReadOnly)) {
                fprintf(stderr, "Error opening %s for reading\n", qPrintable(source));
                return EXIT_FAILURE;
            }
            QJsonParseError error = {0, QJsonParseError::NoError};
            metaObjects = QJsonDocument::fromJson(f.readAll(), &error);
            if (error.error != QJsonParseError::NoError) {
                fprintf(stderr, "Error parsing %s\n", qPrintable(source));
                return EXIT_FAILURE;
            }
        }

        auto processMetaObject = [&](const QJsonObject &metaObject) {
            const QJsonArray classes = metaObject[QLatin1String("classes")].toArray();
            for (const auto &cls : classes) {
                QJsonObject classDef = cls.toObject();
                if (acceptClassForQmlTypeRegistration(classDef)) {
                    const QString include = metaObject[QLatin1String("inputFile")].toString();
                    const bool declaredInHeader = include.endsWith(QLatin1String(".h"));
                    if (declaredInHeader) {
                        includes.append(include);
                        classDef.insert(QLatin1String("registerable"), true);
                    } else {
                        fprintf(stderr, "Cannot generate QML type registration for class %s "
                                        "because it is not declared in a header.",
                                qPrintable(classDef.value(QLatin1String("qualifiedClassName"))
                                           .toString()));
                    }
                    types.append(classDef);
                } else {
                    foreignTypes.append(classDef);
                }
            }
        };

        if (metaObjects.isArray()) {
            const QJsonArray metaObjectsArray = metaObjects.array();
            for (const auto &metaObject : metaObjectsArray) {
                if (!metaObject.isObject()) {
                    fprintf(stderr, "Error parsing %s: JSON is not an object\n",
                            qPrintable(source));
                    return EXIT_FAILURE;
                }

                processMetaObject(metaObject.toObject());
            }
        } else if (metaObjects.isObject()) {
            processMetaObject(metaObjects.object());
        } else {
            fprintf(stderr, "Error parsing %s: JSON is not an object or an array\n",
                    qPrintable(source));
            return EXIT_FAILURE;
        }
    }

    const QLatin1String qualifiedClassNameKey("qualifiedClassName");
    auto sortTypes = [&](QVector<QJsonObject> &types) {
        std::sort(types.begin(), types.end(), [&](const QJsonObject &a, const QJsonObject &b) {
            return a.value(qualifiedClassNameKey).toString() <
                    b.value(qualifiedClassNameKey).toString();
        });
    };

    sortTypes(types);

    fprintf(output, "\n#include <QtQml/qqmlmoduleregistration.h>");
    const bool privateIncludes = parser.isSet(privateIncludesOption);
    for (const QString &include : qAsConst(includes)) {
        if (privateIncludes && include.endsWith(QLatin1String("_p.h")))
            fprintf(output, "\n#include <private/%s>", qPrintable(include));
        else
            fprintf(output, "\n#include <%s>", qPrintable(include));
    }

    fprintf(output, "\n\n");

    QString moduleAsSymbol = module;
    moduleAsSymbol.replace(QLatin1Char('.'), QLatin1Char('_'));

    const QString functionName = QStringLiteral("qml_register_types_") + moduleAsSymbol;

    fprintf(output, "void %s()\n{\n", qPrintable(functionName));
    const auto majorVersion = parser.value(majorVersionOption);

    for (const QJsonObject &classDef : qAsConst(types)) {
        if (!classDef.value(QLatin1String("registerable")).toBool())
            continue;

        const QString className = classDef[QLatin1String("qualifiedClassName")].toString();
        fprintf(output, "\n    qmlRegisterTypesAndRevisions<%s>(\"%s\", %s);", qPrintable(className),
                qPrintable(module), qPrintable(majorVersion));
    }

    fprintf(output, "\n    qmlRegisterModule(\"%s\", %s, QT_VERSION_MINOR);",
            qPrintable(module), qPrintable(majorVersion));
    fprintf(output, "\n}\n");
    fprintf(output, "static const QQmlModuleRegistration registration(\"%s\", %s, %s);\n",
            qPrintable(module), qPrintable(majorVersion), qPrintable(functionName));

    if (!parser.isSet(pluginTypesOption))
        return EXIT_SUCCESS;

    if (parser.isSet(foreignTypesOption)) {
        const QStringList foreignTypesFiles = parser.value(foreignTypesOption)
                .split(QLatin1Char(','));
        for (const QString &types : foreignTypesFiles) {
            QFile typesFile(types);
            if (!typesFile.open(QIODevice::ReadOnly)) {
                fprintf(stderr, "Cannot open foreign types file %s\n", qPrintable(types));
                continue;
            }

            QJsonParseError error = {0, QJsonParseError::NoError};
            QJsonDocument foreignMetaObjects = QJsonDocument::fromJson(typesFile.readAll(), &error);
            if (error.error != QJsonParseError::NoError) {
                fprintf(stderr, "Error parsing %s\n", qPrintable(types));
                continue;
            }

            const QJsonArray foreignObjectsArray = foreignMetaObjects.array();
            for (const auto &metaObject : foreignObjectsArray) {
                if (!metaObject.isObject()) {
                    fprintf(stderr, "Error parsing %s: JSON is not an object\n",
                            qPrintable(types));
                    continue;
                }

                const QJsonArray classes = metaObject[QLatin1String("classes")].toArray();
                for (const auto &cls : classes)
                    foreignTypes.append(cls.toObject());
            }
        }
    }

    sortTypes(foreignTypes);
    types += foreignRelatedTypes(types, foreignTypes);
    sortTypes(types);

    QmlTypesCreator creator;
    creator.setOwnTypes(std::move(types));
    creator.setForeignTypes(std::move(foreignTypes));
    creator.setModule(module);
    creator.setMajorVersion(parser.value(majorVersionOption).toInt());

    creator.generate(parser.value(pluginTypesOption), parser.value(dependenciesOption));
    return EXIT_SUCCESS;
}
