#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <LayerShellQt/Window>
#include <QMargins>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QTextStream>
//#include <QSocketNotifier>
//#include <QLocalServer>
//#include <QLocalSocket>

//#include <QDebug>
#include <cerrno>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include <thread>
#include <mutex>

class MapChangeEvent : public QEvent{
		  public:
					 static constexpr QEvent::Type Type =static_cast<QEvent::Type>(QEvent::User + 1);

					 MapChangeEvent()
								: QEvent(Type){}
};

const char *socketPath = "/tmp/dbdoverlay.sock";
const char *pidPath = "/tmp/dbdoverlay.pid";

std::string requestedMap;
std::mutex mapMutex;

bool setMap(QLabel &image, const QDir &mapsDir, const QString &mapName, double opacity);

class MapLabel : public QLabel{
		  public:
					 MapLabel(QWidget *parent, const QDir &mapsDir, double opacity)
								: QLabel(parent), mapsDir(mapsDir), opacity(opacity){}
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
													 setMap(*this, mapsDir, mapName, opacity);
										  }

										  //delete event;
										  return true;
								}

								return QLabel::event(event);
					 }

		private:
					 QDir mapsDir;
					 double opacity;
};

//QLocalServer *server;

int main(int argc, char *argv[]){
		  
		  if (argc == 1){
					 pid_t pid = fork();
					 
					 if (pid > 0){
								return 0;
					 }

					 if (pid == -1) {
								return 1;
					 }

					 if (pid == 0){
								QFile pidFile(pidPath);

								if (pidFile.open(QIODevice::WriteOnly | QIODevice::Text)){
										  QTextStream pidStream(&pidFile);
										  pidStream << getpid();
								}
					 }
		  }

		  if (argc == 3 && QString(argv[1]) == "-map") {
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

		  if (argc == 2 && QString(argv[1]) == "-status"){
					 QFile pidFile(pidPath);

					 if (!pidFile.open(QIODevice::ReadOnly | QIODevice::Text)){
								qDebug() << "Overlay is not running.";
								return 0;
					 }

					 QTextStream pidStream(&pidFile);
					 QString pidText = pidStream.readAll();

					 pid_t pid = pidText.toLongLong();

					 if (kill(pid, 0) == 0){
								qDebug() << "Overlay is running. PID:" << pid;
					 } else{
								qDebug() << "Overlay is not running.";
					 }

					 //if (kill(pid, 0) == 0){
					 //			qDebug() << "Overlay is running. PID:" << pid;
					 //}
					 //else {
					 //			qDebug() << "Overlay is not running.";
					 //}
					 
					 return 0;
		  }

		  if (argc == 2 && QString(argv[1]) == "-stop"){
					 QFile pidFile(pidPath);

					 if (!pidFile.open(QIODevice::ReadOnly | QIODevice::Text)){
								qDebug() << "Overlay is not running.";
								return 0;
					 }

					 QTextStream pidStream(&pidFile);
					 QString pidText = pidStream.readAll();

					 pid_t pid = pidText.toLongLong();

					 if (kill(pid, SIGTERM) == 0){
								unlink(pidPath);
								qDebug() << "Overlay stopped.";
					 }
					 else {
								qDebug() << "Could not stop overlay.";
								qDebug() << "Error:" << strerror(errno);
					 }

					 return 0;
		  }

		  if (argc == 2 && QString(argv[1]) == "-select"){
					 QDir mapsDir(QDir::homePath() + "/.local/share/dbdoverlay/maps");

					 QStringList mapFiles = mapsDir.entryList(
										  QStringList() << "*.png",
										  QDir::Files
										  );

					 qDebug() << "Available maps:";

					 for (int i = 0; i < mapFiles.size(); i++){
								qDebug() << i + 1 << "." << mapFiles[i];
					 }

					 qDebug() << "Select a map with the arrow keys. Press Enter to select.";

					 int selection = 0;

					 struct termios oldSettings;
					 struct termios newSettings;

					 tcgetattr(STDIN_FILENO, &oldSettings);

					 newSettings = oldSettings;
					 newSettings.c_lflag &= ~(ICANON | ECHO);

					 tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);

					 char key;

					 while (true){
								read(STDIN_FILENO, &key, 1);

								if (key == '\n'){
										  break;
								}

								if (key == 27){
										  fd_set set;
										  FD_ZERO(&set);
										  FD_SET(STDIN_FILENO, &set);

										  timeval timeout{};
										  timeout.tv_sec = 0;
										  timeout.tv_usec = 50000;

										  int result = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout);

										  if (result == 0){
													 tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);

													 qDebug() << "Selection cancelled.";

													 return 0;
										  }

										  char sequence[2];

										  read(STDIN_FILENO, &sequence[0], 1);
										  read(STDIN_FILENO, &sequence[1], 1);

										  if (sequence[0] == '['){
													 if (sequence[1] == 'A'){
																selection--;

																if (selection < 0){
																		  selection = mapFiles.size() - 1;
																}
													 }
													 else if (sequence[1] == 'B'){
																selection++;

																if (selection >= mapFiles.size()) {
																		  selection = 0;
																}
													 }
										  }	
								}

								qDebug() << "Current selection:" << mapFiles[selection];
					 }

					 tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);

					 if (selection >= 0 && selection <= mapFiles.size()){
								QString mapName = mapFiles[selection];

								qDebug() << "Selected:" << mapName;

								int clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);

								if (clientSocket == -1){
										  qDebug() << "Could not connect to overlay.";
										  return 1;
								}

								sockaddr_un address{};
								address.sun_family = AF_UNIX;

								std::strncpy(address.sun_path, socketPath, sizeof(address.sun_path) - 1);

								if (connect(clientSocket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1){
										  close(clientSocket);
										  qDebug() << "Could not connect to overlay.";
										  return 1;
								}

								QByteArray mapData = mapName.toUtf8();

								write(clientSocket, mapData.constData(), mapData.size());

								close(clientSocket);

								qDebug() << "Map sent to overlay.";
					 }

					 return 0;
		  }

		  QApplication app(argc, argv);

		  QString configPath = QDir::homePath() + "/.config/dbdoverlay/config.conf";

		  double opacity = 0.7;
		  int size = 300;
		  double sizey = 0;
		  int marginTop = 10;
		  int marginBottom = 10;
		  int marginRight = 10;
		  int marginLeft = 10;
		  bool anchorTop = true;
		  bool anchorRight = true;
		  bool anchorBottom = false;
		  bool anchorLeft = false;

		  QFile configFile(configPath);

		  if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
					 qDebug() << "Could not open config:" << configPath;
		  }else {
					 QTextStream configStream(&configFile);

					 while (!configStream.atEnd()) {
								QString line = configStream.readLine();

								QStringList parts = line.split("=");

								if (parts.size() == 2){
										  QString key = parts[0];
										  QString value = parts[1];

										  if (key == "opacity"){
													 opacity = value.toDouble();
										  }

										  if (key == "size"){
													 size = value.toInt();
													 sizey = static_cast<double>(size) * 1.1;
										  }

										  if (key == "margin_top"){
													 marginTop = value.toInt();
										  }

										  if (key == "margin_button"){
													 marginBottom = value.toInt();
										  }

										  if (key == "margin_right"){
													 marginRight = value.toInt();
										  }

										  if (key == "margin_left"){
													 marginLeft = value.toInt();
										  }

										  if (key == "anchor_top") {
													 if (value == "true"){
																anchorTop = true;
													 }
													 else {
																anchorTop = false;
													 }
										  }

										  if (key == "anchor_bottom"){
													if (value == "true"){
															  anchorBottom = true;
													}
													else{
															  anchorTop = false;
													}
										  }

										  if (key == "anchor_left"){
													 if (value == "true"){
																anchorLeft = true;
													 }
													 else{
																anchorLeft = false;
													 }
										  }
										  
										  if (key == "anchor_right"){
													 if (value == "true"){
																anchorRight = true;
													 }
													 else{
																anchorRight = false;
													 }
										  }

								}
					 }
		  }

		  app.setApplicationName("dbdoverlay");
		  //app.setDesktopFileName("dbdoverlay");

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

		  LayerShellQt::Window::Anchors anchors;

		  if (anchorTop == true){
					 anchors |= LayerShellQt::Window::AnchorTop;
		  }

		  if (anchorBottom == true){
					 anchors |= LayerShellQt::Window::AnchorBottom;
		  }

		  if (anchorLeft == true){
					 anchors |= LayerShellQt::Window::AnchorLeft;
		  }

		  if (anchorRight == true){
					 anchors |= LayerShellQt::Window::AnchorRight;
		  }

		  layerWindow->setAnchors(anchors);

		  layerWindow->setMargins(QMargins(marginLeft, marginTop, marginRight, marginBottom));

		  QDir mapsDir(QDir::homePath() + "/.local/share/dbdoverlay/maps");

		  MapLabel image(&window, mapsDir, opacity);

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

		  if (!setMap(image, mapsDir, currentMap, opacity)){
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
		  image.resize(size, sizey);

		  window.resize(size, sizey);


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
		  unlink(pidPath);

		  return result;
}

bool setMap(QLabel &image, const QDir &mapsDir, const QString &mapName, double opacity){
		  QPixmap map(mapsDir.filePath(mapName));

		  if (map.isNull()) {
					 return false;
		  }

		  QPixmap transparentMap(map.size());
		  transparentMap.fill(Qt::transparent);

		  QPainter painter(&transparentMap);
		  painter.setOpacity(opacity);
		  painter.drawPixmap(0, 0, map);
		  painter.end();

		  image.setPixmap(transparentMap);

		  return true;
}
