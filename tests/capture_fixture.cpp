#include <QApplication>
#include <QPalette>
#include <QTimer>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle(app.arguments().value(1, "Prism capture fixture"));
    window.setAutoFillBackground(true);
    window.resize(320, 240); window.move(50, 50);
    bool red = false;
    const auto paint = [&] {
        QPalette palette = window.palette();
        palette.setColor(QPalette::Window, red ? QColor(210, 30, 50) : QColor(30, 50, 210));
        red = !red; window.setPalette(palette);
    };
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, paint);
    timer.start(400); paint(); window.show();
    return app.exec();
}
