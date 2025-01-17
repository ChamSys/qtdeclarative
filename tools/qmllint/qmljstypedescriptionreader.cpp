/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "qmljstypedescriptionreader.h"

#include <private/qqmljsparser_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsengine_p.h>

#include <QDir>

#define QTC_ASSERT_STRINGIFY_HELPER(x) #x
#define QTC_ASSERT_STRINGIFY(x) QTC_ASSERT_STRINGIFY_HELPER(x)
#define QTC_ASSERT_STRING(cond) qDebug() << (\
    "\"" cond"\" in file " __FILE__ ", line " QTC_ASSERT_STRINGIFY(__LINE__))
#define QTC_ASSERT(cond, action) if (Q_LIKELY(cond)) {} else { QTC_ASSERT_STRING(#cond); action; } do {} while (0)

using namespace QQmlJS;
using namespace QQmlJS::AST;
using namespace LanguageUtils;

QString toString(const AST::UiQualifiedId *qualifiedId, QChar delimiter = QLatin1Char('.'))
{
    QString result;

    for (const UiQualifiedId *iter = qualifiedId; iter; iter = iter->next) {
        if (iter != qualifiedId)
            result += delimiter;

        result += iter->name;
    }

    return result;
}

TypeDescriptionReader::TypeDescriptionReader(const QString &fileName, const QString &data)
    : _fileName (fileName), _source(data), _objects(0)
{
}

TypeDescriptionReader::~TypeDescriptionReader()
{
}

bool TypeDescriptionReader::operator()(
        QHash<QString, FakeMetaObject::ConstPtr> *objects,
        QList<ModuleApiInfo> *moduleApis,
        QStringList *dependencies)
{
    Engine engine;

    Lexer lexer(&engine);
    Parser parser(&engine);

    lexer.setCode(_source, /*line = */ 1, /*qmlMode = */true);

    if (!parser.parse()) {
        _errorMessage = QString::fromLatin1("%1:%2: %3").arg(
                    QString::number(parser.errorLineNumber()),
                    QString::number(parser.errorColumnNumber()),
                    parser.errorMessage());
        return false;
    }

    _objects = objects;
    _moduleApis = moduleApis;
    _dependencies = dependencies;
    readDocument(parser.ast());

    return _errorMessage.isEmpty();
}

QString TypeDescriptionReader::errorMessage() const
{
    return _errorMessage;
}

QString TypeDescriptionReader::warningMessage() const
{
    return _warningMessage;
}

void TypeDescriptionReader::readDocument(UiProgram *ast)
{
    if (!ast) {
        addError(SourceLocation(), tr("Could not parse document."));
        return;
    }

    if (!ast->headers || ast->headers->next || !AST::cast<AST::UiImport *>(ast->headers->headerItem)) {
        addError(SourceLocation(), tr("Expected a single import."));
        return;
    }

    UiImport *import = AST::cast<AST::UiImport *>(ast->headers->headerItem);
    if (toString(import->importUri) != QLatin1String("QtQuick.tooling")) {
        addError(import->importToken, tr("Expected import of QtQuick.tooling."));
        return;
    }

    if (!import->version) {
        addError(import->firstSourceLocation(), tr("Import statement without version."));
        return;
    }

    if (import->version->majorVersion != 1) {
        addError(import->version->firstSourceLocation(), tr("Major version different from 1 not supported."));
        return;
    }

    if (!ast->members || !ast->members->member || ast->members->next) {
        addError(SourceLocation(), tr("Expected document to contain a single object definition."));
        return;
    }

    UiObjectDefinition *module = AST::cast<UiObjectDefinition *>(ast->members->member);
    if (!module) {
        addError(SourceLocation(), tr("Expected document to contain a single object definition."));
        return;
    }

    if (toString(module->qualifiedTypeNameId) != QLatin1String("Module")) {
        addError(SourceLocation(), tr("Expected document to contain a Module {} member."));
        return;
    }

    readModule(module);
}

void TypeDescriptionReader::readModule(UiObjectDefinition *ast)
{
    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiObjectDefinition *component = AST::cast<UiObjectDefinition *>(member);

        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);
        if (script && (toString(script->qualifiedId) == QStringLiteral("dependencies"))) {
            readDependencies(script);
            continue;
        }

        QString typeName;
        if (component)
            typeName = toString(component->qualifiedTypeNameId);

        if (!component || (typeName != QLatin1String("Component") && typeName != QLatin1String("ModuleApi"))) {
            continue;
        }

        if (typeName == QLatin1String("Component"))
            readComponent(component);
        else if (typeName == QLatin1String("ModuleApi"))
            readModuleApi(component);
    }
}

