/*
 * Copyright (C) 2013-2014 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "engineuwsgi.h"
#include "plugin.h"

#include <Cutelyst/application.h>

#include <QCoreApplication>
#include <QSocketNotifier>
#include <QPluginLoader>
#include <QFileInfo>
#include <QDir>

using namespace Cutelyst;

static QList<EngineUwsgi *> coreEngines;

void cuteOutput(QtMsgType, const QMessageLogContext &, const QString &);
void uwsgi_cutelyst_loop(void);

/**
 * This function is called as soon as
 * the plugin is loaded
 */
extern "C" void uwsgi_cutelyst_on_load()
{
    uwsgi_register_loop( (char *) "CutelystQtLoop", uwsgi_cutelyst_loop);

    (void) new QCoreApplication(uwsgi.argc, uwsgi.argv);

    if (qEnvironmentVariableIsEmpty("CUTELYST_NO_UWSGI_LOG")) {
        qInstallMessageHandler(cuteOutput);
    }
}

extern "C" int uwsgi_cutelyst_init()
{
    uwsgi_log("Initializing Cutelyst plugin\n");

    uwsgi.loop = (char *) "CutelystQtLoop";

    return 0;
}

extern "C" void uwsgi_cutelyst_post_fork()
{
    Q_FOREACH (EngineUwsgi *engine, coreEngines) {
        if (engine->thread() != qApp->thread()) {
            engine->thread()->start();
        } else {
            Q_EMIT engine->postFork();
        }
    }
}

extern "C" int uwsgi_cutelyst_request(struct wsgi_request *wsgi_req)
{
    // empty request ?
    if (!wsgi_req->uh->pktsize) {
        qCDebug(CUTELYST_UWSGI) << "Empty request. skip.";
        return -1;
    }

    // get uwsgi variables
    if (uwsgi_parse_vars(wsgi_req)) {
        qCDebug(CUTELYST_UWSGI) << "Invalid request. skip.";
        return -1;
    }

    coreEngines.at(wsgi_req->async_id)->processRequest(wsgi_req);

    return UWSGI_OK;
}

#ifdef UWSGI_GO_CHEAP_CODE // Actually we only need uwsgi 2.0.1
static void fsmon_reload(struct uwsgi_fsmon *fs)
{
    qCDebug(CUTELYST_UWSGI) << "Reloading application due to file change";
    QFileInfo fileInfo(fs->path);
    int count = 0;
    // Ugly hack to wait for 2 seconds for the file to be filled
    while (fileInfo.size() == 0 && count < 10) {
        ++count;
        qCDebug(CUTELYST_UWSGI) << "Sleeping as the application file is empty" << count;
        usleep(200 * 1000);
        fileInfo.refresh();
    }

    uwsgi_reload(uwsgi.argv);
}
#endif // UWSGI_GO_CHEAP_CODE

/**
 * This function is called when the master process is exiting
 */
extern "C" void uwsgi_cutelyst_master_cleanup()
{
    qCDebug(CUTELYST_UWSGI) << "Master process finishing" << QCoreApplication::applicationPid();
    delete qApp;
    qCDebug(CUTELYST_UWSGI) << "Master process finished" << QCoreApplication::applicationPid();
}

/**
 * This function is called when the child process is exiting
 */
extern "C" void uwsgi_cutelyst_atexit()
{
    qCDebug(CUTELYST_UWSGI) << "Child process finishing:" << QCoreApplication::applicationPid();

    Q_FOREACH (EngineUwsgi *engine, coreEngines) {
        engine->stop();
    }
    qDeleteAll(coreEngines);

    qCDebug(CUTELYST_UWSGI) << "Child process finished:" << QCoreApplication::applicationPid();
}

