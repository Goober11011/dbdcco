#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <LayerShellQt/Window>
#include <QMargins>
#include <QDir>
#include <QEvent>
//#include <QSocketNotifier>
//#include <QLocalServer>
//#include <QLocalSocket>

//#include <QDebug>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

#include <thread>
#include <mutex>

class MapChangeEvent : public QEvent{
		  public:
					 static constexpr QEvent::Type Type =static_cast<QEvent::Type>(QEvent::User + 1);

					 MapChangeEvent()
								: QEvent(Type){}
};

const char *socketPath = "/tmp/dbdoverlay.sock";

std::string requestedMap;
std::mutex mapMutex;

bool setMap(QLabel &image, const QDir &mapsDir, const QString &mapName);

class MapLabel : public QLabel{
		  public:
					 MapLabel(QWidget *parent, const QDir &mapsDir)
								: QLabel(parent), mapsDir(mapsDir){}
		 protected:
					 bool event(QEvent *event) override{
								if (event->type() == MapChangeEvent::Type) {
										  qDebug() << "Map change event recieved";

										  QString mapName;

										  {
													 std::lock_guard<std::mutex> lock(mapMutex);
													 mapName = QString::fromStdString(requestedMap);
													 requestedMap.clear();
										  }

										  if (!mapName.isEmpty()) {
													 setMap(*this, mapsDir, mapName);
										  }

										  //delete event;
										  return true;
								}

								return QLabel::event(event);
					 }

		private:
					 QDir mapsDir;
};

//QLocalServer *server;

int main(int argc, char *argv[]){
		  //qDebug() << "App name:" << QCoreApplication::applicationName();
		  if (argc == 3 && QString(argv[1]) == "--map") {
					 int clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);

					 if (clientSocket == -1) {
								return 1;
					 }

					 sockaddr_un address{};
					 address.sun_family = AF_UNIX;

					 std::strncpy(
										  address.sun_path,
										  socketPath,
										  sizeof(address.sun_path) - 1
									 );

					 if (connect(
										  clientSocket,
										  reinterpret_cast<sockaddr *>(&address),
										  sizeof(address)
									) == -1) {
									close(clientSocket);
									return 1;
					 }

					 write(
										  clientSocket,
										  argv[2],
										  std::strlen(argv[2])
							);

					 close(clientSocket);
					 return 0;
		  }



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

		  QDir mapsDir("maps");

		  MapLabel image(&window, mapsDir);

		  int serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);

		  if (serverSocket == -1){
					 return 1;
		  }

		  sockaddr_un serverAddress{};
		  serverAddress.sun_family = AF_UNIX;

		  std::strncpy(
		  						serverAddress.sun_path,
		  						socketPath,
		  						sizeof(serverAddress.sun_path) - 1
		  				  );

		  unlink(socketPath);

		  if (bind(
										  serverSocket,
										  reinterpret_cast<sockaddr *>(&serverAddress),
										  sizeof(serverAddress)
					 ) == -1) {
					 close(serverSocket);
					 return 1;
		  }

		  if (listen(serverSocket, 5) == -1) {
					 close(serverSocket);
					 unlink(socketPath);
					 return 1;
		  }

		  std::thread socketThread([&]() {
							while (true) {
								int clientSocket = accept(serverSocket, nullptr, nullptr);

								if (clientSocket == -1) {
									continue;
								}

								char buffer[1024];

								ssize_t bytesRead = read(
													 clientSocket,
													 buffer,
													 sizeof(buffer) - 1
													 );

								if (bytesRead > 0) {
									buffer[bytesRead] = '\0';

									qDebug() << "Received:" << buffer;

									{
										std::lock_guard<std::mutex> lock(mapMutex);
										requestedMap = buffer;
									}

									QCoreApplication::postEvent(
														 &image,
														 new MapChangeEvent()
														 );

								}

								close(clientSocket);
							}
		  });



		  QString currentMap = "lakemine.png";

		  if (!setMap(image, mapsDir, currentMap)){
					 return 1;
		  }

		  //QSocketNotifier *socketNotifier =
			//		 new QSocketNotifier(
			//							  serverSocket,
			//							  QSocketNotifier::Read,
			//							  &app
			//							  );

		  //QObject::connect(
			//					socketNotifier,
			//					&QSocketNotifier::activated,
			//					[&](int socket) {
			//					int clientSocket = accept(socket, nullptr, nullptr);

			//					if (clientSocket == -1) {
			//						return;
			//					}

			//					char buffer[1024];

			//					ssize_t bytesRead = read(
			//										 clientSocket,
			//										 buffer,
			//										 sizeof(buffer) -1
			//					);

			//					if (bytesRead > 0) {
			//						buffer[bytesRead] = '\0';

			//						QString mapName = QString::fromUtf8(buffer).trimmed();

			//						if (setMap(image, mapsDir, mapName)) {
			//								  currentMap = mapName;
			//						}
			//					}

			//					close(clientSocket);
			//					}
		//  );

		  QStringList mapFiles = mapsDir.entryList(
								QStringList() << "*.png",
								QDir::Files
								);


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

		  int result = app.exec();

		  close(serverSocket);
		  unlink(socketPath);

		  return result;
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