void TypeDescriptionReader::addError(const SourceLocation &loc, const QString &message)
{
    _errorMessage += QString::fromLatin1("%1:%2:%3: %4\n").arg(
                QDir::toNativeSeparators(_fileName),
                QString::number(loc.startLine),
                QString::number(loc.startColumn),
                message);
}

void TypeDescriptionReader::addWarning(const SourceLocation &loc, const QString &message)
{
    _warningMessage += QString::fromLatin1("%1:%2:%3: %4\n").arg(
                QDir::toNativeSeparators(_fileName),
                QString::number(loc.startLine),
                QString::number(loc.startColumn),
                message);
}

void TypeDescriptionReader::readDependencies(UiScriptBinding *ast)
{
    ExpressionStatement *stmt = AST::cast<ExpressionStatement*>(ast->statement);
    if (!stmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected dependency definitions"));
        return;
    }
    ArrayPattern *exp = AST::cast<ArrayPattern *>(stmt->expression);
    if (!exp) {
        addError(stmt->expression->firstSourceLocation(), tr("Expected dependency definitions"));
        return;
    }
    for (PatternElementList *l = exp->elements; l; l = l->next) {
        //StringLiteral *str = AST::cast<StringLiteral *>(l->element->initializer);
        StringLiteral *str = AST::cast<StringLiteral *>(l->element->initializer);
        *_dependencies << str->value.toString();
    }
}

void TypeDescriptionReader::readComponent(UiObjectDefinition *ast)
{
    FakeMetaObject::Ptr fmo(new FakeMetaObject);

    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiObjectDefinition *component = AST::cast<UiObjectDefinition *>(member);
        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);
        if (component) {
            QString name = toString(component->qualifiedTypeNameId);
            if (name == QLatin1String("Property"))
                readProperty(component, fmo);
            else if (name == QLatin1String("Method") || name == QLatin1String("Signal"))
                readSignalOrMethod(component, name == QLatin1String("Method"), fmo);
            else if (name == QLatin1String("Enum"))
                readEnum(component, fmo);
            else
                addWarning(component->firstSourceLocation(),
                           tr("Expected only Property, Method, Signal and Enum object definitions, not \"%1\".")
                           .arg(name));
        } else if (script) {
            QString name = toString(script->qualifiedId);
            if (name == QLatin1String("name")) {
                fmo->setClassName(readStringBinding(script));
            } else if (name == QLatin1String("prototype")) {
                fmo->setSuperclassName(readStringBinding(script));
            } else if (name == QLatin1String("defaultProperty")) {
                fmo->setDefaultPropertyName(readStringBinding(script));
            } else if (name == QLatin1String("exports")) {
                readExports(script, fmo);
            } else if (name == QLatin1String("exportMetaObjectRevisions")) {
                readMetaObjectRevisions(script, fmo);
            } else if (name == QLatin1String("attachedType")) {
                fmo->setAttachedTypeName(readStringBinding(script));
            } else if (name == QLatin1String("isSingleton")) {
                fmo->setIsSingleton(readBoolBinding(script));
            } else if (name == QLatin1String("isCreatable")) {
                fmo->setIsCreatable(readBoolBinding(script));
            } else if (name == QLatin1String("isComposite")) {
                fmo->setIsComposite(readBoolBinding(script));
            } else {
                addWarning(script->firstSourceLocation(),
                           tr("Expected only name, prototype, defaultProperty, attachedType, exports, "
                              "isSingleton, isCreatable, isComposite and exportMetaObjectRevisions "
                              "script bindings, not \"%1\".").arg(name));
            }
        } else {
            addWarning(member->firstSourceLocation(), tr("Expected only script bindings and object definitions."));
        }
    }

    if (fmo->className().isEmpty()) {
        addError(ast->firstSourceLocation(), tr("Component definition is missing a name binding."));
        return;
    }

    // ### add implicit export into the package of c++ types
    fmo->addExport(fmo->className(), QStringLiteral("<cpp>"), ComponentVersion());
    fmo->updateFingerprint();
    _objects->insert(fmo->className(), fmo);
}

