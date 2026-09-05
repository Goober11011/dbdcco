#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <LayerShellQt/Window>
#include <QMargins>
#include <QDir>
//#include <QLocalServer>
//#include <QLocalSocket>

//#include <QDebug>

bool setMap(QLabel &image, const QDir &mapsDir, const QString &mapName);

//QLocalServer *server;

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
								Qt::WindowDoesNotAcceptFocus |
								Qt::WindowTransparentForInput
								);
		
		  window.winId();

		  auto *layerWindow = LayerShellQt::Window::get(window.windowHandle());

		  layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
		  layerWindow->setExclusiveZone(0);

		  layerWindow->setAnchors(
								LayerShellQt::Window::Anchors(
										  LayerShellQt::Window::AnchorTop |
										  LayerShellQt::Window::AnchorRight 
										  )
								);

		  layerWindow->setMargins(QMargins(10, 10, 10, 10));

		  QLabel image(&window);

		  QDir mapsDir("maps");

		  QString currentMap = "lakemine.png";

		  QStringList mapFiles = mapsDir.entryList(
								QStringList() << "*.png",
								QDir::Files
								);

		  if (!setMap(image, mapsDir, currentMap)){
					 return 1;
		  }

		  image.setScaledContents(true);
		  image.resize(300, 300);

		  window.resize(300, 300);


		  //QLocalServer *server = new QLocalServer(&app);

		  //QLocalServer::removeServer("dbdoverlay");

		  //server->listen("dbdoverlay");

		  //QObject::connect(server, &QLocalServer::newConnection, [&]() {
			//					QLocalSocket *socket = server->nextPendingConnection();
			//
								//QObject::connect(socket, &QLocalSocket::readyRead, [&](){
								//					 QString mapName = QString::fromUtf8(socket->readAll()).trimmed();

//													 if(setMap(image, mapsDir, mapName)) {
//													 	currentMap = mapName;
								//					 }

							//						 socket->disconnectFromServer();
						//		});
		//	});

  

		  window.show();

		  return app.exec();
}

bool setMap(QLabel &image, const QDir &mapsDir, const QString &mapName){
		  QPixmap map(mapsDir.filePath(mapName));

		  if (map.isNull()) {
					 return false;
		  }

		  QPixmap transparentMap(map.size());
		  transparentMap.fill(Qt::transparent);

		  QPainter painter(&transparentMap);
		  painter.setOpacity(0.7);
		  painter.drawPixmap(0, 0, map);
		  painter.end();

		  image.setPixmap(transparentMap);

		  return true;
}
