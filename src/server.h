#ifndef DAEMON_SERVER_H
#define DAEMON_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QJsonObject>

constexpr size_t SERVER_DEFAULT_PORT = 3841;

struct ServerRequest {
	QTcpSocket* socket;
	std::optional<size_t> id;

	ServerRequest(QTcpSocket* socket, std::optional<size_t> id) : socket(socket), id(id) {}
	ServerRequest(QTcpSocket* socket, const QJsonObject& request) : socket(socket) {
		if (request.contains("id"))
			this->id = request["id"].toInt();
	}
};

class DaemonServer : public QObject {
	Q_OBJECT

public:
	DaemonServer(QObject *parent = nullptr);
	void listen(const QHostAddress&, quint16 port);
	void send(QTcpSocket&, const QJsonObject&);

	static QJsonObject error(size_t code, const QString& message);

private slots:
	void serverNewConnection();
	void clientDisconnected();
	void clientReadyRead();

private:
	QTcpServer m_server;

signals:
	void jsonReceived(QTcpSocket*, const QJsonObject&);

};

QJsonObject jsonError(size_t code, const QString& msg);
void sendError(QTcpSocket*, const QJsonObject&, size_t code, const QString&);

#endif
