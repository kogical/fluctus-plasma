// SPDX-License-Identifier: MIT
#include <QGuiApplication>
#include <QQuickView>
#include <QTimer>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.loadFromModule("LiquidGlassDemo", "Demo");
    view.resize(980, 620);
    view.show();
    const QStringList args = app.arguments();
    const int grabIdx = args.indexOf(QStringLiteral("--grab"));
    if (grabIdx > 0 && grabIdx + 1 < args.size()) {
        const QString out = args.at(grabIdx + 1);
        QTimer::singleShot(900, &view, [&view, out] {
            view.grabWindow().save(out);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
