/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2020 Aetf <aetf@unlimitedcode.works>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fdosecrets/dbus/DBusObject.h"

#include <QDBusMetaType>
#include <QThread>
#include <QtDBus>

namespace FdoSecrets
{
    QString camelToPascal(const QString& camel)
    {
        if (camel.isEmpty()) {
            return camel;
        }
        return camel.at(0).toUpper() + camel.mid(1);
    }

    bool prepareInputParams(const QVector<int>& inputTypes,
                            const QVariantList& args,
                            QVarLengthArray<void*, 10>& params,
                            QVariantList& auxParams)
    {
        // prepare params
        for (int count = 0; count != inputTypes.size(); ++count) {
            const auto& id = inputTypes.at(count);
            const auto& arg = args.at(count);

            if (arg.userType() == id) {
                // shortcut for no conversion
                params.append(const_cast<void*>(arg.constData()));
                continue;
            }

            // we need at least one conversion, allocate a slot in auxParams
            auxParams.append(QVariant(id));
            auto& out = auxParams.last();
            // first handle QDBusArgument to wire types
            if (arg.userType() == qMetaTypeId<QDBusArgument>()) {
                QMetaType wireId(typeToWireType(id).dbusTypeId);
                out = QVariant(wireId, nullptr);

                const auto& in = arg.value<QDBusArgument>();
                if (!QDBusMetaType::demarshall(in, wireId, out.data())) {
                    qDebug() << "Internal error: failed QDBusArgument conversion from" << arg << "to type"
                             << wireId.name();
                    return false;
                }
            } else {
                // make a copy to store the converted value
                out = arg;
            }
            // other conversions are handled here
            if (!out.convert(id)) {
                qDebug() << "Internal error: failed conversion from" << arg << "to type" << QMetaType::typeName(id)
                         << id;
                return false;
            }
            // good to go
            params.append(const_cast<void*>(out.constData()));
        }
        return true;
    }

    void DBusMgr::populateMethodCache(const QMetaObject& mo)
    {
        for (int i = mo.methodOffset(); i != mo.methodCount(); ++i) {
            auto mm = mo.method(i);

            // only register public Q_INVOKABLE methods
            if (mm.access() != QMetaMethod::Public || mm.methodType() != QMetaMethod::Method) {
                continue;
            }
            if (mm.returnType() != qMetaTypeId<DBusResult>()) {
                continue;
            }

            auto iface = mo.classInfo(mo.indexOfClassInfo("D-Bus Interface")).value();
            if (!iface) {
                continue;
            }

            // map from function name to dbus name
            auto member = camelToPascal(mm.name());
            // also "remove" => "Delete" due to c++ keyword restriction
            if (member == "Remove") {
                member = QStringLiteral("Delete");
            }
            auto cacheKey = QStringLiteral("%1.%2").arg(iface, member);

            // skip if we already have it
            auto it = m_cachedMethods.find(cacheKey);
            if (it != m_cachedMethods.end()) {
                continue;
            }

            MethodData md;
            md.isProperty = mm.tag() && mm.tag() == QStringLiteral("DBUS_PROPERTY");
            md.slotIdx = mm.methodIndex();

            bool valid = true;
            // assumes output params (reference parameter) all follows input params
            bool outputBegin = false;
            for (const auto& paramType : mm.parameterTypes()) {
                auto id = QMetaType::type(paramType);

                // handle the first optional calling client param
                if (id == qMetaTypeId<DBusClientPtr>()) {
                    md.needsCallingClient = true;
                    continue;
                }

                // handle output types
                if (paramType.endsWith('&')) {
                    outputBegin = true;
                    id = QMetaType::type(paramType.left(paramType.length() - 1));
                    md.outputTypes.append(id);
                    auto paramData = typeToWireType(id);
                    if (paramData.signature.isEmpty()) {
                        qDebug() << "Internal error: unhandled new output type for dbus signature" << paramType;
                        valid = false;
                        break;
                    }
                    md.outputTargetTypes.append(paramData.dbusTypeId);
                    continue;
                }

                // handle input types
                if (outputBegin) {
                    qDebug() << "Internal error: invalid method parameter order, no input parameter after output ones"
                             << mm.name();
                    valid = false;
                    break;
                }
                auto sig = typeToWireType(id).signature;
                if (sig.isEmpty()) {
                    qDebug() << "Internal error: unhandled new parameter type for dbus signature" << paramType;
                    valid = false;
                    break;
                }
                md.inputTypes.append(id);
                md.signature += sig;
            }
            if (valid) {
                m_cachedMethods.insert(cacheKey, md);
            }
        }
    }

