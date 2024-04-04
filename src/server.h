#ifndef DAEMON_SERVER_H
#define DAEMON_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QJsonObject>
#include <QTimer>
#include "mtbusb.h"

constexpr size_t SERVER_DEFAULT_PORT = 3841;
constexpr size_t SERVER_KEEP_ALIVE_SEND_PERIOD_MS = 5000;

struct ServerRequest {
	QTcpSocket *socket;
	std::optional<size_t> id;

	ServerRequest(QTcpSocket *socket, std::optional<size_t> id = std::nullopt) : socket(socket), id(id) {}
	ServerRequest(QTcpSocket *socket, const QJsonObject& request) : socket(socket) {
		if (request.contains("id"))
			this->id = request["id"].toInt();
	}
};

class DaemonServer : public QObject {
	Q_OBJECT

public:
	DaemonServer(QObject *parent = nullptr);
	void listen(const QHostAddress&, quint16 port, bool keepAlive=true);
	void send(QTcpSocket&, const QJsonObject&);
	void send(QTcpSocket*, const QJsonObject&);
	void broadcast(const QJsonObject&);

	static QJsonObject error(size_t code, const QString& message);

private slots:
	void serverNewConnection();
	void clientDisconnected();
	void clientReadyRead();
	void tKeepAliveTick();

private:
	QTcpServer m_server;
	QTimer m_tKeepAlive;
	std::map<QTcpSocket*, bool> clients;

signals:
	void jsonReceived(QTcpSocket*, const QJsonObject&);
	void clientDisconnected(QTcpSocket*);

};

QJsonObject jsonError(size_t code, const QString &msg);
QJsonObject jsonError(Mtb::CmdError);
QJsonObject jsonOkResponse(const QJsonObject &request);
void sendError(QTcpSocket*, const QJsonObject&, size_t code, const QString&);
void sendError(QTcpSocket*, const QJsonObject&, Mtb::CmdError);
void sendError(QTcpSocket*, const QJsonObject &request, const QJsonObject &error);
void sendAccessDenied(QTcpSocket*, const QJsonObject&);

#endif