void TypeDescriptionReader::readModuleApi(UiObjectDefinition *ast)
{
    ModuleApiInfo apiInfo;

    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);

        if (script) {
            const QString name = toString(script->qualifiedId);
            if (name == QLatin1String("uri")) {
                apiInfo.uri = readStringBinding(script);
            } else if (name == QLatin1String("version")) {
                apiInfo.version = readNumericVersionBinding(script);
            } else if (name == QLatin1String("name")) {
                apiInfo.cppName = readStringBinding(script);
            } else {
                addWarning(script->firstSourceLocation(),
                           tr("Expected only uri, version and name script bindings."));
            }
        } else {
            addWarning(member->firstSourceLocation(), tr("Expected only script bindings."));
        }
    }

    if (!apiInfo.version.isValid()) {
        addError(ast->firstSourceLocation(), tr("ModuleApi definition has no or invalid version binding."));
        return;
    }

    if (_moduleApis)
        _moduleApis->append(apiInfo);
}

void TypeDescriptionReader::readSignalOrMethod(UiObjectDefinition *ast, bool isMethod, FakeMetaObject::Ptr fmo)
{
    FakeMetaMethod fmm;
    // ### confusion between Method and Slot. Method should be removed.
    if (isMethod)
        fmm.setMethodType(FakeMetaMethod::Slot);
    else
        fmm.setMethodType(FakeMetaMethod::Signal);

    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiObjectDefinition *component = AST::cast<UiObjectDefinition *>(member);
        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);
        if (component) {
            QString name = toString(component->qualifiedTypeNameId);
            if (name == QLatin1String("Parameter"))
                readParameter(component, &fmm);
            else
                addWarning(component->firstSourceLocation(), tr("Expected only Parameter object definitions."));
        } else if (script) {
            QString name = toString(script->qualifiedId);
            if (name == QLatin1String("name"))
                fmm.setMethodName(readStringBinding(script));
            else if (name == QLatin1String("type"))
                fmm.setReturnType(readStringBinding(script));
            else if (name == QLatin1String("revision"))
                fmm.setRevision(readIntBinding(script));
            else
                addWarning(script->firstSourceLocation(), tr("Expected only name and type script bindings."));

        } else {
            addWarning(member->firstSourceLocation(), tr("Expected only script bindings and object definitions."));
        }
    }

    if (fmm.methodName().isEmpty()) {
        addError(ast->firstSourceLocation(), tr("Method or signal is missing a name script binding."));
        return;
    }

    fmo->addMethod(fmm);
}

void TypeDescriptionReader::readProperty(UiObjectDefinition *ast, FakeMetaObject::Ptr fmo)
{
    QString name;
    QString type;
    bool isPointer = false;
    bool isReadonly = false;
    bool isList = false;
    int revision = 0;

    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);
        if (!script) {
            addWarning(member->firstSourceLocation(), tr("Expected script binding."));
            continue;
        }

        QString id = toString(script->qualifiedId);
        if (id == QLatin1String("name"))
            name = readStringBinding(script);
        else if (id == QLatin1String("type"))
            type = readStringBinding(script);
        else if (id == QLatin1String("isPointer"))
            isPointer = readBoolBinding(script);
        else if (id == QLatin1String("isReadonly"))
            isReadonly = readBoolBinding(script);
        else if (id == QLatin1String("isList"))
            isList = readBoolBinding(script);
        else if (id == QLatin1String("revision"))
            revision = readIntBinding(script);
        else
            addWarning(script->firstSourceLocation(), tr("Expected only type, name, revision, isPointer, isReadonly and isList script bindings."));
    }

    if (name.isEmpty() || type.isEmpty()) {
        addError(ast->firstSourceLocation(), tr("Property object is missing a name or type script binding."));
        return;
    }

    fmo->addProperty(FakeMetaProperty(name, type, isList, !isReadonly, isPointer, revision));
}

void TypeDescriptionReader::readEnum(UiObjectDefinition *ast, FakeMetaObject::Ptr fmo)
{
    FakeMetaEnum fme;

    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);
        if (!script) {
            addWarning(member->firstSourceLocation(), tr("Expected script binding."));
            continue;
        }

        QString name = toString(script->qualifiedId);
        if (name == QLatin1String("name"))
            fme.setName(readStringBinding(script));
        else if (name == QLatin1String("values"))
            readEnumValues(script, &fme);
        else
            addWarning(script->firstSourceLocation(), tr("Expected only name and values script bindings."));
    }

    fmo->addEnum(fme);
}