    bool DBusMgr::handleMessage(const QDBusMessage& message, const QDBusConnection&)
    {
        // save a mutable copy of the message, as we may modify it to unify property access
        // and method call
        RequestedMethod req{
            message.interface(),
            message.member(),
            message.signature(),
            message.arguments(),
            RequestType::Method,
        };

        if (req.interface == "org.freedesktop.DBus.Introspectable") {
            // introspection can be handled by Qt, just return false
            return false;
        } else if (req.interface == "org.freedesktop.DBus.Properties") {
            // but we need to handle properties ourselves like regular functions
            if (!rewriteRequestForProperty(req)) {
                // invalid message
                qDebug() << "Invalid message" << message;
                return false;
            }
        }

        // who's calling?
        const auto& client = findClient(message.service());
        if (!client) {
            // the client already died
            return false;
        }

        // activate the target object
        return activateObject(client, message.path(), req, message);
    }

    bool DBusMgr::rewriteRequestForProperty(RequestedMethod& req)
    {
        if (req.member == "Set" && req.signature == "ssv") {
            // convert to normal method call: SetName
            req.interface = req.args.at(0).toString();
            req.member = req.member + req.args.at(1).toString();
            // unwrap the QDBusVariant and expose the inner signature
            auto arg = req.args.last().value<QDBusVariant>().variant();
            req.args = {arg};
            if (arg.userType() == qMetaTypeId<QDBusArgument>()) {
                req.signature = arg.value<QDBusArgument>().currentSignature();
            } else if (arg.userType() == QMetaType::QString) {
                req.signature = "s";
            } else {
                qDebug() << "Unhandled SetProperty value type" << QMetaType::typeName(arg.userType()) << arg.userType();
                return false;
            }
        } else if (req.member == "Get" && req.signature == "ss") {
            // convert to normal method call: Name
            req.interface = req.args.at(0).toString();
            req.member = req.args.at(1).toString();
            req.signature = "";
            req.args = {};
            req.type = RequestType::PropertyGet;
        } else if (req.member == "GetAll" && req.signature == "s") {
            // special handled in activateObject
            req.interface = req.args.at(0).toString();
            req.member = "";
            req.signature = "";
            req.args = {};
            req.type = RequestType::PropertyGetAll;
        } else {
            return false;
        }
        return true;
    }

