#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
//#include <QDebug>

int main(int argc, char *argv[]){
		  //qDebug() << "App name:" << QCoreApplication::applicationName();
		  QApplication app(argc, argv);

		  app.setApplicationName("dbdoverlay");
		  app.setDesktopFileName("dbdoverlay");

		  QWidget window;
			
		  window.setAttribute(Qt::WA_TranslucentBackground);
		  window.setWindowFlags(
								Qt::FramelessWindowHint |
							  	Qt::Tool |
								Qt::WindowStaysOnTopHint |
								Qt::WindowDoesNotAcceptFocus |
								Qt::WindowTransparentForInput
								);

		  QLabel image(&window);

		  QPixmap map("trickstersdelusion.png");

		  QPixmap transparentMap(map.size());
		  transparentMap.fill(Qt::transparent);

		  QPainter painter(&transparentMap);
		  painter.setOpacity(0.7);
		  painter.drawPixmap(0,0, map);
		  painter.end();


		  image.setPixmap(transparentMap);
		  image.setScaledContents(true);
		  image.resize(300, 300);

		  window.resize(300, 300);
		  window.show();

		  return app.exec();
}