void TypeDescriptionReader::readParameter(UiObjectDefinition *ast, FakeMetaMethod *fmm)
{
    QString name;
    QString type;

    for (UiObjectMemberList *it = ast->initializer->members; it; it = it->next) {
        UiObjectMember *member = it->member;
        UiScriptBinding *script = AST::cast<UiScriptBinding *>(member);
        if (!script) {
            addWarning(member->firstSourceLocation(), tr("Expected script binding."));
            continue;
        }

        const QString id = toString(script->qualifiedId);
        if (id == QLatin1String("name")) {
            name = readStringBinding(script);
        } else if (id == QLatin1String("type")) {
            type = readStringBinding(script);
        } else if (id == QLatin1String("isPointer")) {
            // ### unhandled
        } else if (id == QLatin1String("isReadonly")) {
            // ### unhandled
        } else if (id == QLatin1String("isList")) {
            // ### unhandled
        } else {
            addWarning(script->firstSourceLocation(), tr("Expected only name and type script bindings."));
        }
    }

    fmm->addParameter(name, type);
}

QString TypeDescriptionReader::readStringBinding(UiScriptBinding *ast)
{
    QTC_ASSERT(ast, return QString());

    if (!ast->statement) {
        addError(ast->colonToken, tr("Expected string after colon."));
        return QString();
    }

    ExpressionStatement *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected string after colon."));
        return QString();
    }

    StringLiteral *stringLit = AST::cast<StringLiteral *>(expStmt->expression);
    if (!stringLit) {
        addError(expStmt->firstSourceLocation(), tr("Expected string after colon."));
        return QString();
    }

    return stringLit->value.toString();
}

bool TypeDescriptionReader::readBoolBinding(AST::UiScriptBinding *ast)
{
    QTC_ASSERT(ast, return false);

    if (!ast->statement) {
        addError(ast->colonToken, tr("Expected boolean after colon."));
        return false;
    }

    ExpressionStatement *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected boolean after colon."));
        return false;
    }

    TrueLiteral *trueLit = AST::cast<TrueLiteral *>(expStmt->expression);
    FalseLiteral *falseLit = AST::cast<FalseLiteral *>(expStmt->expression);
    if (!trueLit && !falseLit) {
        addError(expStmt->firstSourceLocation(), tr("Expected true or false after colon."));
        return false;
    }

    return trueLit;
}

double TypeDescriptionReader::readNumericBinding(AST::UiScriptBinding *ast)
{
    QTC_ASSERT(ast, return qQNaN());

    if (!ast->statement) {
        addError(ast->colonToken, tr("Expected numeric literal after colon."));
        return 0;
    }

    ExpressionStatement *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected numeric literal after colon."));
        return 0;
    }

    NumericLiteral *numericLit = AST::cast<NumericLiteral *>(expStmt->expression);
    if (!numericLit) {
        addError(expStmt->firstSourceLocation(), tr("Expected numeric literal after colon."));
        return 0;
    }

    return numericLit->value;
}

ComponentVersion TypeDescriptionReader::readNumericVersionBinding(UiScriptBinding *ast)
{
    ComponentVersion invalidVersion;

    if (!ast || !ast->statement) {
        addError((ast ? ast->colonToken : SourceLocation()), tr("Expected numeric literal after colon."));
        return invalidVersion;
    }

    ExpressionStatement *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected numeric literal after colon."));
        return invalidVersion;
    }

    NumericLiteral *numericLit = AST::cast<NumericLiteral *>(expStmt->expression);
    if (!numericLit) {
        addError(expStmt->firstSourceLocation(), tr("Expected numeric literal after colon."));
        return invalidVersion;
    }

    return ComponentVersion(_source.mid(numericLit->literalToken.begin(), numericLit->literalToken.length));
}

int TypeDescriptionReader::readIntBinding(AST::UiScriptBinding *ast)
{
    double v = readNumericBinding(ast);
    int i = static_cast<int>(v);

    if (i != v) {
        addError(ast->firstSourceLocation(), tr("Expected integer after colon."));
        return 0;
    }

    return i;
}