    bool DBusMgr::activateObject(const DBusClientPtr& client,
                                 const QString& path,
                                 const RequestedMethod& req,
                                 const QDBusMessage& msg)
    {
        auto obj = m_objects.value(path, nullptr);
        if (!obj) {
            qDebug() << "DBusMgr::handleMessage with unknown path" << msg;
            return false;
        }
        Q_ASSERT_X(QThread::currentThread() == obj->thread(),
                   "QDBusConnection: internal threading error",
                   "function called for an object that is in another thread!!");

        auto mo = obj->metaObject();
        // either interface matches, or interface is empty if req is property get all
        QString interface = mo->classInfo(mo->indexOfClassInfo("D-Bus Interface")).value();
        if (req.interface != interface && !(req.type == RequestType::PropertyGetAll && req.interface.isEmpty())) {
            qDebug() << "DBusMgr::handleMessage with mismatch interface" << msg;
            return false;
        }

        // special handle of property getall
        if (req.type == RequestType::PropertyGetAll) {
            return objectPropertyGetAll(client, obj, interface, msg);
        }

        // find the slot to call
        auto cacheKey = QStringLiteral("%1.%2").arg(req.interface, req.member);
        auto it = m_cachedMethods.find(cacheKey);
        if (it == m_cachedMethods.end()) {
            qDebug() << "DBusMgr::handleMessage with nonexisting method" << cacheKey;
            return false;
        }

        // requested signature is verified by Qt to match the content of arguments,
        // but this list of arguments itself is untrusted
        if (it->signature != req.signature || it->inputTypes.size() != req.args.size()) {
            qDebug() << "Message signature does not match, expected" << it->signature << it->inputTypes.size() << "got"
                     << req.signature << req.args.size();
            return false;
        }

        DBusResult ret;
        QVariantList outputArgs;
        if (!deliverMethod(client, obj, *it, req.args, ret, outputArgs)) {
            qDebug() << "Failed to deliver method" << msg;
            return sendDBus(msg.createErrorReply(QDBusError::InternalError, tr("Failed to deliver message")));
        }

        if (!ret.ok()) {
            return sendDBus(msg.createErrorReply(ret, ""));
        }
        if (req.type == RequestType::PropertyGet) {
            // property get need the reply wrapped in QDBusVariant
            outputArgs[0] = QVariant::fromValue(QDBusVariant(outputArgs.first()));
        }
        return sendDBus(msg.createReply(outputArgs));
    }

    bool DBusMgr::objectPropertyGetAll(const DBusClientPtr& client,
                                       DBusObject* obj,
                                       const QString& interface,
                                       const QDBusMessage& msg)
    {
        QVariantMap result;

        // prefix match the cacheKey
        auto prefix = interface + ".";
        for (auto it = m_cachedMethods.constBegin(); it != m_cachedMethods.constEnd(); ++it) {
            if (!it.key().startsWith(prefix)) {
                continue;
            }
            if (!it.value().isProperty) {
                continue;
            }
            auto name = it.key().mid(prefix.size());

            DBusResult ret;
            QVariantList outputArgs;
            if (!deliverMethod(client, obj, it.value(), {}, ret, outputArgs)) {
                // ignore any error per spec
                continue;
            }
            if (ret.err()) {
                // ignore any error per spec
                continue;
            }
            Q_ASSERT(outputArgs.size() == 1);

            result.insert(name, outputArgs.first());
        }

        return sendDBus(msg.createReply(QVariantList{result}));
    }

    bool DBusMgr::deliverMethod(const DBusClientPtr& client,
                                DBusObject* obj,
                                const MethodData& method,
                                const QVariantList& args,
                                DBusResult& ret,
                                QVariantList& outputArgs)
    {
        QVarLengthArray<void*, 10> params;
        QVariantList auxParams;

        // the first one is for return type
        params.append(&ret);

        if (method.needsCallingClient) {
            auxParams.append(QVariant::fromValue(client));
            params.append(const_cast<void*>(auxParams.last().constData()));
        }

        // prepare input
        if (!prepareInputParams(method.inputTypes, args, params, auxParams)) {
            qDebug() << "Failed to prepare input params";
            return false;
        }

        // prepare output args
        outputArgs.reserve(outputArgs.size() + method.outputTypes.size());
        for (const auto& outputType : asConst(method.outputTypes)) {
            outputArgs.append(QVariant(outputType));
            params.append(const_cast<void*>(outputArgs.last().constData()));
        }

        // call it
        bool fail = obj->qt_metacall(QMetaObject::InvokeMetaMethod, method.slotIdx, params.data()) >= 0;
        if (fail) {
            // generate internal error
            qWarning() << "Internal error: Failed to deliver message";
            return false;
        }

        if (!ret.ok()) {
            // error reply
            return true;
        }

        // output args need to be converted before they can be directly sent out:
        for (int i = 0; i != outputArgs.size(); ++i) {
            auto& outputArg = outputArgs[i];
            if (!outputArg.convert(method.outputTargetTypes.at(i))) {
                qWarning() << "Internal error: Failed to convert message output to type"
                           << method.outputTargetTypes.at(i);
                return false;
            }
        }

        return true;
    }
} // namespace FdoSecrets