extern "C" void uwsgi_cutelyst_init_apps()
{
    const QString &applicationName = QCoreApplication::applicationName();
    QString path(options.app);
    if (path.isEmpty()) {
        qCCritical(CUTELYST_UWSGI) << "Cutelyst application name or path was not set";
        exit(1);
    }

    qCDebug(CUTELYST_UWSGI) << "Cutelyst loading application:" << options.app;

#ifdef UWSGI_GO_CHEAP_CODE
    if (options.reload) {
        // Register application auto reload
        char *file = qstrdup(path.toUtf8().constData());
        uwsgi_register_fsmon(file, fsmon_reload, NULL);
    }
#endif // UWSGI_GO_CHEAP_CODE

    QDir cwd(uwsgi.cwd);

    // Set the configuration env
    QStringList args = QCoreApplication::arguments();
    QString config(options.config);
    if (!config.isNull()) {
        QFileInfo fileInfo(config);
        if (fileInfo.isRelative()) {
            config = cwd.absoluteFilePath(config);
        }
        qputenv("CUTELYST_CONFIG", config.toUtf8());
    } else if (args.contains("--ini")) {
        int index = args.indexOf("--ini");
        if (index != -1 && index < args.size()) {
            QFileInfo fileInfo(args.at(index + 1));
            if (fileInfo.isRelative()) {
                config = cwd.absoluteFilePath(args.at(index + 1));
            }
            qputenv("CUTELYST_CONFIG", config.toUtf8());
        }
    }

    // if the path is relative build a path
    // relative to the current working directory
    QFileInfo fileInfo(path);
    if (fileInfo.isRelative()) {
        path = cwd.absoluteFilePath(path);
    }

    QPluginLoader *loader = new QPluginLoader(path);
    if (!loader->load()) {
        qCCritical(CUTELYST_UWSGI) << "Could not load application:" << loader->errorString();
        exit(1);
    }

    QObject *instance = loader->instance();
    if (!instance) {
        qCCritical(CUTELYST_UWSGI) << "Could not get a QObject instance: %s\n" << loader->errorString();
        exit(1);
    }

    Application *app = qobject_cast<Application *>(instance);
    if (!app) {
        qCCritical(CUTELYST_UWSGI) << "Could not cast Cutelyst::Application from instance: %s\n" << loader->errorString();
        exit(1);
    }

    // Sets the application name with the name from our library
    if (QCoreApplication::applicationName() == applicationName) {
        QCoreApplication::setApplicationName(app->metaObject()->className());
    }
    qCDebug(CUTELYST_UWSGI) << "Loaded application:" << QCoreApplication::applicationName();

    EngineUwsgi *mainEngine = new EngineUwsgi(app);
    if (!mainEngine->initApplication(app, false)) {
        qCCritical(CUTELYST_UWSGI) << "Failed to init application.";
        exit(1);
    }
    coreEngines.append(mainEngine);

    EngineUwsgi *engine = mainEngine;
    for (int i = 0; i < uwsgi.cores; ++i) {
        // Create the desired threads
        // i > 0 the main thread counts as one thread
        if (uwsgi.threads > 1 && i > 0) {
            engine = new EngineUwsgi(app);
            engine->setThread(new QThread);

            // Post fork might fail when on threaded mode
            QObject::connect(engine, &EngineUwsgi::engineDisabled,
                             mainEngine, &EngineUwsgi::reuseEngineRequests);

            coreEngines.append(engine);
        }

        // Add core request
        struct wsgi_request *wsgi_req = new wsgi_request;
        memset(wsgi_req, 0, sizeof(struct wsgi_request));
        wsgi_req->async_id = i;
        engine->addUnusedRequest(wsgi_req);
    }

    // register a new app under a specific "mountpoint"
    uwsgi_add_app(1, CUTELYST_MODIFIER1, (char *) "", 0, NULL, NULL);

    delete loader;
}

void uwsgi_cutelyst_watch_signal(int signalFD)
{
    QSocketNotifier *socketNotifier = new QSocketNotifier(signalFD, QSocketNotifier::Read);
    QObject::connect(socketNotifier, &QSocketNotifier::activated,
                     [=](int fd) {
        socketNotifier->setEnabled(false);
        uwsgi_receive_signal(fd, (char *) "worker", uwsgi.mywid);
        socketNotifier->setEnabled(true);
    });
}

void uwsgi_cutelyst_loop()
{
    // ensure SIGPIPE is ignored
    signal(SIGPIPE, SIG_IGN);

    // FIX for some reason this is not being set by UWSGI
    uwsgi.wait_read_hook = uwsgi_simple_wait_read_hook;

    // monitor signals
    if (uwsgi.signal_socket > -1) {
        uwsgi_cutelyst_watch_signal(uwsgi.signal_socket);
        uwsgi_cutelyst_watch_signal(uwsgi.my_signal_socket);
    }

    // start the qt event loop
    qApp->exec();
}

void cuteOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        uwsgi_log("%s[debug] %s\n", context.category, localMsg.constData());
        break;
    case QtWarningMsg:
        uwsgi_log("%s[warn] %s\n", context.category, localMsg.constData());
        break;
    case QtCriticalMsg:
        uwsgi_log("%s[crit] %s\n", context.category, localMsg.constData());
        break;
    case QtFatalMsg:
        uwsgi_log("%s[fatal] %s\n", context.category, localMsg.constData());
        abort();
    }
}