void TypeDescriptionReader::readExports(UiScriptBinding *ast, FakeMetaObject::Ptr fmo)
{
    QTC_ASSERT(ast, return);

    if (!ast->statement) {
        addError(ast->colonToken, tr("Expected array of strings after colon."));
        return;
    }

    ExpressionStatement *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected array of strings after colon."));
        return;
    }

    ArrayPattern *arrayLit = AST::cast<ArrayPattern *>(expStmt->expression);
    if (!arrayLit) {
        addError(expStmt->firstSourceLocation(), tr("Expected array of strings after colon."));
        return;
    }

    for (PatternElementList *it = arrayLit->elements; it; it = it->next) {
        StringLiteral *stringLit = AST::cast<StringLiteral *>(it->element->initializer);
        if (!stringLit) {
            addError(arrayLit->firstSourceLocation(), tr("Expected array literal with only string literal members."));
            return;
        }
        QString exp = stringLit->value.toString();
        int slashIdx = exp.indexOf(QLatin1Char('/'));
        int spaceIdx = exp.indexOf(QLatin1Char(' '));
        ComponentVersion version(exp.mid(spaceIdx + 1));

        if (spaceIdx == -1 || !version.isValid()) {
            addError(stringLit->firstSourceLocation(), tr("Expected string literal to contain 'Package/Name major.minor' or 'Name major.minor'."));
            continue;
        }
        QString package;
        if (slashIdx != -1)
            package = exp.left(slashIdx);
        QString name = exp.mid(slashIdx + 1, spaceIdx - (slashIdx+1));

        // ### relocatable exports where package is empty?
        fmo->addExport(name, package, version);
    }
}

void TypeDescriptionReader::readMetaObjectRevisions(UiScriptBinding *ast, FakeMetaObject::Ptr fmo)
{
    QTC_ASSERT(ast, return);

    if (!ast->statement) {
        addError(ast->colonToken, tr("Expected array of numbers after colon."));
        return;
    }

    ExpressionStatement *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected array of numbers after colon."));
        return;
    }

    ArrayPattern *arrayLit = AST::cast<ArrayPattern *>(expStmt->expression);
    if (!arrayLit) {
        addError(expStmt->firstSourceLocation(), tr("Expected array of numbers after colon."));
        return;
    }

    int exportIndex = 0;
    const int exportCount = fmo->exports().size();
    for (PatternElementList *it = arrayLit->elements; it; it = it->next, ++exportIndex) {
        NumericLiteral *numberLit = cast<NumericLiteral *>(it->element->initializer);
        if (!numberLit) {
            addError(arrayLit->firstSourceLocation(), tr("Expected array literal with only number literal members."));
            return;
        }

        if (exportIndex >= exportCount) {
            addError(numberLit->firstSourceLocation(), tr("Meta object revision without matching export."));
            return;
        }

        const double v = numberLit->value;
        const int metaObjectRevision = static_cast<int>(v);
        if (metaObjectRevision != v) {
            addError(numberLit->firstSourceLocation(), tr("Expected integer."));
            return;
        }

        fmo->setExportMetaObjectRevision(exportIndex, metaObjectRevision);
    }
}

void TypeDescriptionReader::readEnumValues(AST::UiScriptBinding *ast, LanguageUtils::FakeMetaEnum *fme)
{
    if (!ast)
        return;
    if (!ast->statement) {
        addError(ast->colonToken, tr("Expected object literal after colon."));
        return;
    }

    auto *expStmt = AST::cast<ExpressionStatement *>(ast->statement);
    if (!expStmt) {
        addError(ast->statement->firstSourceLocation(), tr("Expected expression after colon."));
        return;
    }

    if (auto *objectLit = AST::cast<ObjectPattern *>(expStmt->expression)) {
        for (PatternPropertyList *it = objectLit->properties; it; it = it->next) {
            if (PatternProperty *assignement = it->property) {
                if (auto *name = AST::cast<StringLiteralPropertyName *>(assignement->name)) {
                    fme->addKey(name->id.toString());
                    continue;
                }
            }
            addError(it->firstSourceLocation(), tr("Expected strings as enum keys."));
        }
    } else if (auto *arrayLit = AST::cast<ArrayPattern *>(expStmt->expression)) {
        for (PatternElementList *it = arrayLit->elements; it; it = it->next) {
            if (PatternElement *element = it->element) {
                if (auto *name = AST::cast<StringLiteral *>(element->initializer)) {
                    fme->addKey(name->value.toString());
                    continue;
                }
            }
            addError(it->firstSourceLocation(), tr("Expected strings as enum keys."));
        }
    } else {
        addError(ast->statement->firstSourceLocation(),
                 tr("Expected either array or object literal as enum definition."));
    }
}
