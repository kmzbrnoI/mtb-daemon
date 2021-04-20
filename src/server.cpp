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
	this->clients.clear();
	if (!m_server.listen(addr, port))
		throw std::logic_error(m_server.errorString().toStdString());
}

void DaemonServer::serverNewConnection() {
	QTcpSocket* client = m_server.nextPendingConnection();
	log("New client: "+client->peerAddress().toString(), Mtb::LogLevel::Info);
	QObject::connect(client, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
	QObject::connect(client, SIGNAL(readyRead()), this, SLOT(clientReadyRead()));
	this->clients.insert_or_assign(client, true);
}

void DaemonServer::clientDisconnected() {
	QTcpSocket* client = static_cast<QTcpSocket*>(QObject::sender());
	log("Client disconnected: "+client->peerAddress().toString(), Mtb::LogLevel::Info);
	client->deleteLater();

	if (this->clients.find(client) != this->clients.end())
		this->clients.erase(client);

	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
		if (modules[i] != nullptr)
			modules[i]->clientDisconnected(client);
		if (subscribes[i].find(client) != subscribes[i].end())
			subscribes[i].erase(client);
	}
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

void DaemonServer::send(QTcpSocket* socket, const QJsonObject& jsonObj) {
	if (socket != nullptr)
		this->send(*socket, jsonObj);
}

void DaemonServer::broadcast(const QJsonObject& json) {
	for (const auto& pair : this->clients) {
		QTcpSocket* const socket = pair.first;
		this->send(*socket, json);
	}
}

QJsonObject DaemonServer::error(size_t code, const QString& message) {
	return {{"code", static_cast<int>(code)}, {"message", message}};
}

QJsonObject jsonError(size_t code, const QString& msg) {
	return QJsonObject{
		{"code", static_cast<int>(code)},
		{"message", msg},
	};
}

void sendError(QTcpSocket* socket, const QJsonObject& request, size_t code,
                      const QString& message) {
	QJsonObject response {
		{"command", request["command"]},
		{"type", "response"},
		{"status", "error"},
		{"error", jsonError(code, message)},
	};
	if (request.contains("id"))
		response["id"] = request["id"];
	server.send(*socket, response);
}
