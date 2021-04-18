#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include "server.h"
#include "mtbusb/mtbusb.h"
#include "main.h"

DaemonServer::DaemonServer(QObject *parent) : QObject(parent) {
	QObject::connect(&m_server, SIGNAL(newConnection()), this, SLOT(serverNewConnection()));
}

void DaemonServer::listen(const QHostAddress& addr, quint16 port) {
	m_server.listen(addr, port);
}

void DaemonServer::serverNewConnection() {
	QTcpSocket* client = m_server.nextPendingConnection();
	QObject::connect(client, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
	QObject::connect(client, SIGNAL(readyRead()), this, SLOT(clientReadyRead()));
}

void DaemonServer::clientDisconnected() {
	QTcpSocket* client = static_cast<QTcpSocket*>(QObject::sender());
	client->deleteLater();

	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
		if (subscribes[i].find(client) != subscribes[i].end())
			subscribes[i].erase(client);
}

void DaemonServer::clientReadyRead() {
	QTcpSocket* client = static_cast<QTcpSocket*>(QObject::sender());
	if (client->canReadLine()) {
		QByteArray data = client->readLine();
		if (data.size() > 0) {
			QJsonObject json = QJsonDocument::fromJson(data).object();
			jsonReceived(client, json);
		}
	}
}

void DaemonServer::send(QTcpSocket& socket, const QJsonObject& jsonObj) {
	socket.write(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
	socket.write("\n");
}

QJsonObject DaemonServer::error(size_t code, const QString& message) {
	return {{"code", static_cast<int>(code)}, {"message", message}};
}

QJsonObject jsonError(size_t code, const QString& msg) {
	QJsonObject error;
	error["code"] = static_cast<int>(code);
	error["message"] = msg;
	return error;
}

QJsonObject sendError(QTcpSocket* socket, const QJsonObject& request, size_t code,
                      const QString& message) {
	QJsonObject response;
	QJsonObject error;
	response["command"] = request["command"];
	response["type"] = "response";
	if (request.contains("id"))
		response["id"] = request["id"];
	response["status"] = "error";
	response["error"] = jsonError(code, message);
	server.send(*socket, response);
}
